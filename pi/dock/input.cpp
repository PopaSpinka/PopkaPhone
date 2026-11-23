#include "input.h"

#include "link.h"

#include <linux/input.h>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

bool has(int fd, int type, int code, int max)
{
    unsigned long bits[(KEY_MAX + 8 * sizeof(long)) / (8 * sizeof(long))] = {};
    if (ioctl(fd, EVIOCGBIT(type, max), bits) < 0)
        return false;
    return bits[code / (8 * sizeof(long))] >> (code % (8 * sizeof(long))) & 1;
}

double smoothstep(double a, double b, double x)
{
    const double t = std::clamp((x - a) / (b - a), 0.0, 1.0);
    return t * t * (3 - 2 * t);
}

// Клавиатура Apple (Magic Keyboard 2021, 004C:029C) у Pi — драйвер ядра hid-apple: верхний ряд без fn — свои клавиши
// (яркость, «луна», плеер, громкость: их KeyLayout знает), с fn — F1–F12, fn со стрелками — Home, End, PgUp, PgDn
// (таблица из hid-apple.ko ядра 6.18, docs/PI.md, «Клавиатура Apple»). F3–F5 без fn — Mission Control, Spotlight и
// диктовка (KEY_SCALE, KEY_SEARCH, KEY_MICMUTE): столу ни к чему — F3–F5, как у клавиатуры телефона (F5 — обновить страницу)
int appleKey(int code)
{
    switch (code) {
    case KEY_SCALE: return KEY_F3;
    case KEY_SEARCH: return KEY_F4;
    case KEY_MICMUTE: return KEY_F5;
    default: return code;
    }
}

void capsLed(int fd, bool on)
{
    const input_event ev[2] = {{{}, EV_LED, LED_CAPSL, on}, {{}, EV_SYN, SYN_REPORT, 0}};
    if (::write(fd, ev, sizeof ev) < 0)
        fprintf(stderr, "огонёк CapsLock: %s\n", strerror(errno));
}

} // namespace

Input::Input()
{
    // огонёк CapsLock — клавиатурам: ядро его само не зажжёт (клавиатуры забраны себе — EVIOCGRAB)
    m_keys.onCaps = [this](bool on) {
        for (const auto &[fd, apple] : m_keyboards)
            capsLed(fd, on);
    };
}

Input::~Input()
{
    for (auto &[fd, name] : m_devices)
        ::close(fd);
}

void Input::scan()
{
    for (int i = 0; i < 64; ++i) {
        const std::string path = "/dev/input/event" + std::to_string(i);
        if (m_paths.count(path) || access(path.c_str(), F_OK))
            continue;
        int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);   // на запись — ради огонька CapsLock
        if (fd < 0)
            fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        // клавиатура — с буквами; или клавиши громкости (у клавиатур они бывают отдельным устройством «Consumer
        // Control» — без букв)
        const bool keyboard = (has(fd, EV_KEY, KEY_A, KEY_MAX) && has(fd, EV_KEY, KEY_SPACE, KEY_MAX))
                              || has(fd, EV_KEY, KEY_VOLUMEUP, KEY_MAX);
        const bool mouse = has(fd, EV_REL, REL_X, REL_MAX) && has(fd, EV_REL, REL_Y, REL_MAX)
                           && has(fd, EV_KEY, BTN_LEFT, KEY_MAX);
        // трекпад: касания многими пальцами и кнопка (сенсорные экраны кнопки не имеют)
        const bool pad = has(fd, EV_ABS, ABS_MT_POSITION_X, ABS_MAX) && has(fd, EV_ABS, ABS_MT_SLOT, ABS_MAX)
                         && has(fd, EV_KEY, BTN_TOOL_FINGER, KEY_MAX) && has(fd, EV_KEY, BTN_LEFT, KEY_MAX);
        if (!keyboard && !mouse && !pad) {
            ::close(fd);
            m_paths[path] = -1;   // не наше — больше не смотреть (пока номер не освободится)
            continue;
        }
        char name[128] = "?";
        ioctl(fd, EVIOCGNAME(sizeof name), name);
        ioctl(fd, EVIOCGRAB, 1);   // себе: консоль (getty) клавиш не получит
        int clock = CLOCK_MONOTONIC;   // метки событий — в часах приёмника: по ним досчёт движения
        ioctl(fd, EVIOCSCLOCKID, &clock);
        m_paths[path] = fd;
        m_devices[fd] = name;
        m_hiRes[fd] = has(fd, EV_REL, REL_WHEEL_HI_RES, REL_MAX);
        if (pad) {
            input_absinfo ax{}, ay{};
            ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &ax);
            ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &ay);
            Pad &p = m_pads[fd];
            p.resX = ax.resolution > 0 ? ax.resolution : 40;
            p.resY = ay.resolution > 0 ? ay.resolution : 40;
            p.minX = ax.minimum, p.minY = ay.minimum;
            p.engine.width = (ax.maximum - ax.minimum) / p.resX;
            p.engine.height = (ay.maximum - ay.minimum) / p.resY;
            padSetup(p);
            fprintf(stderr, "ввод: %s — трекпад %.0f × %.0f мм\n", name, p.engine.width, p.engine.height);
            continue;
        }
        if (has(fd, EV_ABS, ABS_X, ABS_MAX) && has(fd, EV_ABS, ABS_Y, ABS_MAX)) {
            input_absinfo ax{}, ay{};
            ioctl(fd, EVIOCGABS(ABS_X), &ax);
            ioctl(fd, EVIOCGABS(ABS_Y), &ay);
            if (ax.maximum > ax.minimum && ay.maximum > ay.minimum)
                m_abs[fd] = Abs{ax.minimum, ax.maximum, ay.minimum, ay.maximum, ax.value, ay.value, false};
        }
        bool apple = false;
        if (has(fd, EV_KEY, KEY_A, KEY_MAX)) {   // с буквами (не «Consumer Control»): огонёк CapsLock — как сейчас
            input_id id{};
            ioctl(fd, EVIOCGID, &id);
            apple = (id.vendor == 0x004c || id.vendor == 0x05ac) && has(fd, EV_KEY, KEY_FN, KEY_MAX);   // ведёт hid-apple
            m_keyboards[fd] = apple;
            capsLed(fd, m_keys.caps());
        }
        fprintf(stderr, "ввод: %s — %s%s%s\n", name, keyboard ? "клавиатура" : "", apple ? " Apple" : "",
                mouse ? (keyboard ? " и мышь" : "мышь") : "");
    }
    // освободившиеся номера — смотреть снова
    for (auto it = m_paths.begin(); it != m_paths.end();)
        it = it->second < 0 && access(it->first.c_str(), F_OK) ? m_paths.erase(it) : std::next(it);
}

