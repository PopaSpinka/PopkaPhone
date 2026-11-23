#include "android_move.h"

#include "android_surface.h"

#include <QLoggingCategory>
#include <QQuickWindow>
#include <QWaylandQuickItem>
#include <QWaylandSurface>
#include <QWaylandView>
#include <QWaylandBufferRef>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

Q_LOGGING_CATEGORY(lcAndroidMove, "naomi.android")

QHash<QString, QRect> AndroidMove::s_normal;

AndroidMove::AndroidMove(QQuickItem *parent)
    : QQuickItem(parent)
{
    // слои меняются по одному (место, размер — отдельными сигналами, по слою за раз): смотреть, когда пачка пришла
    m_check.setSingleShot(true);
    m_check.setInterval(0);
    connect(&m_check, &QTimer::timeout, this, &AndroidMove::check);
    // Отпустили — Android переносит задачу по команде за ~0,2 с (первый раз после запуска naomid — ещё 1,5 с: она
    // открывает оболочку Android); до тех пор окно держит сдвиг стола (стоит, где отпустили). Не перенёс за 4 с —
    // сдвиг остаётся: окно там, где отпустили, ввод — по месту у Android (Qt Wayland переводит точки через элемент)
    m_settle.setSingleShot(true);
    m_settle.setInterval(4000);
    connect(&m_settle, &QTimer::timeout, this, [this] {
        qCInfo(lcAndroidMove, "окно Android: перенос не доведён за 4 с — у окна остаётся сдвиг стола %.1f, %.1f точки",
               m_offset.x(), m_offset.y());
    });
    // слои сменились сами — спросить Android о задаче, когда пачка смен прошла
    m_askTasks.setSingleShot(true);
    m_askTasks.setInterval(150);
    connect(&m_askTasks, &QTimer::timeout, this, [this] {
        emit wantTasks();
        m_sameTasks.start();
    });
    // Спросили — а нового списка нет: у Android всё по-прежнему (naomid шлёт список, только когда он сменился). Рамка,
    // уехавшая за слоями по догадке (check), — обратно, по прежнему ответу
    m_sameTasks.setSingleShot(true);
    m_sameTasks.setInterval(400);
    connect(&m_sameTasks, &QTimer::timeout, this, [this] {
        if (m_known && !m_tasks.isEmpty() && !m_moving && !m_settle.isActive() && !m_previewSent)
            setTasks(m_tasks);
    });
    m_applyTask.setSingleShot(true);
    m_applyTask.setInterval(120);
    connect(&m_applyTask, &QTimer::timeout, this, &AndroidMove::applyTask);
    m_fitEnd.setSingleShot(true);
    m_fitEnd.setInterval(1500);
    connect(&m_fitEnd, &QTimer::timeout, this, [this] {
        m_fitDone = true;
        setFitting(false);
    });
    m_previewEnd.setSingleShot(true);
    m_previewEnd.setInterval(4000);
    connect(&m_previewEnd, &QTimer::timeout, this, [this] {
        qCInfo(lcAndroidMove, "окно Android: размер не принят за 4 с — рамка снова его");
        endPreview();
    });
}

// Слои, сменившиеся к кадру стола, — проверить до его отрисовки: шаг Android и вычет стола — в одном кадре. Через
// очередь событий вычет опаздывал на кадр-другой, и при отпускании окно на это время уходило вдвое (запись потока)
void AndroidMove::itemChange(ItemChange change, const ItemChangeData &data)
{
    if (change == ItemSceneChange) {
        disconnect(m_sync);
        if (data.window)
            m_sync = connect(data.window, &QQuickWindow::beforeSynchronizing, this, [this] {
                if (m_check.isActive()) {
                    m_check.stop();
                    check();
                }
            }, Qt::DirectConnection);
    }
    QQuickItem::itemChange(change, data);
}

void AndroidMove::setSurface(QQuickItem *surface)
{
    if (surface == m_surface)
        return;
    if (m_surface)
        m_surface->disconnect(this);
    m_surface = surface;
    if (m_surface) {
        connect(m_surface, &QQuickItem::childrenChanged, this, &AndroidMove::watchLayers);
        watchLayers();
    }
    emit surfaceChanged();
}

