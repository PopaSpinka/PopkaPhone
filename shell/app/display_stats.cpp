#include "display_stats.h"

#include <QGuiApplication>
#include <QSettings>

DisplayStats::DisplayStats(QObject *parent)
    : QObject(parent)
    , m_snapshot(reinterpret_cast<NaomiFrameSnapshotFn>(
          QGuiApplication::platformFunction(kNaomiFrameSnapshot)))
    , m_setRefresh(reinterpret_cast<NaomiSetRefreshFn>(
          QGuiApplication::platformFunction(kNaomiSetRefresh)))
{
    if (!m_snapshot)
        return;  // не eglfs_naomi (например, запуск на Mac) — цифр нет
    connect(&m_timer, &QTimer::timeout, this, [this] {
        m_snapshot(&m_s);
        emit changed();
    });
    m_timer.start(250);
}

// у драйвера: размер экрана Qt в точках округлён (393 × 851 точки — 1080,75 × 2340,25 пикселя)
static NaomiDisplaySizes sizes()
{
    NaomiDisplaySizes s{};
    if (auto fn = reinterpret_cast<NaomiDisplaySizesFn>(QGuiApplication::platformFunction(kNaomiDisplaySizes)))
        fn(&s);
    return s;
}

QSize DisplayStats::renderSize() const
{
    const NaomiDisplaySizes s = sizes();
    return {s.renderWidth, s.renderHeight};
}

QSize DisplayStats::panelSize() const
{
    const NaomiDisplaySizes s = sizes();
    return {s.panelWidth, s.panelHeight};
}

int DisplayStats::savedRenderWidth()
{
    return QSettings("naomi", "shell").value("display/render", 1080).toInt();
}

void DisplayStats::setRenderWidth(int width)
{
    if (width != 720 && width != 1080)
        return;
    QSettings("naomi", "shell").setValue("display/render", width);
    if (width != renderSize().width())
        emit restartRequested();
}

void DisplayStats::setActive(bool on)
{
    if (!m_snapshot)
        return;
    if (on)
        m_timer.start(250);
    else
        m_timer.stop();
}

bool DisplayStats::setRefresh(int hz)
{
    return m_setRefresh && m_setRefresh(hz);
}
