#include "term_screen.h"

#include <algorithm>
#include <atomic>

namespace term {
namespace {

std::atomic<quint64> g_uid {1};   // номера строк — общие на все вкладки: узлы отрисовки не путаются при смене вкладки

// линии DEC (ESC ( 0) — знаки 0x5f…0x7e: рамки ncurses и старых программ
const char32_t kDecGraphics[32] = {
    0x00a0, 0x25c6, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0, 0x00b1, 0x2424, 0x240b, 0x2518, 0x2510, 0x250c,
    0x2514, 0x253c, 0x23ba, 0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524, 0x2534, 0x252c, 0x2502, 0x2264, 0x2265,
    0x03c0, 0x2260, 0x00a3, 0x00b7};

template<typename T>
void rotateUp(T &lines, size_t base, int top, int bottom, Line &&fresh)
{
    lines.erase(lines.begin() + base + top);
    lines.insert(lines.begin() + base + bottom, std::move(fresh));
}

template<typename T>
void rotateDown(T &lines, size_t base, int top, int bottom, Line &&fresh)
{
    lines.erase(lines.begin() + base + bottom);
    lines.insert(lines.begin() + base + top, std::move(fresh));
}

}   // namespace

// Ширина знака в клетках — как wcwidth glibc (по ней bash и программы считают, где курсор): широкие (W, F в
// EastAsianWidth Юникода — иероглифы, хангыль, эмодзи) — 2, составные (диакритика, невидимые соединители) — 0
int charWidth(char32_t c)
{
    if (c < 0x300)
        return 1;
    if ((c <= 0x36f) || (c >= 0x483 && c <= 0x489) || (c >= 0x591 && c <= 0x5bd) || (c >= 0x610 && c <= 0x61a)
        || (c >= 0x64b && c <= 0x65f) || (c >= 0x1ab0 && c <= 0x1aff) || (c >= 0x1dc0 && c <= 0x1dff)
        || (c >= 0x200b && c <= 0x200f) || (c >= 0x2028 && c <= 0x202e) || (c >= 0x2060 && c <= 0x2064)
        || (c >= 0x20d0 && c <= 0x20ff) || (c >= 0xfe00 && c <= 0xfe0f) || (c >= 0xfe20 && c <= 0xfe2f) || c == 0xfeff
        || (c >= 0xe0100 && c <= 0xe01ef))
        return 0;
    if ((c >= 0x1100 && c <= 0x115f) || (c >= 0x231a && c <= 0x231b) || (c >= 0x2329 && c <= 0x232a)
        || (c >= 0x23e9 && c <= 0x23ec) || c == 0x23f0 || c == 0x23f3 || (c >= 0x25fd && c <= 0x25fe)
        || (c >= 0x2614 && c <= 0x2615) || (c >= 0x2648 && c <= 0x2653) || c == 0x267f || c == 0x2693 || c == 0x26a1
        || (c >= 0x26aa && c <= 0x26ab) || (c >= 0x26bd && c <= 0x26be) || (c >= 0x26c4 && c <= 0x26c5) || c == 0x26ce
        || c == 0x26d4 || c == 0x26ea || (c >= 0x26f2 && c <= 0x26f3) || c == 0x26f5 || c == 0x26fa || c == 0x26fd
        || c == 0x2705 || (c >= 0x270a && c <= 0x270b) || c == 0x2728 || c == 0x274c || c == 0x274e
        || (c >= 0x2753 && c <= 0x2755) || c == 0x2757 || (c >= 0x2795 && c <= 0x2797) || c == 0x27b0 || c == 0x27bf
        || (c >= 0x2b1b && c <= 0x2b1c) || c == 0x2b50 || c == 0x2b55 || (c >= 0x2e80 && c <= 0x303e)
        || (c >= 0x3041 && c <= 0x33ff) || (c >= 0x3400 && c <= 0x4dbf) || (c >= 0x4e00 && c <= 0x9fff)
        || (c >= 0xa000 && c <= 0xa4cf) || (c >= 0xa960 && c <= 0xa97f) || (c >= 0xac00 && c <= 0xd7a3)
        || (c >= 0xf900 && c <= 0xfaff) || (c >= 0xfe10 && c <= 0xfe19) || (c >= 0xfe30 && c <= 0xfe6f)
        || (c >= 0xff00 && c <= 0xff60) || (c >= 0xffe0 && c <= 0xffe6) || c == 0x1f004 || c == 0x1f0cf
        || c == 0x1f18e || (c >= 0x1f191 && c <= 0x1f19a) || (c >= 0x1f200 && c <= 0x1f251)
        || (c >= 0x1f300 && c <= 0x1f64f) || (c >= 0x1f680 && c <= 0x1f6ff) || (c >= 0x1f7e0 && c <= 0x1f7eb)
        || (c >= 0x1f90c && c <= 0x1f9ff) || (c >= 0x1fa70 && c <= 0x1faff) || (c >= 0x20000 && c <= 0x3fffd))
        return 2;
    return 1;
}

Screen::Screen(int cols, int rows)
    : m_cols(std::max(2, cols))
    , m_rows(std::max(1, rows))
{
    m_bottom = m_rows - 1;
    for (int i = 0; i < m_rows; ++i)
        m_main.push_back(blankLine());
    m_tabs.assign(size_t(m_cols), false);
    for (int i = 8; i < m_cols; i += 8)
        m_tabs[size_t(i)] = true;
}

int Screen::lineCount() const { return m_alt ? int(m_altRows.size()) : int(m_main.size()); }

const Line &Screen::line(int index) const { return m_alt ? m_altRows[size_t(index)] : m_main[size_t(index)]; }

Line &Screen::row(int r) { return m_alt ? m_altRows[size_t(r)] : m_main[m_main.size() - size_t(m_rows) + size_t(r)]; }

Line Screen::blankLine() const
{
    Line l;
    l.cells.assign(size_t(m_cols), erased());
    l.uid = g_uid++;
    return l;
}

void Screen::touch(Line &l)
{
    ++l.version;
    ++m_changes;
}

Cell Screen::erased() const
{
    Cell c;
    c.bg = m_pen.bg;   // стирают цветом фона пера (bce, как у xterm)
    return c;
}

bool Screen::blank(const Line &l) const
{
    return std::all_of(l.cells.begin(), l.cells.end(), [](const Cell &c) { return c.ch == U' ' && c.bg == kDefault; });
}

void Screen::clearHistory()
{
    if (m_alt || history() <= 0)
        return;
    const int h = history();
    m_dropped += quint64(h);
    m_main.erase(m_main.begin(), m_main.begin() + h);
    ++m_changes;
}

QString Screen::text(int index, int from, int to) const
{
    QString out;
    if (index < 0 || index >= lineCount())
        return out;
    const Line &l = line(index);
    to = std::min(to, int(l.cells.size()));
    for (int x = std::max(0, from); x < to; ++x) {
        const char32_t c = l.cells[size_t(x)].ch;
        if (c == 0)
            continue;
        if (c > 0xffff) {
            out += QChar(QChar::highSurrogate(c));
            out += QChar(QChar::lowSurrogate(c));
        } else {
            out += QChar(char16_t(c));
        }
    }
    return out;
}

// ---------- разбор ----------
void Screen::feed(const char *data, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        const auto b = static_cast<unsigned char>(data[i]);
        if (m_state == Ground) {
            if (m_utf8Need) {
                if ((b & 0xc0) == 0x80) {
                    m_utf8 = m_utf8 << 6 | (b & 0x3f);
                    if (--m_utf8Need == 0)
                        print(m_utf8 > 0x10ffff || (m_utf8 >= 0xd800 && m_utf8 <= 0xdfff) ? 0xfffd : m_utf8);
                    continue;
                }
                m_utf8Need = 0;
                print(0xfffd);
            }
            if (b >= 0x20 && b < 0x7f) {
                print(b);
            } else if (b >= 0xc2 && b <= 0xdf) {
                m_utf8 = b & 0x1f;
                m_utf8Need = 1;
            } else if (b >= 0xe0 && b <= 0xef) {
                m_utf8 = b & 0x0f;
                m_utf8Need = 2;
            } else if (b >= 0xf0 && b <= 0xf4) {
                m_utf8 = b & 0x07;
                m_utf8Need = 3;
            } else if (b >= 0x80) {
                print(0xfffd);
            } else if (b != 0x7f) {
                control(b);
            }
            continue;
        }
        if (b == 0x18 || b == 0x1a) {   // CAN, SUB — последовательность прочь
            m_state = Ground;
            continue;
        }
        if (b == 0x1b && (m_state == Escape || m_state == EscapeInter || m_state == Csi || m_state == Charset)) {
            m_state = Escape;
            m_inter = 0;
            continue;
        }
        switch (m_state) {
        case Escape:
            if (b == '[') {
                m_state = Csi;
                m_params.clear();
                m_private = 0;
                m_inter = 0;
            } else if (b == ']') {
                m_state = Osc;
                m_osc.clear();
            } else if (b == 'P' || b == 'X' || b == '^' || b == '_') {   // DCS, SOS, PM, APC — пропустить до ST
                m_state = Dcs;
            } else if (b == '(' || b == ')' || b == '*' || b == '+') {
                m_charsetSlot = b == '(' ? 0 : b == ')' ? 1 : 2;
                m_state = Charset;
            } else if (b >= 0x20 && b <= 0x2f) {
                m_inter = char(b);
                m_state = EscapeInter;
            } else if (b < 0x20) {
                control(b);
            } else {
                m_state = Ground;
                escDispatch(char(b));
            }
            break;
        case EscapeInter:
            if (b < 0x20)
                control(b);
            else if (b > 0x2f) {
                m_state = Ground;
                escDispatch(char(b));
            }
            break;
        case Charset:
            if (m_charsetSlot < 2)
                m_charset[m_charsetSlot] = char(b);
            m_state = Ground;
            break;
        case Csi:
            if (b >= '0' && b <= '9') {
                if (m_params.empty())
                    m_params.push_back({-1, false});
                Param &p = m_params.back();
                p.value = std::min((p.value < 0 ? 0 : p.value) * 10 + (b - '0'), 99999);
            } else if (b == ';' || b == ':') {
                if (m_params.empty())
                    m_params.push_back({-1, false});
                if (m_params.size() < 32)
                    m_params.push_back({-1, b == ':'});
            } else if (b >= 0x3c && b <= 0x3f) {
                m_private = char(b);
            } else if (b >= 0x20 && b <= 0x2f) {
                m_inter = char(b);
            } else if (b >= 0x40 && b <= 0x7e) {
                m_state = Ground;
                csiDispatch(char(b));
            } else if (b < 0x20) {
                control(b);
            }
            break;
        case Osc:
            if (b == 0x07) {
                m_state = Ground;
                oscDispatch();
            } else if (b == 0x1b) {
                m_state = OscEscape;
            } else if (m_osc.size() < 8192) {
                m_osc.append(char(b));
            }
            break;
        case OscEscape:
            m_state = Ground;
            if (b == '\\')
                oscDispatch();
            break;
        case Dcs:
            if (b == 0x1b)
                m_state = DcsEscape;
            break;
        case DcsEscape:
            m_state = b == '\\' ? Ground : Dcs;
            break;
        case Ground:
            break;
        }
    }
}

