#include "preview_video.h"

#include <gst/gst.h>

#include <QLoggingCategory>
#include <QUrl>

#include <chrono>
#include <cstring>
#include <future>
#include <thread>

Q_LOGGING_CATEGORY(lcPreviewVideo, "naomi.preview")

namespace {

qint64 nowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// чей это обратный вызов: ролик (поколение) и конвейер (основной или первый кадр)
struct Ctx
{
    PreviewVideo *self;
    int generation;
    bool poster;
};

void freeCtx(gpointer data, GClosure *) { delete static_cast<Ctx *>(data); }
void freeCtxNotify(gpointer data) { delete static_cast<Ctx *>(data); }

GstFlowReturn onSample(GstElement *sink, gpointer data, const char *action)
{
    auto *c = static_cast<Ctx *>(data);
    GstSample *sample = nullptr;
    g_signal_emit_by_name(sink, action, &sample);
    if (!sample)
        return GST_FLOW_ERROR;
    c->self->sampleArrived(sample, c->generation, c->poster);   // забирает себе
    return GST_FLOW_OK;
}
GstFlowReturn onNewSample(GstElement *sink, gpointer data) { return onSample(sink, data, "pull-sample"); }
// кадр на паузе (первый, после перемотки) приходит «предкадром»
GstFlowReturn onNewPreroll(GstElement *sink, gpointer data) { return onSample(sink, data, "pull-preroll"); }

GstBusSyncReply onBus(GstBus *, GstMessage *message, gpointer data)
{
    auto *c = static_cast<Ctx *>(data);
    const GstMessageType type = GST_MESSAGE_TYPE(message);
    if (type == GST_MESSAGE_EOS || type == GST_MESSAGE_ERROR)
        c->self->busMessage(message, c->generation);
    // GST_BUS_DROP — сообщение наше, и освободить его должны мы: без этого каждое оставалось жить и держало свой
    // источник — сам конвейер и его элементы. Конвейер после закрытия ролика не уничтожался: на каждый ролик
    // «Просмотра» в оболочке оставались пара сокетов шины, буферы декодера (dma-buf) и ~7 МБ (прогон стола
    // 19 ноября 2026: за 18 роликов сокетов 17 → 61, dma-buf 20 → 46, память 224 → 350 МБ)
    gst_message_unref(message);
    return GST_BUS_DROP;
}

// какой декодер выбрал конвейер — в журнал (проверкам: аппаратный ли)
void onElementAdded(GstBin *, GstBin *, GstElement *element, gpointer data)
{
    auto *c = static_cast<Ctx *>(data);
    GstElementFactory *f = gst_element_get_factory(element);
    const char *klass = f ? gst_element_factory_get_metadata(f, GST_ELEMENT_METADATA_KLASS) : nullptr;
    if (klass && strstr(klass, "Decoder/Video"))
        qCInfo(lcPreviewVideo, "видео: декодер %s%s", GST_OBJECT_NAME(f), c->poster ? " (первый кадр)" : "");
}

// первый кадр — без декодеров Android: их запуск и есть та секунда (GstAutoplugSelectResult: 0 — пробовать, 2 — мимо)
int skipAndroid(GstElement *, GstPad *, GstCaps *, GstElementFactory *factory, gpointer)
{
    return g_str_has_prefix(GST_OBJECT_NAME(factory), "amc") ? 2 : 0;
}

void posterPad(GstElement *, GstPad *pad, gpointer conv)
{
    GstPad *sink = gst_element_get_static_pad(GST_ELEMENT(conv), "sink");
    if (!gst_pad_is_linked(sink))
        gst_pad_link(pad, sink);
    gst_object_unref(sink);
}

// GstVideoMeta (gst/video/gstvideometa.h, заголовков gst-plugins-base в среде сборки нет): раскладка та же с 1.0 —
// откуда плоскости и шаг строк, если декодер положил кадр не плотно
struct VideoMetaMirror
{
    GstMeta meta;
    GstBuffer *buffer;
    int flags;
    int format;
    guint id;
    guint width, height, n_planes;
    gsize offset[4];
    gint stride[4];
};

// YUV → RGB по colorimetry кадра (GstVideoColorimetry: «bt709», «bt601», «jpeg» или «2:3:5:1» —
// диапазон:матрица:передача:основные): матрица и диапазон; нет — BT.709 для HD, BT.601 для SD, ограниченный
QMatrix4x4 yuvMatrix(const char *colorimetry, int height)
{
    const QString c = QString::fromLatin1(colorimetry ? colorimetry : "");
    bool full = false;
    double kr = height >= 720 ? 0.2126 : 0.299, kb = height >= 720 ? 0.0722 : 0.114;
    auto bt601 = [&] { kr = 0.299; kb = 0.114; };
    auto bt709 = [&] { kr = 0.2126; kb = 0.0722; };
    auto bt2020 = [&] { kr = 0.2627; kb = 0.0593; };
    if (c == QLatin1String("bt601") || c == QLatin1String("smpte240m"))
        bt601();
    else if (c == QLatin1String("bt709"))
        bt709();
    else if (c.startsWith(QLatin1String("bt2020")) || c.startsWith(QLatin1String("bt2100")))
        bt2020();
    else if (c == QLatin1String("jpeg")) {
        bt601();
        full = true;
    } else if (const QStringList p = c.split(':'); p.size() == 4) {
        full = p[0] == QLatin1String("1");
        const int m = p[1].toInt();   // 3 — BT.709, 4 — BT.601, 6 — BT.2020
        if (m == 3) bt709();
        else if (m == 4) bt601();
        else if (m == 6) bt2020();
    }
    const double kg = 1 - kr - kb;
    const double ys = full ? 1 : 255.0 / 219, yo = full ? 0 : 16.0 / 255, cs = full ? 1 : 255.0 / 224;
    const double rv = cs * 2 * (1 - kr), bu = cs * 2 * (1 - kb);
    const double gu = cs * 2 * (1 - kb) * kb / kg, gv = cs * 2 * (1 - kr) * kr / kg;
    // строка — канал: R = ys·(Y − yo) + rv·(V − ½) и т. д.; столбец 4 — сдвиги
    return QMatrix4x4(float(ys), 0, float(rv), float(-ys * yo - rv * 0.5),
                      float(ys), float(-gu), float(-gv), float(-ys * yo + (gu + gv) * 0.5),
                      float(ys), float(bu), 0, float(-ys * yo - bu * 0.5),
                      0, 0, 0, 1);
}

} // namespace