void AndroidMove::watchLayers()
{
    if (!m_surface)
        return;
    for (QQuickItem *layer : m_surface->childItems()) {
        // слой, уже подключённый раньше, — без второго подключения
        connect(layer, &QQuickItem::xChanged, &m_check, qOverload<>(&QTimer::start), Qt::UniqueConnection);
        connect(layer, &QQuickItem::yChanged, &m_check, qOverload<>(&QTimer::start), Qt::UniqueConnection);
        connect(layer, &QQuickItem::widthChanged, &m_check, qOverload<>(&QTimer::start), Qt::UniqueConnection);
        connect(layer, &QQuickItem::heightChanged, &m_check, qOverload<>(&QTimer::start), Qt::UniqueConnection);
        connect(layer, &QQuickItem::visibleChanged, &m_check, qOverload<>(&QTimer::start), Qt::UniqueConnection);
    }
    m_check.start();
}

// Слой с картинкой: буфер есть (или уничтожен клиентом, а AndroidSurface держит прежнюю картинку). Лишним слоям
// Waydroid при смене расклада ставит пустой буфер (neutralize_remaining_subsurfaces) — их место и размер прежние, но
// они ничто: по такому слою стол ставил полосу (было: после «вернуть» из «во весь экран» полоса — шире окна)
bool AndroidMove::hasPicture(const QQuickItem *layer)
{
    auto *item = qobject_cast<const QWaylandQuickItem *>(layer);
    if (!item || !item->isVisible() || !item->surface() || item->width() <= 0 || item->height() <= 0)
        return false;
    return item->surface()->hasContent() || (item->view() && item->view()->currentBuffer().isDestroyed());
}

QList<QRectF> AndroidMove::content() const
{
    QList<QRectF> out;
    if (m_surface)
        for (QQuickItem *layer : m_surface->childItems()) {
            if (!hasPicture(layer))
                continue;
            const auto *own = qobject_cast<AndroidSurface *>(layer);
            out << (own ? own->fullRect() : QRectF(layer->position(), layer->size()));
        }
    return out;
}