void Screen::control(unsigned char c)
{
    switch (c) {
    case 0x08:   // BS
        if (m_cx > 0)
            --m_cx;
        m_wrapPending = false;
        break;
    case 0x09: {   // HT
        int x = m_cx + 1;
        while (x < m_cols - 1 && !m_tabs[size_t(x)])
            ++x;
        m_cx = std::min(x, m_cols - 1);
        m_wrapPending = false;
        break;
    }
    case 0x0a:
    case 0x0b:
    case 0x0c:
        lineFeed();
        break;
    case 0x0d:
        m_cx = 0;
        m_wrapPending = false;
        break;
    case 0x0e:
        m_gl = 1;
        break;
    case 0x0f:
        m_gl = 0;
        break;
    case 0x1b:
        m_state = Escape;
        m_inter = 0;
        break;
    default:   // звонок и прочее — молча
        break;
    }
}

void Screen::print(char32_t c)
{
    if (m_charset[m_gl] == '0' && c >= 0x5f && c <= 0x7e)
        c = kDecGraphics[c - 0x5f];
    const int w = charWidth(c);
    if (w == 0)
        return;   // составной знак — без него (упрощение: диакритику отдельно не рисуем)
    if (m_wrapPending) {
        if (m_autowrap) {
            Line &l = row(m_cy);
            l.wrapped = true;
            touch(l);
            lineFeed();
            m_cx = 0;
        }
        m_wrapPending = false;
    }
    if (w == 2 && m_cx >= m_cols - 1) {
        if (!m_autowrap)
            return;
        Line &l = row(m_cy);
        l.cells[size_t(m_cx)] = erased();
        l.wrapped = true;
        touch(l);
        lineFeed();
        m_cx = 0;
    }
    Line &l = row(m_cy);
    auto &cs = l.cells;
    if (m_insert) {
        cs.insert(cs.begin() + m_cx, size_t(w), erased());
        cs.resize(size_t(m_cols));
    }
    // клетки, что были половинами широкого знака, — без пары не остаются
    auto unpair = [&](int x) {
        if (x < 0 || x >= m_cols)
            return;
        if (cs[size_t(x)].ch == 0 && x > 0)
            cs[size_t(x) - 1].ch = U' ';
        if (x + 1 < m_cols && cs[size_t(x) + 1].ch == 0)
            cs[size_t(x) + 1].ch = U' ';
    };
    unpair(m_cx);
    if (w == 2)
        unpair(m_cx + 1);
    Cell cell = m_pen;
    cell.ch = c;
    cs[size_t(m_cx)] = cell;
    if (w == 2) {
        cell.ch = 0;
        cs[size_t(m_cx) + 1] = cell;
    }
    touch(l);
    m_last = c;
    m_cx += w;
    if (m_cx >= m_cols) {
        m_cx = m_cols - 1;
        m_wrapPending = true;
    }
}

