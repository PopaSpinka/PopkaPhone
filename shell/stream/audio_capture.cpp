#include "audio_capture.h"

#include <QLoggingCategory>

#include <cmath>
#include <ctime>
#include <pthread.h>
#include <unistd.h>
#include <utility>
#include <vector>

Q_LOGGING_CATEGORY(lcCapture, "naomi.audio")

// libpulse (PulseAudio 16), асинхронный интерфейс: объявления по pulse/thread-mainloop.h, pulse/context.h,
// pulse/stream.h, pulse/sample.h, pulse/def.h, pulse/error.h. Не pa_simple: ему не задать PA_STREAM_DONT_MOVE
extern "C" {
struct pa_threaded_mainloop;
struct pa_mainloop_api;
struct pa_context;
struct pa_stream;
struct pa_sample_spec {
    int format;          // pa_sample_format_t
    uint32_t rate;
    uint8_t channels;
};
struct pa_buffer_attr {
    uint32_t maxlength, tlength, prebuf, minreq, fragsize;
};
typedef void (*pa_context_notify_cb_t)(pa_context *c, void *userdata);
typedef void (*pa_stream_notify_cb_t)(pa_stream *s, void *userdata);
typedef void (*pa_stream_request_cb_t)(pa_stream *s, size_t nbytes, void *userdata);
pa_threaded_mainloop *pa_threaded_mainloop_new(void);
int pa_threaded_mainloop_start(pa_threaded_mainloop *m);
void pa_threaded_mainloop_stop(pa_threaded_mainloop *m);
void pa_threaded_mainloop_free(pa_threaded_mainloop *m);
void pa_threaded_mainloop_lock(pa_threaded_mainloop *m);
void pa_threaded_mainloop_unlock(pa_threaded_mainloop *m);
void pa_threaded_mainloop_wait(pa_threaded_mainloop *m);
void pa_threaded_mainloop_signal(pa_threaded_mainloop *m, int wait_for_accept);
pa_mainloop_api *pa_threaded_mainloop_get_api(pa_threaded_mainloop *m);
pa_context *pa_context_new(pa_mainloop_api *mainloop, const char *name);
void pa_context_set_state_callback(pa_context *c, pa_context_notify_cb_t cb, void *userdata);
int pa_context_connect(pa_context *c, const char *server, int flags, const void *api);
int pa_context_get_state(const pa_context *c);
int pa_context_errno(const pa_context *c);
void pa_context_disconnect(pa_context *c);
void pa_context_unref(pa_context *c);
pa_stream *pa_stream_new(pa_context *c, const char *name, const pa_sample_spec *ss, const void *map);
void pa_stream_set_state_callback(pa_stream *s, pa_stream_notify_cb_t cb, void *userdata);
void pa_stream_set_read_callback(pa_stream *s, pa_stream_request_cb_t cb, void *userdata);
int pa_stream_connect_record(pa_stream *s, const char *dev, const pa_buffer_attr *attr, int flags);
int pa_stream_get_state(const pa_stream *s);
int pa_stream_peek(pa_stream *s, const void **data, size_t *nbytes);
int pa_stream_drop(pa_stream *s);
int pa_stream_get_latency(pa_stream *s, uint64_t *r_usec, int *negative);
int pa_stream_disconnect(pa_stream *s);
void pa_stream_unref(pa_stream *s);
const char *pa_strerror(int error);
// пауза записи, пока на выход никто не играет (start, gateSink): события потоков и список потоков с их выходом.
// Из структур сведений — только начала: они не менялись с 0.9 (pa_sink_info — name, index; pa_sink_input_info —
// index, name, owner_module, client, sink), а одинаковые начала у C-структур лежат одинаково
struct pa_operation;
struct pa_sink_info_head { const char *name; uint32_t index; };
struct pa_sink_input_info_head { uint32_t index; const char *name; uint32_t owner_module; uint32_t client; uint32_t sink; };
typedef void (*pa_context_subscribe_cb_t)(pa_context *c, int t, uint32_t idx, void *userdata);
typedef void (*pa_info_cb_t)(pa_context *c, const void *info, int eol, void *userdata);
void pa_operation_unref(pa_operation *o);
pa_operation *pa_stream_cork(pa_stream *s, int b, void *cb, void *userdata);
void pa_context_set_subscribe_callback(pa_context *c, pa_context_subscribe_cb_t cb, void *userdata);
pa_operation *pa_context_subscribe(pa_context *c, int mask, void *cb, void *userdata);
pa_operation *pa_context_get_sink_info_by_name(pa_context *c, const char *name, pa_info_cb_t cb, void *userdata);
pa_operation *pa_context_get_sink_input_info_list(pa_context *c, pa_info_cb_t cb, void *userdata);
}

