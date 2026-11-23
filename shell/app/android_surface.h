// НаомиФон — поверхность окна Android на столе (docs/ANDROID.md, «Перетаскивание и мерцание»). В QML —
// AndroidSurface (Naomi.Desk).
//   Waydroid при каждой смене расклада экрана Android (окно сдвинули, всплыло окошко, сменился фокус) уничтожает
//   буферы всех своих окон (buffer_map.clear) и следом присылает новые. Qt Wayland, получив «буфер уничтожен»,
//   поверхность сразу считает пустой: кадр стола, нарисованный между этими сообщениями, — без Android. С тремя окнами
//   сообщения не влезают в один пакет, и стол попадал между ними: 9 кадров из 1542 за 20 с — все окна Android
//   пропадали на кадр (запись потока приставки, 18 ноября 2026; с одним окном — 0). Здесь поверхность, чей буфер
//   клиент уничтожил, а новый ещё не прислал, держит прежнюю картинку. Подповерхности (слои Waydroid) — такие же.
//   Два исправления для окна, где Android кладёт картинку приложения под свою полосу (AndroidMove, docs/ANDROID.md
//   «Полоса окна и панель»): shift — слой ниже своего места на столько (единицы элемента), с картинкой и нажатиями:
//   точку слоя Waydroid переводит в Android по месту слоя у Android, а там, под полосой, приложение у Android и лежит
//   — игра получает нажатие ровно там, где его видно; clip — от слоя видна и нажимается только верхняя полоска такой
//   высоты (окно приложения: полоса Android под нашей — за неё тащат; ниже — пустое место и чёрный низ, накрывавшие
//   игру)
//   Лишняя работа (docs/ANDROID-AUDIT.md, этап 1). Waydroid на каждый кадр Android заново подтверждает буферы всех
//   слоёв всех окон, менялись они или нет, а Qt Wayland на каждое подтверждение просит кадр стола: свёрнутая игра
//   заставляла стол рисовать и сжимать 75 одинаковых кадров в секунду. Здесь кадр стола просит только слой, который
//   видно и у которого новый буфер или новое место (committed); буфер невидимого слоя заперт. Наведение мыши Android не получает (hover*): на каждое
//   движение он двигал свой курсор и пересобирал кадр — 73 кадра в секунду и +1,5–1,9 Вт от мыши над окном; нажатие
//   несёт с собой место, перетаскивание идёт как было. NAOMI_ANDROID_HOVER=1 — слать наведение, как раньше
//   Прозрачный холст (docs/ANDROID-AUDIT.md, этап 2). Окно Waydroid — холст во весь экран Android из одной прозрачной
//   точки (буфер 1 × 1, растянут), слои приложения — поверх. Qt Wayland рисовал и его: полноэкранный слой со
//   смешением на каждое окно Android в каждом кадре стола — ~1 мс видеочипа на окно. Заливку из одной прозрачной
//   точки не рисуем вовсе (emptyFill). NAOMI_ANDROID_CANVAS=1 — рисовать, как раньше
#pragma once

#include <QWaylandQuickItem>

class QSGSimpleTextureNode;
struct wl_resource;

class AndroidSurface : public QWaylandQuickItem
{
    Q_OBJECT
public:
    explicit AndroidSurface(QQuickItem *parent = nullptr);
    // Qt Wayland зовёт для новой подповерхности (subsurfaceHandler): слой — тоже AndroidSurface
    Q_INVOKABLE void handleSubsurfaceAdded(QWaylandSurface *child);
    void setShift(qreal shift);
    void setClip(qreal height);   // 0 — слой целиком
    QPointF basePosition() const { return m_base; }   // место подповерхности у Waydroid, без shift
    // слой целиком (единицы родителя, без shift): Waydroid обрезает слой по краю экрана Android (окно, спущенное
    // ниже края, — короче), а буфер — весь; обрезка — viewport (sourceGeometry)
    QRectF fullRect() const;
    // Контур окна (единицы родителя — элемента поверхности окна; пустой — без контура) и радиус его углов: слой,
    // задевший угол окна, видно только внутри контура — углы скруглены, как у окон стола
    // (DeskStyle.outline), поля тени вокруг задачи — прочь (docs/ANDROID.md, «Полоса окна и панель»)
    void setOutline(const QRectF &window, qreal corner);

    bool contains(const QPointF &point) const override;

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void hoverEnterEvent(QHoverEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;
    void itemChange(ItemChange change, const ItemChangeData &data) override;

private:
    void watchSurface();
    void committed();
    bool hoverKept(QHoverEvent *event);
    bool emptyFill();
    struct ::wl_resource *m_fillOf = nullptr;   // для какого буфера emptyFill уже посчитан
    bool m_fillEmpty = false;
    QMetaObject::Connection m_commit;
    // прошлое подтверждение: буфер и его раскладка — то же самое в новом кадра стола не просит
    struct Shown {
        struct ::wl_resource *buffer = nullptr;
        QRectF source;
        QSize destination;
        int scale = 0;
        bool operator==(const Shown &o) const
        {
            return buffer == o.buffer && source == o.source && destination == o.destination && scale == o.scale;
        }
    } m_shownBuffer;
    bool m_missed = false;   // новое пришло, пока слой не видно, — нарисовать, когда покажут
    qreal unit() const;
    void outline(QSGSimpleTextureNode *node);
    void place(const QPoint &surfacePos);
    QPointF m_base;
    qreal m_shift = 0;
    qreal m_clip = 0;
    QRectF m_outline;
    qreal m_corner = 0;
};