// ---------- строки ----------
void Screen::lineFeed()
{
    m_wrapPending = false;
    if (m_cy == m_bottom)
        scrollUp(m_top, m_bottom, 1, true);
    else if (m_cy < m_rows - 1)
        ++m_cy;
}

void Screen::reverseIndex()
{
    m_wrapPending = false;
    if (m_cy == m_top)
        scrollDown(m_top, m_bottom, 1);
    else if (m_cy > 0)
        --m_cy;
}

void Screen::scrollUp(int top, int bottom, int n, bool toHistory)
{
    n = std::min(n, bottom - top + 1);
    if (n <= 0)
        return;
    if (toHistory && !m_alt && top == 0 && bottom == m_rows - 1) {   // строки уходят в историю
        for (int i = 0; i < n; ++i)
            m_main.push_back(blankLine());
        while (int(m_main.size()) > m_rows + m_maxHistory) {
            m_main.pop_front();
            ++m_dropped;
        }
    } else {
        for (int i = 0; i < n; ++i) {
            if (m_alt)
                rotateUp(m_altRows, 0, top, bottom, blankLine());
            else
                rotateUp(m_main, m_main.size() - size_t(m_rows), top, bottom, blankLine());
        }
    }
    ++m_changes;
}

void Screen::scrollDown(int top, int bottom, int n)
{
    n = std::min(n, bottom - top + 1);
    if (n <= 0)
        return;
    for (int i = 0; i < n; ++i) {
        if (m_alt)
            rotateDown(m_altRows, 0, top, bottom, blankLine());
        else
            rotateDown(m_main, m_main.size() - size_t(m_rows), top, bottom, blankLine());
    }
    ++m_changes;
}

