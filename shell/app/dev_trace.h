// НаомиФон — счётчики конвейера кадра Qt Quick для разбора плавности (NAOMI_TRACE=1).
// Раз в секунду в журнал: сколько раз поток интерфейса продвинул анимации, сколько раз сцена
// синхронизировалась с потоком отрисовки, сколько кадров отрисовано, сколько событий касания пришло
// в окно, самая длинная пауза между ними и как касания разложились по кадрам.
// NAOMI_TRACE=timeline — ещё и один раз временная шкала потоков (≈ 0,25 с) после секунды касания.
// Из QML (контекст naomiTrace) — handlerPoint(): обновления точки обработчика под пальцем; по ним видно,
// сколько кадров касания прошло без обновления кольца и не прыгало ли оно.
#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <atomic>

class QQuickWindow;

class DevTrace : public QObject
{
    Q_OBJECT
public:
    DevTrace();
    void attach(QQuickWindow *window);   // после загрузки QML

    Q_INVOKABLE void handlerPoint(qreal x, qreal y);   // поток интерфейса, из QML

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void report();
    void mark(char code);          // метка на шкале: любой поток
    void dumpTimeline();

    std::atomic<int> m_animating{0};   // afterAnimating — поток интерфейса
    std::atomic<int> m_syncs{0};       // beforeSynchronizing — поток отрисовки
    std::atomic<int> m_swaps{0};       // frameSwapped — поток отрисовки
    int m_touches = 0;
    int m_touchesThisFrame = 0;
    int m_framesByTouches[5] = {};     // кадров интерфейса, между которыми пришло 0,1,2,3,4+ касаний
    qint64 m_lastTouchNs = 0;
    qint64 m_maxTouchGapNs = 0;
    // обработчик под пальцем (QML)
    bool m_handlerThisFrame = false;
    int m_framesWithoutHandler = 0;    // кадров при касании без обновления точки обработчика
    qint64 m_lastHandlerNs = 0;
    qint64 m_maxHandlerGapNs = 0;
    qreal m_lastX = -1, m_lastY = -1;
    qreal m_maxHandlerStep = 0;        // самый большой шаг между обновлениями, логические точки
    QElapsedTimer m_clock;
    QTimer m_timer;

    static constexpr int kMarks = 4096;
    bool m_timeline = false;
    bool m_dumped = false;
    qint64 m_touchStartNs = 0;
    std::atomic<int> m_markIndex{0};
    qint64 m_markNs[kMarks] = {};
    char m_markCode[kMarks] = {};
};
