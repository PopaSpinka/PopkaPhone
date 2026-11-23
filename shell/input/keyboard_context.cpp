#include "keyboard_context.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QTimer>
#include <QInputMethodEvent>
#include <QInputMethodQueryEvent>
#include <QKeyEvent>

namespace {
KeyboardContext *g_instance = nullptr;

// D-Bus: клавиши от оболочки (трансляция экрана) — программе, у которой фокус
class ExternalKeyboard : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Input")
public:
    explicit ExternalKeyboard(KeyboardContext *ctx) : QObject(ctx), m_ctx(ctx) {}
public slots:
    Q_SCRIPTABLE void key(bool down, int qtKey, int modifiers, const QString &text)
    {
        m_ctx->externalKey(down, qtKey, modifiers, text);
    }
private:
    KeyboardContext *m_ctx;
};
}

KeyboardContext *KeyboardContext::instance()
{
    return g_instance;
}

KeyboardContext::KeyboardContext()
{
    g_instance = this;
    // только в программах (клиентах Wayland оболочки): оболочка печатает в свои поля сама
    QTimer::singleShot(0, this, [this] {
        if (QGuiApplication::platformName() != QLatin1String("wayland"))
            return;
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (bus.registerService(QStringLiteral("org.naomi.Input.p%1").arg(QCoreApplication::applicationPid())))
            bus.registerObject(QStringLiteral("/input"), new ExternalKeyboard(this),
                               QDBusConnection::ExportScriptableSlots);
    });
}

KeyboardContext::~KeyboardContext()
{
    if (g_instance == this)
        g_instance = nullptr;
}

void KeyboardContext::showInputPanel()
{
    // поле само просит клавиатуру (касание по нему); без поля ввода в фокусе — не показываем
    if (query(Qt::ImEnabled).toBool())
        setVisible(true);
}

void KeyboardContext::hideInputPanel()
{
    setVisible(false);
}

// Фокус ушёл к другому элементу без ввода — клавиатура прячется. Фокуса нет вовсе (окно программы потеряло
// фокус Wayland: её увели домой) — остаётся как была: она часть окна и уезжает вместе с ним
void KeyboardContext::setFocusObject(QObject *object)
{
    if (object && m_visible && !query(Qt::ImEnabled).toBool())
        setVisible(false);
}

void KeyboardContext::update(Qt::InputMethodQueries queries)
{
    if (m_visible && (queries & Qt::ImEnabled) && QGuiApplication::focusObject() && !query(Qt::ImEnabled).toBool())
        setVisible(false);
}

void KeyboardContext::commit(const QString &text)
{
    QObject *focus = QGuiApplication::focusObject();
    if (!focus)
        return;
    QInputMethodEvent e;
    e.setCommitString(text);
    QCoreApplication::sendEvent(focus, &e);
}

// исправление слова: одно событие ввода с заменой (программе по Wayland — тем же протоколом text-input)
void KeyboardContext::replaceBefore(int count, const QString &text)
{
    QObject *focus = QGuiApplication::focusObject();
    if (!focus)
        return;
    QInputMethodEvent e;
    e.setCommitString(text, -count, count);
    QCoreApplication::sendEvent(focus, &e);
}

void KeyboardContext::key(int qtKey)
{
    QObject *focus = QGuiApplication::focusObject();
    if (!focus)
        return;
    const QString text = qtKey == Qt::Key_Return ? QStringLiteral("\r")
                       : qtKey == Qt::Key_Backspace ? QStringLiteral("\b") : QString();
    QKeyEvent press(QEvent::KeyPress, qtKey, Qt::NoModifier, text);
    QCoreApplication::sendEvent(focus, &press);
    QKeyEvent release(QEvent::KeyRelease, qtKey, Qt::NoModifier, text);
    QCoreApplication::sendEvent(focus, &release);
}

void KeyboardContext::setRealKeys(bool on)
{
    s_realKeys = on;
}