void Input::close(int fd)
{
    fprintf(stderr, "ввод: %s отключено\n", m_devices[fd].c_str());
    ::close(fd);
    m_devices.erase(fd);
    m_hiRes.erase(fd);
    m_abs.erase(fd);
    m_keyboards.erase(fd);
    if (auto it = m_pads.find(fd); it != m_pads.end()) {   // уснул посреди прокрутки или щелчка — отпустить
        it->second.engine.drop(m_eventUs);
        m_pads.erase(it);
        sendButtons();
    }
    for (auto it = m_paths.begin(); it != m_paths.end(); ++it)
        if (it->second == fd) {
            m_paths.erase(it);
            break;
        }
}

int Input::pointerMods() const
{
    return (m_keys.shift() ? 16 : 0) | (m_keys.ctrl() ? 32 : 0) | (m_keys.alt() ? 64 : 0);
}

void Input::read(int fd)
{
    input_event ev[64];
    for (;;) {
        const ssize_t n = ::read(fd, ev, sizeof ev);
        if (n < 0 && errno == EAGAIN)
            return;
        if (n <= 0) {
            close(fd);   // выдернули
            return;
        }
        const auto pad = m_pads.find(fd);
        const auto kb = m_keyboards.find(fd);
        const bool apple = kb != m_keyboards.end() && kb->second;
        for (size_t i = 0; i < size_t(n) / sizeof ev[0]; ++i) {
            const input_event &e = ev[i];
            m_eventUs = int64_t(e.input_event_sec) * 1000000 + e.input_event_usec;
            if (pad != m_pads.end()) {
                padEvent(fd, e);
                continue;
            }
            if (e.type == EV_KEY) {
                key(apple ? appleKey(e.code) : e.code, e.value);
            } else if (e.type == EV_REL) {
                const bool hi = m_hiRes[fd];
                if (e.code == REL_X)
                    m_dx += e.value;
                else if (e.code == REL_Y)
                    m_dy += e.value;
                else if (e.code == REL_WHEEL_HI_RES || (e.code == REL_WHEEL && !hi))
                    m_wy -= e.value * (e.code == REL_WHEEL ? 1.0 : 1 / 120.0);   // от себя — вверх
                else if (e.code == REL_HWHEEL_HI_RES || (e.code == REL_HWHEEL && !hi))
                    m_wx += e.value * (e.code == REL_HWHEEL ? 1.0 : 1 / 120.0);
            } else if (e.type == EV_ABS && m_abs.count(fd)) {
                Abs &a = m_abs[fd];
                if (e.code == ABS_X)
                    a.x = e.value, a.moved = true;
                else if (e.code == ABS_Y)
                    a.y = e.value, a.moved = true;
            } else if (e.type == EV_SYN && e.code == SYN_REPORT) {
                if (auto it = m_abs.find(fd); it != m_abs.end() && it->second.moved) {
                    Abs &a = it->second;
                    a.moved = false;
                    if (onAbsolute)
                        onAbsolute(double(a.x - a.minX) / (a.maxX - a.minX), double(a.y - a.minY) / (a.maxY - a.minY));
                }
                if (m_dx || m_dy) {
                    // ускорение: медленно — точно (1:1), быстро — до 2,5 раза дальше (мягкое; сильное — до 4, без —
                    // 1:1; поверх — скорость из настроек стола). Скорость руки — за ~50 мс (несколько отчётов), а не
                    // по одному: у живой мыши отсчётов в отчёте то 7, то 12, и ускорение по отчёту скакало (на 0,3 и
                    // больше у 5 % соседних) — курсор то разгонялся, то тормозил
                    const int64_t now = m_eventUs;
                    m_lastMotionUs = now;
                    m_recent.push_back({now, std::hypot(m_dx, m_dy)});
                    double sum = 0;
                    int64_t from = now - 16000;   // после паузы — как за один шаг 62,5 Гц
                    for (auto it = m_recent.rbegin(); it != m_recent.rend(); ++it) {
                        if (now - it->first >= 50000) {
                            from = it->first;
                            break;
                        }
                        sum += it->second;
                    }
                    while (m_recent.size() > 1 && now - m_recent.front().first > 120000)
                        m_recent.pop_front();
                    const double speed = sum / std::clamp((now - from) / 1000.0, 1.0, 100.0);   // отсчётов в мс
                    const double gain = (m_accel == 0 ? 1.0 : 1 + (m_accel == 2 ? 3.0 : 1.5) * smoothstep(0.3, 3.0, speed))
                                        * m_speed;
                    if (onRaw)
                        onRaw(now, int(m_dx), int(m_dy), gain);
                    if (onMotion)
                        onMotion(m_dx * gain, m_dy * gain, now);
                    m_dx = m_dy = 0;
                }
                if (m_wx || m_wy) {
                    if (onWheel)
                        onWheel(m_wx, m_wy);
                    m_wx = m_wy = 0;
                }
            }
        }
    }
}

