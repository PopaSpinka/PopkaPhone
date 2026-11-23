#include "android_surface.h"

#include "outline_cut.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QImage>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGSimpleTextureNode>
#include <QSGTextureMaterial>
#include <QWaylandOutput>
#include <QWaylandBufferRef>
#include <QWaylandSurface>
#include <QWaylandView>
#include <QtWaylandCompositor/private/qwaylandview_p.h>

AndroidSurface::AndroidSurface(QQuickItem *parent)
    : QWaylandQuickItem(parent)
{
    setSubsurfaceHandler(this);
    // после Qt Wayland (его обработчик смены поверхности подключён раньше — в конструкторе QWaylandQuickItem)
    connect(view(), &QWaylandView::surfaceChanged, this, &AndroidSurface::watchSurface);
}

// Qt Wayland просит кадр стола на каждое подтверждение поверхности (redraw → update). Вместо этого — committed
void AndroidSurface::watchSurface()
{
    disconnect(m_commit);
    m_shownBuffer = {};
    QWaylandSurface *s = surface();
    if (!s)
        return;
    disconnect(s, &QWaylandSurface::redraw, this, &QQuickItem::update);
    m_commit = connect(s, &QWaylandSurface::redraw, this, &AndroidSurface::committed);
    setBufferLocked(!isVisible());
    committed();
}

// Подтверждение от Waydroid. Тот же буфер тем же местом, что в прошлом подтверждении, — картинка та же (новый кадр
// приложения — всегда другой буфер его очереди): кадр стола не нужен. Но Qt Wayland уже отметил «пришёл буфер»
// (QWaylandView::bufferCommitted), и первый же кадр стола по другому поводу нашёл бы его «новым» (advance) и попросил
// ещё кадр — а там новое подтверждение, и так 75 раз в секунду, пока Android подтверждает. Отметку снимаем, если
// прошлое подтверждение уже показано (показанный буфер — тот же)
void AndroidSurface::committed()
{
    QWaylandSurface *s = surface();
    if (!s)
        return;
    QWaylandViewPrivate *v = QWaylandViewPrivate::get(view());
    const Shown now{v->nextBuffer.wl_buffer(), s->sourceGeometry(), s->destinationSize(), s->bufferScale()};
    if (now == m_shownBuffer) {
        QMutexLocker lock(&v->bufferMutex);
        if (v->currentBuffer.wl_buffer() == v->nextBuffer.wl_buffer())
            v->nextBufferCommitted = false;
        return;
    }
    m_shownBuffer = now;
    if (!isVisible()) {   // окно свёрнуто: буфер заперт (itemChange), кадр — когда покажут
        m_missed = true;
        return;
    }
    update();
}

// Слой не видно (окно свёрнуто или ещё не показано) — буфер заперт: Qt Wayland не берёт новые (иначе каждый кадр стола
// находил бы новый буфер невидимой игры и просил следующий); показали — отпереть и нарисовать, что пришло
void AndroidSurface::itemChange(ItemChange change, const ItemChangeData &data)
{
    QWaylandQuickItem::itemChange(change, data);
    if (change != ItemVisibleHasChanged)
        return;
    setBufferLocked(!data.boolValue);
    if (data.boolValue && m_missed) {
        m_missed = false;
        update();
    }
}

// Наведение Android не шлём (android_surface.h). Событие всё равно наше — иначе его получило бы окно стола под этим;
// мимо области ввода (прозрачный холст окна Waydroid) — как у Qt Wayland: не наше, идёт ниже
bool AndroidSurface::hoverKept(QHoverEvent *event)
{
    static const bool send = qEnvironmentVariableIntValue("NAOMI_ANDROID_HOVER") != 0;
    if (send)
        return false;
    if (!surface() || !inputRegionContains(event->position()))
        event->ignore();
    return true;
}

void AndroidSurface::hoverEnterEvent(QHoverEvent *event)
{
    if (!hoverKept(event))
        QWaylandQuickItem::hoverEnterEvent(event);
}

void AndroidSurface::hoverMoveEvent(QHoverEvent *event)
{
    if (!hoverKept(event))
        QWaylandQuickItem::hoverMoveEvent(event);
}

// Как у Qt Wayland (QWaylandQuickItem::handleSubsurfaceAdded), только элемент — свой
void AndroidSurface::handleSubsurfaceAdded(QWaylandSurface *child)
{
    auto *item = new AndroidSurface;
    item->setSurface(child);
    item->setVisible(true);
    item->setParentItem(this);
    item->setParent(this);
    // место подповерхности — как у Qt Wayland (handleSubsurfacePosition), плюс shift
    connect(child, &QWaylandSurface::subsurfacePositionChanged, item, &AndroidSurface::place);
    connect(child, &QWaylandSurface::destroyed, item, &QObject::deleteLater);
}

// Единиц элемента в точке поверхности — как у Qt Wayland (QWaylandQuickItemPrivate::scaleFactor): масштаб вывода на
// devicePixelRatio окна. Не mapFromSurface: тот делит на размер элемента, а его Qt Wayland округляет до целых — у слоя
// в 52 пикселя высотой (28 точек) место выходило на 9 точек выше (проверено: окно у нижнего края экрана)
qreal AndroidSurface::unit() const
{
    const qreal output = view() && view()->output() ? view()->output()->scaleFactor() : 1;
    return output / (window() ? window()->devicePixelRatio() : 1);
}

void AndroidSurface::place(const QPoint &surfacePos)
{
    m_base = QPointF(surfacePos) * unit();
    setPosition(m_base + QPointF(0, m_shift));
    if (m_corner > 0)
        update();   // контур — от родителя: слой сдвинулся — обрезка другая
}