VideoFrame::~VideoFrame()
{
    if (mapping) {
        gst_buffer_unmap(gst_sample_get_buffer(sample), static_cast<GstMapInfo *>(mapping));
        delete static_cast<GstMapInfo *>(mapping);
    }
    if (sample)
        gst_sample_unref(sample);
}

// GStreamer — в своём потоке, с запуска оболочки: первый gst_init читает реестр модулей ~70 мс, и в потоке интерфейса
// это было замирание стола на первом ролике «Просмотра» (89 мс, прогон стола 19 ноября 2026: 73 % выборок стека —
// gst_update_registry; docs/DESKTOP.md, «Прогон стола»)
static std::shared_future<bool> gstReady()
{
    static const std::shared_future<bool> ready = std::async(std::launch::async, [] {
        GError *error = nullptr;
        const bool ok = gst_init_check(nullptr, nullptr, &error);
        if (!ok) {
            qCWarning(lcPreviewVideo, "GStreamer не запустился: %s", error ? error->message : "");
            g_clear_error(&error);
            return false;
        }
        // и модули, что берёт «Просмотр», — тоже здесь: первый ролик грузил их (dlopen) в потоках GStreamer, держа
        // блокировку загрузчика, а поток интерфейса в это время стоял в dlsym (libhybris ищет функции GL при первом
        // вызове): кадр стола 65 мс (выборки стека: glDeleteRenderbuffers → dlsym → pthread_mutex_lock)
        for (const char *name : {"playbin", "uridecodebin", "decodebin", "appsink", "videoconvert", "typefind",
                                 "qtdemux", "matroskademux", "h264parse", "h265parse", "vp9parse", "queue",
                                 "multiqueue", "pulsesink", "autoaudiosink", "audioconvert", "audioresample", "volume",
                                 "avdec_h264", "openh264dec", "libde265dec", "vp9dec", "vp8dec", "opusdec",
                                 "vorbisdec", "avdec_aac", "faad"})
            if (GstElementFactory *f = gst_element_factory_find(name)) {
                if (GstPluginFeature *loaded = gst_plugin_feature_load(GST_PLUGIN_FEATURE(f)))
                    gst_object_unref(loaded);
                gst_object_unref(f);
            }
        GList *decoders = gst_element_factory_list_get_elements(
            GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_NONE);
        for (GList *l = decoders; l; l = l->next)
            if (g_str_has_prefix(GST_OBJECT_NAME(l->data), "amc")) {   // аппаратные (libhybris) — один модуль на все
                if (GstPluginFeature *loaded = gst_plugin_feature_load(GST_PLUGIN_FEATURE(l->data)))
                    gst_object_unref(loaded);
                break;
            }
        gst_plugin_feature_list_free(decoders);
        return true;
    }).share();
    return ready;
}

