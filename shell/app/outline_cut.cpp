#include "outline_cut.h"

#include <algorithm>
#include <cmath>
#include <functional>

#include <QCoreApplication>
#include <QLineF>
#include <QList>
#include <QPointF>
#include <QSGGeometryNode>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QSGSimpleTextureNode>
#include <QSGTextureMaterial>
#include <cstring>

namespace {
// Слой, обрезанный по контуру окна: та же текстура, что у узла Qt Wayland, но треугольниками по контуру
struct OutlineNode : QSGGeometryNode {
    OutlineNode()
        : geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0)
    {
        geometry.setDrawingMode(QSGGeometry::DrawTriangles);
        setGeometry(&geometry);
        setMaterial(&material);
        setOpaqueMaterial(&opaque);
        setFlag(OwnedByParent);   // узел поверхности удаляет Qt — вместе с ним и этот
    }
    QSGGeometry geometry;
    QSGTextureMaterial material;
    QSGOpaqueTextureMaterial opaque;
};

// Кайма сглаживания: текстура × видимость вершины (shaders/outline.vert, .frag — рядом с программой).
// solid — текстура без своей прозрачности (shaders/solid.frag): старший байт пикселя не читается, вместо него 1
struct FeatherMaterial : QSGMaterial {
    FeatherMaterial() { setFlag(Blending); }
    QSGMaterialType *type() const override
    {
        static QSGMaterialType t[2];
        return &t[solid];
    }
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
    int compare(const QSGMaterial *other) const override
    {
        const auto *o = static_cast<const FeatherMaterial *>(other);
        return texture == o->texture ? int(filtering) - int(o->filtering) : (texture < o->texture ? -1 : 1);
    }
    QSGTexture *texture = nullptr;
    QSGTexture::Filtering filtering = QSGTexture::Linear;
    bool solid = false;
};

struct FeatherShader : QSGMaterialShader {
    explicit FeatherShader(bool solid)
    {
        const QString dir = QCoreApplication::applicationDirPath() + QLatin1String("/shaders/");
        setShaderFileName(VertexStage, dir + QLatin1String("outline.vert.qsb"));
        setShaderFileName(FragmentStage, dir + QLatin1String(solid ? "solid.frag.qsb" : "outline.frag.qsb"));
    }
    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override
    {
        QByteArray *buf = state.uniformData();
        bool changed = false;
        if (state.isMatrixDirty()) {
            const QMatrix4x4 m = state.combinedMatrix();
            std::memcpy(buf->data(), m.constData(), 64);
            changed = true;
        }
        if (state.isOpacityDirty()) {
            const float opacity = state.opacity();
            std::memcpy(buf->data() + 64, &opacity, 4);
            changed = true;
        }
        return changed;
    }
    void updateSampledImage(RenderState &state, int, QSGTexture **texture, QSGMaterial *material, QSGMaterial *) override
    {
        auto *m = static_cast<FeatherMaterial *>(material);
        m->texture->setFiltering(m->filtering);
        m->texture->setHorizontalWrapMode(QSGTexture::ClampToEdge);
        m->texture->setVerticalWrapMode(QSGTexture::ClampToEdge);
        m->texture->commitTextureOperations(state.rhi(), state.resourceUpdateBatch());
        *texture = m->texture;
    }
};

QSGMaterialShader *FeatherMaterial::createShader(QSGRendererInterface::RenderMode) const
{
    return new FeatherShader(solid);
}

struct FeatherVertex { float x, y, tx, ty, a; };

const QSGGeometry::AttributeSet &featherAttributes()
{
    static const QSGGeometry::Attribute attrs[] = {
        QSGGeometry::Attribute::createWithAttributeType(0, 2, QSGGeometry::FloatType, QSGGeometry::PositionAttribute),
        QSGGeometry::Attribute::createWithAttributeType(1, 2, QSGGeometry::FloatType, QSGGeometry::TexCoordAttribute),
        QSGGeometry::Attribute::createWithAttributeType(2, 1, QSGGeometry::FloatType, QSGGeometry::UnknownAttribute)};
    static const QSGGeometry::AttributeSet set = {3, sizeof(FeatherVertex), attrs};
    return set;
}

struct FeatherNode : QSGGeometryNode {
    FeatherNode()
        : geometry(featherAttributes(), 0)
    {
        geometry.setDrawingMode(QSGGeometry::DrawTriangles);
        setGeometry(&geometry);
        setMaterial(&material);
        setFlag(OwnedByParent);
    }
    QSGGeometry geometry;
    FeatherMaterial material;
};