// Окно Android и его полоса (docs/ANDROID.md, «Полоса окна и панель»). Самый большой слой окна — поверхность окна
// приложения: задача и поля тени вокруг (surfaceInsets 15 пикселей Android у окна freeform; развёрнутое — во весь
// экран, без полей). Полоса Android (DecorCaptionView, 42 dp) — в этой поверхности сверху, над ней стол рисует свою
// (AndroidWindow.qml). Слой ровно по задаче шириной и от её верха, но короче на высоту полосы, — картинка приложения,
// которую Android кладёт под свою полосу: так у Subway Surfers (Unity, окно с FULLSCREEN) — полоса накрывала верх
// игры, а внизу окно приложения заливало чёрным полосу той же высоты (слои SurfaceFlinger: окно 618 × 1098, картинка
// 618 × 1034 от его верха; у Android 13 полоса по умолчанию лежит поверх приложения —
// Window.mOverlayWithDecorCaptionEnabled; приложение, которое не отступает от неё само, — под ней). Стол ставит
// картинку ниже полосы целиком, с нажатиями (AndroidSurface::setShift), а от окна приложения оставляет только полоску
// сверху (setClip: полоса Android под нашей — за неё тащат; пустое место и чёрный низ — прочь). Нажатие в картинку
// Waydroid переводит по месту слоя у Android — туда, где игра у него и лежит: игра получает его ровно там, где его
// видно (проверено: пауза, «Продолжить», магазин)
void AndroidMove::frameWindow()
{
    auto *surface = qobject_cast<QWaylandQuickItem *>(m_surface.data());
    QRectF f;
    m_task = {};
    if (surface && surface->surface() && m_surface->width() > 0) {
        // слои целиком (AndroidSurface::fullRect): окно, спущенное за край экрана Android, Waydroid обрезает —
        // окно приложения и картинка короче на разное (проверено: после переноса к низу картинка 995 из 1034)
        QList<AndroidSurface *> layers;
        AndroidSurface *window = nullptr;
        QRectF w;
        for (QQuickItem *layer : m_surface->childItems())
            if (auto *item = qobject_cast<AndroidSurface *>(layer); item && !hasPicture(item)) {
                item->setShift(0);   // пустой — ничего не рисует; получит картинку — пересчитается с прочими
                item->setClip(0);
                item->setOutline({}, 0);
            } else if (item) {
                layers << item;
                const QRectF r = item->fullRect();
                if (r.width() * r.height() > w.width() * w.height()) {
                    w = r;
                    window = item;
                }
            }
        const qreal px = this->px();
        if (m_known) {
            // рамка — задача Android (applyTask): её границы знает только он
            f = QRectF(m_taskPx.topLeft() / px, m_taskPx.size() / px);
        } else if (!w.isEmpty()) {
            // Android ещё не ответил — по слоям: самый большой — окно приложения с полями тени 15 пикселей
            // (развёрнутое — во весь экран, без полей)
            if (m_full)
                w = QRectF(QPointF(), m_surface->size());
            const qreal inset = m_full || (w.left() <= 1 && w.top() <= 1) ? 0 : 15 / px;
            f = w.adjusted(inset, inset, -inset, -inset);
        }
        // больше видимой области — не показывать, пока не переставлено (applyTask); и по слоям, до ответа Android
        if (!f.isEmpty() && !m_fitDone && m_preview.isEmpty() && !m_panel
            && !fitted(QRectF(f.topLeft() * px, f.size() * px)).isEmpty())
            setFitting(true);
        if (!f.isEmpty()) {
            constexpr qreal kCaptionMin = 40, kCaptionMax = 100;   // пикселей Android: полоса — 42 dp, 63 пикселя
            const qreal tol = 2 / px;
            qreal under = 0;
            for (AndroidSurface *item : layers) {
                const QRectF r = item->fullRect();
                const qreal shortBy = f.height() - r.height();
                const bool misplaced = item != window && std::abs(r.left() - f.left()) < tol
                                       && std::abs(r.top() - f.top()) < tol && std::abs(r.width() - f.width()) < tol
                                       && shortBy * px >= kCaptionMin && shortBy * px <= kCaptionMax;
                item->setShift(misplaced ? shortBy : 0);
                if (misplaced)
                    under = shortBy;
            }
            for (AndroidSurface *item : layers)
                item->setClip(item == window && under > 0 ? f.top() - item->basePosition().y() + under : 0);
            m_task = f;
            // Тянут край — рамка окна стола (setPreview), отпустили — пока Android не встал в неё (setTasks)
            if (!m_preview.isEmpty())
                f = m_preview;
            // углы окна — как у окон стола: DeskStyle.corner(ширина, высота) в точках стола; картинка Android — по
            // рамке (тянут край: больше рамки — срезана, меньше — под ней фон окна, AndroidWindow.qml)
            const qreal k = m_surface->scale();
            const qreal corner = k > 0 && !m_panel ? std::min(0.35 * std::min(f.width(), f.height()) * k, 32.0) / k : 0;
            for (AndroidSurface *item : layers)
                item->setOutline(f, corner);
            f = QRectF(f.topLeft() * k + (m_full ? QPointF() : m_offset), f.size() * k);
        }
    }
    if (f != m_window) {
        m_window = f;
        emit windowChanged();
    }
}

qreal AndroidMove::px() const
{
    auto *surface = qobject_cast<QWaylandQuickItem *>(m_surface.data());
    return surface && surface->surface() && m_surface->width() > 0
               ? surface->surface()->destinationSize().width() / m_surface->width()
               : 1;
}

// Размер рамки тот же — с допуском 2 пикселя Android на округления (смена оформления — тень — десятки пикселей)
bool AndroidMove::sameSize(QSizeF a, QSizeF b) const
{
    const qreal tol = 2 / px();
    return std::abs(a.width() - b.width()) < tol && std::abs(a.height() - b.height()) < tol;
}

// Сдвиг задачи по слоям: слои, что есть и были, с прежним размером, сдвинулись все на одно и то же — Android перенёс
// задачу на столько (единицы элемента поверхности); нет — пусто. Не по одному самому большому: при переносе Android
// бывает, меняет окнам тень (фокус), и размер самого большого — другой, а у «Настроек» в две колонки самым большим
// становится то одна колонка, то другая (было: перенос не засчитывался — окно уезжало вдвое, 18 ноября 2026)
QPointF AndroidMove::layersMoved(const QHash<const QQuickItem *, QRectF> &now) const
{
    const qreal tol = 2 / px();
    std::optional<QPointF> delta;
    for (auto it = now.cbegin(); it != now.cend(); ++it) {
        const QRectF was = m_layers.value(it.key());
        if (was.isEmpty() || !sameSize(was.size(), it.value().size()))
            continue;
        const QPointF d = it.value().topLeft() - was.topLeft();
        if (!delta)
            delta = d;
        else if (std::abs(d.x() - delta->x()) > tol || std::abs(d.y() - delta->y()) > tol)
            return {};   // слои сдвинулись по-разному — это не перенос задачи
    }
    return delta.value_or(QPointF());
}