PreviewVideo::PreviewVideo(QObject *parent)
    : QObject(parent)
{
    gstReady();   // начать сейчас, к первому ролику готово
    m_tick.setInterval(250);
    connect(&m_tick, &QTimer::timeout, this, &PreviewVideo::updatePosition);
}

PreviewVideo::~PreviewVideo()
{
    // выход оболочки: конвейеры — сразу и здесь же, их потоки не должны пережить этот объект
    ++m_generation;
    for (GstElement *p : {m_play, m_poster})
        if (p) {
            gst_element_set_state(p, GST_STATE_NULL);
            gst_object_unref(p);
        }
    if (m_pending)
        gst_sample_unref(m_pending);
}

void PreviewVideo::teardown(GstElement *pipeline)
{
    if (!pipeline)
        return;
    std::thread([pipeline] {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }).detach();
}

void PreviewVideo::open(const QString &path)
{
    close();
    const bool ready = gstReady().get();
    ++m_generation;
    m_error.clear();
    m_ended = false;
    m_started = false;
    m_position = m_duration = 0;
    m_openedUs = nowUs();
    const QByteArray uri = QUrl::fromLocalFile(path).toEncoded();

    GstElement *play = ready ? gst_element_factory_make("playbin", nullptr) : nullptr;
    GstElement *sink = ready ? gst_element_factory_make("appsink", nullptr) : nullptr;
    if (!play || !sink) {
        if (play)
            gst_object_unref(play);
        if (sink)
            gst_object_unref(sink);
        m_error = QStringLiteral("нет GStreamer playbin");
        emit stateChanged();
        return;
    }
    GstCaps *nv12 = gst_caps_from_string("video/x-raw,format=NV12");
    // кадры — NV12, как у декодера (цвет — шейдер); не копятся: не успели — старый выброшен
    g_object_set(sink, "caps", nv12, "max-buffers", 2, "drop", TRUE, "sync", TRUE, "emit-signals", TRUE, nullptr);
    g_signal_connect_data(sink, "new-sample", G_CALLBACK(onNewSample), new Ctx{this, m_generation, false}, freeCtx,
                          GConnectFlags(0));
    g_signal_connect_data(sink, "new-preroll", G_CALLBACK(onNewPreroll), new Ctx{this, m_generation, false}, freeCtx,
                          GConnectFlags(0));
    g_signal_connect_data(play, "deep-element-added", G_CALLBACK(onElementAdded), new Ctx{this, m_generation, false},
                          freeCtx, GConnectFlags(0));
    // GstPlayFlags: видео, звук, громкость — без субтитров, деинтерлейса и цветокоррекции
    g_object_set(play, "uri", uri.constData(), "video-sink", sink, "flags", 0x1 | 0x2 | 0x10, nullptr);
    GstBus *bus = gst_element_get_bus(play);
    gst_bus_set_sync_handler(bus, onBus, new Ctx{this, m_generation, false}, freeCtxNotify);
    gst_object_unref(bus);
    m_play = play;

    // Первый кадр — программным декодером (uridecodebin без amc…): кадр на паузе, предкадром
    GstElement *poster = gst_pipeline_new(nullptr);
    GstElement *dec = gst_element_factory_make("uridecodebin", nullptr);
    GstElement *conv = gst_element_factory_make("videoconvert", nullptr);
    GstElement *psink = gst_element_factory_make("appsink", nullptr);
    if (poster && dec && conv && psink) {
        GstCaps *raw = gst_caps_from_string("video/x-raw");
        g_object_set(dec, "uri", uri.constData(), "caps", raw, "expose-all-streams", FALSE, nullptr);
        gst_caps_unref(raw);
        g_object_set(psink, "caps", nv12, "max-buffers", 1, "sync", FALSE, "emit-signals", TRUE, nullptr);
        g_signal_connect(dec, "autoplug-select", G_CALLBACK(skipAndroid), nullptr);
        g_signal_connect(dec, "pad-added", G_CALLBACK(posterPad), conv);
        g_signal_connect_data(psink, "new-preroll", G_CALLBACK(onNewPreroll), new Ctx{this, m_generation, true},
                              freeCtx, GConnectFlags(0));
        g_signal_connect_data(dec, "deep-element-added", G_CALLBACK(onElementAdded),
                              new Ctx{this, m_generation, true}, freeCtx, GConnectFlags(0));
        gst_bin_add_many(GST_BIN(poster), dec, conv, psink, nullptr);
        gst_element_link(conv, psink);
        m_poster = poster;
        gst_element_set_state(poster, GST_STATE_PAUSED);
    } else {
        for (GstElement *e : {poster, dec, conv, psink})
            if (e)
                gst_object_unref(e);
    }
    gst_caps_unref(nv12);

    m_playing = gst_element_set_state(play, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE;
    m_tick.start();
    qCInfo(lcPreviewVideo, "видео: открываю %s", qPrintable(path));
    emit stateChanged();
    emit positionChanged();
}

void PreviewVideo::close()
{
    if (!m_play && !m_poster && !m_frame)
        return;
    ++m_generation;
    teardown(m_play);
    m_play = nullptr;
    stopPoster();
    m_tick.stop();
    m_frame.reset();
    m_playing = m_ended = m_started = false;
    m_frameSize = QSize();
    emit frameSizeChanged();
    emit stateChanged();
}

void PreviewVideo::stopPoster()
{
    teardown(m_poster);
    m_poster = nullptr;
}

void PreviewVideo::toggle()
{
    if (!m_play)
        return;
    if (m_ended) {   // доиграл — с начала
        m_ended = false;
        m_playing = true;
        gst_element_seek_simple(m_play, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE), 0);
    } else {
        m_playing = !m_playing;
    }
    gst_element_set_state(m_play, m_playing ? GST_STATE_PLAYING : GST_STATE_PAUSED);
    emit stateChanged();
}

