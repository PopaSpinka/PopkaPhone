// НаомиФон — экран вкладки «Терминала» стола (docs/TERMINAL.md): разбор вывода программ (UTF-8 и управляющие
// последовательности xterm — то, что обещает TERM=xterm-256color: курсор, стирание, области прокрутки, вставка и
// удаление строк и знаков, 16, 256 и 24-битные цвета, запасной экран, режимы клавиш курсора, мыши и вставки скобками,
// ответы на запросы положения курсора и цветов) и сетка знаков с историей.
//   Строки — история и экран одной очередью (экран — последние rows строк); у запасного экрана истории нет. У каждой
//   строки свой номер (uid) на всё время работы оболочки и счётчик правок (version): отрисовка (TermView) держит узел
//   сцены за номер — прокрутка и новые строки двигают узлы, перерисовывается только то, что правили.
#pragma once

#include <QByteArray>
#include <QString>

#include <cstdint>
#include <deque>
#include <functional>
#include <vector>

namespace term {

enum Attr : uint16_t {
    Bold = 1,
    Faint = 2,
    Italic = 4,
    Underline = 8,
    Inverse = 16,
    Hidden = 32,
    Strike = 64,
};

// цвет: 0 — по умолчанию; 0x01000000 | номер (0–255); 0x02000000 | 0xRRGGBB
constexpr uint32_t kDefault = 0;
constexpr uint32_t indexed(int i) { return 0x01000000u | uint32_t(i & 0xff); }
constexpr uint32_t rgb(int r, int g, int b) { return 0x02000000u | uint32_t(r & 255) << 16 | uint32_t(g & 255) << 8 | uint32_t(b & 255); }

struct Cell {
    char32_t ch = U' ';   // 0 — вторая половина широкого знака
    uint32_t fg = kDefault;
    uint32_t bg = kDefault;
    uint16_t attr = 0;
};

struct Line {
    std::vector<Cell> cells;
    quint64 uid = 0;
    quint32 version = 0;
    bool wrapped = false;   // продолжается следующей (перенос по краю, а не перевод строки)
};

int charWidth(char32_t c);   // 0 — составной знак, 2 — широкий (иероглифы, эмодзи)

class Screen
{
public:
    Screen(int cols, int rows);

    void feed(const char *data, size_t size);
    void resize(int cols, int rows);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    // строки текущего буфера: 0 … count-1, экран — последние rows
    int lineCount() const;
    const Line &line(int index) const;
    int history() const { return lineCount() - m_rows; }
    quint64 dropped() const { return m_dropped; }   // строк ушло из начала истории за всё время
    void clearHistory();

    int cursorRow() const { return m_cy; }
    int cursorCol() const { return m_cx; }
    bool cursorVisible() const { return m_cursorVisible; }
    int cursorShape() const { return m_cursorShape; }   // DECSCUSR: 0–2 блок, 3–4 черта снизу, 5–6 черта
    bool altScreen() const { return m_alt; }
    bool appCursor() const { return m_appCursor; }
    bool bracketedPaste() const { return m_bracketedPaste; }
    int mouseMode() const { return m_mouseMode; }      // 0, 1000 (нажатия), 1002 (и тянут), 1003 (любое движение)
    bool mouseSgr() const { return m_mouseSgr; }
    bool alternateScroll() const { return m_altScroll; }
    quint64 changes() const { return m_changes; }      // растёт с каждой правкой — пора перерисовать
    QString title() const { return m_title; }

    // текст строки index, знаки [from, to) — для выделения
    QString text(int index, int from, int to) const;

    std::function<void(const QByteArray &)> reply;   // ответы программе (положение курсора, цвета, кто мы)
    uint32_t defaultFg = 0xece9e3, defaultBg = 0x0e0f0c;   // для ответа на запрос цветов (OSC 10/11)

private:
    enum State { Ground, Escape, EscapeInter, Csi, Osc, OscEscape, Dcs, DcsEscape, Charset };
    struct Param {
        int value;
        bool sub;   // после двоеточия (38:2::r:g:b)
    };
    struct Saved {
        int cx = 0, cy = 0;
        Cell pen;
        bool origin = false, wrapPending = false;
        char charset[2] = {'B', 'B'};
        int gl = 0;
    };

    Line &row(int r);
    Line blankLine() const;
    void touch(Line &l);
    void touchRow(int r) { touch(row(r)); }
    Cell erased() const;

    void print(char32_t c);
    void control(unsigned char c);
    void escDispatch(char final);
    void csiDispatch(char final);
    void oscDispatch();
    void sgr();
    void setMode(bool on);

    void lineFeed();
    void reverseIndex();
    void scrollUp(int top, int bottom, int n, bool toHistory = false);
    void scrollDown(int top, int bottom, int n);
    void eraseInLine(int r, int from, int to);
    void moveTo(int row, int col);
    void switchScreen(bool alt, bool clear, bool keepCursor);
    void saveCursor(Saved &s) const;
    void restoreCursor(const Saved &s);
    bool blank(const Line &l) const;
    void softReset();
    void fullReset();
    int param(int i, int def) const;

    int m_cols, m_rows;
    std::deque<Line> m_main;    // история и экран
    std::vector<Line> m_altRows;
    bool m_alt = false;
    quint64 m_dropped = 0;
    quint64 m_changes = 0;
    int m_maxHistory = 5000;

    int m_cx = 0, m_cy = 0;
    bool m_wrapPending = false;
    Cell m_pen;
    int m_top = 0, m_bottom = 0;   // область прокрутки
    bool m_origin = false, m_autowrap = true, m_insert = false;
    bool m_cursorVisible = true, m_appCursor = false, m_bracketedPaste = false, m_mouseSgr = false, m_altScroll = true;
    int m_mouseMode = 0;
    int m_cursorShape = 0;
    char m_charset[2] = {'B', 'B'};   // G0, G1: B — ASCII, 0 — линии DEC
    int m_gl = 0;
    int m_charsetSlot = 0;
    std::vector<bool> m_tabs;
    Saved m_saved, m_savedAlt;
    char32_t m_last = U' ';
    QString m_title;

    State m_state = Ground;
    std::vector<Param> m_params;
    bool m_paramStarted = false;
    char m_private = 0;
    char m_inter = 0;
    QByteArray m_osc;
    uint32_t m_utf8 = 0;
    int m_utf8Need = 0;
};

}   // namespace term