void AndroidMove::check()
{
    if (!m_surface)
        return;
    if (!m_known && !m_applyTask.isActive() && !m_tasks.isEmpty())
        setTasks(m_tasks);   // список пришёл раньше слоёв окна — naomid шлёт его только при изменении
    QHash<const QQuickItem *, QRectF> now;
    for (QQuickItem *layer : m_surface->childItems())
        if (hasPicture(layer)) {
            const auto *own = qobject_cast<AndroidSurface *>(layer);
            now.insert(layer, own ? own->fullRect() : QRectF(layer->position(), layer->size()));
        }
    const QPointF moveBy = layersMoved(now);
    const bool changed = now != m_layers;
    const bool moved = !moveBy.isNull();
    // Android перенёс задачу: рамка — следом, кадр в кадр с картинкой, не дожидаясь его ответа. Свой размер ждёт ответа
    // (рамка — та, что просили): перенос слоёв тут — шаг Android к ней, а не новое место задачи.
    // Перенос, которого стол не просил, — только чистый сдвиг: слои те же и того же размера. «Настройки», раскладываясь
    // в две колонки, двигают правую на ширину левой — стол принимал это за перенос задачи, а Android отвечал прежними
    // границами (ответ без изменений naomid не шлёт): рамка оставалась на 654 точки правее окна, левая колонка —
    // срезана (20 ноября 2026). Свой перенос (m_settle) — как раньше: Android при нём меняет окнам тень и размер
    bool pure = now.size() == m_layers.size();
    for (auto it = now.cbegin(); pure && it != now.cend(); ++it)
        pure = m_layers.contains(it.key()) && sameSize(m_layers.value(it.key()).size(), it.value().size());
    if (m_known && moved && !m_previewSent && (pure || m_settle.isActive()))
        m_taskPx.translate(moveBy * px());
    if (changed && m_applyTask.isActive())
        m_applyTask.start();   // ответ Android ждёт, пока слои успокоятся
    // Размер или состав слоёв сменился сам (приложение разложилось заново, Android ограничил размер, окно стало двумя)
    // или сдвиг не наш — спросить Android, где задача (wantTasks → naomiAndroid.refreshTasks); после своих переноса и
    // размера naomid перечитывает задачи сама
    if (changed && !now.isEmpty() && (!moved || !m_settle.isActive()))
        m_askTasks.start();
    if (!m_known) {
        // Android ещё не ответил — во весь ли экран, по слоям: все вместе закрывают весь экран Android («Настройки» в два
        // столбца — два окна, 872 и 1528 пикселей)
        QRectF all;
        for (const QRectF &c : now)
            all |= c;
        setFull(all.left() <= 1 && all.top() <= 1 && all.right() >= m_surface->width() - 2
                && all.bottom() >= m_surface->height() - 2);
    }
    m_layers = now;
    frameWindow();
    // Отпустили — Android переносит задачу (moveTask): каждый его шаг стол тут же вычитает из своего сдвига — картинка
    // стоит, где её отпустили, ни кадра скачка, а сдвиг стола уходит к нулю. Встал Android не там (своё ограничение у
    // края) — остаток остаётся у окна. Угадывать «куда он перенесёт» не годилось: на 12 пикселей мимо — и сдвиг не
    // снимался, окно уезжало вдвое (проверено записью). Схватили снова, пока Android ещё не довёл, — вычет идёт и под
    // мышью (точка, от которой тащат, сдвигается с ним). Перенос по слоям не узнан (все сменили размер) — вычтет ответ
    // Android (applyTask)
    if (m_settle.isActive() && moved)
        compensate(moveBy * m_surface->scale());
}

