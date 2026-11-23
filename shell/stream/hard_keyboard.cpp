#include "hard_keyboard.h"

#include <QDir>
#include <QFileSystemWatcher>
#include <QLoggingCategory>
#include <QSocketNotifier>
#include <QTimer>

#include <linux/input.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcKeyboard, "naomi.keyboard")

static_assert(KeyLayout::kMediaToggle == Qt::Key_MediaTogglePlayPause && KeyLayout::kMediaNext == Qt::Key_MediaNext
              && KeyLayout::kMediaPrevious == Qt::Key_MediaPrevious && KeyLayout::kScreenLock == Qt::Key_ScreenSaver
              && KeyLayout::kSleep == Qt::Key_Sleep && KeyLayout::kBrightnessUp == Qt::Key_MonBrightnessUp
              && KeyLayout::kBrightnessDown == Qt::Key_MonBrightnessDown && KeyLayout::kVolumeUp == Qt::Key_VolumeUp && KeyLayout::kF1 == Qt::Key_F1);

namespace {

bool has(int fd, int code)
{
    unsigned long bits[(KEY_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), bits) < 0)
        return false;
    return bits[code / (8 * sizeof(long))] >> (code % (8 * sizeof(long))) & 1;
}

} // namespace

HardKeyboard::HardKeyboard(QObject *parent)
    : QObject(parent)
{
    m_keys.onKey = [this](bool down, uint32_t qtKey, uint32_t mods, const std::string &text) {
        emit key(down, int(qtKey), int(mods), QString::fromStdString(text));
    };
    m_keys.onLayout = [this](bool ru) { emit layoutToggled(ru); };
    m_keys.onCaps = [this](bool on) { setCapsLed(on); };
    auto *watcher = new QFileSystemWatcher({QStringLiteral("/dev/input")}, this);
    connect(watcher, &QFileSystemWatcher::directoryChanged, this, &HardKeyboard::scan);
    scan();
}

HardKeyboard::~HardKeyboard()
{
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it)
        ::close(it.key());
}

void HardKeyboard::scan()
{
    const bool was = present();
    QStringList open;
    for (const Device &d : std::as_const(m_devices))
        open << d.path;
    const QStringList names = QDir(QStringLiteral("/dev/input")).entryList({QStringLiteral("event*")}, QDir::System);
    for (const QString &n : names) {
        const QString path = QStringLiteral("/dev/input/") + n;
        if (open.contains(path))
            continue;
        // на запись — ради огонька CapsLock; узел только что появился — права udev могли ещё не встать, а смена прав
        // каталог не будит: при отказе — ещё раз через полсекунды, до шести раз на узел
        const int fd = ::open(path.toLocal8Bit().constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            if (errno == EACCES && ++m_denied[path] <= 6)
                QTimer::singleShot(500, this, &HardKeyboard::scan);
            continue;
        }
        m_denied.remove(path);
        // клавиатура — с буквами: кнопки самого телефона (питание, громкость) и пульт наушников — не она
        if (!has(fd, KEY_A) || !has(fd, KEY_SPACE)) {
            ::close(fd);
            continue;
        }
        char name[128] = "?";
        ioctl(fd, EVIOCGNAME(sizeof name), name);
        ioctl(fd, EVIOCGRAB, 1);
        // Apple (0x004C по Bluetooth, 0x05AC по USB) без клавиш громкости в описании — драйвер hid-apple её не ведёт
        input_id id{};
        ioctl(fd, EVIOCGID, &id);
        const bool apple = (id.vendor == 0x004c || id.vendor == 0x05ac) && !has(fd, KEY_VOLUMEUP);
        auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, [this, fd] { read(fd); });
        m_devices.insert(fd, {path, QString::fromUtf8(name), notifier, apple});
        qCInfo(lcKeyboard, "клавиатура: %s (%s)%s", name, qPrintable(path), apple ? ", верхний ряд Apple — сами" : "");
    }
    for (auto it = m_denied.begin(); it != m_denied.end();)   // узел пропал — счёт отказов заново
        it = names.contains(it.key().section(QLatin1Char('/'), -1)) ? std::next(it) : m_denied.erase(it);
    if (present() != was) {
        if (present())
            setCapsLed(m_keys.caps());
        emit presentChanged();
    }
}

