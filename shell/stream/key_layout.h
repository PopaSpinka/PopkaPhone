// НаомиФон — клавиши настоящей клавиатуры (коды evdev) в то, что получает стол: буквы — текстом по раскладке (EN / RU
// ЙЦУКЕН, как у Windows: ё — на `~`, № — Shift+3), сочетания с Ctrl, Alt, Win — клавишей Qt по латинской букве на
// клавише при любой раскладке, служебные — клавишами Qt. Раскладка — ⌘+Пробел (Win+Пробел; как у Mac) или Alt+Shift
// (когда пару отпускают, ничего не нажав между: иначе Shift+Alt+3 — скриншот стола — менял бы её); CapsLock —
// заглавные. Пуск / пауза, «назад», «вперёд», клавиша блокировки и «луна» — клавишами Qt: плееру и выключателю стола. Один код на две клавиатуры: приставки (pi/dock/input.cpp, docs/PI.md, «Клавиатура») и самого телефона
// (hard_keyboard.cpp, docs/DESKTOP.md, «Клавиатура Bluetooth»). Без Qt: приставка собирается без него.
#pragma once

#include <linux/input-event-codes.h>

#include <cstdint>
#include <functional>
#include <string>

class KeyLayout
{
public:
    // Qt::Key и модификаторы Qt (как у окна Mac)
    static constexpr uint32_t kShift = 0x02000000, kCtrl = 0x04000000, kAlt = 0x08000000, kMeta = 0x10000000;
    static constexpr uint32_t kEsc = 0x01000000, kTab = 0x01000001, kBackspace = 0x01000003, kReturn = 0x01000004,
                              kInsert = 0x01000006, kDelete = 0x01000007, kHome = 0x01000010, kEnd = 0x01000011,
                              kLeft = 0x01000012, kUp = 0x01000013, kRight = 0x01000014, kDown = 0x01000015,
                              kPageUp = 0x01000016, kPageDown = 0x01000017, kF1 = 0x01000030, kMenu = 0x01000055,
                              kVolumeDown = 0x01000070, kVolumeMute = 0x01000071, kVolumeUp = 0x01000072,
                              kMediaPrevious = 0x01000082, kMediaNext = 0x01000083, kMediaToggle = 0x01000086,
                              kScreenLock = 0x010000ba,   // Qt::Key_ScreenSaver
                              kSleep = 0x01020004,        // Qt::Key_Sleep
                              kBrightnessUp = 0x010000b2, kBrightnessDown = 0x010000b3;   // Qt::Key_MonBrightness…

    std::function<void(bool down, uint32_t qtKey, uint32_t qtMods, const std::string &text)> onKey;
    std::function<void(bool russian)> onLayout;   // сменили сочетанием
    std::function<void(bool caps)> onCaps;        // CapsLock: огонёк на клавиатуре

    bool russian() const { return m_ru; }
    void setRussian(bool ru) { m_ru = ru; }       // со стороны (щелчок по EN / RU на панели стола); onLayout не зовёт
    bool shift() const { return m_shift[0] || m_shift[1]; }
    bool ctrl() const { return m_ctrl[0] || m_ctrl[1]; }
    bool alt() const { return m_alt[0] || m_alt[1]; }
    bool caps() const { return m_caps; }
    // клавиатуру отключили с зажатыми клавишами: их отпускания уже не придут
    void releaseModifiers()
    {
        m_shift[0] = m_shift[1] = m_ctrl[0] = m_ctrl[1] = m_alt[0] = m_alt[1] = m_meta[0] = m_meta[1] = false;
        m_layoutPair = m_otherSinceMod = false;
    }