// Android сдвинул окно на d (точки стола), пока стол держит его сдвигом, — сдвиг стола меньше на столько же
void AndroidMove::compensate(QPointF d)
{
    m_offset -= d;
    if (m_moving)
        m_pressOffset -= d;
    place();
    emit offsetChanged();
    if (!m_moving && std::abs(m_offset.x()) < 3 && std::abs(m_offset.y()) < 3) {
        m_settle.stop();
        qCInfo(lcAndroidMove, "окно Android перенесено — сдвиг стола снят");
    }
}

void AndroidMove::setPanel(bool panel)
{
    if (panel == m_panel)
        return;
    m_panel = panel;
    frameWindow();
    emit panelChanged();
}

QRect AndroidMove::screenRect() const
{
    return m_surface ? androidRect(QRectF(QPointF(), m_surface->size())) : QRect();
}

QRect AndroidMove::seenRect() const
{
    const QRect s = screenRect();
    return s.adjusted(0, m_hiddenPx, 0, 0);
}

QRect AndroidMove::fitted(const QRectF &task) const
{
    const QRect seen = seenRect(), screen = screenRect();
    const QRect t = task.toRect();
    if (seen.isEmpty() || t.isEmpty() || (t.width() <= seen.width() + 2 && t.height() <= seen.height() + 2))
        return {};
    if (std::abs(t.top() - screen.top()) <= 2 && std::abs(t.height() - screen.height()) <= 2)
        return {};   // на весь экран Android (полоса — под панелью стола): так и задумано
    QRect to(0, 0, std::min(t.width(), seen.width()), std::min(t.height(), seen.height()));
    to.moveTo(std::clamp(t.left(), seen.left(), seen.left() + seen.width() - to.width()),
              std::clamp(t.top(), seen.top(), seen.top() + seen.height() - to.height()));
    return to;
}

void AndroidMove::setFitting(bool fitting)
{
    if (fitting == m_fitting)
        return;
    m_fitting = fitting;
    if (fitting)
        m_fitEnd.start();
    else
        m_fitEnd.stop();
    emit fittingChanged();
}

void AndroidMove::setFull(bool full)
{
    if (full == m_full)
        return;
    m_full = full;
    place();
    emit fullChanged();
}

// Задачи Android (naomiAndroid.tasks, от naomid): своя — пакета этого окна; их у пакета несколько — ближайшая к рамке
// (или, пока её нет, к слоям). Рамку не выводить из слоёв: самый большой слой — не окно (у «Настроек» шире 1200
// пикселей — правая из двух колонок, а при нажатии Android меняет колонкам тень, и самой большой становится другая)
void AndroidMove::setTasks(const QVariantList &tasks)
{
    m_sameTasks.stop();
    m_tasks = tasks;
    if (!m_surface || m_pkg.isEmpty())
        return;
    const qreal p = px();
    QRectF nearTo = m_known ? QRectF(m_task.topLeft() * p, m_task.size() * p) : QRectF();
    if (!m_known) {
        for (const QRectF &c : content())
            nearTo |= c;
        nearTo = QRectF(nearTo.topLeft() * p, nearTo.size() * p);
    }
    QRectF best;
    QString mode;
    qreal bestD = std::numeric_limits<qreal>::max();
    for (const QVariant &v : tasks) {
        const QVariantMap t = v.toMap();
        if (t.value("pkg").toString() != m_pkg)
            continue;
        const QRectF r(t.value("x").toReal(), t.value("y").toReal(), t.value("w").toReal(), t.value("h").toReal());
        const QPointF d = r.center() - nearTo.center();
        const qreal dist = std::abs(d.x()) + std::abs(d.y()) + std::abs(r.width() - nearTo.width())
                           + std::abs(r.height() - nearTo.height());
        if (r.isEmpty() || dist >= bestD)
            continue;
        bestD = dist;
        best = r;
        mode = t.value("mode").toString();
    }
    if (best.isEmpty())
        return;
    m_pendingTask = best;
    m_pendingMode = mode;
    // Свой перенос или размер ещё идёт — ответ, пришедший раньше, чем Android переложил слои, принять потом: рамка
    // сейчас едет со слоями (check), а по ответу встала бы на новое место ещё до картинки — и потом уехала бы с ней
    // вдвое (проверено: «Настройки» — на 865 пикселей левее картинки). Иначе — сразу
    if (m_settle.isActive() || m_previewSent || m_moving)
        m_applyTask.start();
    else
        applyTask();
}