void Screen::eraseInLine(int r, int from, int to)
{
    Line &l = row(r);
    auto &cs = l.cells;
    from = std::max(0, from);
    to = std::min(to, m_cols);
    if (from >= to)
        return;
    if (from > 0 && cs[size_t(from)].ch == 0)
        cs[size_t(from) - 1].ch = U' ';
    if (to < m_cols && cs[size_t(to)].ch == 0)
        cs[size_t(to)].ch = U' ';
    const Cell e = erased();
    std::fill(cs.begin() + from, cs.begin() + to, e);
    if (to == m_cols)
        l.wrapped = false;
    touch(l);
}

void Screen::moveTo(int r, int c)
{
    if (m_origin)
        r = std::clamp(r + m_top, m_top, m_bottom);
    else
        r = std::clamp(r, 0, m_rows - 1);
    m_cy = r;
    m_cx = std::clamp(c, 0, m_cols - 1);
    m_wrapPending = false;
}

void Screen::saveCursor(Saved &s) const
{
    s.cx = m_cx;
    s.cy = m_cy;
    s.pen = m_pen;
    s.origin = m_origin;
    s.wrapPending = m_wrapPending;
    s.charset[0] = m_charset[0];
    s.charset[1] = m_charset[1];
    s.gl = m_gl;
}

void Screen::restoreCursor(const Saved &s)
{
    m_cx = std::clamp(s.cx, 0, m_cols - 1);
    m_cy = std::clamp(s.cy, 0, m_rows - 1);
    m_pen = s.pen;
    m_origin = s.origin;
    m_wrapPending = s.wrapPending && m_cx == m_cols - 1;
    m_charset[0] = s.charset[0];
    m_charset[1] = s.charset[1];
    m_gl = s.gl;
}

