// Проверка трекпада приставки (pi/dock/trackpad.*) без трекпада и без Pi — на Mac:
//   c++ -std=c++20 -Ipi/dock pi/tests/trackpad_test.cpp pi/dock/trackpad.cpp -o /tmp/trackpad_test && /tmp/trackpad_test
// Без аргументов — выдуманные пальцы: каждое правило движка своим случаем (дрожь, касание, щелчок, прокрутка, лежащий
// палец, три пальца, смахивание, щипок, печать). «gain» — таблица ускорения. «replay файл» — запись настоящего
// трекпада (строки «время_мкс щелчок число_пальцев [id x_мм y_мм major minor pressure]…»): что движок из неё сделал.
#include "trackpad.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace {

int failed = 0;
#define CHECK(cond, ...)                                                                                              \
    do {                                                                                                              \
        if (!(cond)) {                                                                                                \
            ++failed;                                                                                                 \
            printf("  НЕ ТАК (%s:%d): %s — ", __FILE__, __LINE__, #cond);                                             \
            printf(__VA_ARGS__);                                                                                      \
            printf("\n");                                                                                             \
        }                                                                                                             \
    } while (0)

struct Sim
{
    Trackpad pad;
    int64_t t = 1000000;
    std::map<int, Trackpad::Touch> fingers;
    bool click = false;
    // что вышло
    double mx = 0, my = 0, path = 0, sx = 0, sy = 0;
    int moves = 0, scrolls = 0, scrollEnds = 0, zoom = 0, swipes = 0, lastSwipe = -1, swipeFingers = 0, buttons = 0;
    std::string presses;   // история кнопок: «1 0 2 0»

    Sim()
    {
        pad.onMove = [this](double dx, double dy, int64_t) { mx += dx, my += dy, path += std::hypot(dx, dy), ++moves; };
        pad.onButtons = [this](int b) { buttons = b, presses += (presses.empty() ? "" : " ") + std::to_string(b); };
        pad.onScroll = [this](double dx, double dy, int64_t, bool f) { f ? (sx += dx, sy += dy, ++scrolls) : ++scrollEnds; };
        pad.onSwipe = [this](Trackpad::Swipe d, int n) { ++swipes, lastSwipe = d, swipeFingers = n; };
        pad.onZoom = [this](int s) { zoom += s; };
    }
    void down(int id, double x, double y, int major = 300, int pressure = 60) { fingers[id] = {id, x, y, major, major * 3 / 4, pressure}; }
    void up(int id) { fingers.erase(id); }
    void step(int ms = 11)   // отчёт трекпада через ms
    {
        t += ms * 1000;
        pad.tick(t);
        std::vector<Trackpad::Touch> v;
        for (const auto &[id, f] : fingers)
            v.push_back(f);
        pad.frame(t, v, click);
    }
    void wait(int ms)   // трекпад молчит (пальцев нет) или пальцы стоят
    {
        for (int i = 0; i < ms; i += 11) {
            if (fingers.empty()) {
                t += 11000;
                pad.tick(t);
            } else {
                step();
            }
        }
    }
    void glide(double dx, double dy, int ms, const std::vector<int> &ids)   // пальцы едут на dx, dy за ms
    {
        const int steps = std::max(1, ms / 11);
        for (int i = 0; i < steps; ++i) {
            for (int id : ids)
                fingers[id].x += dx / steps, fingers[id].y += dy / steps;
            step();
        }
    }
    void liftAll() { fingers.clear(); step(); }
    void reset() { mx = my = path = sx = sy = 0, moves = scrolls = scrollEnds = zoom = swipes = 0, presses.clear(); }
};

void cases()
{
    printf("дрожь: палец лежит секунду и дрожит на 0,03 мм\n");
    {
        Sim s;
        s.down(1, 80, 60);
        s.step();
        for (int i = 0; i < 90; ++i) {
            s.fingers[1].x = 80 + 0.03 * std::sin(i * 1.7), s.fingers[1].y = 60 + 0.03 * std::cos(i * 2.3);
            s.step();
        }
        s.liftAll();
        s.wait(300);
        CHECK(s.moves == 0, "курсор сдвинулся %d раз на %.2f", s.moves, s.path);
        CHECK(s.presses.empty(), "кнопки: %s", s.presses.c_str());
    }
    printf("палец ведут медленно, он встал и дрожит — курсор стоит; повели дальше — едет без скачка\n");
    {
        Sim s;
        s.down(1, 60, 60);
        s.step();
        s.glide(20, 0, 600, {1});
        const double before = s.mx;
        for (int i = 0; i < 60; ++i) {
            s.fingers[1].x = 80 + 0.03 * std::sin(i * 1.7);
            s.step();
        }
        CHECK(std::fabs(s.mx - before) < 0.5, "стоящий палец сдвинул курсор на %.2f", s.mx - before);   // пока не признан стоящим (80 мс)
        const int was = s.moves;
        s.glide(0.6, 0, 110, {1});
        CHECK(s.moves > was, "после стоянки курсор не поехал");
        CHECK(s.mx - before < 0.6 * 6, "после стоянки скачок: %.2f точек за 0,6 мм", s.mx - before);
        CHECK(before > 60 && s.my == 0, "ход 20 мм дал %.1f точек (вбок %.1f)", before, s.my);
    }
    printf("касание — щелчок\n");
    {
        Sim s;
        s.down(1, 80, 60);
        s.step();
        s.fingers[1].x += 0.3;   // пятно растёт — центр ползёт
        s.wait(55);
        s.liftAll();
        s.wait(100);
        CHECK(s.presses == "1 0", "кнопки: %s", s.presses.c_str());
        CHECK(s.moves == 0, "курсор сдвинулся на %.2f", s.path);
    }
    printf("касание двумя пальцами — правый щелчок; три — ничего\n");
    {
        Sim s;
        s.down(1, 70, 60);
        s.step();
        s.down(2, 92, 55);
        s.wait(60);
        s.up(1);
        s.step();
        s.liftAll();
        s.wait(100);
        CHECK(s.presses == "2 0", "кнопки: %s", s.presses.c_str());
        CHECK(s.moves == 0 && s.scrolls == 0, "ход %d, прокрутка %d", s.moves, s.scrolls);
        s.reset();
        s.down(1, 60, 60), s.down(2, 80, 55), s.down(3, 100, 58);
        s.wait(60);
        s.liftAll();
        s.wait(900);
        CHECK(s.presses.empty(), "кнопки после касания тремя: %s", s.presses.c_str());
    }
    printf("палец полежал и ушёл — не щелчок; повели сразу — не щелчок\n");
    {
        Sim s;
        s.down(1, 80, 60);
        s.wait(400);
        s.liftAll();
        s.wait(100);
        CHECK(s.presses.empty(), "кнопки: %s", s.presses.c_str());
        s.down(1, 80, 60);
        s.step();
        s.glide(6, 0, 66, {1});
        s.liftAll();
        s.wait(100);
        CHECK(s.presses.empty(), "кнопки: %s", s.presses.c_str());
        CHECK(s.mx > 10, "ход 6 мм дал %.1f", s.mx);
    }
    printf("щелчок трекпада: курсор съехал перед нажатием — возвращается; под нажатым стоит\n");
    {
        Sim s;
        s.down(1, 80, 60);
        s.wait(300);
        s.glide(3, 0, 150, {1});      // подвели
        s.wait(200);
        const double aimed = s.mx;
        s.glide(0.5, 0.3, 44, {1});   // давят — пятно ползёт
        s.click = true;
        s.step();
        CHECK(std::fabs(s.mx - aimed) < 0.01 && std::fabs(s.my) < 0.01, "щелчок пришёлся мимо на %.2f, %.2f", s.mx - aimed, s.my);
        s.glide(0.8, 0.2, 60, {1});   // под нажатым ещё ползёт
        CHECK(std::fabs(s.mx - aimed) < 0.01, "под нажатым курсор уехал на %.2f", s.mx - aimed);
        s.click = false;
        s.step();
        CHECK(s.presses == "1 0", "кнопки: %s", s.presses.c_str());
        s.reset();
        s.click = true;               // нажали и тащат
        s.step();
        s.glide(15, 0, 200, {1});
        CHECK(s.mx > 40 && s.buttons == 1, "перетаскивание нажатием: ход %.1f, кнопки %d", s.mx, s.buttons);
        s.click = false;
        s.step();
    }
    printf("щелчок двумя пальцами — правый; большой палец жмёт снизу, указательный выше — левый; угол\n");
    {
        Sim s;
        s.down(1, 70, 60), s.down(2, 92, 55);
        s.wait(300);
        s.click = true;
        s.step();
        CHECK(s.buttons == 2, "кнопки %d", s.buttons);
        s.click = false;
        s.step();
        s.liftAll();
        s.wait(50);
        s.down(1, 90, 40);
        s.wait(300);
        s.down(2, 50, 104);
        s.wait(80);
        s.click = true;
        s.step();
        CHECK(s.buttons == 1, "большой палец снизу: кнопки %d", s.buttons);
        s.click = false;
        s.step();
        s.liftAll();
        s.wait(50);
        s.pad.cfg.right = 2;
        s.down(1, 140, 100);
        s.wait(300);
        s.click = true;
        s.step();
        CHECK(s.buttons == 2, "угол: кнопки %d", s.buttons);
        s.click = false;
        s.step();
        s.liftAll();
        s.reset();
        s.down(1, 140, 100);
        s.wait(50);
        s.liftAll();
        s.wait(100);
        CHECK(s.presses == "2 0", "касание в углу: %s", s.presses.c_str());
    }
    printf("прокрутка двумя пальцами: ось держится, пальцы сняли по одному — курсор стоит\n");
    {
        Sim s;
        s.down(1, 70, 30), s.down(2, 92, 26);
        s.step();
        s.glide(1.5, 30, 300, {1, 2});
        CHECK(s.sy > 100 && s.sx == 0, "прокрутка %.1f, вбок %.1f", s.sy, s.sx);
        CHECK(s.moves == 0, "курсор сдвинулся при прокрутке");
        s.up(2);
        s.step();
        s.glide(0.6, 0.4, 44, {1});
        s.liftAll();
        s.wait(100);
        CHECK(s.scrollEnds == 1, "концов прокрутки %d", s.scrollEnds);
        CHECK(s.moves == 0 && s.presses.empty(), "после прокрутки: ход %.2f, кнопки %s", s.path, s.presses.c_str());
        s.reset();
        s.pad.cfg.natural = false;
        s.down(1, 70, 30), s.down(2, 92, 26);
        s.step();
        s.glide(0, 30, 300, {1, 2});
        CHECK(s.sy < -100, "обратное направление: %.1f", s.sy);
    }
    printf("прокрутка наискось — свободная; по горизонтали — без вертикали\n");
    {
        Sim s;
        s.down(1, 60, 30), s.down(2, 82, 26);
        s.step();
        s.glide(20, 20, 300, {1, 2});
        CHECK(s.sx > 50 && s.sy > 50, "наискось: %.1f, %.1f", s.sx, s.sy);
        s.liftAll();
        s.reset();
        s.down(1, 60, 60), s.down(2, 82, 56);
        s.step();
        s.glide(30, 2, 300, {1, 2});
        CHECK(s.sx > 100 && s.sy == 0, "по горизонтали: %.1f, %.1f", s.sx, s.sy);
    }
    printf("лежащий большой палец: курсор ведёт другой палец, прокрутки нет, касание — щелчок\n");
    {
        Sim s;
        s.down(1, 40, 105);
        s.wait(500);
        s.down(2, 90, 40);
        s.step();
        s.glide(30, 0, 300, {2});
        CHECK(s.mx > 100 && s.scrolls == 0, "ход %.1f, прокрутка %d", s.mx, s.scrolls);
        s.up(2);
        s.step();
        s.wait(200);
        s.reset();
        s.down(2, 90, 40);
        s.wait(55);
        s.up(2);
        s.step();
        s.wait(100);
        CHECK(s.presses == "1 0", "касание при лежащем пальце: %s", s.presses.c_str());
        // легли вместе, ведут верхним — нижний лежит
        Sim b;
        b.down(1, 45, 100), b.down(2, 90, 45);
        b.step();
        b.glide(25, -5, 250, {2});
        CHECK(b.mx > 60 && b.scrolls == 0, "легли вместе: ход %.1f, прокрутка %d", b.mx, b.scrolls);
    }
    printf("двое ведут, большой палец лёг внизу вместе с ними — прокрутка, а не перетаскивание\n");
    {
        Sim s;
        s.down(1, 60, 40), s.down(2, 82, 36), s.down(3, 40, 104);
        s.step();
        s.glide(0, 14, 300, {1, 2});
        s.liftAll();
        s.wait(900);
        CHECK(s.presses.empty() && s.scrolls > 0, "кнопки %s, прокрутка %d", s.presses.c_str(), s.scrolls);
    }
    printf("три пальца — перетаскивание: сняли — держит, вернули — тащит дальше, срок вышел — отпустил\n");
    {
        Sim s;
        s.down(1, 50, 60), s.down(2, 70, 55), s.down(3, 90, 58);
        s.step();
        s.glide(20, 0, 250, {1, 2, 3});
        CHECK(s.presses == "1" && s.mx > 60, "кнопки %s, ход %.1f", s.presses.c_str(), s.mx);
        s.liftAll();
        s.wait(400);
        CHECK(s.buttons == 1, "через 0,4 с после снятия кнопки %d", s.buttons);
        const double was = s.mx;
        s.down(1, 50, 60);            // пальцы ложатся не разом
        s.step();
        s.down(2, 70, 55), s.down(3, 90, 58);
        s.step();
        s.glide(20, 0, 250, {1, 2, 3});
        CHECK(s.presses == "1" && s.mx > was + 60, "после возврата: кнопки %s, ход %.1f", s.presses.c_str(), s.mx - was);
        s.liftAll();
        s.wait(600);
        CHECK(s.buttons == 1, "через 0,6 с кнопки %d", s.buttons);
        s.wait(200);
        CHECK(s.presses == "1 0", "через 0,8 с кнопки %s", s.presses.c_str());
    }
    printf("кнопка держится: касание отпускает и щёлкает; один палец повели — отпускает, курсор едет\n");
    {
        Sim s;
        s.down(1, 50, 60), s.down(2, 70, 55), s.down(3, 90, 58);
        s.step();
        s.glide(20, 0, 250, {1, 2, 3});
        s.liftAll();
        s.wait(200);
        s.down(1, 80, 60);
        s.wait(55);
        s.liftAll();
        s.wait(100);
        CHECK(s.presses == "1 0 1 0", "касание: %s", s.presses.c_str());
        s.reset();
        s.down(1, 50, 60), s.down(2, 70, 55), s.down(3, 90, 58);
        s.step();
        s.glide(20, 0, 250, {1, 2, 3});
        s.liftAll();
        s.wait(200);
        s.reset();
        s.down(1, 80, 60);
        s.step();
        s.glide(10, 0, 150, {1});
        CHECK(s.presses == "0" && s.mx > 15, "один палец: кнопки %s, ход %.1f", s.presses.c_str(), s.mx);
    }
    printf("три пальца, перетаскивание не тремя — смахивание тремя; четыре — смахивание всегда\n");
    {
        Sim s;
        s.down(1, 40, 60), s.down(2, 60, 55), s.down(3, 80, 55), s.down(4, 100, 60);
        s.step();
        s.glide(-14, 1, 120, {1, 2, 3, 4});
        s.up(4);
        s.step();
        s.glide(-3, 0, 33, {1, 2, 3});
        s.liftAll();
        s.wait(900);
        CHECK(s.swipes == 1 && s.lastSwipe == Trackpad::Left && s.swipeFingers == 4, "смахиваний %d, куда %d, пальцев %d", s.swipes, s.lastSwipe, s.swipeFingers);
        CHECK(s.presses.empty() && s.moves == 0, "кнопки %s, ход %d", s.presses.c_str(), s.moves);
        s.reset();
        s.down(1, 40, 80), s.down(2, 60, 75), s.down(3, 80, 75), s.down(4, 100, 80);
        s.step();
        s.glide(1, -12, 110, {1, 2, 3, 4});
        s.liftAll();
        CHECK(s.swipes == 1 && s.lastSwipe == Trackpad::Up, "вверх: %d, %d", s.swipes, s.lastSwipe);
        s.reset();
        s.pad.cfg.drag = 0;
        s.down(1, 40, 40), s.down(2, 60, 35), s.down(3, 80, 38);
        s.step();
        s.glide(0, 12, 110, {1, 2, 3});
        s.liftAll();
        s.wait(900);
        CHECK(s.swipes == 1 && s.lastSwipe == Trackpad::Down && s.swipeFingers == 3 && s.presses.empty(), "тремя вниз: %d, %d, кнопки %s", s.swipes, s.lastSwipe, s.presses.c_str());
    }
    printf("четыре пальца ложатся по одному на ходу — перетаскивание не начинается\n");
    {
        Sim s;
        s.down(1, 40, 60);
        s.step();
        s.down(2, 60, 55);
        s.glide(2, 0, 11, {1, 2});
        s.down(3, 80, 55);
        s.glide(4, 0, 22, {1, 2, 3});
        s.down(4, 100, 60);
        s.glide(14, 0, 110, {1, 2, 3, 4});
        s.liftAll();
        s.wait(900);
        CHECK(s.presses.empty() && s.swipes == 1, "кнопки %s, смахиваний %d", s.presses.c_str(), s.swipes);
    }
    printf("щипок\n");
    {
        Sim s;
        s.down(1, 70, 60), s.down(2, 90, 55);
        s.step();
        for (int i = 0; i < 30; ++i) {
            s.fingers[1].x -= 0.6, s.fingers[1].y += 0.3, s.fingers[2].x += 0.6, s.fingers[2].y -= 0.3;
            s.step();
        }
        CHECK(s.zoom >= 3 && s.scrolls == 0, "шагов %d, прокрутка %d", s.zoom, s.scrolls);
        for (int i = 0; i < 30; ++i) {
            s.fingers[1].x += 0.6, s.fingers[1].y -= 0.3, s.fingers[2].x -= 0.6, s.fingers[2].y += 0.3;
            s.step();
        }
        CHECK(std::abs(s.zoom) <= 1, "обратно: %d", s.zoom);   // шаг — с запасом в обе стороны
    }
    printf("печать: касание сразу после клавиши не считается; через секунду — считается\n");
    {
        Sim s;
        s.pad.typed(s.t);
        s.pad.typed(s.t + 80000);
        s.t += 150000;
        s.down(1, 80, 60);
        s.step();
        s.glide(20, 0, 250, {1});
        s.liftAll();
        CHECK(s.moves == 0 && s.presses.empty(), "ход %d, кнопки %s", s.moves, s.presses.c_str());
        s.wait(1000);
        s.down(1, 80, 60);
        s.step();
        s.glide(20, 0, 250, {1});
        CHECK(s.mx > 60, "ход %.1f", s.mx);
    }
    printf("ладонь (широкое пятно) не считается\n");
    {
        Sim s;
        s.down(1, 20, 90, 960);
        s.step();
        s.glide(10, -10, 200, {1});
        s.liftAll();
        s.wait(100);
        CHECK(s.moves == 0 && s.presses.empty(), "ход %d, кнопки %s", s.moves, s.presses.c_str());
    }
    printf("двойное касание — перетаскивание; два касания подряд — двойной щелчок\n");
    {
        Sim s;
        s.pad.cfg.drag = 2;
        s.down(1, 80, 60);
        s.wait(55);
        s.liftAll();
        s.wait(90);
        s.down(1, 80, 60);
        s.step();
        s.glide(15, 0, 250, {1});
        CHECK(s.presses == "1" && s.mx > 30, "тащим: кнопки %s, ход %.1f", s.presses.c_str(), s.mx);
        s.liftAll();
        s.wait(50);
        CHECK(s.presses == "1 0", "отпустили: %s", s.presses.c_str());
        s.reset();
        s.down(1, 80, 60);
        s.wait(55);
        s.liftAll();
        s.wait(90);
        s.down(1, 80, 60);
        s.wait(55);
        s.liftAll();
        s.wait(400);
        CHECK(s.presses == "1 0 1 0", "двойной щелчок: %s", s.presses.c_str());
        s.reset();
        s.down(1, 80, 60);
        s.wait(55);
        s.liftAll();
        s.wait(400);
        CHECK(s.presses == "1 0", "одно касание: %s", s.presses.c_str());
    }
    printf("скачок на 30 мм за отчёт — не ход\n");
    {
        Sim s;
        s.down(1, 60, 60);
        s.wait(300);
        s.glide(5, 0, 110, {1});
        const double was = s.mx;
        s.fingers[1].x += 30;
        s.step();
        CHECK(std::fabs(s.mx - was) < 0.01, "скачок дал %.1f", s.mx - was);
    }
}

void gainTable()
{
    printf("палец, мм/с   указатель, точек/мм (30 %% / 50 %% / 70 %%)   без ускорения   сильное   прокрутка 50 %%\n");
    for (double v : {2.0, 5.0, 10.0, 20.0, 40.0, 80.0, 150.0, 250.0, 400.0, 600.0})
        printf("%8.0f      %6.1f %6.1f %6.1f                         %6.1f        %6.1f     %6.1f\n", v, Trackpad::pointerGain(v, 30, 1),
               Trackpad::pointerGain(v, 50, 1), Trackpad::pointerGain(v, 70, 1), Trackpad::pointerGain(v, 50, 0),
               Trackpad::pointerGain(v, 50, 2), Trackpad::scrollGain(v, 50));
}

int replay(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return 1;
    }
    Trackpad pad;
    static const char *modes[] = {"покой", "курсор", "два?", "прокрутка", "щипок", "три?", "тащит", "держит", "смах?", "конец"};
    double mx = 0, my = 0, sx = 0, sy = 0;
    int64_t now = 0;
    int moves = 0;
    auto flush = [&] {
        if (moves)
            printf("%10.3f   ход курсора %+.1f, %+.1f точек за %d отчётов\n", now / 1e6, mx, my, moves);
        mx = my = 0, moves = 0;
    };
    pad.onMove = [&](double dx, double dy, int64_t) { mx += dx, my += dy, ++moves; };
    pad.onButtons = [&](int b) { flush(), printf("%10.3f   КНОПКИ %d\n", now / 1e6, b); };
    pad.onScroll = [&](double dx, double dy, int64_t, bool fingers) {
        sx += dx, sy += dy;
        if (!fingers)
            printf("%10.3f   прокрутка %+.1f, %+.1f точек — пальцы сняты\n", now / 1e6, sx, sy), sx = sy = 0;
    };
    pad.onSwipe = [&](Trackpad::Swipe d, int n) { flush(), printf("%10.3f   СМАХИВАНИЕ %s, пальцев %d\n", now / 1e6, (const char *[]){"влево", "вправо", "вверх", "вниз"}[d], n); };
    pad.onZoom = [&](int s) { printf("%10.3f   масштаб %+d\n", now / 1e6, s); };
    char line[4096];
    int64_t first = -1, last = 0;
    Trackpad::Mode mode = Trackpad::Mode::Idle;
    while (fgets(line, sizeof line, f)) {
        long long t;
        int click, n, used = 0;
        if (sscanf(line, "%lld %d %d%n", &t, &click, &n, &used) < 3)
            continue;
        if (first < 0)
            first = t;
        t -= first;
        for (int64_t k = last + 5000; k < t; k += 5000)   // сроки — как в приставке, каждые 5 мс
            now = k, pad.tick(k);
        std::vector<Trackpad::Touch> v(n);
        const char *p = line + used;
        for (int i = 0; i < n; ++i) {
            int u = 0;
            sscanf(p, "%d %lf %lf %d %d %d%n", &v[i].id, &v[i].x, &v[i].y, &v[i].major, &v[i].minor, &v[i].pressure, &u);
            p += u;
        }
        now = last = t;
        pad.tick(t);
        pad.frame(t, v, click);
        if (pad.mode() != mode) {
            flush();
            mode = pad.mode();
            printf("%10.3f   %s (пальцев %d)\n", t / 1e6, modes[int(mode)], pad.fingers());
        }
    }
    flush();
    fclose(f);
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "gain")) {
        gainTable();
        return 0;
    }
    if (argc > 2 && !strcmp(argv[1], "replay"))
        return replay(argv[2]);
    cases();
    printf(failed ? "\nне прошло: %d\n" : "\nвсё прошло\n", failed);
    return failed ? 1 : 0;
}
