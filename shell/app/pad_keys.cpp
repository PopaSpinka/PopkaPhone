#include "pad_keys.h"

#include "game_library.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QWaylandCompositor>
#include <QWaylandKeyboard>
#include <QWaylandQuickItem>
#include <QWaylandSeat>

#include <linux/input.h>

#include <cerrno>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcPadKeys, "naomi.games")

namespace {

// клавиши окна «Кнопки геймпада» (её клавиатура, PadKeyboard.qml) → коды клавиш (evdev) английской клавиатуры
const QHash<QString, int> &keyCodes()
{
    static const QHash<QString, int> codes = [] {
        QHash<QString, int> c{{QStringLiteral("Esc"), KEY_ESC}, {QStringLiteral("-"), KEY_MINUS}, {QStringLiteral("="), KEY_EQUAL},
            {QStringLiteral("Backspace"), KEY_BACKSPACE}, {QStringLiteral("Tab"), KEY_TAB}, {QStringLiteral("["), KEY_LEFTBRACE},
            {QStringLiteral("]"), KEY_RIGHTBRACE}, {QStringLiteral("\\"), KEY_BACKSLASH}, {QStringLiteral("Caps"), KEY_CAPSLOCK},
            {QStringLiteral(";"), KEY_SEMICOLON}, {QStringLiteral("'"), KEY_APOSTROPHE}, {QStringLiteral("Enter"), KEY_ENTER},
            {QStringLiteral("Shift"), KEY_LEFTSHIFT}, {QStringLiteral(","), KEY_COMMA}, {QStringLiteral("."), KEY_DOT},
            {QStringLiteral("/"), KEY_SLASH}, {QStringLiteral("RShift"), KEY_RIGHTSHIFT}, {QStringLiteral("Ctrl"), KEY_LEFTCTRL},
            {QStringLiteral("Alt"), KEY_LEFTALT}, {QStringLiteral("Space"), KEY_SPACE}, {QStringLiteral("RAlt"), KEY_RIGHTALT},
            {QStringLiteral("RCtrl"), KEY_RIGHTCTRL}, {QStringLiteral("Up"), KEY_UP}, {QStringLiteral("Down"), KEY_DOWN},
            {QStringLiteral("Left"), KEY_LEFT}, {QStringLiteral("Right"), KEY_RIGHT}};
        const char *digits = "1234567890";
        for (int i = 0; digits[i]; ++i)
            c.insert(QString(QLatin1Char(digits[i])), KEY_1 + i);
        const char *rows[] = {"QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
        const int starts[] = {KEY_Q, KEY_A, KEY_Z};
        for (int r = 0; r < 3; ++r)
            for (int i = 0; rows[r][i]; ++i)
                c.insert(QString(QLatin1Char(rows[r][i])), starts[r] + i);
        return c;
    }();
    return codes;
}

// кнопка геймпада (evdev) → кнопка раскладки. Как у naomi-game: по месту — правая A, нижняя B, верхняя X, левая Y
// (так подписан и 8BitDo в режиме Switch); ★ («снимок» у Switch) приходит кнопкой BTN_Z
QString controlOf(int code)
{
    switch (code) {
    case BTN_EAST: return QStringLiteral("a");
    case BTN_SOUTH: return QStringLiteral("b");
    case BTN_NORTH: return QStringLiteral("x");
    case BTN_WEST: return QStringLiteral("y");
    case BTN_TL: return QStringLiteral("l1");
    case BTN_TR: return QStringLiteral("r1");
    case BTN_TL2: return QStringLiteral("l2");
    case BTN_TR2: return QStringLiteral("r2");
    case BTN_SELECT: return QStringLiteral("minus");
    case BTN_START: return QStringLiteral("plus");
    case BTN_MODE: return QStringLiteral("home");
    case BTN_Z: return QStringLiteral("star");
    case BTN_THUMBL: return QStringLiteral("l3");
    case BTN_THUMBR: return QStringLiteral("r3");
    case BTN_DPAD_UP: return QStringLiteral("du");
    case BTN_DPAD_DOWN: return QStringLiteral("dd");
    case BTN_DPAD_LEFT: return QStringLiteral("dl");
    case BTN_DPAD_RIGHT: return QStringLiteral("dr");
    default: return {};
    }
}

// ось и куда отклонена → кнопка раскладки; курки-оси (ABS_Z, ABS_RZ — у геймпадов Xbox) — только «нажат»
QString axisControl(int code, int dir)
{
    const auto pick = [dir](const char *minus, const char *plus) { return QString::fromLatin1(dir < 0 ? minus : plus); };
    switch (code) {
    case ABS_X: return pick("ls_l", "ls_r");
    case ABS_Y: return pick("ls_u", "ls_d");
    case ABS_RX: return pick("rs_l", "rs_r");
    case ABS_RY: return pick("rs_u", "rs_d");
    case ABS_HAT0X: return pick("dl", "dr");
    case ABS_HAT0Y: return pick("du", "dd");
    case ABS_Z: return dir > 0 ? QStringLiteral("l2") : QString();
    case ABS_RZ: return dir > 0 ? QStringLiteral("r2") : QString();
    default: return {};
    }
}

constexpr int kAxes[] = {ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_HAT0X, ABS_HAT0Y, ABS_Z, ABS_RZ};

} // namespace

PadKeys::PadKeys(GameLibrary *games, QObject *parent)
    : QObject(parent)
    , m_games(games)
    , m_file(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/naomi/desktop/pads.json"))
{
    load();
    connect(m_games, &GameLibrary::gamepadsChanged, this, [this] {
        if (m_mode != Mode::Off)
            openPads();   // подключили ещё один, пока переводим
    });
}

PadKeys::~PadKeys()
{
    closePads();
}

void PadKeys::load()
{
    QFile f(m_file);
    if (f.open(QIODevice::ReadOnly))
        m_layouts = QJsonDocument::fromJson(f.readAll()).object();
}

void PadKeys::save() const
{
    QDir().mkpath(QFileInfo(m_file).absolutePath());
    QSaveFile f(m_file);
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(m_layouts).toJson(QJsonDocument::Indented));
    f.commit();
}

QVariantMap PadKeys::layout(const QString &game, const QString &system) const
{
    const QJsonObject o = m_layouts.value(game).toObject();
    const QString mode = o.value(QStringLiteral("mode")).toString();
    return {{QStringLiteral("mode"), !mode.isEmpty() ? mode : system == QLatin1String("win") ? QStringLiteral("keys")
                                                                                              : QStringLiteral("native")},
            {QStringLiteral("keys"), o.value(QStringLiteral("keys")).toObject().toVariantMap()}};
}

void PadKeys::setLayout(const QString &game, const QVariantMap &layout)
{
    if (game.isEmpty())
        return;
    m_layouts.insert(game, QJsonObject::fromVariantMap(layout));
    save();
    ++m_revision;
    emit layoutsChanged();
    if (game == m_game) {   // окно этой игры впереди — новая раскладка сразу
        releaseAll();
        setTarget(m_target, m_game, m_system);
    }
}

void PadKeys::setTarget(QObject *item, const QString &game, const QString &system)
{
    auto *target = qobject_cast<QWaylandQuickItem *>(item);
    if (target != m_target || game != m_game)
        releaseAll();   // клавиши, зажатые прошлому окну, — ему же и отпустить
    m_target = target;
    m_game = target ? game : QString();
    m_system = system;
    m_keys.clear();
    if (target) {
        const QVariantMap l = layout(game, system);
        if (l.value(QStringLiteral("mode")).toString() == QLatin1String("keys")) {
            const QVariantMap keys = l.value(QStringLiteral("keys")).toMap();
            for (auto it = keys.cbegin(); it != keys.cend(); ++it)
                if (const int code = keyCodes().value(it.value().toString()))
                    m_keys.insert(it.key(), code);
        }
    }
    update();
}

void PadKeys::setListening(bool on)
{
    if (on == m_listening)
        return;
    m_listening = on;
    update();
    emit listeningChanged();
}

void PadKeys::update()
{
    const Mode mode = m_listening ? Mode::Listen : m_target && !m_keys.isEmpty() ? Mode::Keys : Mode::Off;
    if (mode == m_mode)
        return;
    releaseAll();
    const bool was = active();
    m_mode = mode;
    if (mode == Mode::Off) {
        closePads();
        qCInfo(lcPadKeys, "кнопки геймпада: выкл — геймпад у игры");
    } else {
        openPads();
        if (mode == Mode::Keys)
            qCInfo(lcPadKeys, "кнопки геймпада: клавишами (%lld) — %s", qlonglong(m_keys.size()), qPrintable(m_game));
        else
            qCInfo(lcPadKeys, "кнопки геймпада: выбор кнопки в окне");
    }
    if (was != active())
        emit activeChanged();
}

// Геймпады — наши, пока переводим (или выбираем кнопку): EVIOCGRAB — события только нам, игре (naomi-game, Wine) — нет
void PadKeys::openPads()
{
    QSet<QString> open;
    for (const Pad &p : std::as_const(m_pads))
        open.insert(p.path);
    for (const QString &path : m_games->padNodes()) {
        if (open.contains(path))
            continue;
        const int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        if (ioctl(fd, EVIOCGRAB, 1) < 0)
            qCWarning(lcPadKeys, "кнопки геймпада: %s не забрать себе: %s", qPrintable(path), strerror(errno));
        Pad pad;
        pad.path = path;
        char name[128] = "?";
        ioctl(fd, EVIOCGNAME(sizeof name), name);
        pad.name = QString::fromUtf8(name);
        for (const int code : kAxes) {
            input_absinfo a{};
            if (ioctl(fd, EVIOCGABS(code), &a) == 0 && a.maximum > a.minimum) {
                pad.absMin.insert(code, a.minimum);
                pad.absMax.insert(code, a.maximum);
            }
        }
        pad.notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(pad.notifier, &QSocketNotifier::activated, this, [this, fd] { read(fd); });
        m_pads.insert(fd, pad);
        qCInfo(lcPadKeys, "кнопки геймпада: геймпад наш — %s (%s)", name, qPrintable(path));
    }
}

void PadKeys::closePads()
{
    for (auto it = m_pads.begin(); it != m_pads.end(); ++it) {
        delete it->notifier;
        ioctl(it.key(), EVIOCGRAB, 0);
        ::close(it.key());
    }
    m_pads.clear();
}

void PadKeys::read(int fd)
{
    input_event ev[64];
    for (;;) {
        const ssize_t n = ::read(fd, ev, sizeof ev);
        if (n < 0 && errno == EAGAIN)
            return;
        if (n <= 0) {   // отключили
            Pad pad = m_pads.take(fd);
            delete pad.notifier;
            ::close(fd);
            releaseAll();
            return;
        }
        for (size_t i = 0; i < size_t(n) / sizeof ev[0]; ++i) {
            const input_event &e = ev[i];
            if (e.type == EV_KEY && e.value != 2) {
                const QString c = controlOf(e.code);
                if (!c.isEmpty())
                    button(c, e.value);
                else if (e.value && !m_unknown.contains(e.code)) {
                    m_unknown.insert(e.code);
                    qCInfo(lcPadKeys, "кнопки геймпада: незнакомая кнопка %d (0x%x) у %s", e.code, e.code,
                           qPrintable(m_pads.value(fd).name));
                }
            } else if (e.type == EV_ABS) {
                auto it = m_pads.find(fd);
                if (it != m_pads.end())
                    axis(*it, e.code, e.value);
            } else if (e.type == EV_SYN && e.code == SYN_DROPPED) {
                releaseAll();   // ядро потеряло события: что зажато — неизвестно, отпустить всё
                for (Pad &p : m_pads)
                    p.dir.clear();
            }
        }
    }
}

void PadKeys::axis(Pad &pad, int code, int value)
{
    const int lo = pad.absMin.value(code), hi = pad.absMax.value(code);
    if (hi <= lo)
        return;
    const int was = pad.dir.value(code);
    int now = was;
    if (code == ABS_HAT0X || code == ABS_HAT0Y) {
        now = value < 0 ? -1 : value > 0 ? 1 : 0;
    } else if (code == ABS_Z || code == ABS_RZ) {   // курок: 0…1
        const double v = double(value - lo) / (hi - lo);
        now = v > 0.5 ? 1 : v < 0.33 ? 0 : was;
    } else {   // стик: −1…1; между третью и половиной — как было (не дребезжит)
        const double v = 2.0 * (value - lo) / (hi - lo) - 1.0;
        now = std::abs(v) > 0.5 ? (v < 0 ? -1 : 1) : std::abs(v) < 0.33 ? 0 : was;
    }
    if (now == was)
        return;
    pad.dir.insert(code, now);
    if (was)
        button(axisControl(code, was), false);
    if (now)
        button(axisControl(code, now), true);
}

void PadKeys::button(const QString &control, bool down)
{
    if (control.isEmpty())
        return;
    int &n = m_down[control];   // два геймпада жмут одно — отпущено, когда отпустили оба
    if (down ? n++ > 0 : n == 0 || --n > 0)
        return;
    if (m_mode == Mode::Listen) {
        if (down)
            emit pressed(control);
        return;
    }
    const int code = m_mode == Mode::Keys ? m_keys.value(control) : 0;
    if (!code)
        return;
    int &k = m_keyDown[code];   // стик влево и крестовина влево на одной клавише — отпущена, когда отпустили обе
    if (down ? k++ == 0 : k > 0 && --k == 0)
        sendKey(code, down);
}

void PadKeys::releaseAll()
{
    for (auto it = m_keyDown.cbegin(); it != m_keyDown.cend(); ++it)
        if (it.value() > 0)
            sendKey(it.key(), false);
    m_keyDown.clear();
    m_down.clear();
}

// Клавиша окну игры — клавиатурой Wayland, как клавиатура зрителя (StreamInput::keyToGame): код по месту, без раскладки
void PadKeys::sendKey(int code, bool down)
{
    QWaylandQuickItem *item = m_target;
    if (!item || !item->surface())
        return;
    QWaylandSeat *seat = item->compositor() ? item->compositor()->defaultSeat() : nullptr;
    QWaylandKeyboard *keyboard = seat ? seat->keyboard() : nullptr;
    if (!keyboard)
        return;
    if (seat->keyboardFocus() != item->surface())
        item->takeFocus(seat);
    down ? keyboard->sendKeyPressEvent(uint(code + 8)) : keyboard->sendKeyReleaseEvent(uint(code + 8));
}