void AndroidSurface::setShift(qreal shift)
{
    if (qFuzzyCompare(1 + shift, 1 + m_shift))
        return;
    m_shift = shift;
    setPosition(m_base + QPointF(0, m_shift));
    if (m_corner > 0)
        update();
}

void AndroidSurface::setOutline(const QRectF &window, qreal corner)
{
    if (window == m_outline && qFuzzyCompare(1 + corner, 1 + m_corner))
        return;
    m_outline = window;
    m_corner = corner;
    update();
}

void AndroidSurface::setClip(qreal height)
{
    if (qFuzzyCompare(1 + height, 1 + m_clip))
        return;
    m_clip = height;
    update();
}

// Буфер слоя приложения Waydroid не масштабирует (кадр — точка в точку, обрезан по краю экрана): у края экрана слой
// целиком — буфер от места слоя минус обрезка. Досчитывать по видимой части нельзя: у окна, от которого видна полоска,
// ошибка округления множится (от 71 пикселя из 1034 — на десятки). Со стороны, что не у края экрана, — видимая часть:
// окно, которое тянут за край, Android рисует в поверхности размером во весь экран, а показывает её часть — рамку
// окна (было: наша полоса — до края стола, запись потока 18 ноября 2026, кадр 598). Слой-заливка (буфер 1 × 1,
// растянут) — как есть
QRectF AndroidSurface::fullRect() const
{
    const QWaylandSurface *s = surface();
    const QRectF src = s ? s->sourceGeometry() : QRectF();
    const QSize dst = s ? s->destinationSize() : QSize();
    if (!s || src.isEmpty() || dst.isEmpty() || s->bufferScale() <= 0 || std::abs(dst.width() - src.width()) > 2
        || std::abs(dst.height() - src.height()) > 2)
        return QRectF(m_base, size());
    const qreal k = unit();
    const QSizeF buffer = QSizeF(s->bufferSize()) / s->bufferScale();
    const QRectF whole(m_base - src.topLeft() * k, buffer * k);
    const QRectF seen(m_base, QSizeF(dst) * k);
    const QSizeF screen = parentItem() ? parentItem()->size() : QSizeF();
    constexpr qreal kEdge = 1;
    return QRectF(QPointF(seen.left() <= kEdge ? whole.left() : seen.left(),
                          seen.top() <= kEdge ? whole.top() : seen.top()),
                  QPointF(seen.right() >= screen.width() - kEdge ? whole.right() : seen.right(),
                          seen.bottom() >= screen.height() - kEdge ? whole.bottom() : seen.bottom()));
}

bool AndroidSurface::contains(const QPointF &point) const
{
    if (m_clip > 0 && point.y() >= m_clip)
        return false;
    return QWaylandQuickItem::contains(point);
}

// Заливка из одной прозрачной точки (холст окна Waydroid; буфер — общая память 1 × 1): рисовать нечего
bool AndroidSurface::emptyFill()
{
    static const bool paint = qEnvironmentVariableIntValue("NAOMI_ANDROID_CANVAS") != 0;
    if (paint || !view())
        return false;
    const QWaylandBufferRef buffer = view()->currentBuffer();
    if (!buffer.hasBuffer() || buffer.isDestroyed() || buffer.size() != QSize(1, 1) || !buffer.isSharedMemory())
        return false;
    if (buffer.wl_buffer() != m_fillOf) {
        m_fillOf = buffer.wl_buffer();
        const QImage image = buffer.image();
        m_fillEmpty = !image.isNull() && qAlpha(image.pixel(0, 0)) == 0;
    }
    return m_fillEmpty;
}

QSGNode *AndroidSurface::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    // буфер уничтожен клиентом, нового ещё нет — прежняя картинка (её текстура жива, пока вид держит буфер)
    if (oldNode && view() && view()->currentBuffer().isDestroyed())
        return oldNode;
    QSGNode *node = QWaylandQuickItem::updatePaintNode(oldNode, data);
    if (node && emptyFill()) {
        delete node;
        return nullptr;
    }
    // clip: только верхняя полоска — на экране (часть элемента сверху) и в выборке из буфера (низ кадра GL — первым)
    if (auto *texture = dynamic_cast<QSGSimpleTextureNode *>(node); texture && m_clip > 0 && m_clip < height()) {
        const QRectF src = texture->sourceRect();
        const qreal part = m_clip / height();
        const bool invertY = view()->currentBuffer().origin() == QWaylandSurface::OriginBottomLeft;
        texture->setRect(invertY ? QRectF(0, m_clip, width(), -m_clip) : QRectF(0, 0, width(), m_clip));
        texture->setSourceRect(invertY ? QRectF(src.x(), src.y() + src.height() * (1 - part), src.width(),
                                                src.height() * part)
                                       : QRectF(src.x(), src.y(), src.width(), src.height() * part));
    }
    outline(dynamic_cast<QSGSimpleTextureNode *>(node));
    return node;
}

// Скруглённые углы окна (setOutline). Узел Qt Wayland — прямоугольник, а слой, задевший угол окна (окно приложения:
// в его верхних углах — полоса Android и поля тени; внизу — картинка приложения), надо резать по кривой: рисует его дочерний узел — та же
// текстура треугольниками по контуру ∩ слой, а сам узел Qt Wayland — пустой прямоугольник. Слой, угла не задевший, — как
// есть (всплывающее окошко у края задачи не режется). Сама обрезка — outline_cut.h (общая со страницей браузера)
void AndroidSurface::outline(QSGSimpleTextureNode *node)
{
    cutToOutline(node, m_outline.translated(-position()), m_corner,
                 QRectF(0, 0, width(), m_clip > 0 && m_clip < height() ? m_clip : height()));
}