    // событие EV_KEY: value 1 — нажата, 0 — отпущена, 2 — повтор (ещё одно нажатие)
    void key(int code, int value)
    {
        const bool down = value != 0;
        auto mod = [&](bool *side, int left, int right) {
            if (code != left && code != right)
                return false;
            side[code == right] = down;
            return true;
        };
        const bool shiftHeld = shift(), altHeld = alt();
        if (mod(m_shift, KEY_LEFTSHIFT, KEY_RIGHTSHIFT) || mod(m_alt, KEY_LEFTALT, KEY_RIGHTALT)) {
            // Alt+Shift — раскладка: второй из пары нажат, пока держат первый, и переключается, когда пару отпускают,
            // если между ними ничего не нажимали (как у Windows). Не в миг нажатия — иначе сочетания
            // Shift+Alt+клавиша (скриншот стола Shift+Alt+3) меняли бы раскладку
            const bool shiftKey = code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT;
            if (value == 1 && (shiftKey ? altHeld : shiftHeld) && !m_otherSinceMod)
                m_layoutPair = true;
            if (value == 0 && m_layoutPair) {
                m_layoutPair = false;
                if (!m_otherSinceMod)
                    toggle();
            }
            if (value == 1 && !shiftHeld && !altHeld)
                m_otherSinceMod = false;
            return;
        }
        if (mod(m_ctrl, KEY_LEFTCTRL, KEY_RIGHTCTRL) || mod(m_meta, KEY_LEFTMETA, KEY_RIGHTMETA))
            return;
        if (code == KEY_CAPSLOCK) {
            if (value == 1) {
                m_caps = !m_caps;
                if (onCaps)
                    onCaps(m_caps);
            }
            return;
        }
        if (down)
            m_otherSinceMod = true;
        const bool meta = m_meta[0] || m_meta[1];
        const uint32_t mods = (shift() ? kShift : 0) | (ctrl() ? kCtrl : 0) | (alt() ? kAlt : 0) | (meta ? kMeta : 0);
        // ⌘+Пробел (Win+Пробел) — раскладка; сам пробел столу не уходит
        if (code == KEY_SPACE && meta && !ctrl() && !alt() && !shift()) {
            if (value == 1)
                toggle();
            return;
        }
        if (!onKey)
            return;
        if (const uint32_t special = specialKey(code)) {
            onKey(down, special, mods, special == kReturn ? "\r" : special == kTab ? "\t" : "");
            return;
        }
        const Chars *c = charsOf(code);
        if (!c)
            return;
        if (ctrl() || alt() || meta) {   // сочетание: клавиша Qt по латинице на клавише, при любой раскладке
            const unsigned char latin = static_cast<unsigned char>(c->en[0]);
            onKey(down, latin >= 'a' && latin <= 'z' ? latin - 'a' + 'A' : latin, mods, "");
            return;
        }
        const char *base = m_ru ? c->ru : c->en;
        const bool upper = shift() != (m_caps && isLetter(base));
        onKey(down, 0, 0, m_ru ? (upper ? c->ruShift : c->ru) : (upper ? c->enShift : c->en));
    }

private:
    // клавиша → текст: EN, EN с Shift, RU, RU с Shift
    struct Chars { int code; const char *en, *enShift, *ru, *ruShift; };

    static const Chars *charsOf(int code)
    {
        static const Chars kChars[] = {
            {KEY_GRAVE, "`", "~", "ё", "Ё"},      {KEY_1, "1", "!", "1", "!"},      {KEY_2, "2", "@", "2", "\""},
            {KEY_3, "3", "#", "3", "№"},          {KEY_4, "4", "$", "4", ";"},      {KEY_5, "5", "%", "5", "%"},
            {KEY_6, "6", "^", "6", ":"},          {KEY_7, "7", "&", "7", "?"},      {KEY_8, "8", "*", "8", "*"},
            {KEY_9, "9", "(", "9", "("},          {KEY_0, "0", ")", "0", ")"},      {KEY_MINUS, "-", "_", "-", "_"},
            {KEY_EQUAL, "=", "+", "=", "+"},      {KEY_Q, "q", "Q", "й", "Й"},      {KEY_W, "w", "W", "ц", "Ц"},
            {KEY_E, "e", "E", "у", "У"},          {KEY_R, "r", "R", "к", "К"},      {KEY_T, "t", "T", "е", "Е"},
            {KEY_Y, "y", "Y", "н", "Н"},          {KEY_U, "u", "U", "г", "Г"},      {KEY_I, "i", "I", "ш", "Ш"},
            {KEY_O, "o", "O", "щ", "Щ"},          {KEY_P, "p", "P", "з", "З"},      {KEY_LEFTBRACE, "[", "{", "х", "Х"},
            {KEY_RIGHTBRACE, "]", "}", "ъ", "Ъ"}, {KEY_BACKSLASH, "\\", "|", "\\", "/"}, {KEY_A, "a", "A", "ф", "Ф"},
            {KEY_S, "s", "S", "ы", "Ы"},          {KEY_D, "d", "D", "в", "В"},      {KEY_F, "f", "F", "а", "А"},
            {KEY_G, "g", "G", "п", "П"},          {KEY_H, "h", "H", "р", "Р"},      {KEY_J, "j", "J", "о", "О"},
            {KEY_K, "k", "K", "л", "Л"},          {KEY_L, "l", "L", "д", "Д"},      {KEY_SEMICOLON, ";", ":", "ж", "Ж"},
            {KEY_APOSTROPHE, "'", "\"", "э", "Э"}, {KEY_Z, "z", "Z", "я", "Я"},     {KEY_X, "x", "X", "ч", "Ч"},
            {KEY_C, "c", "C", "с", "С"},          {KEY_V, "v", "V", "м", "М"},      {KEY_B, "b", "B", "и", "И"},
            {KEY_N, "n", "N", "т", "Т"},          {KEY_M, "m", "M", "ь", "Ь"},      {KEY_COMMA, ",", "<", "б", "Б"},
            {KEY_DOT, ".", ">", "ю", "Ю"},        {KEY_SLASH, "/", "?", ".", ","},  {KEY_102ND, "\\", "|", "\\", "/"},
            {KEY_SPACE, " ", " ", " ", " "},      {KEY_KP0, "0", "0", "0", "0"},    {KEY_KP1, "1", "1", "1", "1"},
            {KEY_KP2, "2", "2", "2", "2"},        {KEY_KP3, "3", "3", "3", "3"},    {KEY_KP4, "4", "4", "4", "4"},
            {KEY_KP5, "5", "5", "5", "5"},        {KEY_KP6, "6", "6", "6", "6"},    {KEY_KP7, "7", "7", "7", "7"},
            {KEY_KP8, "8", "8", "8", "8"},        {KEY_KP9, "9", "9", "9", "9"},    {KEY_KPDOT, ".", ".", ",", ","},
            {KEY_KPPLUS, "+", "+", "+", "+"},     {KEY_KPMINUS, "-", "-", "-", "-"}, {KEY_KPASTERISK, "*", "*", "*", "*"},
            {KEY_KPSLASH, "/", "/", "/", "/"},
        };
        for (const Chars &c : kChars)
            if (c.code == code)
                return &c;
        return nullptr;
    }

