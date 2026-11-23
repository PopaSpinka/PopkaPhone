// НаомиФон — эквалайзер окна звука рабочего стола (docs/DESKTOP.md, «Звук»): перенос viz НаомиОС (bcdesk.c viz_*,
// audio.c ring_spectrum) — те же числа. Звук — что сейчас играет выход стола, только пока running (окно звука открыто):
// звук на «Динамиках» (выход desk) — его и так снимает трансляция для приставки (stream: deskSound), эквалайзер берёт
// последние 64 мс оттуда — без своего захвата и без ожидания; иначе — свой захват монитора выхода по умолчанию
// (динамик телефона), с открытия окна. Каждые 33 мс: спектр (SoundSpectrum) → выравнивание (каждая полоса — к среднему
// за 4 с наполовину, сглаживание соседями, пол и потолок — 15 % и 90 % полос) → столбики: вверх — 0,6 пути за кадр,
// вниз — 1,6 высоты в секунду, пик держится 0,35 с. Первый кадр после открытия считается сразу и столбики встают на
// место без подъёма — окно появляется с ними, как будто было открыто. Рисует точки 2 × 2 янтарём с шагом 3: горящие —
// .95, до пика — .26; нижний ряд горит всегда. Кадр стола — только когда точки сменились (в тишине — ни одного).
#pragma once

#include "audio_capture.h"
#include "sound_history.h"
#include "sound_spectrum.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QTimer>

#include <atomic>
#include <memory>
#include <vector>

class StreamService;

class SoundEqualizer : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
    // куда идёт звук (naomid Output): desk — «Динамики» (звук берётся у трансляции), другое — свой захват монитора
    // выхода по умолчанию; сменился — открывается заново
    Q_PROPERTY(QString output READ output WRITE setOutput NOTIFY outputChanged)
    // трансляция (naomiStream): её звук стола
    Q_PROPERTY(QObject *stream READ stream WRITE setStream NOTIFY streamChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    // поправка уровня, дБ: монитор пустого выхода «Монитор» PulseAudio отдаёт уже после громкости (у динамика телефона —
    // до неё); громкость снимается обратно — столбики от неё не зависят, как у НаомиОС (PipeWire — до громкости)
    Q_PROPERTY(double gainDb READ gainDb WRITE setGainDb NOTIFY gainDbChanged)

public:
    explicit SoundEqualizer(QQuickItem *parent = nullptr);
    ~SoundEqualizer() override;

    bool running() const { return m_running; }
    void setRunning(bool on);
    QString output() const { return m_output; }
    void setOutput(const QString &output);
    QObject *stream() const;
    void setStream(QObject *stream);
    QColor color() const { return m_color; }
    void setColor(const QColor &c);
    double gainDb() const { return m_gainDb; }
    void setGainDb(double db);
    void paint(QPainter *painter) override;

signals:
    void runningChanged();
    void outputChanged();
    void streamChanged();
    void colorChanged();
    void gainDbChanged();

private:
    void updateSource(bool reopen);   // откуда звук: у трансляции или свой захват (reopen — открыть свой заново)
    void tick();
    void showDots();   // точки из уровней; сменились — перерисовать
    void targets(const std::vector<double> &db, double dt);
    void reset();

    bool m_running = false;
    QString m_output;
    QPointer<StreamService> m_stream;
    QColor m_color{216, 178, 90};
    double m_gainDb = 0;
    AudioCapture m_capture;
    std::shared_ptr<SoundHistory> m_own = std::make_shared<SoundHistory>(4096);   // свой захват
    std::shared_ptr<const SoundHistory> m_source = m_own;                          // откуда берётся сейчас
    SoundSpectrum m_spectrum;
    QTimer m_timer;
    QElapsedTimer m_clock;
    qint64 m_lastMs = 0;
    int m_cols = 0;
    bool m_fresh = true;
    bool m_settle = true;   // первый спектр после открытия: столбики — сразу на место
    double m_top = 0, m_floor = 0;
    std::vector<float> m_samples;
    std::vector<double> m_db, m_flat, m_smooth, m_sorted;
    std::vector<double> m_avg, m_target, m_level, m_peak;
    std::vector<qint64> m_peakUntil;
    std::vector<uint8_t> m_dotsOn, m_dotsTop;   // нарисованное: горящих точек и до пика по столбикам
    int m_rows = 0;
};