void HardKeyboard::read(int fd)
{
    input_event ev[64];
    for (;;) {
        const ssize_t n = ::read(fd, ev, sizeof ev);
        if (n < 0 && errno == EAGAIN)
            return;
        if (n <= 0) {
            drop(fd);   // отключили, уснула
            return;
        }
        const bool apple = m_devices.value(fd).apple;
        for (size_t i = 0; i < size_t(n) / sizeof ev[0]; ++i) {
            if (ev[i].type != EV_KEY)
                continue;
            qCDebug(lcKeyboard, "клавиша %d: %d", ev[i].code, ev[i].value);
            if (const int code = apple ? appleKey(ev[i].code, ev[i].value) : ev[i].code)
                m_keys.key(code, ev[i].value);
        }
    }
}

// Клавиатура Apple, как у Mac: верхний ряд без fn — яркость (F1, F2), «луна» (F6), пуск / пауза, «назад», «вперёд»,
// громкость; с fn — они же F-клавишами (F3–F5 — всегда F3–F5: им у стола дела нет); fn со стрелками — Home, End, PgUp, PgDn, fn+⌫ — Delete.
// Возвращает код для перевода (0 — клавиша дальше не идёт: сам fn).
int HardKeyboard::appleKey(int code, int value)
{
    if (code == KEY_BRIGHTNESSDOWN) {
        m_fn = value != 0;
        return 0;
    }
    if (value == 0 || m_appleDown.contains(code))   // отпускание и повтор — тем, чем нажали
        return value == 0 ? m_appleDown.take(code) : m_appleDown.value(code);
    static const struct { int code, plain, fn; } kMap[] = {
        {KEY_F1, KEY_BRIGHTNESSDOWN, KEY_F1}, {KEY_F2, KEY_BRIGHTNESSUP, KEY_F2},   // яркость монитора приставки
        {KEY_F6, KEY_SLEEP, KEY_F6},          // «луна»: уйти со стола с приглушением монитора
        {KEY_F7, KEY_PREVIOUSSONG, KEY_F7},   {KEY_F8, KEY_PLAYPAUSE, KEY_F8},     {KEY_F9, KEY_NEXTSONG, KEY_F9},
        {KEY_F10, KEY_MUTE, KEY_F10},         {KEY_F11, KEY_VOLUMEDOWN, KEY_F11},  {KEY_F12, KEY_VOLUMEUP, KEY_F12},
        {KEY_LEFT, KEY_LEFT, KEY_HOME},       {KEY_RIGHT, KEY_RIGHT, KEY_END},     {KEY_UP, KEY_UP, KEY_PAGEUP},
        {KEY_DOWN, KEY_DOWN, KEY_PAGEDOWN},   {KEY_BACKSPACE, KEY_BACKSPACE, KEY_DELETE},
    };
    int out = code;
    for (const auto &m : kMap)
        if (m.code == code)
            out = m_fn ? m.fn : m.plain;
    m_appleDown.insert(code, out);
    return out;
}

void HardKeyboard::drop(int fd)
{
    const Device d = m_devices.take(fd);
    qCInfo(lcKeyboard, "клавиатура отключена: %s", qPrintable(d.name));
    d.notifier->setEnabled(false);
    d.notifier->deleteLater();
    ::close(fd);
    m_keys.releaseModifiers();
    m_fn = false;
    m_appleDown.clear();
    if (!present())
        emit presentChanged();
}

void HardKeyboard::setCapsLed(bool on)
{
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        const input_event ev[2] = {{{}, EV_LED, LED_CAPSL, on}, {{}, EV_SYN, SYN_REPORT, 0}};
        if (::write(it.key(), ev, sizeof ev) < 0)
            qCDebug(lcKeyboard, "огонёк CapsLock: %s", strerror(errno));
    }
}