// Прямоугольник со скруглёнными углами — как DeskStyle.outline у окон стола («непрерывные» углы, по 16 шагов): углы
// по часовой (y вниз) от правого верхнего
QList<QPointF> rounded(const QRectF &r, qreal e)
{
    struct Corner { qreal cx, cy, sx, sy; bool down; };
    const Corner corners[] = {{r.right() - e, r.top() + e, 1, -1, true}, {r.right() - e, r.bottom() - e, 1, 1, false},
                              {r.left() + e, r.bottom() - e, -1, 1, true}, {r.left() + e, r.top() + e, -1, -1, false}};
    QList<QPointF> pts;
    for (const Corner &c : corners)
        for (int j = 0; j <= 16; ++j) {
            const qreal t = (c.down ? 16 - j : j) / 16.0 * M_PI / 2;
            pts << QPointF(c.cx + c.sx * e * std::pow(std::max(std::cos(t), 0.0), 0.4),
                           c.cy + c.sy * e * std::pow(std::max(std::sin(t), 0.0), 0.4));
        }
    return pts;
}

// Выпуклый многоугольник ∩ прямоугольник (Сазерленд — Ходжмен): остаётся выпуклым
QList<QPointF> clipTo(QList<QPointF> poly, const QRectF &d)
{
    auto edge = [&poly](const std::function<qreal(const QPointF &)> &side) {   // side ≥ 0 — внутри
        QList<QPointF> out;
        for (qsizetype i = 0; i < poly.size(); ++i) {
            const QPointF a = poly[i], b = poly[(i + 1) % poly.size()];
            const qreal sa = side(a), sb = side(b);
            if (sa >= 0)
                out << a;
            if ((sa >= 0) != (sb >= 0))
                out << a + (b - a) * (sa / (sa - sb));
        }
        poly = out;
    };
    edge([&](const QPointF &p) { return p.x() - d.left(); });
    edge([&](const QPointF &p) { return d.right() - p.x(); });
    edge([&](const QPointF &p) { return p.y() - d.top(); });
    edge([&](const QPointF &p) { return d.bottom() - p.y(); });
    return poly;
}
} // namespace