// Ответ Android: рамка — его границы задачи; во весь экран — его режим «во весь экран» или границы во весь экран;
// тянули край и отпустили — Android встал в новую рамку (±2 пикселя) — рамка снова его
void AndroidMove::applyTask()
{
    if (m_pendingTask.isEmpty() || !m_surface)
        return;
    const QRectF best = std::exchange(m_pendingTask, {});
    const bool first = !m_known;
    // Свой перенос: Android встал, а по слоям стол этого не узнал (слои сменили размер вместе с переносом) — вычесть
    // по его ответу то, что ещё не вычтено (рамка уже ехала со слоями — разница только неузнанное)
    if (m_known && m_settle.isActive() && sameSize(best.size() / px(), m_taskPx.size() / px())) {
        const QPointF d = (best.topLeft() - m_taskPx.topLeft()) / px() * m_surface->scale();
        if (!d.isNull())
            compensate(d);
    }
    m_known = true;
    m_taskPx = best;
    const QRectF screen = QRectF(screenRect()), seen = QRectF(seenRect());
    auto same = [](const QRectF &a, const QRectF &b) {
        return std::abs(a.left() - b.left()) <= 2 && std::abs(a.top() - b.top()) <= 2
               && std::abs(a.right() - b.right()) <= 2 && std::abs(a.bottom() - b.bottom()) <= 2;
    };
    // на весь экран — задача во весь экран Android (с полосой под панелью стола) или в его режиме «во весь экран»;
    // развёрнуто — во всю видимую область
    setPanel(m_pendingMode == QLatin1String("fullscreen") || same(best, screen));
    setFull(m_panel || same(best, seen));
    if (m_previewSent) {
        const QRectF want = QRectF(androidRect(m_preview));
        if (std::abs(best.left() - want.left()) <= 2 && std::abs(best.top() - want.top()) <= 2
            && std::abs(best.width() - want.width()) <= 2 && std::abs(best.height() - want.height()) <= 2) {
            qCInfo(lcAndroidMove, "окно Android встало в новый размер");
            endPreview();
        }
    }
    frameWindow();
    if (first)
        qCInfo(lcAndroidMove, "окно %s: задача Android %.0f,%.0f %.0fx%.0f", qPrintable(m_pkg), best.x(), best.y(),
               best.width(), best.height());
    // один раз на окно, когда его размеры уже известны (в миг создания окна экран Android у элемента ещё пуст)
    if (!m_fitDone && !seenRect().isEmpty() && !m_task.isEmpty()) {
        m_fitDone = true;
        const QRect fit = m_panel ? QRect() : fitted(best);
        if (fit.isEmpty()) {
            setFitting(false);
        } else {   // рамка окна — туда сразу; покажется, когда Android встанет (endPreview) или через 1,5 с
            setFitting(true);
            resizeTo(fit, "открыто больше видимой области — в неё", false);
        }
    }
    bringIntoView();
}

// Окно, которое за краем экрана Android так, что его не взять (стол стал меньше — с приставки на окно Mac, экран
// Android 2400 × 1290 → 1920 × 1146; или Android сам), — Android переносит в экран, как keep держит перетаскиваемое
void AndroidMove::bringIntoView()
{
    if (!m_known || m_full || m_moving || m_settle.isActive() || !m_preview.isEmpty() || m_task.isEmpty()
        || m_surface->scale() <= 0)
        return;
    const QPointF in = keep(m_offset);
    if (in == m_offset)
        return;
    const QRect from = androidRect(m_task);
    if (from == m_broughtFrom)
        return;   // уже просили — Android не пустил; не просить по кругу
    m_broughtFrom = from;
    const QPointF at = (m_task.topLeft() + in / m_surface->scale()) * px();
    const QPoint to(qRound(at.x()), qRound(at.y()));
    qCInfo(lcAndroidMove, "окно Android за краем экрана — в экран: %d,%d → %d,%d", from.x(), from.y(), to.x(), to.y());
    emit moveTask(from, to);
}

void AndroidMove::press(QPointF at)
{
    if (!m_surface || m_full || m_task.isEmpty())
        return;
    m_pressAt = at;
    m_pressOffset = m_offset;
    m_moving = true;
    emit movingChanged();
}

void AndroidMove::drag(QPointF at)
{
    if (!m_moving)
        return;
    const QPointF to = keep(m_pressOffset + (at - m_pressAt));
    if (to == m_offset)
        return;
    m_offset = to;
    place();
    emit offsetChanged();
}