void Screen::switchScreen(bool alt, bool clear, bool keepCursor)
{
    if (alt == m_alt) {
        if (alt && clear)
            for (int r = 0; r < m_rows; ++r)
                eraseInLine(r, 0, m_cols);
        return;
    }
    if (alt) {
        if (keepCursor)
            saveCursor(m_savedAlt);
        m_altRows.clear();
        for (int i = 0; i < m_rows; ++i)
            m_altRows.push_back(blankLine());
        m_alt = true;
    } else {
        m_alt = false;
        m_altRows.clear();
        if (keepCursor)
            restoreCursor(m_savedAlt);
    }
    m_wrapPending = false;
    ++m_changes;
}

// ---------- последовательности ----------
int Screen::param(int i, int def) const
{
    return i < int(m_params.size()) && m_params[size_t(i)].value >= 0 ? m_params[size_t(i)].value : def;
}

void Screen::escDispatch(char final)
{
    const char inter = m_inter;
    m_inter = 0;
    if (inter == '#') {
        if (final == '8')   // DECALN: экран буквами E
            for (int r = 0; r < m_rows; ++r) {
                Line &l = row(r);
                for (Cell &c : l.cells)
                    c = Cell {U'E', kDefault, kDefault, 0};
                touch(l);
            }
        return;
    }
    if (inter)
        return;
    switch (final) {
    case '7':
        saveCursor(m_saved);
        break;
    case '8':
        restoreCursor(m_saved);
        break;
    case 'D':
        lineFeed();
        break;
    case 'E':
        m_cx = 0;
        lineFeed();
        break;
    case 'M':
        reverseIndex();
        break;
    case 'H':
        m_tabs[size_t(m_cx)] = true;
        break;
    case 'c':
        fullReset();
        break;
    default:   // = > (клавиатура справа), N O (разовая смена набора), \ (ST)
        break;
    }
}