void cutToOutline(QSGSimpleTextureNode *node, const QRectF &o, qreal corner, const QRectF &drawn, qreal pixel, bool solid)
{
    if (!node)
        return;
    // Рисующий узел: обычный (материалы Qt) или, для текстуры без прозрачности, со своим шейдером — он же рисует и
    // тогда, когда углов нет: сам узел Qt Wayland прочёл бы старший байт как прозрачность
    QSGNode *first = node->childCount() ? node->firstChild() : nullptr;
    auto *cut = dynamic_cast<OutlineNode *>(first);
    auto *fill = dynamic_cast<FeatherNode *>(first);
    if (first && (solid ? !fill : !cut)) {
        node->removeChildNode(first);
        delete first;
        first = nullptr;
        cut = nullptr;
        fill = nullptr;
    }
    const QRectF rect = node->rect(), src = node->sourceRect();
    const qreal e = std::min({corner, o.width() / 2, o.height() / 2});
    const bool corners = e > 0 && (drawn.intersects(QRectF(o.left(), o.top(), e, e))
                                   || drawn.intersects(QRectF(o.right() - e, o.top(), e, e))
                                   || drawn.intersects(QRectF(o.left(), o.bottom() - e, e, e))
                                   || drawn.intersects(QRectF(o.right() - e, o.bottom() - e, e, e)));
    QSGTexture *texture = node->texture();
    if ((!corners && !solid) || !texture || rect.width() == 0 || rect.height() == 0 || src.isEmpty()) {
        if (first) {
            node->removeChildNode(first);
            delete first;
        }
        return;
    }
    if (solid && !fill) {
        fill = new FeatherNode;
        fill->material.solid = true;
        node->appendChildNode(fill);
    } else if (!solid && !cut) {
        cut = new OutlineNode;
        node->appendChildNode(cut);
    }
    QSGGeometryNode *shape = solid ? static_cast<QSGGeometryNode *>(fill) : cut;
    const QList<QPointF> whole = {drawn.topLeft(), drawn.topRight(), drawn.bottomRight(), drawn.bottomLeft()};
    QList<QPointF> poly;
    for (const QPointF &p : corners ? clipTo(rounded(o, e), drawn) : whole)   // без повторов подряд: у них нет направления ребра
        if (poly.isEmpty() || QLineF(poly.last(), p).length() > 1e-3)
            poly << p;
    if (poly.size() > 1 && QLineF(poly.last(), poly.first()).length() <= 1e-3)
        poly.removeLast();
    const QSizeF ts = texture->textureSize();
    const QRectF sub = texture->normalizedTextureSubRect();
    auto uv = [&](const QPointF &p) {   // точка элемента → текстура: так же, как узел Qt Wayland кладёт src на rect
        const qreal x = src.x() + (p.x() - rect.x()) / rect.width() * src.width();
        const qreal y = src.y() + (p.y() - rect.y()) / rect.height() * src.height();
        return QPointF(sub.x() + x / ts.width() * sub.width(), sub.y() + y / ts.height() * sub.height());
    };
    // Сглаживание края (pixel > 0): непрозрачная часть — контур, отступивший внутрь на полпикселя, вокруг — кайма в
    // пиксель, видимость от 1 до 0 (вершина сдвигается по биссектрисе соседних рёбер). Край по сетке пикселей остаётся
    // резким, кривая угла — без ступенек, как у маски
    const qsizetype n = poly.size();
    QList<QPointF> inner = poly, outer;
    const bool feather = corners && pixel > 0 && n >= 3;
    if (feather) {
        qreal area = 0;
        for (qsizetype k = 0; k < n; ++k)
            area += poly[k].x() * poly[(k + 1) % n].y() - poly[(k + 1) % n].x() * poly[k].y();
        const qreal sign = area >= 0 ? 1 : -1;
        auto normal = [&](qsizetype k) {   // наружу от ребра k → k + 1
            const QPointF d = poly[(k + 1) % n] - poly[k];
            const qreal len = std::hypot(d.x(), d.y());
            return QPointF(sign * d.y() / len, -sign * d.x() / len);
        };
        outer.resize(n);
        for (qsizetype k = 0; k < n; ++k) {
            const QPointF a = normal((k + n - 1) % n), b = normal(k);
            const qreal dot = a.x() * b.x() + a.y() * b.y();
            const QPointF m = (a + b) / std::max(1 + dot, 0.5);   // острее 120° — не дальше двух полупикселей
            inner[k] = poly[k] - m * (pixel / 2);
            outer[k] = poly[k] + m * (pixel / 2);
        }
    }
    const int triangles = n >= 3 ? int(n) - 2 : 0;
    shape->geometry()->allocate(triangles * 3);
    if (solid) {
        auto *v = static_cast<FeatherVertex *>(fill->geometry.vertexData());
        for (int k = 0; k < triangles; ++k)
            for (const QPointF &p : {inner[0], inner[k + 1], inner[k + 2]}) {
                const QPointF t = uv(p);
                *v++ = {float(p.x()), float(p.y()), float(t.x()), float(t.y()), 1};
            }
    } else {
        QSGGeometry::TexturedPoint2D *v = cut->geometry.vertexDataAsTexturedPoint2D();
        for (int k = 0; k < triangles; ++k)
            for (const QPointF &p : {inner[0], inner[k + 1], inner[k + 2]}) {
                const QPointF t = uv(p);
                (v++)->set(float(p.x()), float(p.y()), float(t.x()), float(t.y()));
            }
    }
    auto *ring = shape->childCount() ? static_cast<FeatherNode *>(shape->firstChild()) : nullptr;
    if (!feather) {
        if (ring) {
            shape->removeChildNode(ring);
            delete ring;
        }
    } else {
        if (!ring) {
            ring = new FeatherNode;
            ring->material.solid = solid;
            shape->appendChildNode(ring);
        }
        ring->geometry.allocate(int(n) * 6);
        auto *f = static_cast<FeatherVertex *>(ring->geometry.vertexData());
        auto put = [&](const QPointF &p, float alpha) {
            const QPointF t = uv(p);
            *f++ = {float(p.x()), float(p.y()), float(t.x()), float(t.y()), alpha};
        };
        for (qsizetype k = 0; k < n; ++k) {
            const qsizetype m = (k + 1) % n;
            put(inner[k], 1); put(outer[k], 0); put(outer[m], 0);
            put(inner[k], 1); put(outer[m], 0); put(inner[m], 1);
        }
        ring->material.texture = texture;
        ring->material.filtering = node->filtering();
        ring->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    }
    if (solid) {
        fill->material.texture = texture;
        fill->material.filtering = node->filtering();
    } else {
        cut->material.setTexture(texture);
        cut->material.setFiltering(node->filtering());
        cut->opaque.setTexture(texture);
        cut->opaque.setFiltering(node->filtering());
    }
    shape->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    node->setRect(QRectF());   // сам узел — пусто; Qt Wayland в следующий раз поставит свой прямоугольник заново
}