void KeyboardContext::externalKey(bool down, int qtKey, int modifiers, const QString &text)
{
    QObject *focus = QGuiApplication::focusObject();
    if (!focus)
        return;
    if (!qtKey && !text.isEmpty()) {
        const bool ascii = text.size() == 1 && text[0].unicode() < 0x80;
        if (!s_realKeys || (!ascii && query(Qt::ImEnabled).toBool())) {
            if (down)
                commit(text);
            return;
        }
        qtKey = ascii ? text[0].toUpper().unicode() : 0;   // клавиши Qt для латиницы и цифр — их коды ASCII
    }
    QKeyEvent e(down ? QEvent::KeyPress : QEvent::KeyRelease, qtKey, Qt::KeyboardModifiers(modifiers), text);
    QCoreApplication::sendEvent(focus, &e);
}

QString KeyboardContext::textBeforeCursor() const
{
    const QString text = query(Qt::ImSurroundingText).toString();
    const int cursor = query(Qt::ImCursorPosition).toInt();
    return text.left(qBound(0, cursor, int(text.size())));
}

int KeyboardContext::hints() const
{
    return query(Qt::ImHints).toInt();
}

void KeyboardContext::setRect(qreal x, qreal y, qreal w, qreal h)
{
    const QRectF r(x, y, w, h);
    if (r == m_rect)
        return;
    m_rect = r;
    emitKeyboardRectChanged();
}

bool KeyboardContext::trackStart()
{
    QObject *item = QGuiApplication::focusObject();
    if (!item || !item->property("cursorRectangle").isValid())
        return false;
    m_trackItem = item;
    m_trackPoint = item->property("cursorRectangle").toRectF().center();
    m_arrowCarry = 0;
    m_tracking = true;
    emit trackPointChanged();
    emit trackingChanged();
    return true;
}

void KeyboardContext::trackMove(qreal dx, qreal dy)
{
    QObject *item = m_trackItem.data();
    if (!m_tracking || !item)
        return;
    // курсор чуть быстрее пальца (просьба пользователя: +10–14 %) — ровно, без разгона у краёв
    const qreal gain = 1.12;
    const qreal h = qMax(item->property("height").toReal(), item->property("contentHeight").toReal());
    m_trackPoint = QPointF(qBound<qreal>(0, m_trackPoint.x() + dx * gain, item->property("width").toReal()),
                           qBound<qreal>(0, m_trackPoint.y() + dy * gain, h));
    emit trackPointChanged();
    int pos = -1;
    if (QMetaObject::invokeMethod(item, "positionAt", Q_RETURN_ARG(int, pos), Q_ARG(qreal, m_trackPoint.x()),
                                  Q_ARG(qreal, m_trackPoint.y()))) {
        if (pos >= 0 && item->property("cursorPosition").toInt() != pos)
            item->setProperty("cursorPosition", pos);
        return;
    }
    m_arrowCarry += dx * gain;   // однострочное поле (TextInput): стрелками
    while (qAbs(m_arrowCarry) >= 9) {
        key(m_arrowCarry > 0 ? Qt::Key_Right : Qt::Key_Left);
        m_arrowCarry -= m_arrowCarry > 0 ? 9 : -9;
    }
}

void KeyboardContext::trackEnd()
{
    if (!m_tracking)
        return;
    m_tracking = false;
    m_trackItem = nullptr;
    emit trackingChanged();
}

QVariant KeyboardContext::query(Qt::InputMethodQuery q) const
{
    QObject *focus = QGuiApplication::focusObject();
    if (!focus)
        return {};
    QInputMethodQueryEvent e(q);
    QCoreApplication::sendEvent(focus, &e);
    return e.value(q);
}

void KeyboardContext::setVisible(bool visible)
{
    if (visible == m_visible)
        return;
    m_visible = visible;
    emitInputPanelVisibleChanged();
    emit visibleChanged();
}

QPlatformInputContext *KeyboardContextPlugin::create(const QString &key, const QStringList &)
{
    return key.compare(QLatin1String("naomi"), Qt::CaseInsensitive) == 0 ? new KeyboardContext : nullptr;
}

#include "keyboard_context.moc"
