// НаомиФон — проверка экрана «Терминала» (term::Screen, docs/TERMINAL.md): последовательности по одной, а с файлом —
// вывод настоящей программы (device/term/capture.py) как его увидит терминал. Сборка на телефоне, в среде сборки:
//   g++ -std=c++17 -O1 -fPIC -I ~/naomi/src/shell/app $(pkg-config --cflags Qt6Core) screen_test.cpp \
//       ~/naomi/src/shell/app/term_screen.cpp $(pkg-config --libs Qt6Core) -o screen_test
#include "term_screen.h"

#include <QFile>
#include <cstdio>
#include <string>

using term::Screen;

static int fails = 0;
static void check(const char *name, bool ok, const std::string &got = {})
{
    printf("%s %s%s%s\n", ok ? "✓" : "✗", name, got.empty() ? "" : " — ", got.c_str());
    fails += !ok;
}
static std::string row(const Screen &s, int r)
{
    QString t = s.text(s.history() + r, 0, s.cols());
    while (t.endsWith(' '))
        t.chop(1);
    return t.toStdString();
}
static void feed(Screen &s, const char *t) { s.feed(t, strlen(t)); }

int main(int argc, char **argv)
{
    if (argc > 1) {   // вывод программы: экран как есть
        QFile f(argv[1]);
        f.open(QIODevice::ReadOnly);
        const QByteArray d = f.readAll();
        Screen s(argc > 3 ? atoi(argv[2]) : 80, argc > 3 ? atoi(argv[3]) : 24);
        QByteArray replies;
        s.reply = [&](const QByteArray &b) { replies += b; };
        s.feed(d.constData(), size_t(d.size()));
        printf("┌ %s: %d × %d, история %d, курсор %d,%d %s, запасной экран %d, ответов %lld\n", argv[1], s.cols(), s.rows(),
               s.history(), s.cursorRow(), s.cursorCol(), s.cursorVisible() ? "виден" : "скрыт", s.altScreen(),
               qlonglong(replies.size()));
        for (int r = 0; r < s.rows(); ++r)
            printf("│%s\n", row(s, r).c_str());
        return 0;
    }
    {
        Screen s(80, 24);
        feed(s, "hello\r\nworld");
        check("строки и перевод строки", row(s, 0) == "hello" && row(s, 1) == "world" && s.cursorCol() == 5);
    }
    {
        Screen s(80, 24);
        std::string t(85, 'x');
        feed(s, t.c_str());
        check("перенос по краю", row(s, 0).size() == 80 && row(s, 1) == "xxxxx" && s.line(s.history()).wrapped);
    }
    {
        Screen s(80, 24);
        for (int i = 1; i <= 30; ++i)
            feed(s, ("line " + std::to_string(i) + "\r\n").c_str());
        check("история растёт", s.history() == 7 && row(s, 0) == "line 8", std::to_string(s.history()) + " " + row(s, 0));
        feed(s, "\x1b[2J\x1b[H");
        check("стереть экран и домой", row(s, 0).empty() && s.cursorRow() == 0 && s.cursorCol() == 0 && s.history() == 7);
        feed(s, "\x1b[3J");
        check("стереть историю (E3)", s.history() == 0);
    }
    {
        Screen s(20, 6);
        feed(s, "a\r\nb\r\nc\r\nd\r\ne\r\nf");
        feed(s, "\x1b[2;4r\x1b[4;1H\n");   // область 2–4, перевод строки внизу области
        check("область прокрутки", row(s, 0) == "a" && row(s, 1) == "c" && row(s, 2) == "d" && row(s, 3).empty()
                                       && row(s, 4) == "e" && s.history() == 0,
              row(s, 0) + "|" + row(s, 1) + "|" + row(s, 2) + "|" + row(s, 3) + "|" + row(s, 4));
    }
    {
        Screen s(20, 5);
        feed(s, "main\r\n");
        feed(s, "\x1b[?1049h\x1b[Halt screen");
        check("запасной экран", s.altScreen() && row(s, 0) == "alt screen");
        feed(s, "\x1b[?1049l");
        check("и обратно — основной цел, курсор на месте", !s.altScreen() && row(s, 0) == "main" && s.cursorRow() == 1);
    }
    {
        Screen s(20, 5);
        feed(s, "\x1b[38;2;10;20;30mA\x1b[38:2::1:2:3mB\x1b[48;5;196mC\x1b[1;4;7mD\x1b[0mE");
        const auto &c = s.line(s.history()).cells;
        check("цвета 24 бит (;) и (:), 256, атрибуты",
              c[0].fg == term::rgb(10, 20, 30) && c[1].fg == term::rgb(1, 2, 3) && c[2].bg == term::indexed(196)
                  && (c[3].attr & term::Bold) && (c[3].attr & term::Underline) && (c[3].attr & term::Inverse)
                  && c[4].attr == 0 && c[4].fg == term::kDefault);
    }
    {
        Screen s(20, 5);
        feed(s, "\xe3\x81\x82x\xf0\x9f\x98\x80");   // あ x 😀
        const auto &c = s.line(s.history()).cells;
        check("широкие знаки", c[0].ch == 0x3042 && c[1].ch == 0 && c[2].ch == U'x' && c[3].ch == 0x1f600 && c[4].ch == 0
                                   && s.cursorCol() == 5);
        feed(s, "\r\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82");
        check("кириллица", row(s, 0).rfind("Привет", 0) == 0, row(s, 0));
    }
    {
        Screen s(20, 5);
        QByteArray rep;
        s.reply = [&](const QByteArray &b) { rep += b; };
        feed(s, "\x1b[3;7H\x1b[6n\x1b[c\x1b]11;?\x07");
        check("ответы: положение, кто мы, цвет фона", rep == "\x1b[3;7R\x1b[?62;22c\x1b]11;rgb:0e0e/0f0f/0c0c\x1b\\",
              rep.toStdString().substr(1));
    }
    {
        Screen s(20, 5);
        feed(s, "\x1b(0lqqk\x1b(B");
        check("линии DEC", row(s, 0) == "┌──┐", row(s, 0));
        feed(s, "\r\nabcdef\x1b[3G\x1b[2@");
        check("вставка знаков", row(s, 1) == "ab  cdef", row(s, 1));
        feed(s, "\x1b[2P");
        check("удаление знаков", row(s, 1) == "abcdef", row(s, 1));
        feed(s, "\x1b[2X");
        check("стирание знаков", row(s, 1) == "ab  ef", row(s, 1));
    }
    {
        Screen s(40, 10);
        for (int i = 1; i <= 15; ++i)
            feed(s, ("l" + std::to_string(i) + "\r\n").c_str());
        feed(s, "$ ");
        s.resize(40, 6);
        check("ниже окно — курсор на месте, строки сверху в историю", row(s, 5) == "$" && s.cursorRow() == 5,
              row(s, 5) + " " + std::to_string(s.cursorRow()));
        s.resize(40, 10);
        check("выше — строки из истории назад", row(s, 9) == "$" && row(s, 0) == "l7", row(s, 0));
        s.resize(20, 10);
        check("уже — строки экрана по ширине", s.line(s.history()).cells.size() == 20);
    }
    {
        Screen s(10, 3);
        feed(s, "\x1b[?25l\x1b[?1h\x1b[?2004h\x1b[?1000h\x1b[?1006h\x1b[5 q");
        check("режимы", !s.cursorVisible() && s.appCursor() && s.bracketedPaste() && s.mouseMode() == 1000
                            && s.mouseSgr() && s.cursorShape() == 5);
        feed(s, "\x1b" "c");
        check("полный сброс", s.cursorVisible() && !s.appCursor() && !s.bracketedPaste() && s.mouseMode() == 0);
    }
    {   // уголок: последовательность разорвана между кадрами
        Screen s(20, 3);
        const char *a = "\x1b[3", *b = "1mR\xd0", *c = "\x9f";
        s.feed(a, strlen(a));
        s.feed(b, strlen(b));
        s.feed(c, strlen(c));
        const auto &cells = s.line(s.history()).cells;
        check("последовательность и UTF-8 через границу кадров", cells[0].ch == U'R' && cells[0].fg == term::indexed(1)
                                                                   && cells[1].ch == 0x41f);
    }
    printf("ИТОГ: %s\n", fails ? "есть ошибки" : "всё прошло");
    return fails ? 1 : 0;
}