void PreviewVideo::pause()
{
    if (m_play && m_playing)
        toggle();
}

void PreviewVideo::seek(double fraction)
{
    gint64 duration = 0;
    if (!m_play || !gst_element_query_duration(m_play, GST_FORMAT_TIME, &duration) || duration <= 0)
        return;
    gst_element_seek_simple(m_play, GST_FORMAT_TIME, GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                            gint64(qBound(0.0, fraction, 1.0) * double(duration)));
    if (m_ended) {
        m_ended = false;
        emit stateChanged();
    }
    updatePosition();
}

void PreviewVideo::updatePosition()
{
    gint64 pos = 0, dur = 0;
    if (!m_play)
        return;
    // раз в секунду, пока играет, — строка в журнал: кадров прислал декодер и дошло до стола
    if (const qint64 now = nowUs(); now - m_statFrom >= 1000000) {
        int arrived;
        {
            QMutexLocker lock(&m_mutex);
            arrived = m_statArrived;
            m_statArrived = 0;
        }
        if (m_playing && m_started)
            qCInfo(lcPreviewVideo, "видео: кадров %d (декодер прислал %d)", m_statShown, arrived);
        m_statShown = 0;
        m_statFrom = now;
    }
    const double p = gst_element_query_position(m_play, GST_FORMAT_TIME, &pos) ? pos / 1e9 : m_position;
    const double d = gst_element_query_duration(m_play, GST_FORMAT_TIME, &dur) ? dur / 1e9 : m_duration;
    if (p != m_position || d != m_duration) {
        m_position = p;
        m_duration = d;
        emit positionChanged();
    }
}

void PreviewVideo::sampleArrived(GstSample *sample, int generation, bool poster)
{
    QMutexLocker lock(&m_mutex);
    ++m_statArrived;
    if (m_pending)
        gst_sample_unref(m_pending);
    m_pending = sample;
    m_pendingGeneration = generation;
    m_pendingPoster = poster;
    if (!m_queued) {
        m_queued = true;
        QMetaObject::invokeMethod(this, &PreviewVideo::deliver, Qt::QueuedConnection);
    }
}