void Input::key(int code, int value)
{
    const bool down = value != 0;   // 2 — повтор: ещё одно нажатие
    // кнопки мыши
    const int bit = code == BTN_LEFT ? 1 : code == BTN_RIGHT ? 2 : code == BTN_MIDDLE ? 4 : 0;
    if (bit) {
        if (value == 2)
            return;
        m_buttons = down ? m_buttons | bit : m_buttons & ~bit;
        sendButtons();
        return;
    }
    if (code == BTN_SIDE || code == BTN_EXTRA) {   // боковые — «назад» / «вперёд» (Alt+← / Alt+→)
        if (value != 2 && onKey)
            onKey(down, code == BTN_SIDE ? KeyLayout::kLeft : KeyLayout::kRight, KeyLayout::kAlt, "");
        return;
    }
    // печатают (буквы, цифры, знаки, пробел, ⌫, ⏎ — не Shift, Ctrl, Alt, Win и не F-клавиши) — трекпадам пауза
    if (down && ((code >= KEY_1 && code <= KEY_SLASH && code != KEY_LEFTCTRL && code != KEY_LEFTSHIFT) || code == KEY_SPACE))
        for (auto &[fd, p] : m_pads)
            p.engine.typed(m_eventUs);
    m_keys.key(code, value);
}

void Input::sendButtons()
{
    int b = m_buttons;
    for (const auto &[fd, p] : m_pads)
        b |= p.buttons;
    if (onButtons)
        onButtons(b);
}

void Input::tapKey(uint32_t qtKey, uint32_t mods)
{
    if (!onKey)
        return;
    onKey(true, qtKey, mods, "");
    onKey(false, qtKey, mods, "");
}

void Input::setPad(const PadConfig &cfg, const PadActions &actions, bool show)
{
    m_padCfg = cfg;
    m_padActions = actions;
    m_padShow = show;
    for (auto &[fd, p] : m_pads) {
        p.engine.cfg = cfg;
        p.shownMode = -1;   // раздел открыли — что на трекпаде сейчас, сразу
    }
}

void Input::tick(int64_t now)
{
    for (auto &[fd, p] : m_pads) {
        p.engine.tick(now);
        padShow(p, now);    // срок вышел без отчёта трекпада (кнопку отпустили) — столу
    }
}