void Screen::csiDispatch(char final)
{
    const int n = std::max(1, param(0, 1));
    if (m_private == '?') {
        if (final == 'h' || final == 'l')
            setMode(final == 'h');
        return;
    }
    if (m_private == '>') {
        if (final == 'c' && reply)
            reply(QByteArrayLiteral("\x1b[>1;10;0c"));
        return;
    }
    if (m_private)
        return;
    if (m_inter == ' ' && final == 'q') {
        m_cursorShape = param(0, 0);
        ++m_changes;
        return;
    }
    if (m_inter == '!' && final == 'p') {
        softReset();
        return;
    }
    if (m_inter)
        return;
    switch (final) {
    case '@': {   // ICH
        Line &l = row(m_cy);
        l.cells.insert(l.cells.begin() + m_cx, size_t(std::min(n, m_cols - m_cx)), erased());
        l.cells.resize(size_t(m_cols));
        touch(l);
        m_wrapPending = false;
        break;
    }
    case 'A':
        m_cy = std::max(m_cy >= m_top ? m_top : 0, m_cy - n);
        m_wrapPending = false;
        break;
    case 'B':
    case 'e':
        m_cy = std::min(m_cy <= m_bottom ? m_bottom : m_rows - 1, m_cy + n);
        m_wrapPending = false;
        break;
    case 'C':
    case 'a':
        m_cx = std::min(m_cols - 1, m_cx + n);
        m_wrapPending = false;
        break;
    case 'D':
        m_cx = std::max(0, m_cx - n);
        m_wrapPending = false;
        break;
    case 'E':
        m_cy = std::min(m_cy <= m_bottom ? m_bottom : m_rows - 1, m_cy + n);
        m_cx = 0;
        m_wrapPending = false;
        break;
    case 'F':
        m_cy = std::max(m_cy >= m_top ? m_top : 0, m_cy - n);
        m_cx = 0;
        m_wrapPending = false;
        break;
    case 'G':
    case '`':
        m_cx = std::clamp(param(0, 1) - 1, 0, m_cols - 1);
        m_wrapPending = false;
        break;
    case 'H':
    case 'f':
        moveTo(param(0, 1) - 1, param(1, 1) - 1);
        break;
    case 'I':
        for (int i = 0; i < n; ++i)
            control(0x09);
        break;
    case 'J':
        switch (param(0, 0)) {
        case 0:
            eraseInLine(m_cy, m_cx, m_cols);
            for (int r = m_cy + 1; r < m_rows; ++r)
                eraseInLine(r, 0, m_cols);
            break;
        case 1:
            for (int r = 0; r < m_cy; ++r)
                eraseInLine(r, 0, m_cols);
            eraseInLine(m_cy, 0, m_cx + 1);
            break;
        case 2:
            for (int r = 0; r < m_rows; ++r)
                eraseInLine(r, 0, m_cols);
            break;
        case 3:
            clearHistory();
            break;
        }
        break;
    case 'K':
        switch (param(0, 0)) {
        case 0:
            eraseInLine(m_cy, m_cx, m_cols);
            break;
        case 1:
            eraseInLine(m_cy, 0, m_cx + 1);
            break;
        case 2:
            eraseInLine(m_cy, 0, m_cols);
            break;
        }
        break;
    case 'L':
        if (m_cy >= m_top && m_cy <= m_bottom) {
            scrollDown(m_cy, m_bottom, n);
            m_cx = 0;
        }
        break;
    case 'M':
        if (m_cy >= m_top && m_cy <= m_bottom) {
            scrollUp(m_cy, m_bottom, n);
            m_cx = 0;
        }
        break;
    case 'P': {   // DCH
        Line &l = row(m_cy);
        const int k = std::min(n, m_cols - m_cx);
        if (m_cx + k < m_cols && l.cells[size_t(m_cx + k)].ch == 0)
            l.cells[size_t(m_cx + k)].ch = U' ';
        l.cells.erase(l.cells.begin() + m_cx, l.cells.begin() + m_cx + k);
        l.cells.resize(size_t(m_cols), erased());
        touch(l);
        m_wrapPending = false;
        break;
    }
    case 'S':
        scrollUp(m_top, m_bottom, n);
        break;
    case 'T':
        if (m_params.size() <= 1)
            scrollDown(m_top, m_bottom, n);
        break;
    case 'X':
        eraseInLine(m_cy, m_cx, m_cx + n);
        m_wrapPending = false;
        break;
    case 'Z':
        for (int i = 0; i < n; ++i) {
            int x = m_cx - 1;
            while (x > 0 && !m_tabs[size_t(x)])
                --x;
            m_cx = std::max(0, x);
        }
        m_wrapPending = false;
        break;
    case 'b':
        for (int i = 0; i < std::min(n, 65535); ++i)
            print(m_last);
        break;
    case 'c':
        if (param(0, 0) == 0 && reply)
            reply(QByteArrayLiteral("\x1b[?62;22c"));   // VT220 с цветом
        break;
    case 'd':
        m_cy = std::clamp(param(0, 1) - 1 + (m_origin ? m_top : 0), 0, m_rows - 1);
        m_wrapPending = false;
        break;
    case 'g':
        if (param(0, 0) == 0)
            m_tabs[size_t(m_cx)] = false;
        else if (param(0, 0) == 3)
            std::fill(m_tabs.begin(), m_tabs.end(), false);
        break;
    case 'h':
    case 'l':
        for (const Param &p : m_params)
            if (p.value == 4)
                m_insert = final == 'h';
        break;
    case 'm':
        sgr();
        break;
    case 'n':
        if (!reply)
            break;
        if (param(0, 0) == 5)
            reply(QByteArrayLiteral("\x1b[0n"));
        else if (param(0, 0) == 6)
            reply(QStringLiteral("\x1b[%1;%2R").arg(m_cy + 1 - (m_origin ? m_top : 0)).arg(m_cx + 1).toLatin1());
        break;
    case 'r': {
        const int top = std::clamp(param(0, 1) - 1, 0, m_rows - 1);
        const int bottom = std::clamp(param(1, m_rows) - 1, 0, m_rows - 1);
        if (top < bottom) {
            m_top = top;
            m_bottom = bottom;
            moveTo(0, 0);
        }
        break;
    }
    case 's':
        saveCursor(m_saved);
        break;
    case 'u':
        restoreCursor(m_saved);
        break;
    case 't':
        if (param(0, 0) == 18 && reply)
            reply(QStringLiteral("\x1b[8;%1;%2t").arg(m_rows).arg(m_cols).toLatin1());
        break;
    default:
        break;
    }
}