// Задача у Android — туда, где окно видно на столе: её место у Android плюс сдвиг стола (у края — ограниченный keep,
// а не путь мыши). Без переноса окно у Android оставалось бы там, где его взяли: Android прячет окно, целиком закрытое
// другим, по своим координатам, а Waydroid такое окно уничтожает (docs/ANDROID.md)
// Растягивание — как перетаскивание: за край тянет стол (рамка окна, полоса и фон — в такт стола), Android получает
// одно «задача — в эту рамку», когда отпустили. Сам Android тянул край по шагу на каждое движение мыши и
// перекладывал приложение каждый раз: «Калькулятор» догонял мышь секунды (запись потока 18 ноября 2026), а его окно на
// это время — поверхность во весь экран
void AndroidMove::setPreview(QRectF rect)
{
    if (!m_surface || m_full || m_surface->scale() <= 0 || rect.isEmpty())
        return;
    const bool was = !m_preview.isEmpty();
    m_preview = QRectF((rect.topLeft() - m_offset) / m_surface->scale(), rect.size() / m_surface->scale());
    m_previewSent = false;
    m_previewEnd.stop();
    frameWindow();
    if (!was)
        emit resizingChanged();
}

void AndroidMove::commitResize()
{
    if (m_preview.isEmpty() || m_previewSent)
        return;
    const QRect from = androidRect(m_task), to = androidRect(m_preview);
    if (from == to) {
        endPreview();
        return;
    }
    m_previewSent = true;
    m_previewEnd.start();
    qCInfo(lcAndroidMove, "окно Android: размер %d,%d %dx%d → %d,%d %dx%d", from.x(), from.y(), from.width(),
           from.height(), to.x(), to.y(), to.width(), to.height());
    emit resizeTask(from, to);
}

void AndroidMove::endPreview()
{
    if (m_preview.isEmpty())
        return;
    m_preview = {};
    m_previewSent = false;
    m_previewEnd.stop();
    frameWindow();
    emit resizingChanged();
    setFitting(false);
}

QRect AndroidMove::androidRect(const QRectF &r) const
{
    const qreal p = px();
    return QRect(qRound(r.x() * p), qRound(r.y() * p), qRound(r.width() * p), qRound(r.height() * p));
}

// Развернуть — как у окон стола: во всю область под панелью, то есть во всю видимую часть экрана Android (без скрытой
// полосы сверху), окном Android (задача freeform такого размера: полоса Android и картинка — как у обычного окна,
// приложение раскладывается заново по новому размеру); где было — запомнить (resizeTo). Вернуть — туда (normalRect)
void AndroidMove::toggleMaximized()
{
    if (m_panel)
        return;   // на весь экран — полосы нет, двойного щелчка тоже
    askZoom(m_full ? normalRect() : seenRect(), m_full ? "вернуть" : "развернуть");
}

void AndroidMove::askZoom(const QRect &to, const char *what)
{
    m_zoomTo = to;
    m_zoomWhat = what;
    emit zoomAsked();
}

void AndroidMove::zoomNow()
{
    if (!m_zoomTo.isEmpty())
        resizeTo(std::exchange(m_zoomTo, {}), m_zoomWhat);
}

// На весь экран — как браузер в режиме панели (docs/ANDROID.md, «На весь экран»): задача — во весь экран Android, его
// полоса — в скрытой полосе экрана под панелью стола, приложение — на всю видимую область; вернуть — туда, где окно было
void AndroidMove::togglePanel()
{
    askZoom(m_panel ? normalRect() : screenRect(), m_panel ? "из «на весь экран» — окном" : "на весь экран");
}

// где окно было до «развернуть» или «на весь экран»; не запомнено (так его открыл Android) — по середине видимой
// области, половина ширины и три четверти высоты
QRect AndroidMove::normalRect() const
{
    const QRect normal = s_normal.value(m_pkg);
    if (!normal.isEmpty())
        return normal;
    const QRect seen = seenRect();
    return QRect(seen.x() + seen.width() / 4, seen.y() + seen.height() / 8, seen.width() / 2, seen.height() * 3 / 4);
}

