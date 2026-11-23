#include "android_scroll.h"

#include <QWaylandCompositor>
#include <QWaylandQuickItem>
#include <QWaylandSeat>
#include <QWaylandSurface>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kFinger = 0;         // номер касания Wayland
constexpr qreal kSlopPx = 12 + 1;  // порог касания Android: 8 dp при плотности 240 (device/android-setup.sh) и ещё пиксель
constexpr qreal kRunPx = 160;      // запас хода пальца от места касания до края
constexpr qreal kEdgePx = 8;       // ближе к краю слоя палец не подходит
}

AndroidScroll::AndroidScroll(QObject *parent)
    : QObject(parent)
{
    m_end.setSingleShot(true);
    m_end.setInterval(140);
    connect(&m_end, &QTimer::timeout, this, &AndroidScroll::release);
}

qreal AndroidScroll::perUnit() const
{
    const qreal k = m_layer->mapToSurface(QPointF(100, 0)).x() - m_layer->mapToSurface(QPointF(0, 0)).x();
    return k > 0 ? k / 100 : 1;
}

// Слой — подповерхность окна Waydroid; окно — весь экран Android: палец — внутри слоя и внутри экрана
QRectF AndroidScroll::reach() const
{
    QRectF r = m_layer->boundingRect();
    if (QQuickItem *screen = m_layer->parentItem())
        r &= m_layer->mapRectFromItem(screen, screen->boundingRect());
    const qreal edge = kEdgePx / perUnit();
    return r.width() > 4 * edge && r.height() > 4 * edge ? r.adjusted(edge, edge, -edge, -edge) : r;
}

void AndroidScroll::send(Qt::TouchPointState state, QPointF at)
{
    QWaylandSeat *seat = m_compositor ? m_compositor->defaultSeat() : nullptr;
    QWaylandSurface *surface = m_layer ? m_layer->surface() : nullptr;
    if (!seat || !surface)
        return;
    seat->sendTouchPointEvent(surface, kFinger, m_layer->mapToSurface(at), state);
    seat->sendTouchFrameEvent(surface->client());
}

// Палец — туда, где колесо началось; до края в сторону хода мало — отступить назад, чтобы не перехватывать каждый кадр
bool AndroidScroll::press(QPointF dir)
{
    const QRectF r = reach();
    if (r.isEmpty())
        return false;
    const qreal run = kRunPx / perUnit();
    QPointF at(std::clamp(m_grip.x(), r.left(), r.right()), std::clamp(m_grip.y(), r.top(), r.bottom()));
    if (std::abs(dir.y()) >= std::abs(dir.x())) {
        const qreal room = std::min(run, r.height() * 0.4);
        at.setY(dir.y() > 0 ? std::min(at.y(), r.bottom() - room) : std::max(at.y(), r.top() + room));
    } else {
        const qreal room = std::min(run, r.width() * 0.4);
        at.setX(dir.x() > 0 ? std::min(at.x(), r.right() - room) : std::max(at.x(), r.left() + room));
    }
    m_at = at;
    m_down = true;
    send(Qt::TouchPointPressed, m_at);
    return true;
}

void AndroidScroll::release()
{
    m_end.stop();
    if (!m_down)
        return;
    m_down = false;
    send(Qt::TouchPointReleased, m_at);   // слоя уже нет — Waydroid убрал касание вместе с ним
    if (m_layer)
        emit released(m_layer);
}

void AndroidScroll::scroll(QWaylandQuickItem *layer, QPointF at, QPointF d, bool end)
{
    if (m_down && layer != m_layer)
        release();
    if (!d.isNull() && layer && layer->surface()) {
        if (!m_down) {
            m_layer = layer;
            m_grip = at;
            m_first = true;
            if (!press(d))
                return;
        }
        QPointF to = m_at + d;
        if (m_first) {   // порог — по той оси, куда пошли
            const qreal slop = kSlopPx / perUnit();
            if (std::abs(d.y()) >= std::abs(d.x()))
                to.ry() += std::copysign(slop, d.y());
            else
                to.rx() += std::copysign(slop, d.x());
            m_first = false;
        }
        QRectF r = reach();
        if (!r.contains(to)) {   // перехват: список в движении берёт новое касание сразу
            send(Qt::TouchPointReleased, m_at);
            press(d);
            r = reach();
            to = m_at + d;
            to = QPointF(std::clamp(to.x(), r.left(), r.right()), std::clamp(to.y(), r.top(), r.bottom()));
        }
        m_at = to;
        send(Qt::TouchPointMoved, m_at);
    }
    if (end)
        release();
    else if (m_down)
        m_end.start();
}