void Screen::setMode(bool on)
{
    for (const Param &p : m_params) {
        switch (p.value) {
        case 1:
            m_appCursor = on;
            break;
        case 6:
            m_origin = on;
            moveTo(0, 0);
            break;
        case 7:
            m_autowrap = on;
            if (!on)
                m_wrapPending = false;
            break;
        case 25:
            m_cursorVisible = on;
            ++m_changes;
            break;
        case 47:
        case 1047:
            switchScreen(on, p.value == 1047, false);
            break;
        case 1048:
            if (on)
                saveCursor(m_saved);
            else
                restoreCursor(m_saved);
            break;
        case 1049:
            switchScreen(on, true, true);
            break;
        case 1000:
        case 1002:
        case 1003:
            m_mouseMode = on ? p.value : 0;
            break;
        case 1006:
            m_mouseSgr = on;
            break;
        case 1007:
            m_altScroll = on;
            break;
        case 2004:
            m_bracketedPaste = on;
            break;
        default:   // 3 (132 знака), 5 (обратный экран), 12 (мигание), 1004 (фокус), 2026 (кадр целиком) — без них
            break;
        }
    }
}

void Screen::sgr()
{
    if (m_params.empty()) {
        m_pen = Cell {};
        return;
    }
    const auto value = [this](size_t i) { return i < m_params.size() && m_params[i].value >= 0 ? m_params[i].value : 0; };
    // 38/48/58: 5;n — номер, 2;r;g;b — цвет; через двоеточие — 5:n, 2::r:g:b или 2:r:g:b
    const auto extended = [&](size_t &i, uint32_t *target) {
        uint32_t c = 0;
        bool ok = false;
        if (i + 1 < m_params.size() && m_params[i + 1].sub) {
            size_t end = i + 1;
            while (end + 1 < m_params.size() && m_params[end + 1].sub)
                ++end;
            const int mode = value(i + 1);
            const size_t subs = end - (i + 1);   // после «режима»
            if (mode == 5 && subs >= 1) {
                c = indexed(value(i + 2));
                ok = true;
            } else if (mode == 2 && subs >= 3) {
                c = rgb(value(end - 2), value(end - 1), value(end));
                ok = true;
            }
            i = end;
        } else {
            const int mode = value(i + 1);
            if (mode == 5 && i + 2 < m_params.size()) {
                c = indexed(value(i + 2));
                ok = true;
                i += 2;
            } else if (mode == 2 && i + 4 < m_params.size()) {
                c = rgb(value(i + 2), value(i + 3), value(i + 4));
                ok = true;
                i += 4;
            } else {
                i += 1;
            }
        }
        if (ok && target)
            *target = c;
    };
    for (size_t i = 0; i < m_params.size(); ++i) {
        const int p = value(i);
        uint16_t &a = m_pen.attr;
        switch (p) {
        case 0: m_pen = Cell {}; break;
        case 1: a |= Bold; break;
        case 2: a |= Faint; break;
        case 3: a |= Italic; break;
        case 4:
            if (i + 1 < m_params.size() && m_params[i + 1].sub && value(i + 1) == 0)
                a &= ~Underline;
            else
                a |= Underline;
            break;
        case 7: a |= Inverse; break;
        case 8: a |= Hidden; break;
        case 9: a |= Strike; break;
        case 21: a |= Underline; break;
        case 22: a &= ~(Bold | Faint); break;
        case 23: a &= ~Italic; break;
        case 24: a &= ~Underline; break;
        case 27: a &= ~Inverse; break;
        case 28: a &= ~Hidden; break;
        case 29: a &= ~Strike; break;
        case 38: extended(i, &m_pen.fg); break;
        case 39: m_pen.fg = kDefault; break;
        case 48: extended(i, &m_pen.bg); break;
        case 49: m_pen.bg = kDefault; break;
        case 58: extended(i, nullptr); break;   // цвет подчёркивания — без него
        default:
            if (p >= 30 && p <= 37)
                m_pen.fg = indexed(p - 30);
            else if (p >= 40 && p <= 47)
                m_pen.bg = indexed(p - 40);
            else if (p >= 90 && p <= 97)
                m_pen.fg = indexed(p - 90 + 8);
            else if (p >= 100 && p <= 107)
                m_pen.bg = indexed(p - 100 + 8);
            break;
        }
        while (i + 1 < m_params.size() && m_params[i + 1].sub)   // хвосты вроде 4:3 — прочь
            ++i;
    }
}

