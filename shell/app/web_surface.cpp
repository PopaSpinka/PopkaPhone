#include "web_surface.h"

#include "outline_cut.h"

#include <QImage>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QWaylandBufferRef>
#include <QWaylandClient>
#include <QWaylandCompositor>
#include <QWaylandPointer>
#include <QWaylandSeat>
#include <QWaylandSurface>
#include <QWaylandView>
#include <QWheelEvent>
#include <QtWaylandCompositor/private/qwaylandpointer_p.h>

void WebSurface::setOutline(const QRectF &outline)
{
    if (outline == m_outline)
        return;
    m_outline = outline;
    update();
    emit outlineChanged();
}

void WebSurface::setCorner(qreal corner)
{
    if (qFuzzyCompare(1 + corner, 1 + m_corner))
        return;
    m_corner = corner;
    update();
    emit outlineChanged();
}

void WebSurface::setPixel(qreal pixel)
{
    if (qFuzzyCompare(1 + pixel, 1 + m_pixel))
        return;
    m_pixel = pixel;
    update();
    emit outlineChanged();
}

// Кадр wl_shm элемента — своим узлом; рисует его cutToOutline(…, solid): старший байт пикселя за прозрачность не
// считается (у Wine в нём ноль — окно выходило чёрным). Байты кадра уходят в текстуру как есть, без копии: перевод
// каждого кадра в формат с прозрачностью стоил потоку оболочки половину большого ядра на игре 1560×721 (21 ноября 2026).
// Нет такого кадра — nullptr. oldNode — прошлый узел элемента: свой — обновляется, чужой — удаляется
static QSGSimpleTextureNode *solidNode(QWaylandQuickItem *item, QSGNode *oldNode)
{
    if (!item->view() || !item->window())
        return nullptr;
    const QWaylandBufferRef buffer = item->view()->currentBuffer();
    if (!buffer.hasBuffer() || !buffer.isSharedMemory())
        return nullptr;
    const QImage image = buffer.image();
    if (image.isNull())
        return nullptr;
    auto *own = dynamic_cast<QSGSimpleTextureNode *>(oldNode);
    if (!own || !own->ownsTexture()) {   // узел остался от Qt Wayland — свой
        delete oldNode;
        own = new QSGSimpleTextureNode;
        own->setOwnsTexture(true);
    }
    own->setTexture(item->window()->createTextureFromImage(image));
    own->setFiltering(item->smooth() ? QSGTexture::Linear : QSGTexture::Nearest);
    own->setRect(QRectF(0, 0, item->width(), item->height()));
    // видимая часть кадра (wp_viewport: буфер у Wine больше окна)
    const QRectF source = item->surface() ? item->surface()->sourceGeometry() : QRectF();
    own->setSourceRect(source.isValid() ? source : QRectF(0, 0, image.width(), image.height()));
    return own;
}

SolidSubsurface::SolidSubsurface(QWaylandSurface *surface, QQuickItem *parent)
{
    setSubsurfaceHandler(this);
    setSurface(surface);
    setParentItem(parent);
    setParent(parent);
    // место слоя в родителе ведёт сам Qt Wayland (его закрытый слот)
    connect(surface, SIGNAL(subsurfacePositionChanged(QPoint)), this, SLOT(handleSubsurfacePosition(QPoint)));
    connect(surface, &QObject::destroyed, this, &QObject::deleteLater);
}

void SolidSubsurface::handleSubsurfaceAdded(QWaylandSurface *child)
{
    new SolidSubsurface(child, this);
}

QSGNode *SolidSubsurface::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    if (QSGSimpleTextureNode *node = solidNode(this, oldNode)) {
        cutToOutline(node, QRectF(), 0, QRectF(0, 0, width(), height()), 0, true);
        return node;
    }
    if (auto *own = dynamic_cast<QSGSimpleTextureNode *>(oldNode); own && own->ownsTexture()) {
        delete oldNode;
        oldNode = nullptr;
    }
    return QWaylandQuickItem::updatePaintNode(oldNode, data);
}

void WebSurface::wheelEvent(QWheelEvent *event)
{
    QWaylandQuickShellSurfaceItem::wheelEvent(event);   // wl_pointer.axis — Qt
    if (!m_pointerFrames || !event->isAccepted() || !surface() || !surface()->client() || !compositor())
        return;
    QWaylandSeat *seat = compositor()->seatFor(event);
    if (!seat || !seat->pointer())
        return;
    auto *pointer = static_cast<QWaylandPointerPrivate *>(QObjectPrivate::get(seat->pointer()));
    const auto resources = pointer->resourceMap().values(surface()->client()->client());
    for (QtWaylandServer::wl_pointer::Resource *resource : resources)
        pointer->send_frame(resource->handle);
}

void WebSurface::setOpaque(bool opaque)
{
    if (opaque == m_opaque)
        return;
    m_opaque = opaque;
    // вложенные слои окна — тоже без прозрачности; задаётся до того, как у элемента появится поверхность
    setSubsurfaceHandler(opaque ? new SolidSubsurfaces(this) : nullptr);
    update();
    emit outlineChanged();
}

QSGNode *WebSurface::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data)
{
    if (m_opaque) {
        if (QSGSimpleTextureNode *own = solidNode(this, oldNode)) {
            cutToOutline(own, m_outline, m_corner, QRectF(0, 0, width(), height()), m_pixel, true);
            return own;
        }
    }
    if (auto *own = dynamic_cast<QSGSimpleTextureNode *>(oldNode); own && own->ownsTexture()) {
        delete oldNode;   // был свой узел — Qt Wayland пусть заведёт свой
        oldNode = nullptr;
    }
    QSGNode *node = QWaylandQuickShellSurfaceItem::updatePaintNode(oldNode, data);
    cutToOutline(dynamic_cast<QSGSimpleTextureNode *>(node), m_outline, m_corner, QRectF(0, 0, width(), height()),
                 m_pixel);
    return node;
}
