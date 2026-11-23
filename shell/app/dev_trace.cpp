#include "dev_trace.h"

#include <QEvent>
#include <QQuickWindow>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>

DevTrace::DevTrace()
    : m_timeline(qgetenv("NAOMI_TRACE") == "timeline")
{
    m_clock.start();
    connect(&m_timer, &QTimer::timeout, this, &DevTrace::report);
    m_timer.start(1000);
}

void DevTrace::attach(QQuickWindow *window)
{
    connect(window, &QQuickWindow::afterAnimating, this, [this] {
        ++m_animating;
        mark('A');
        if (m_lastTouchNs) {  // идёт касание: сколько событий пришло с прошлого кадра
            ++m_framesByTouches[qMin(m_touchesThisFrame, 4)];
            if (!m_handlerThisFrame)
                ++m_framesWithoutHandler;
        }
        m_touchesThisFrame = 0;
        m_handlerThisFrame = false;
    });
    connect(window, &QQuickWindow::beforeSynchronizing, this, [this] { ++m_syncs; mark('S'); },
            Qt::DirectConnection);
    connect(window, &QQuickWindow::afterSynchronizing, this, [this] { mark('s'); },
            Qt::DirectConnection);
    connect(window, &QQuickWindow::afterRendering, this, [this] { mark('R'); }, Qt::DirectConnection);
    connect(window, &QQuickWindow::frameSwapped, this, [this] { ++m_swaps; mark('W'); },
            Qt::DirectConnection);
    window->installEventFilter(this);
}

void DevTrace::mark(char code)
{
    if (!m_timeline || m_dumped)
        return;
    const int i = m_markIndex.fetch_add(1) % kMarks;
    m_markNs[i] = m_clock.nsecsElapsed();
    m_markCode[i] = code;
}

bool DevTrace::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchBegin) {
        const qint64 now = m_clock.nsecsElapsed();
        if (m_lastTouchNs)
            m_maxTouchGapNs = qMax(m_maxTouchGapNs, now - m_lastTouchNs);
        else
            m_touchStartNs = now;
        m_lastTouchNs = now;
        ++m_touches;
        ++m_touchesThisFrame;
        mark('t');
        if (m_timeline && !m_dumped && now - m_touchStartNs > 1000000000)
            dumpTimeline();
    } else if (event->type() == QEvent::TouchEnd) {
        m_lastTouchNs = 0;
        m_lastHandlerNs = 0;
        m_lastX = m_lastY = -1;
    }
    return false;
}

void DevTrace::handlerPoint(qreal x, qreal y)
{
    const qint64 now = m_clock.nsecsElapsed();
    if (m_lastHandlerNs)
        m_maxHandlerGapNs = qMax(m_maxHandlerGapNs, now - m_lastHandlerNs);
    if (m_lastX >= 0)
        m_maxHandlerStep = qMax(m_maxHandlerStep, std::hypot(x - m_lastX, y - m_lastY));
    m_lastHandlerNs = now;
    m_lastX = x;
    m_lastY = y;
    m_handlerThisFrame = true;
    mark('h');
}

// Последние ≈ 0,25 с: «время мс:метки», A — анимации (интерфейс), S/s — начало/конец синхронизации,
// R — отрисовано, W — кадр отдан (поток отрисовки), t — касание пришло в окно, h — обработчик в QML
// обновил точку (интерфейс).
void DevTrace::dumpTimeline()
{
    m_dumped = true;
    const int end = m_markIndex.load();
    const qint64 last = m_markNs[(end - 1) % kMarks];
    std::string out;
    for (int i = std::max(0, end - kMarks); i < end; ++i) {
        const qint64 ns = m_markNs[i % kMarks];
        if (last - ns > 250000000)
            continue;
        char buf[32];
        snprintf(buf, sizeof buf, " %.1f%c", (ns - last) / 1e6, m_markCode[i % kMarks]);
        out += buf;
    }
    fprintf(stderr, "naomi-timeline:%s\n", out.c_str());
}

void DevTrace::report()
{
    const int *f = m_framesByTouches;
    fprintf(stderr, "naomi-trace: анимаций %d, синхронизаций %d, кадров %d, касаний %d (пауза до %.1f мс); "
                    "кадров с 0/1/2/3/4+ касаниями: %d/%d/%d/%d/%d; кольцо: кадров без обновления %d, "
                    "пауза до %.1f мс, шаг до %.0f\n",
            m_animating.exchange(0), m_syncs.exchange(0), m_swaps.exchange(0), m_touches,
            m_maxTouchGapNs / 1e6, f[0], f[1], f[2], f[3], f[4], m_framesWithoutHandler,
            m_maxHandlerGapNs / 1e6, m_maxHandlerStep);
    m_touches = 0;
    m_maxTouchGapNs = 0;
    m_framesWithoutHandler = 0;
    m_maxHandlerGapNs = 0;
    m_maxHandlerStep = 0;
    std::fill(std::begin(m_framesByTouches), std::end(m_framesByTouches), 0);
}