void Screen::oscDispatch()
{
    const int sep = m_osc.indexOf(';');
    const QByteArray code = sep < 0 ? m_osc : m_osc.left(sep);
    const QByteArray arg = sep < 0 ? QByteArray() : m_osc.mid(sep + 1);
    if (code == "0" || code == "2") {
        m_title = QString::fromUtf8(arg);
    } else if ((code == "10" || code == "11") && arg == "?" && reply) {   // какого цвета текст и фон (vim, светлая ли тема)
        const uint32_t c = code == "10" ? defaultFg : defaultBg;
        const auto ch = [](uint32_t v) { return QByteArray::number(v & 0xff, 16).rightJustified(2, '0').repeated(2); };
        reply("\x1b]" + code + ";rgb:" + ch(c >> 16) + "/" + ch(c >> 8) + "/" + ch(c) + "\x1b\\");
    }
}

void Screen::softReset()
{
    m_pen = Cell {};
    m_insert = false;
    m_origin = false;
    m_autowrap = true;
    m_cursorVisible = true;
    m_appCursor = false;
    m_top = 0;
    m_bottom = m_rows - 1;
    m_charset[0] = m_charset[1] = 'B';
    m_gl = 0;
    m_saved = Saved {};
    m_wrapPending = false;
    ++m_changes;
}

void Screen::fullReset()
{
    switchScreen(false, false, false);
    softReset();
    for (int r = 0; r < m_rows; ++r)
        eraseInLine(r, 0, m_cols);
    clearHistory();
    m_cx = m_cy = 0;
    m_tabs.assign(size_t(m_cols), false);
    for (int i = 8; i < m_cols; i += 8)
        m_tabs[size_t(i)] = true;
    m_mouseMode = 0;
    m_mouseSgr = false;
    m_bracketedPaste = false;
    m_cursorShape = 0;
    m_title.clear();
}

// ---------- размер ----------
void Screen::resize(int cols, int rows)
{
    cols = std::max(2, cols);
    rows = std::max(1, rows);
    if (cols == m_cols && rows == m_rows)
        return;

    // основной буфер: экран — последние строки очереди. Ниже — уже: сначала пустые строки под курсором, потом
    // верхние уходят в историю; выше — строки возвращаются из истории
    int &mainCy = m_alt ? m_savedAlt.cy : m_cy;
    if (rows < m_rows) {
        int excess = m_rows - rows, cur = m_rows;
        while (excess > 0 && mainCy < cur - 1 && blank(m_main.back())) {
            m_main.pop_back();
            --cur;
            --excess;
        }
        mainCy = std::max(0, mainCy - excess);
    } else if (rows > m_rows) {
        const int add = rows - m_rows;
        const int fromHistory = m_alt ? 0 : std::min(add, int(m_main.size()) - m_rows);
        mainCy += fromHistory;
        m_cols = cols;   // новые строки — уже новой ширины
        for (int i = fromHistory; i < add; ++i)
            m_main.push_back(blankLine());
    }
    // запасной: курсор остаётся на экране
    if (m_alt) {
        if (rows < m_rows) {
            const int drop = std::max(0, m_cy - (rows - 1));
            m_altRows.erase(m_altRows.begin(), m_altRows.begin() + drop);
            m_cy -= drop;
            m_altRows.resize(size_t(rows));
        } else {
            m_cols = cols;
            while (int(m_altRows.size()) < rows)
                m_altRows.push_back(blankLine());
        }
    }
    m_cols = cols;
    m_rows = rows;
    // строки экрана — точно по ширине; широкий знак, разрезанный краем, — прочь
    const auto fit = [this](Line &l) {
        l.cells.resize(size_t(m_cols));
        if (charWidth(l.cells.back().ch) == 2)
            l.cells.back().ch = U' ';
        ++l.version;
    };
    for (size_t i = m_main.size() - size_t(std::min(m_rows, int(m_main.size()))); i < m_main.size(); ++i)
        fit(m_main[i]);
    for (Line &l : m_altRows)
        fit(l);
    m_tabs.resize(size_t(m_cols), false);
    for (int i = 8; i < m_cols; i += 8)
        m_tabs[size_t(i)] = true;
    m_top = 0;
    m_bottom = m_rows - 1;
    m_cx = std::min(m_cx, m_cols - 1);
    m_cy = std::clamp(m_cy, 0, m_rows - 1);
    m_savedAlt.cy = std::clamp(m_savedAlt.cy, 0, m_rows - 1);
    m_wrapPending = false;
    ++m_changes;
}

}   // namespace term