// Пальцы трекпада — столу, пока открыт раздел «Трекпад» (PadTouch): не чаще 30 в секунду; число пальцев, кнопки или
// «что распознано» сменились — сразу
void Input::padShow(Pad &pad, int64_t now)
{
    if (!m_padShow || !onPadTouch)
        return;
    const auto seen = pad.engine.seen();
    const int mode = int(pad.engine.mode()), held = pad.engine.held(), count = int(seen.size());
    const bool changed = mode != pad.shownMode || count != pad.shownCount || held != pad.shownHeld;
    if (!changed && (count == 0 || now - pad.shownAt < 33000))
        return;
    pad.shownAt = now, pad.shownMode = mode, pad.shownCount = count, pad.shownHeld = held;
    std::vector<uint8_t> b{uint8_t(mode), uint8_t(held), uint8_t(count)};
    for (const auto &f : seen) {
        b.push_back(uint8_t(std::clamp(f.x / pad.engine.width, 0.0, 1.0) * 255));
        b.push_back(uint8_t(std::clamp(f.y / pad.engine.height, 0.0, 1.0) * 255));
        b.push_back(f.counted ? 0 : 1);
    }
    onPadTouch(b.data(), b.size());
}

// --- трекпад ---

// Что трекпад решил — столу: ход и прокрутка у него в точках стола, монитору — в пикселях
void Input::padSetup(Pad &pad)
{
    Trackpad &e = pad.engine;
    e.cfg = m_padCfg;
    e.onMove = [this](double dx, double dy, int64_t t) {
        const double k = pointScale ? pointScale() : 1;
        if (onMotion)
            onMotion(dx * k, dy * k, t);
    };
    e.onScroll = [this](double dx, double dy, int64_t t, bool fingers) {
        const double k = pointScale ? pointScale() : 1;
        if (onScroll)
            onScroll(dx * k, dy * k, t, fingers);
    };
    e.onButtons = [this, &pad](int b) {
        pad.buttons = b;
        sendButtons();
    };
    // смахивание — как у Mac: страница и вкладки идут за пальцами (пальцы вправо — «назад», предыдущая вкладка)
    e.onSwipe = [this](Trackpad::Swipe dir, int) {
        const bool side = dir == Trackpad::Left || dir == Trackpad::Right, back = dir == Trackpad::Right;
        const int action = side ? m_padActions.side : dir == Trackpad::Up ? m_padActions.up : m_padActions.down;
        if (side && action == 1)
            tapKey(back ? KeyLayout::kLeft : KeyLayout::kRight, KeyLayout::kAlt);
        else if (side && action == 2)
            back ? tapKey(0x01000002, KeyLayout::kCtrl | KeyLayout::kShift) : tapKey(KeyLayout::kTab, KeyLayout::kCtrl);   // Qt::Key_Backtab
        else if (!side && action == 1 && onPadAction)
            onPadAction(1);   // меню N стола
    };
    // щипок — масштаб страницы: Ctrl+«=» и Ctrl+«−», как с клавиатуры
    e.onZoom = [this](int steps) { tapKey(steps > 0 ? '=' : '-', KeyLayout::kCtrl); };
}

void Input::padEvent(int fd, const input_event &e)
{
    Pad &p = m_pads[fd];
    if (e.type == EV_ABS) {
        Pad::Finger &f = p.f[p.slot];
        if (e.code == ABS_MT_SLOT)
            p.slot = std::clamp(e.value, 0, 15);
        else if (e.code == ABS_MT_TRACKING_ID)
            f.id = e.value;
        else if (e.code == ABS_MT_POSITION_X)
            f.x = e.value;
        else if (e.code == ABS_MT_POSITION_Y)
            f.y = e.value;
        else if (e.code == ABS_MT_TOUCH_MAJOR)
            f.major = e.value;
        else if (e.code == ABS_MT_TOUCH_MINOR)
            f.minor = e.value;
        else if (e.code == ABS_MT_PRESSURE)
            f.pressure = e.value;
    } else if (e.type == EV_KEY && e.code == BTN_LEFT && e.value != 2) {
        p.click = e.value;   // щелчок (трекпад нажали); пальцы этого отчёта ядро шлёт раньше кнопки — их уже видно
    } else if (e.type == EV_SYN && e.code == SYN_REPORT) {
        std::vector<Trackpad::Touch> touches;
        for (const Pad::Finger &f : p.f)
            if (f.id >= 0)
                touches.push_back({f.id, (f.x - p.minX) / p.resX, (f.y - p.minY) / p.resY, f.major, f.minor, f.pressure});
        p.engine.frame(m_eventUs, touches, p.click);
        padShow(p, m_eventUs);
    }
}