void AndroidMove::resizeTo(const QRect &to, const char *what, bool remember)
{
    if (m_task.isEmpty() || !m_surface || m_surface->scale() <= 0 || to.isEmpty())
        return;
    // второй щелчок двойного начал перетаскивание — оборвать: его отпускание запустило бы вычет переноса, а Android,
    // разворачивая окно, сперва переносит его в угол прежнего размера — стол «вычел» бы это, и вернувшееся окно встало
    // бы вдвое дальше (проверено: 888 → 1776)
    if (m_moving) {
        m_moving = false;
        emit movingChanged();
    }
    m_settle.stop();
    const QRect now = androidRect(m_task);
    if (!m_full && remember)   // где окно видно на столе — туда «вернуть»
        s_normal[m_pkg] = androidRect(m_task.translated(m_offset / m_surface->scale()));
    m_offset = {};   // дальше окно там, куда его ставит Android
    place();
    emit offsetChanged();
    // Рамка — сразу там, куда просим, как у края (setPreview), и ждёт, пока Android туда встанет. Он встаёт не сразу:
    // сперва переносит окно прежнего размера в угол, потом приложение раскладывается заново. Рамка по задаче ехала бы
    // за этим переносом слоёв (check), а задача, прочитанная раньше переноса, уже на новом месте — рамка уезжала вдвое
    // (было: развёрнутый Subway — рамка на 240 пикселей левее экрана, справа — полоса стола)
    const bool was = !m_preview.isEmpty();
    m_preview = QRectF(QPointF(to.topLeft()) / px(), QSizeF(to.size()) / px());
    m_previewSent = true;
    m_previewEnd.start();
    frameWindow();
    if (!was)
        emit resizingChanged();
    qCInfo(lcAndroidMove, "окно Android: %s — %d,%d %dx%d", what, to.x(), to.y(), to.width(), to.height());
    emit resizeTask(now, to);
}

void AndroidMove::release()
{
    if (!m_moving)
        return;
    m_moving = false;
    emit movingChanged();
    if (m_offset == m_pressOffset || m_task.isEmpty() || !m_surface || m_surface->scale() <= 0)
        return;   // щёлкнули, не тащили
    const qreal p = px();
    const QRect from = androidRect(m_task);
    const QPointF at = (m_task.topLeft() + m_offset / m_surface->scale()) * p;
    const QPoint to(qRound(at.x()), qRound(at.y()));
    m_settle.start();
    qCInfo(lcAndroidMove, "окно Android отпущено: задача %d,%d → %d,%d", from.x(), from.y(), to.x(), to.y());
    emit moveTask(from, to);
}

// Окно не уходит за край так, чтобы его нельзя было взять снова: верх (слои вместе, а не одно приложение: полоса
// Android лежит над ним) — на 4 точки ниже верха области под панелью: окно, упёртое в самый верх экрана Android
// (y = 0), Android 13 оставляет без своей полосы (проверено: слой «Caption of Task» у SurfaceFlinger пропадает), и
// раскладка окна под нашей полосой была бы другой; по бокам и снизу на столе остаётся 64 точки
QPointF AndroidMove::keep(QPointF offset) const
{
    QRectF all = m_task;   // задача Android; пока её нет — слои вместе (полоса лежит над приложением)
    if (all.isEmpty())
        for (QQuickItem *layer : m_surface->childItems())
            if (layer->isVisible() && layer->width() > 0 && layer->height() > 0)
                all |= QRectF(layer->position(), layer->size());
    if (all.isEmpty())
        return offset;
    const qreal k = m_surface->scale();
    const qreal kKeep = 64, kTop = m_hiddenPx / px() * k + 4;   // верх — ниже скрытой полосы экрана (под панелью)
    const QRectF f(all.topLeft() * k, all.size() * k);   // окно у Android, точки стола
    // левее края экрана — нет: таких мест Android не принимает (cmd activity task resize — «bad left arg»; Waydroid и
    // сам держит левый край окна не левее экрана, TaskPositioner)
    return {std::clamp(offset.x(), -f.left(), std::max(-f.left(), width() - kKeep - f.left())),
            std::clamp(offset.y(), kTop - f.top(), std::max(kTop - f.top(), height() - kKeep - f.top()))};
}

void AndroidMove::place()
{
    if (m_surface)
        m_surface->setPosition(m_full ? QPointF() : m_offset);
    frameWindow();
}
