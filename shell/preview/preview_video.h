// НаомиФон — видео «Просмотра» рабочего стола (docs/DESKTOP.md, «Просмотр»; у НаомиОС — src/preview-video.c):
// GStreamer playbin, кадры — в appsink, звук — в выход по умолчанию (PulseAudio: куда идёт звук стола).
// Декодер playbin выбирает сам, и на телефоне это аппаратные декодеры Android (amcviddec-c2qti…: H.264, HEVC, VP9,
// 4K) — 25 % одного ядра вместо 73 % у программного H.264, но первый кадр у них — через 1,1–1,9 с (запуск декодера
// Android). Поэтому первый кадр рядом снимает второй, короткий конвейер программным декодером (openh264, libde265,
// vpx — 0,15–0,3 с): окно с ним открывается сразу, а когда пойдёт основной, его первый кадр — тот же (ролик играет с
// начала), подмены не видно. Кадры — NV12, как отдаёт декодер (без пересчёта процессором): две плоскости
// (VideoPlane, video_plane.h) — текстуры видеочипа, цвет считает шейдер стола (colorMatrix — по colorimetry кадра).
#pragma once

#include <QMatrix4x4>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QTimer>

#include <memory>

typedef struct _GstElement GstElement;
typedef struct _GstSample GstSample;
typedef struct _GstBus GstBus;
typedef struct _GstMessage GstMessage;

// Кадр NV12: держит буфер GStreamer отображённым, пока кадр нужен (текстуры берут данные прямо из него)
struct VideoFrame
{
    ~VideoFrame();
    GstSample *sample = nullptr;
    void *mapping = nullptr;   // GstMapInfo
    const unsigned char *y = nullptr, *uv = nullptr;
    int width = 0, height = 0, strideY = 0, strideUV = 0;
    quint64 serial = 0;
};

class PreviewVideo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)       // открыт ролик
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(bool ended READ ended NOTIFY stateChanged)         // доиграл: ▶, щелчок — с начала
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)      // не открылся — почему
    Q_PROPERTY(QSize frameSize READ frameSize NOTIFY frameSizeChanged)   // пусто — кадра ещё нет
    Q_PROPERTY(QMatrix4x4 colorMatrix READ colorMatrix NOTIFY frameSizeChanged)   // YUV → RGB для шейдера
    Q_PROPERTY(double position READ position NOTIFY positionChanged)   // секунды
    Q_PROPERTY(double duration READ duration NOTIFY positionChanged)
public:
    explicit PreviewVideo(QObject *parent = nullptr);
    ~PreviewVideo() override;

    bool active() const { return m_play != nullptr; }
    bool playing() const { return m_playing; }
    bool ended() const { return m_ended; }
    QString error() const { return m_error; }
    QSize frameSize() const { return m_frameSize; }
    QMatrix4x4 colorMatrix() const { return m_matrix; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }

    Q_INVOKABLE void open(const QString &path);
    Q_INVOKABLE void close();
    Q_INVOKABLE void toggle();               // пауза и снова; доиграл — с начала
    Q_INVOKABLE void pause();                // зритель ушёл — на паузу
    Q_INVOKABLE void seek(double fraction);  // по полосе хода

    // последний кадр (поток интерфейса) — плоскостям VideoPlane
    std::shared_ptr<const VideoFrame> frame() const { return m_frame; }

    // потоки GStreamer → сюда (по поколению: ролик сменили — старое не берём)
    void sampleArrived(GstSample *sample, int generation, bool poster);
    void busMessage(GstMessage *message, int generation);

signals:
    void stateChanged();
    void frameSizeChanged();
    void positionChanged();
    void frameChanged();

private:
    void deliver();
    void onEnded(int generation);
    void onError(int generation, const QString &text);
    void stopPoster();
    void updatePosition();
    static void teardown(GstElement *pipeline);   // в NULL и прочь — своим потоком (декодер Android отпускается не сразу)

    GstElement *m_play = nullptr, *m_poster = nullptr;
    int m_generation = 0;
    bool m_playing = false, m_ended = false, m_started = false;
    QString m_error;
    QSize m_frameSize;
    QMatrix4x4 m_matrix;
    double m_position = 0, m_duration = 0;
    QTimer m_tick;   // полоса хода и время на полосе окна — 4 раза в секунду (VIDEO_TICK_MS 250)
    std::shared_ptr<const VideoFrame> m_frame;
    quint64 m_serial = 0;
    qint64 m_openedUs = 0;

    QMutex m_mutex;              // кадр из потока GStreamer
    int m_statArrived = 0, m_statShown = 0;   // за секунду: прислал декодер, дошло до стола (журнал)
    qint64 m_statFrom = 0;
    GstSample *m_pending = nullptr;
    int m_pendingGeneration = 0;
    bool m_pendingPoster = false, m_queued = false;
};