    static bool isLetter(const char *s)
    {
        const auto c = static_cast<unsigned char>(s[0]);
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == 0xD0 || c == 0xD1;   // латиница, кириллица
    }

    static uint32_t specialKey(int code)
    {
        switch (code) {
        case KEY_ESC: return kEsc;
        case KEY_TAB: return kTab;
        case KEY_BACKSPACE: return kBackspace;
        case KEY_ENTER: case KEY_KPENTER: return kReturn;
        case KEY_INSERT: return kInsert;
        case KEY_DELETE: return kDelete;
        case KEY_HOME: return kHome;
        case KEY_END: return kEnd;
        case KEY_LEFT: return kLeft;
        case KEY_UP: return kUp;
        case KEY_RIGHT: return kRight;
        case KEY_DOWN: return kDown;
        case KEY_PAGEUP: return kPageUp;
        case KEY_PAGEDOWN: return kPageDown;
        case KEY_COMPOSE: return kMenu;
        case KEY_VOLUMEUP: return kVolumeUp;       // громкость — окну звука стола (docs/PI.md, «Звук на мониторе»)
        case KEY_VOLUMEDOWN: return kVolumeDown;
        case KEY_MUTE: return kVolumeMute;
        case KEY_PLAYPAUSE: return kMediaToggle;   // плееру (media_keys.h)
        case KEY_NEXTSONG: return kMediaNext;
        case KEY_PREVIOUSSONG: return kMediaPrevious;
        case KEY_SCREENLOCK: return kScreenLock;   // клавиша блокировки: стол выключить
        case KEY_SLEEP: return kSleep;             // «луна»: стол выключить, монитор приглушить
        case KEY_BRIGHTNESSUP: return kBrightnessUp;       // яркость монитора зрителя
        case KEY_BRIGHTNESSDOWN: return kBrightnessDown;
        default:
            if (code >= KEY_F1 && code <= KEY_F10)
                return kF1 + uint32_t(code - KEY_F1);
            if (code == KEY_F11 || code == KEY_F12)
                return kF1 + 10 + uint32_t(code - KEY_F11);
            return 0;
        }
    }

    void toggle()
    {
        m_ru = !m_ru;
        if (onLayout)
            onLayout(m_ru);
    }

    bool m_shift[2] = {}, m_ctrl[2] = {}, m_alt[2] = {}, m_meta[2] = {}, m_caps = false, m_ru = false;
    bool m_otherSinceMod = false;
    bool m_layoutPair = false;   // Alt и Shift нажаты парой: отпустят, ничего не нажав между, — раскладка
};