void PreviewVideo::busMessage(GstMessage *message, int generation)
{
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_EOS) {
        QMetaObject::invokeMethod(this, [this, generation] { onEnded(generation); }, Qt::QueuedConnection);
        return;
    }
    GError *error = nullptr;
    gst_message_parse_error(message, &error, nullptr);
    const QString text = QString::fromUtf8(error ? error->message : "ошибка");
    qCWarning(lcPreviewVideo, "видео: %s", qPrintable(text));
    g_clear_error(&error);
    QMetaObject::invokeMethod(this, [this, generation, text] { onError(generation, text); }, Qt::QueuedConnection);
}

void PreviewVideo::onEnded(int generation)
{
    if (generation != m_generation || !m_play)
        return;
    // доиграл: стоит на последнем кадре (на паузе — перемотка покажет кадр, а не пойдёт дальше сама)
    m_ended = true;
    m_playing = false;
    gst_element_set_state(m_play, GST_STATE_PAUSED);
    updatePosition();
    if (m_duration > 0 && m_position < m_duration) {
        m_position = m_duration;
        emit positionChanged();
    }
    emit stateChanged();
}

void PreviewVideo::onError(int generation, const QString &text)
{
    if (generation != m_generation)
        return;
    close();
    m_error = text;
    emit stateChanged();
}

void PreviewVideo::deliver()
{
    GstSample *sample;
    int generation;
    bool poster;
    {
        QMutexLocker lock(&m_mutex);
        sample = m_pending;
        m_pending = nullptr;
        generation = m_pendingGeneration;
        poster = m_pendingPoster;
        m_queued = false;
    }
    if (!sample)
        return;
    if (generation != m_generation || (poster && m_started)) {
        gst_sample_unref(sample);
        return;
    }
    auto frame = std::make_shared<VideoFrame>();
    frame->sample = sample;
    const GstStructure *st = gst_caps_get_structure(gst_sample_get_caps(sample), 0);
    int w = 0, h = 0;
    gst_structure_get_int(st, "width", &w);
    gst_structure_get_int(st, "height", &h);
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    auto *map = new GstMapInfo;
    if (w <= 0 || h <= 0 || !buffer || !gst_buffer_map(buffer, map, GST_MAP_READ)) {
        delete map;
        return;
    }
    frame->mapping = map;
    // плоскости: по GstVideoMeta, если есть, иначе плотно — как раскладывает GStreamer по умолчанию
    gsize offY = 0, offUV = 0;
    int strideY = (w + 3) & ~3, strideUV = strideY;
    offUV = gsize(strideY) * ((h + 1) & ~1);
    static const GType metaApi = g_type_from_name("GstVideoMetaAPI");
    if (metaApi)
        if (auto *meta = reinterpret_cast<VideoMetaMirror *>(gst_buffer_get_meta(buffer, metaApi));
            meta && meta->n_planes >= 2) {
            offY = meta->offset[0];
            offUV = meta->offset[1];
            strideY = meta->stride[0];
            strideUV = meta->stride[1];
        }
    const int ch = (h + 1) / 2;
    if (offY + gsize(strideY) * h > map->size || offUV + gsize(strideUV) * ch > map->size) {
        qCWarning(lcPreviewVideo, "видео: кадр %dx%d не той раскладки (%zu байт)", w, h, map->size);
        return;
    }
    frame->y = map->data + offY;
    frame->uv = map->data + offUV;
    frame->width = w;
    frame->height = h;
    frame->strideY = strideY;
    frame->strideUV = strideUV;
    frame->serial = ++m_serial;
    m_frame = frame;
    ++m_statShown;
    const qint64 ms = (nowUs() - m_openedUs) / 1000;
    if (poster)
        qCInfo(lcPreviewVideo, "видео: первый кадр %dx%d через %lld мс", w, h, ms);
    if (!poster && !m_started) {
        m_started = true;
        stopPoster();
        qCInfo(lcPreviewVideo, "видео: %dx%d, играет (через %lld мс)", w, h, ms);
    }
    const QMatrix4x4 matrix = yuvMatrix(gst_structure_get_string(st, "colorimetry"), h);
    if (QSize(w, h) != m_frameSize || matrix != m_matrix) {
        m_frameSize = QSize(w, h);
        m_matrix = matrix;
        emit frameSizeChanged();
    }
    emit frameChanged();
}