namespace {

constexpr int kContextReady = 4, kContextFailed = 5, kContextTerminated = 6;   // pa_context_state_t
constexpr int kStreamReady = 2, kStreamFailed = 3, kStreamTerminated = 4;      // pa_stream_state_t
// pa_stream_flags_t: INTERPOLATE_TIMING | AUTO_TIMING_UPDATE | DONT_MOVE | ADJUST_LATENCY
constexpr int kRecordFlags = 0x2 | 0x8 | 0x200 | 0x2000;
constexpr int kSubscribeSinkInput = 0x0004;   // PA_SUBSCRIPTION_MASK_SINK_INPUT

// Кто играет на выход (gateSink) — для паузы записи: номер выхода, сколько потоков на нём, пора пересчитать
struct Gate {
    pa_threaded_mainloop *ml = nullptr;
    uint32_t sink = uint32_t(-1);
    bool sinkKnown = false;
    int counting = 0;      // потоков на выходе — считается (список ещё идёт)
    int players = -1;      // потоков на выходе по последнему списку; −1 — ещё не знаем
    bool dirty = true;     // событие потоков — пересчитать
    bool fresh = false;    // появился новый поток — пауза снимается сразу, до пересчёта (не терять начало звука)
};

void unref(pa_operation *o)
{
    if (o)
        pa_operation_unref(o);
}

int64_t monoUs()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return int64_t(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
}

void wake(pa_threaded_mainloop *m) { pa_threaded_mainloop_signal(m, 0); }

} // namespace

