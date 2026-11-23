// НаомиФон, приставка на Pi — клавиатура, мышь и трекпад, подключённые к Pi (evdev, забраны себе: консоль их не видит).
// Мышь — сдвиг с мягким ускорением; клавиши — как у окна Mac (mirror/renderer.js): перевод в буквы и раскладка —
// KeyLayout (shell/stream/key_layout.h — тот же, что у клавиатуры самого телефона); стол показывает раскладку и может
// попросить сменить — setRussian. Огонёк CapsLock зажигает сам (onCaps). Клавиатура Apple — драйвер ядра hid-apple
// (верхний ряд, fn); F3–F5 без fn — F3–F5 (appleKey; docs/PI.md, «Клавиатура Apple»).
// Трекпад (касания многими пальцами: Magic Trackpad — драйвер ядра hid-magicmouse; docs/PI.md, «Трекпад»): из
// касаний курсор, щелчки, прокрутку и жесты делает Trackpad (trackpad.h) — свой на каждый трекпад; здесь — чтение
// пальцев из ядра и что делать со смахиванием и щипком (клавиши столу).
#pragma once

#include "key_layout.h"
#include "trackpad.h"

#include <linux/input.h>

#include <cstdint>
#include <functional>
#include <deque>
#include <map>
#include <string>
#include <utility>

class Input
{
    KeyLayout m_keys;   // раньше onKey / onLayout: они — его поля

public:
    Input();
    ~Input();
    void scan();                                   // новые клавиатуры и мыши (и пропавшие) — раз в 2 с хватает
    const std::map<int, std::string> &devices() const { return m_devices; }   // fd → имя
    void read(int fd);                             // fd готов
    int pointerMods() const;                       // Shift / Ctrl / Alt — для сообщения Pointer (16 / 32 / 64)
    bool russian() const { return m_keys.russian(); }
    void setRussian(bool ru) { m_keys.setRussian(ru); }   // просьба стола (щелчок по EN / RU на панели); onLayout не зовёт
    // настройки мыши со стола телефона (сообщение Mouse, docs/DESKTOP.md, «Настройки»): speed — множитель сдвига
    // (1 — как было до настроек), accel — ускорение: 0 нет (1:1), 1 мягкое (до 2,5 раза, как было), 2 сильное (до 4)
    void setPointer(double speed, int accel) { m_speed = speed, m_accel = accel; }
    // настройки трекпада (сообщение Pad, «Настройки» → «Трекпад»); смахивания — что делать: вбок — 0 — ничего, 1 —
    // «назад» и «вперёд» (Alt+← / Alt+→), 2 — вкладки (Ctrl+Shift+Tab / Ctrl+Tab); вверх и вниз — 0 — ничего, 1 — меню N
    // стола (onPadAction). show — слать пальцы столу (onPadTouch): открыт раздел «Трекпад»
    struct PadActions { int side = 1, up = 1, down = 0; };
    void setPad(const PadConfig &cfg, const PadActions &actions, bool show);
    void tick(int64_t now);                        // сроки трекпада (щелчок касания, удержание трёх пальцев) — часто

    std::function<void(double dx, double dy, int64_t t)> onMotion;   // пикселей экрана; t — метка события (мкс, CLOCK_MONOTONIC)
    std::function<void(int64_t t, int dx, int dy, double gain)> onRaw;   // отчёт мыши как есть и ускорение (для замеров)
    std::function<void(double fx, double fy)> onAbsolute;        // устройства с абсолютной позицией (планшет, сенсор,
                                                                 // проверочная мышь tools/pi-smooth): доля 0…1
    std::function<void(int buttons)> onButtons;                  // 1 — левая, 2 — правая, 4 — средняя
    std::function<void(double x, double y)> onWheel;             // щелчков колеса (вниз и вправо — плюс)
    // прокрутка трекпадом: ход двух пальцев в пикселях экрана (с ускорением; вниз и вправо — плюс), t — метка события;
    // fingers false — пальцы сняли (dx, dy — нули): прокрутке дальше идти по инерции
    std::function<void(double dx, double dy, int64_t t, bool fingers)> onScroll;
    std::function<double()> pointScale;   // пикселей монитора в точке стола: трекпад считает ход в точках стола
    std::function<void(int action)> onPadAction;                 // смахивание — действие стола (сообщение PadAction)
    std::function<void(const uint8_t *b, size_t n)> onPadTouch;  // пальцы трекпада — столу (сообщение PadTouch)
    std::function<void(bool down, uint32_t qtKey, uint32_t qtMods, const std::string &text)> &onKey = m_keys.onKey;
    std::function<void(bool russian)> &onLayout = m_keys.onLayout;

private:
    void key(int code, int value);
    void close(int fd);
    void sendButtons();                // кнопки мышей и трекпадов вместе — onButtons
    void tapKey(uint32_t qtKey, uint32_t mods);   // нажать и отпустить (смахивание, щипок)
    // трекпад
    struct Pad
    {
        double resX = 40, resY = 40;   // отсчётов на мм
        int minX = 0, minY = 0;
        int slot = 0;
        struct Finger { int id = -1, x = 0, y = 0, major = 0, minor = 0, pressure = 0; };
        Finger f[16];
        bool click = false;            // щелчок нажат
        int buttons = 0;               // что держит трекпад: 1 — левая, 2 — правая
        Trackpad engine;
        int64_t shownAt = 0;           // пальцы столу: когда и что слали (PadTouch)
        int shownMode = -1, shownCount = -1, shownHeld = -1;
    };
    void padEvent(int fd, const input_event &e);
    void padSetup(Pad &pad);
    void padShow(Pad &pad, int64_t now);
    std::map<int, Pad> m_pads;         // fd → трекпад
    PadConfig m_padCfg;
    PadActions m_padActions;
    bool m_padShow = false;
    std::map<int, std::string> m_devices;          // fd → имя
    std::map<std::string, int> m_paths;            // /dev/input/eventN → fd
    std::map<int, bool> m_hiRes;                   // у мыши колесо с точными шагами (REL_WHEEL_HI_RES)
    std::map<int, bool> m_keyboards;               // клавиатуры с буквами (огонёк CapsLock) → Apple (F3–F5 — appleKey)
    struct Abs { int minX, maxX, minY, maxY, x, y; bool moved; };
    std::map<int, Abs> m_abs;                      // устройства с ABS_X / ABS_Y
    int m_buttons = 0;
    double m_dx = 0, m_dy = 0, m_wx = 0, m_wy = 0;
    double m_speed = 1;
    int m_accel = 1;
    int64_t m_lastMotionUs = 0, m_eventUs = 0;
    std::deque<std::pair<int64_t, double>> m_recent;   // отчёты мыши за последние ~50 мс: время, длина сдвига
};