// Захват — свой цикл PulseAudio (pa_threaded_mainloop) и поток, который ждёт данных и режет их на куски ровно по
// chunkMs. Запись — с PA_STREAM_DONT_MOVE: пропал источник (выход «Монитор» пересоздали, наушники Bluetooth ушли) —
// PulseAudio запись обрывает, а не переносит на источник по умолчанию: им у телефона стоит микрофон, а запись с
// микрофона при игре звука вешает драйвер звука Android до перезагрузки (docs/HWTEST.md; так и случилось 16 ноября
// 2026 — захват «Монитора» после его пересоздания тихо писал микрофон). Оборвался — снова через 0,5 с, по имени.
void AudioCapture::start(const std::string &device, uint32_t rate, uint8_t channels, int chunkMs, Chunk onChunk,
                         Format format, const std::string &gateSink, Idle onIdle)
{
    stop();
    auto st = std::make_shared<State>();
    m_state = st;
    m_thread = std::thread([st, device, rate, channels, chunkMs, format, gateSink, onChunk = std::move(onChunk),
                            onIdle = std::move(onIdle)] {
        pthread_setname_np(pthread_self(), "naomi-audio");
        const size_t frames = size_t(rate) * size_t(chunkMs) / 1000;
        const size_t frameBytes = size_t(channels) * (format == F32 ? 4 : 2);
        const size_t bytes = frames * frameBytes;
        const pa_sample_spec spec{int(format), rate, channels};
        // кусок — chunkMs: столько PulseAudio копит, прежде чем отдать (fragsize); остальное — как он решит
        const pa_buffer_attr attr{uint32_t(-1), uint32_t(-1), uint32_t(-1), uint32_t(-1), uint32_t(bytes)};
        const double chunkUs = double(frames) * 1e6 / rate;
        const std::string name = "naomi " + device;
        bool warned = false;
        while (!st->quit) {
            pa_threaded_mainloop *ml = pa_threaded_mainloop_new();
            pa_threaded_mainloop_start(ml);
            {
                std::lock_guard<std::mutex> lock(st->m);
                st->loop = ml;
            }
            pa_threaded_mainloop_lock(ml);
            pa_context *ctx = pa_context_new(pa_threaded_mainloop_get_api(ml), "naomi-shell");
            pa_context_set_state_callback(ctx, [](pa_context *, void *m) { wake(static_cast<pa_threaded_mainloop *>(m)); }, ml);
            pa_stream *s = nullptr;
            const char *why = nullptr;
            if (pa_context_connect(ctx, nullptr, 0, nullptr) < 0) {
                why = pa_strerror(pa_context_errno(ctx));
            } else {
                for (int cs; (cs = pa_context_get_state(ctx)) != kContextReady && !st->quit;) {
                    if (cs == kContextFailed || cs == kContextTerminated) {
                        why = pa_strerror(pa_context_errno(ctx));
                        break;
                    }
                    pa_threaded_mainloop_wait(ml);
                }
            }
            if (!why && !st->quit) {
                s = pa_stream_new(ctx, name.c_str(), &spec, nullptr);
                pa_stream_set_state_callback(s, [](pa_stream *, void *m) { wake(static_cast<pa_threaded_mainloop *>(m)); }, ml);
                pa_stream_set_read_callback(s, [](pa_stream *, size_t, void *m) { wake(static_cast<pa_threaded_mainloop *>(m)); }, ml);
                if (pa_stream_connect_record(s, device.c_str(), &attr, kRecordFlags) < 0)
                    why = pa_strerror(pa_context_errno(ctx));
                for (int ss; !why && (ss = pa_stream_get_state(s)) != kStreamReady && !st->quit;) {
                    if (ss == kStreamFailed || ss == kStreamTerminated) {
                        why = pa_strerror(pa_context_errno(ctx));
                        break;
                    }
                    pa_threaded_mainloop_wait(ml);
                }
            }
            if (why) {
                if (!warned)
                    qCInfo(lcCapture, "звук: %s не открылся (%s) — пробую раз в 0,5 с", device.c_str(), why);
                warned = true;
            } else if (!st->quit) {
                qCInfo(lcCapture, "звук: захват %s, %u Гц × %u%s, куски по %d мс", device.c_str(), rate, channels,
                       format == F32 ? " (float)" : "", chunkMs);
                warned = false;
                // Пауза записи, пока на выход gateSink никто не играет (docs/DESKTOP.md, «Звук стола в тишине»): пустой
                // выход PulseAudio усыпляет (module-suspend-on-idle, 5 с), и ни он, ни мы не просыпаемся 100 раз в
                // секунду ради тишины — было ~5 % ядра в покое. Поток появился — пауза снимается сразу
                Gate gate;
                gate.ml = ml;
                bool corked = false;
                if (!gateSink.empty()) {
                    pa_context_set_subscribe_callback(ctx, [](pa_context *, int t, uint32_t, void *g) {
                        auto *gt = static_cast<Gate *>(g);
                        gt->dirty = true;
                        if ((t & 0x30) == 0x00)   // PA_SUBSCRIPTION_EVENT_NEW
                            gt->fresh = true;
                        wake(gt->ml);
                    }, &gate);
                    unref(pa_context_subscribe(ctx, kSubscribeSinkInput, nullptr, nullptr));
                    unref(pa_context_get_sink_info_by_name(ctx, gateSink.c_str(), [](pa_context *, const void *i, int eol, void *g) {
                        auto *gt = static_cast<Gate *>(g);
                        if (!eol && i) {
                            gt->sink = static_cast<const pa_sink_info_head *>(i)->index;
                            gt->sinkKnown = true;
                            gt->dirty = true;
                        }
                        wake(gt->ml);
                    }, &gate));
                }
                // Метки — по счётчику: t0 (когда сыгран первый отсчёт потока) + номер куска × chunkMs. t0 — по оценкам
                // «сейчас − что ещё в буферах PulseAudio − что у нас сверх куска − кусок − номер × chunkMs»,
                // сглаженно (2 % за кусок: дрожание чтения и оценки метки не трогает); оценка ушла больше чем на
                // 20 мс (поток стоял) — t0 заново
                uint64_t n = 0;
                double t0 = 0;
                bool have = false;
                std::vector<uint8_t> acc;
                while (!st->quit) {
                    if (pa_stream_get_state(s) != kStreamReady || pa_context_get_state(ctx) != kContextReady) {
                        qCInfo(lcCapture, "звук: захват %s оборвался (%s)", device.c_str(),
                               pa_strerror(pa_context_errno(ctx)));
                        break;
                    }
                    if (std::exchange(gate.fresh, false) && corked) {   // новый поток — сразу, пересчёт — следом
                        corked = false;
                        unref(pa_stream_cork(s, 0, nullptr, nullptr));
                        qCInfo(lcCapture, "звук: захват %s — дальше: появился поток", device.c_str());
                        if (onIdle)
                            onIdle(false);
                    }
                    if (gate.sinkKnown && gate.dirty && gate.counting == 0) {   // пересчитать, кто играет на выход
                        gate.dirty = false;
                        gate.counting = 1;
                        gate.players = 0;
                        unref(pa_context_get_sink_input_info_list(ctx, [](pa_context *, const void *i, int eol, void *g) {
                            auto *gt = static_cast<Gate *>(g);
                            if (!eol && i) {
                                if (static_cast<const pa_sink_input_info_head *>(i)->sink == gt->sink)
                                    ++gt->players;
                            } else {
                                gt->counting = 0;
                            }
                            wake(gt->ml);
                        }, &gate));
                    }
                    if (gate.counting == 0 && gate.players >= 0 && (gate.players == 0) != corked) {
                        corked = gate.players == 0;
                        unref(pa_stream_cork(s, corked ? 1 : 0, nullptr, nullptr));
                        if (corked) {
                            acc.clear();   // недорезанный кусок — не склеивать с тем, что придёт после паузы
                            have = false;  // метки — заново от первого куска после паузы
                        }
                        qCInfo(lcCapture, "звук: захват %s — %s", device.c_str(),
                               corked ? "пауза: на выход никто не играет" : "дальше: на выход играют");
                        if (onIdle)
                            onIdle(corked);
                    }
                    bool got = false;
                    const void *data = nullptr;
                    size_t nb = 0;
                    while (pa_stream_peek(s, &data, &nb) == 0 && nb > 0) {
                        const auto *p = static_cast<const uint8_t *>(data);
                        if (p)
                            acc.insert(acc.end(), p, p + nb);
                        else
                            acc.insert(acc.end(), nb, 0);   // дыра в записи — тишиной
                        pa_stream_drop(s);
                        got = true;
                    }
                    while (acc.size() >= bytes) {
                        const int64_t now = monoUs();
                        uint64_t lat = 0;
                        int negative = 0;
                        if (pa_stream_get_latency(s, &lat, &negative) < 0 || negative)
                            lat = 0;
                        const double after = double((acc.size() - bytes) / frameBytes) * 1e6 / rate;
                        const double est = double(now) - double(lat) - after - chunkUs - double(n) * chunkUs;
                        if (!have || std::abs(est - t0) > 20000) {
                            t0 = est;
                            have = true;
                        } else {
                            t0 += (est - t0) * 0.02;
                        }
                        onChunk(int64_t(t0 + double(n) * chunkUs), acc.data(), frames);
                        acc.erase(acc.begin(), acc.begin() + long(bytes));
                        ++n;
                    }
                    if (!got)
                        pa_threaded_mainloop_wait(ml);
                }
            }
            pa_context_set_subscribe_callback(ctx, nullptr, nullptr);
            if (s) {
                pa_stream_disconnect(s);
                pa_stream_unref(s);
            }
            pa_context_disconnect(ctx);
            pa_context_unref(ctx);
            pa_threaded_mainloop_unlock(ml);
            {   // после unlock: stop() берёт замки в порядке «State, потом цикл»
                std::lock_guard<std::mutex> lock(st->m);
                st->loop = nullptr;
            }
            pa_threaded_mainloop_stop(ml);
            pa_threaded_mainloop_free(ml);
            for (int i = 0; i < 50 && !st->quit; ++i)   // снова — через 0,5 с
                usleep(10000);
        }
        st->done = true;
    });
}

// Поток спит в pa_threaded_mainloop_wait до куска звука или смены состояния — его надо разбудить: на паузе в тишине
// (cork) кусков нет вовсе, и без побудки он не выходил никогда — после каждого ухода зрителя оставались поток
// naomi-audio, поток цикла PulseAudio, их сокеты и запись у PulseAudio, а stop() зря ждал свои 0,3 с (рентген
// 19 ноября 2026: 8 таких пар после четырёх прогонов стола). Будим под замком цикла: поток держит его всё время,
// кроме самого ожидания, — побудка не теряется. PulseAudio не отвечает совсем (перезапускается) — не ждать: поток
// доработает сам (держит только своё состояние и обработчик)
void AudioCapture::stop()
{
    if (!m_state)
        return;
    m_state->quit = true;
    {
        std::lock_guard<std::mutex> lock(m_state->m);
        if (auto *ml = static_cast<pa_threaded_mainloop *>(m_state->loop)) {
            pa_threaded_mainloop_lock(ml);
            pa_threaded_mainloop_signal(ml, 0);
            pa_threaded_mainloop_unlock(ml);
        }
    }
    for (int i = 0; i < 60 && !m_state->done; ++i)
        usleep(5000);
    if (m_state->done)
        m_thread.join();
    else
        m_thread.detach();
    m_state.reset();
}
