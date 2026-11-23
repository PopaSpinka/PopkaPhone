// НаомиФон — приставка на Raspberry Pi 5 (docs/PI.md): рабочий стол телефона во весь монитор, клавиатура и мышь Pi
// управляют телефоном. Без рабочего стола Linux: экран — KMS, видео — аппаратный HEVC, ввод — evdev.
//
//   sudo naomi-dock          (tools/pi run — то же службой)
//   NAOMI_DOCK_HOST=адрес    телефон не по кабелю (по умолчанию 10.77.0.1 — сеть по кабелю USB)
//   NAOMI_DOCK_KEYS=папка    ключ naomi_stream и known_hosts_phone (по умолчанию /home/naomi/.ssh)
//   NAOMI_DOCK_TRACE=файл    время каждого кадра (CSV): захват, сжат, пришёл, распакован, на экране, сдвиг часов,
//                            обновление монитора, байт, опорный
//   NAOMI_DOCK_ATRACE=файл   каждый кусок звука (CSV): первый отсчёт (часы телефона), пришёл, сдвиг часов, звучать, запас
//                            мс, путь (c — кабель, d — напрямую, w — Wi-Fi), копия
//   NAOMI_DOCK_AUDIODUMP=файл  звук, отданный устройству (float стерео 48 кГц, без заголовка): разбор искажений
//   NAOMI_DOCK_DUMP=файл     принятое видео как есть (HEVC Annex B): снимки стола, разбор качества (ffmpeg)
//   NAOMI_DOCK_PTRACE=файл   мышь (CSV): m — отчёт мыши как есть (отсчёты, ускорение), r — он же на экране (положение),
//                            s — что ушло телефону в такт, c — курсор к обновлению монитора, w — шаг колеса,
//                            k — стол сменил форму курсора (value — stream::CursorShape)
//   NAOMI_DOCK_AUDIO=устройство  звук стола — в это устройство ALSA (по умолчанию HDMI монитора: default:CARD=vc4hdmi0
//                            у HDMI-A-1, vc4hdmi1 у HDMI-A-2); «-» — без звука (телефон звук не шлёт)
//
// Размер стола, частоту монитора и экран без дока задают настройки стола телефона («Экран», сообщение Screen), Pi их
// помнит (/var/lib/naomi-dock/screen.json) — с ними и включается. Для разведки — свои значения поверх них:
//   NAOMI_DOCK_DESK=ШxВ@М    стол — Ш×В точек, М пикселей кадра в точке; кадр — точки × М, не больше 2560×1600
//   NAOMI_DOCK_HZ=Гц         частота монитора (ближайшая из его режимов; «max» — самая высокая)
//   NAOMI_DOCK_SLEEP=сек     без дока и без клавиш столько — монитор спит или приглушается (0 — никогда)
//   NAOMI_DOCK_CURSORS=файл  все формы курсора (cursor.h) для монитора 1440 строк — одной картинкой PAM (RGBA) и выход:
//                            посмотреть набор без монитора
//   NAOMI_DOCK_TABLET=0      без планшета (tablet.h, docs/TABLET.md): порт tablet::kPort не открывать
//   NAOMI_DOCK_TV=0          без телевизора (docs/TV.md): порты tablet::kTvPort и kTvAgentPort не открывать
//
// Такт задаёт монитор Pi (docs/PI.md, «Плавность»): перед каждым его обновлением телефону уходят положение мыши
// (досчитанное к этому моменту, motion.h), шаг плавной прокрутки и Tick — стол рисует кадр по нему. Момент такта —
// раньше обновления на столько, сколько кадр идёт от такта до распакованного (замер, 98% кадров) + запас: кадр
// приходит как раз к своему обновлению, без очереди и без дрейфа частот телефона и монитора.
// Трансляцию выключили на телефоне (плитка «трансляция», Switch 0) — пути закрыты, остаётся один тихий (пинг раз в
// kParkedPingS), ввод телефону не идёт; включили (Switch 1) — подключиться сразу заново.
// Без дока (docs/PI.md, «Без дока», lock.h) — часы на обоях стола: выход из дока — панель и окна стола уходят, часы
// проявляются в те же обновления монитора (метки Away от телефона); вход — сеанс начинается с одних обоев, Pi говорит
// Reveal, стол возвращается, часы уходят. Телефон пропал сам (нет живого пути kLostUs) — обои поверх застывшего стола
// сами. Без дока клавиша или щелчок мыши будят док (Wake), движение мыши — нет; без дела NAOMI_DOCK_SLEEP — монитор спит
// или (настройки «Без дока») плавно приглушается по DDC/CI (brightness.h) — проснулся, вернулся стол: прежняя яркость;
// «Тест» в тех же настройках (IdleTest) — то же через kIdleTestUs после ухода стола.
// Курсор рисует Pi (cursor.h, набор «Мягкая»), форму называет стол (Cursor): края окон, текст, ссылка, «занято».
// Планшет (tablet.h): ему — те же кадры и курсор, его касания — мышь стола, как от мыши Pi. Телевизор (docs/TV.md) —
// то же приложение и тот же протокол на своём порту; не на связи — приставка включает его сама (ScreenWake от
// телефона: Wake-on-LAN, потом Launch дежурному приложения на ТВ).
// Экраны (docs/SCREENS.md): монитор («pi»), планшет («tablet») и телевизор («tv») — экраны стола, телефон говорит,
// какие из них показывают стол и чей экран главный (Show). Монитор не показывает — на нём заставка без дока, кадры
// идут только планшету и телевизору; главный планшет или телевизор — форма стола его. Клавиатура и мышь приставки управляют столом, на чьём бы экране он ни
// был (и на чужом — окне Mac: тогда сеанс без кадров, один ввод); стол не показан нигде — клавиша или щелчок будят его.
// Связь — link.h, распаковка — decoder.h, экран — display.h, ввод — input.h, звук — audio_out.h, без дока — lock.h,
// планшет и телевизор — tablet.h, wol.h; протокол — stream_protocol.h.
#include "audio_out.h"
#include "brightness.h"
#include "cursor.h"
#include "decoder.h"
#include "display.h"
#include "input.h"
#include "link.h"
#include "lock.h"
#include "motion.h"
#include "tablet.h"
#include "wol.h"

#include "stream_protocol.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/log.h>
}

#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <arpa/inet.h>
#include <deque>
#include <ifaddrs.h>
#include <map>
#include <memory>
#include <tuple>
#include <net/if.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <random>
#include <string>
#include <vector>

namespace {

volatile sig_atomic_t g_quit = 0;

constexpr int64_t kLostUs = 2000000;          // ни один путь не жив столько — телефон пропал: обои и часы
constexpr int64_t kRestAfterUs = 3000000;     // стол стоит столько — запасной путь Wi-Fi пингуется редко
constexpr int64_t kRestPingUs = 1000000;      // так редко
constexpr int64_t kRestAliveUs = 2500000;     // и жив, если данные были за столько
constexpr int64_t kIdleTestUs = 1500000;     // «Тест» в настройках «Без дока»: без дела — столько после ухода стола
constexpr int64_t kWakeSettleUs = 2500000;    // монитор проснулся — столько показывает сигнал, пока не увидит его сам
constexpr int64_t kRevealWaitUs = 1000000;    // сказали Reveal, а стол не возвращается (старый телефон) — часы прочь сами

std::string env(const char *name, const char *fallback)
{
    const char *v = getenv(name);
    return v && *v ? v : fallback;
}

// значения из Hello (JSON телефона, плоский) — без разборщика
std::string jsonStr(const std::string &j, const char *key)
{
    const std::string k = std::string("\"") + key + "\":\"";
    const size_t at = j.find(k);
    if (at == std::string::npos)
        return {};
    const size_t from = at + k.size(), to = j.find('"', from);
    return j.substr(from, to - from);
}

long long jsonInt(const std::string &j, const char *key, long long fallback = 0)
{
    const std::string k = std::string("\"") + key + "\":";
    const size_t at = j.find(k);
    return at == std::string::npos ? fallback : atoll(j.c_str() + at + k.size());
}

std::string readFile(const char *path)
{
    std::string s;
    if (FILE *f = fopen(path, "r")) {
        char b[512];
        for (size_t n; (n = fread(b, 1, sizeof b, f)) > 0;)
            s.append(b, n);
        fclose(f);
    }
    return s;
}

void writeFile(const char *path, const std::string &s)   // целиком или никак: рядом, потом переименование
{
    mkdir("/var/lib/naomi-dock", 0755);
    const std::string tmp = std::string(path) + ".new";
    if (FILE *f = fopen(tmp.c_str(), "w")) {
        const bool ok = fwrite(s.data(), 1, s.size(), f) == s.size();
        if (fclose(f) == 0 && ok)
            rename(tmp.c_str(), path);
    }
}

// Настройки экрана со стола телефона (сообщение Screen, stream_protocol.h). Без них — как было до настроек: стол 1600
// точек в ширину (выбор пользователя 15 ноября 2026), кадр — 15/16 ширины монитора (2400 у 2560: масштаб 1,5), 60 Гц,
// часы на обоях слева сверху (44 и 34 точки стола 1600 × 900), сон монитора через 30 минут (выбор 17 ноября 2026),
// движение мыши его не будит; приглушение вместо сна — до 10 % яркости
struct ScreenCfg {
    int width = 1600, sharp = 2, hz = 60, sleepS = 1800;
    int clockX = 275, clockY = 378;   // левый верх часов, 1/10000 ширины и высоты монитора
    bool clock = true, wakeMove = false;
    bool dim = false;                 // без дела монитор не спит, а приглушается
    int dimLevel = 10;                // до стольких % наибольшей яркости монитора
};
constexpr double kSharp[4] = {0.75, 0.84375, 0.9375, 1.0};   // кадр — такая доля ширины монитора

ScreenCfg parseScreen(const std::string &j)
{
    ScreenCfg c;
    c.width = int(std::clamp(jsonInt(j, "width", c.width), 1024LL, 2048LL)) / 4 * 4;
    c.sharp = int(std::clamp(jsonInt(j, "sharp", c.sharp), 0LL, 3LL));
    c.hz = int(std::clamp(jsonInt(j, "hz", c.hz), 24LL, 240LL));
    c.sleepS = int(std::clamp(jsonInt(j, "sleep", c.sleepS), 0LL, 86400LL));
    c.clock = jsonInt(j, "clock", 1) != 0;
    c.clockX = int(std::clamp(jsonInt(j, "clockX", c.clockX), 0LL, 10000LL));
    c.clockY = int(std::clamp(jsonInt(j, "clockY", c.clockY), 0LL, 10000LL));
    c.wakeMove = jsonInt(j, "wakeMove") != 0;
    c.dim = jsonInt(j, "dim") != 0;
    c.dimLevel = int(std::clamp(jsonInt(j, "dimLevel", c.dimLevel), 0LL, 100LL));
    return c;
}

template <typename T> T get(const uint8_t *p)
{
    T v;
    memcpy(&v, p, sizeof v);
    return v;
}

// Кабель вставлен: адрес телефона — в подсети поднятого интерфейса Pi (usb0 появляется с телефоном на кабеле и
// пропадает, когда кабель вынут). Нет — ssh не запускать: ушёл бы через роутер («No route to host» раз в 1,5 с).
// Имя вместо адреса — не судить (true).
bool onLink(const std::string &host)
{
    in_addr a{};
    if (inet_pton(AF_INET, host.c_str(), &a) != 1)
        return true;
    ifaddrs *list = nullptr;
    if (getifaddrs(&list) != 0)
        return true;
    bool on = false;
    for (ifaddrs *i = list; i && !on; i = i->ifa_next) {
        if (!i->ifa_addr || !i->ifa_netmask || i->ifa_addr->sa_family != AF_INET || (i->ifa_flags & IFF_LOOPBACK)
            || !(i->ifa_flags & IFF_UP) || !(i->ifa_flags & IFF_RUNNING))
            continue;
        const uint32_t ip = reinterpret_cast<sockaddr_in *>(i->ifa_addr)->sin_addr.s_addr;
        const uint32_t mask = reinterpret_cast<sockaddr_in *>(i->ifa_netmask)->sin_addr.s_addr;
        on = (ip & mask) == (a.s_addr & mask);
    }
    freeifaddrs(list);
    return on;
}

double pct(std::vector<double> v, double q)
{
    if (v.empty())
        return NAN;
    std::sort(v.begin(), v.end());
    return v[std::min(v.size() - 1, size_t(q * double(v.size() - 1) + 0.5))];
}

} // namespace

int main()
{
    if (const char *path = getenv("NAOMI_DOCK_CURSORS")) {   // формы курсора — картинкой, без экрана
        const CursorArt art(1440);
        const int n = CursorArt::kSize, count = stream::CursorHidden;
        std::vector<uint8_t> rgba(size_t(n) * n * count * 4);
        for (int s = 0; s < count; ++s) {
            const CursorArt::Image img = art.render(s, 0.3);
            for (int y = 0; y < n; ++y)
                for (int x = 0; x < n; ++x) {
                    const uint32_t c = img.px[size_t(y) * n + x], a = c >> 24;
                    uint8_t *o = &rgba[(size_t(y) * n * count + size_t(s) * n + x) * 4];
                    for (int k = 0; k < 3; ++k)   // из умноженной альфы
                        o[k] = uint8_t(a ? std::min<uint32_t>(255, ((c >> (16 - 8 * k)) & 255) * 255 / a) : 0);
                    o[3] = uint8_t(a);
                    if (x == img.hotX && y == img.hotY)   // горячая точка — красная
                        o[0] = 255, o[1] = o[2] = 0, o[3] = 255;
                }
        }
        FILE *f = fopen(path, "wb");
        if (!f)
            return 1;
        fprintf(f, "P7\nWIDTH %d\nHEIGHT %d\nDEPTH 4\nMAXVAL 255\nTUPLTYPE RGB_ALPHA\nENDHDR\n", n * count, n);
        fwrite(rgba.data(), 1, rgba.size(), f);
        fclose(f);
        return 0;
    }
    signal(SIGINT, [](int) { g_quit = 1; });
    signal(SIGTERM, [](int) { g_quit = 1; });
    signal(SIGPIPE, SIG_IGN);
    setvbuf(stderr, nullptr, _IOLBF, 0);
    av_log_set_level(AV_LOG_ERROR);

    // настройки экрана — запомненные (телефон пришлёт свои, когда подключится); NAOMI_DOCK_* — поверх них
    const char *screenFile = "/var/lib/naomi-dock/screen.json";
    std::string screenJson = readFile(screenFile);
    ScreenCfg screen = parseScreen(screenJson);
    auto envSet = [](const char *name) -> const char * {
        const char *v = getenv(name);
        return v && *v ? v : nullptr;
    };
    const char *deskEnv = envSet("NAOMI_DOCK_DESK"), *hzEnv = envSet("NAOMI_DOCK_HZ"), *sleepEnv = envSet("NAOMI_DOCK_SLEEP");

    Display display;
    if (!display.open(hzEnv ? (std::string(hzEnv) == "max" ? 0 : atof(hzEnv)) : screen.hz))
        return 1;
    Brightness brightness;   // яркость монитора по DDC/CI — приглушение без дока (brightness.h)
    brightness.open(display.name());
    DecodeThread decoder;
    Input input;
    // Удалённые экраны приставки (tablet.h): планшет (docs/TABLET.md; ключ — tools/tablet pair) и телевизор
    // (docs/TV.md; tools/tv pair) — то же приложение и тот же протокол, у каждого свой порт и свой ключ. Что приставка
    // помнит об экране: показывает ли он стол (live — телефон сказал в Show); имя и размер (Screen) — пока не назвал, в
    // «Экранах» его нет; какие обои заставки у него есть; его кнопки (палец — левая, касание двумя — правая) и
    // прокрутка двумя пальцами; курсор Pi на нём виден, пока ведут мышью или трекпадом Pi, касание его прячет; что ему
    // сказано (Hello), когда слали курсор и просили опорный кадр. Часы экрана → часы Pi (clock): наименьшая разность
    // «пришло − время касания» за последние 4 с — путь по Wi-Fi без очереди; касания встают на ось времени Pi ровно,
    // как отчёты мыши (motion.h), без разброса Wi-Fi
    struct Remote {
        const char *id, *label, *kind;   // экран в «Экранах» («tablet», «tv»), имя для журнала, вид (Screens)
        bool light, wake;                // яркость ставится со стола; не на связи — приставка включает его сама
        Tablet link;
        bool live = true;                // до первого Show (и у старого телефона) — вместе с монитором
        std::string name, wall, said;
        int w = 0, h = 0, artH = 0, buttons = 0;
        std::unique_ptr<CursorArt> art;  // курсор — тот же набор под высоту картинки на его экране (View)
        bool cursor = true, scroll = false, keyPending = false;
        bool told = false;               // подключённому экрану стол показывали (Hello со столом) — снимут: Leave
        int64_t cursorAt = 0, keyAt = 0;
        std::deque<std::pair<int64_t, int64_t>> clock;
        Remote(const char *id, const char *label, const char *kind, bool light, bool wake)
            : id(id), label(label), kind(kind), light(light), wake(wake)
        {
        }
    };
    Remote tabletScreen("tablet", "планшет", "tablet", true, false), tvScreen("tv", "ТВ", "tv", false, true);
    constexpr size_t kRemotes = 2;
    Remote *const remotes[kRemotes] = {&tabletScreen, &tvScreen};
    // дежурный телевизора — служба приложения на ТВ, на связи всегда, пока телевизор не спит: ей — Launch
    Tablet tvAgent;
    const std::string tabletDir = "/var/lib/naomi-dock/tablet", tvDir = "/var/lib/naomi-dock/tv";
    if (env("NAOMI_DOCK_TABLET", "1") != "0")
        tabletScreen.link.start(tabletDir, tablet::kPort);
    if (env("NAOMI_DOCK_TV", "1") != "0") {   // сертификат приставки — общий с планшетом, ключ — свой
        tvScreen.link.start(tvDir, tablet::kTvPort, "ТВ", tabletDir, "tools/tv pair");
        tvAgent.start(tvDir, tablet::kTvAgentPort, "ТВ, дежурный", tabletDir, "tools/tv pair");
    }
    auto remoteOf = [&](const std::string &id) -> Remote * {
        for (Remote *r : remotes)
            if (id == r->id)
                return r;
        return nullptr;
    };
    auto anyRemoteLive = [&] {
        return std::any_of(std::begin(remotes), std::end(remotes), [](const Remote *r) { return r->live; });
    };
    auto cursorsOn = [&] {   // мышь или трекпад Pi сдвинули — курсор Pi на удалённых экранах снова виден
        for (Remote *r : remotes)
            r->cursor = true;
    };

    const std::string host = env("NAOMI_DOCK_HOST", "10.77.0.1");
    const std::string keys = env("NAOMI_DOCK_KEYS", "/home/naomi/.ssh");
    // картинка — напрямую по кабелю, мимо SSH (docs/PI.md, «Картинка напрямую»); NAOMI_DOCK_DIRECT=0 — всё через SSH
    const bool direct = env("NAOMI_DOCK_DIRECT", "1") != "0";
    // звук стола — в HDMI монитора, на котором картинка (docs/PI.md, «Звук на мониторе»)
    const std::string audioDevice = env("NAOMI_DOCK_AUDIO", display.name() == "HDMI-A-2" ? "default:CARD=vc4hdmi1"
                                                                                       : "default:CARD=vc4hdmi0");
    std::unique_ptr<AudioOut> audio = audioDevice == "-" ? nullptr : std::make_unique<AudioOut>(audioDevice);
    // стол — под монитор: частота кадров — его (сменилась — Want снова); кодеру — частоты как для 120 к/с
    // (NAOMI_DOCK_ENCFPS — иначе): сжатие быстрее и ровнее (docs/PI.md, «Плавность»)
    const int encodeFpsMin = atoi(env("NAOMI_DOCK_ENCFPS", "120").c_str());
    int fps = 0;
    std::string want;
    auto makeWant = [&] {
        fps = int(std::lround(display.refreshHz()));
        want = "{\"codec\":\"hevc\",\"fps\":" + std::to_string(fps) + ",\"encodeFps\":" +
               std::to_string(std::max(fps, encodeFpsMin)) + (audio ? ",\"audio\":true" : "") + ",\"lock\":true}";
    };
    makeWant();
    // форма стола: ширина — точки из настроек, высота — по пропорциям монитора (кратные 4); масштаб — кадр (резкость:
    // доля ширины монитора, не больше 2560) на ширину, 1…2, кадр не выше 1600
    // Экраны (docs/SCREENS.md) — что телефон сказал в Show: стол показывает монитор (piLive), планшет и телевизор
    // (Remote::live), чужие экраны (otherLive); главный — mainScreen. До первого Show (и у старого телефона) — монитор
    // и удалённые экраны вместе
    bool piLive = true, otherLive = false;
    std::string mainScreen = "pi", showSaid;
    int deskW = 0, deskH = 0, scaleMilli = 0;
    auto deskFor = [&](const ScreenCfg &c) {   // главный — планшет или телевизор: стол его формы
        const Remote *m = remoteOf(mainScreen);
        const bool far = m && m->w > 0 && m->h > 0;
        const double fw = far ? m->w : display.width(), fh = far ? m->h : display.height();
        // Телевизор 4K: резкость считаем по ширине не больше монитора — кадр тот же, что у монитора (обе формы 16:9:
        // главный меняется между ними без смены размера стола — браузер не перезапускается), а распаковывать больше
        // телевизору незачем
        const double sw = far && m->wake ? std::min(fw, double(display.width())) : fw;
        deskW = c.width;
        deskH = std::clamp(int(std::lround(double(deskW) * fh / fw / 4)) * 4, 720, 1280);
        const double frame = std::min(2560.0, std::round(sw * kSharp[c.sharp]));
        scaleMilli = int(std::lround(std::min(std::clamp(frame / deskW, 1.0, 2.0), 1600.0 / deskH) * 1000));
    };
    deskFor(screen);
    if (deskEnv) {
        double s = 1.5;
        sscanf(deskEnv, "%dx%d@%lf", &deskW, &deskH, &s);
        scaleMilli = int(std::lround(s * 1000));
    }

    // курсор — набор «Мягкая» в натуральную величину монитора; форму скажет стол (Cursor), до того — стрелка
    const CursorArt cursorArt(display.height());
    int cursorShape = stream::CursorArrow;
    int64_t busyFrom = 0, busyNext = 0;   // «занято» крутится: с какого момента, когда следующая картинка
    // курсор удалённого экрана — тот же набор под высоту картинки на нём (View): картинкой, без умножения альфы
    auto remoteImage = [&](Remote &r, int64_t now) {
        if (!r.art || !r.link.connected())
            return;
        const CursorArt::Image img = r.art->render(cursorShape, (now - busyFrom) / 1e6);
        std::vector<uint32_t> argb(img.px.size());
        for (size_t i = 0; i < argb.size(); ++i) {
            const uint32_t c = img.px[i], a = c >> 24;
            uint32_t o = a << 24;
            for (int k = 0; k < 3 && a; ++k)
                o |= std::min<uint32_t>(255, ((c >> (8 * k)) & 255) * 255 / a) << (8 * k);
            argb[i] = o;
        }
        r.link.cursorImage(argb, CursorArt::kSize, img.hotX, img.hotY);
    };
    auto showCursor = [&](int shape, int64_t now) {
        if (shape > stream::CursorHidden)
            shape = stream::CursorArrow;
        if (shape != cursorShape && shape == stream::CursorBusy)
            busyFrom = now;
        cursorShape = shape;
        const CursorArt::Image img = cursorArt.render(shape, (now - busyFrom) / 1e6);
        display.setCursor(img.px.data(), img.hotX, img.hotY);
        for (Remote *r : remotes)
            remoteImage(*r, now);
        busyNext = now + 33000;
    };
    showCursor(stream::CursorArrow, monoUs());

    // Без дока (lock.h): с запуска — часы на обоях (последний чистый кадр стола с диска; нет — чёрный), курсора нет;
    // стол появится, когда телефон его покажет
    LockScreen lock;
    if (!lock.open(display, double(display.width()) / deskW))
        return 1;
    bool lockView = true;
    int64_t sleepUs = (sleepEnv ? atoll(sleepEnv) : screen.sleepS) * 1000000LL;
    bool wakeMove = screen.wakeMove;   // движение мыши будит монитор (без дока)
    lock.setClockShown(screen.clock);
    lock.setClockPos(screen.clockX / 10000.0, screen.clockY / 10000.0);
    lock.coverIn(0, 0);
    lock.clockIn(0);
    lock.cursorTo(false, 0, 0);
    fprintf(stderr, "просим у телефона: HEVC, %d к/с (кодеру — как для %d), стол %dx%d точек, масштаб %.3f%s\n", fps,
            std::max(fps, encodeFpsMin), deskW, deskH, scaleMilli / 1000.0, deskEnv ? " (NAOMI_DOCK_DESK)" : "");

    // Пути к телефону (docs/PI.md, «Кабель и Wi-Fi без шва»): кабель (NAOMI_DOCK_HOST, 10.77.0.1) и Wi-Fi — адрес
    // телефон называет сам (Address), он запоминается в /var/lib/naomi-dock/phone-wifi (NAOMI_DOCK_WIFI — свой адрес,
    // «-» — без Wi-Fi). Оба пути держатся сразу, в одном сеансе — один билет: кадры идут копиями по обоим (берётся
    // пришедший первым), ввод — по обоим с одним номером (телефон выполняет раз), основной (значок пути на столе) —
    // кабель, пока он жив, иначе Wi-Fi. Выдернули кабель — картинка уже идёт по Wi-Fi; вставили — снова кабель.
    struct Path {
        Link link;
        std::string host;
        const char *name;
        bool cable;
        int64_t retryAt = 0, nextPing = 0, directAskAt = 0;   // directAskAt — просить прямой (0 — не просить)
        bool resting = false;                                 // запасной путь в покое: пинг редкий (kRestPingUs)
        int64_t graceUntil = 0;                               // вышел из покоя — «жив» по редкому счёту до этого
        bool present = true;                                  // кабель вставлен (onLink; у Wi-Fi — всегда)
        int64_t presentAt = 0;                                // когда проверять снова
    };
    Path cablePath{{}, host, "кабель", true};
    Path wifiPath{{}, "", "Wi-Fi", false};
    Path *paths[2] = {&cablePath, &wifiPath};
    const char *wifiFile = "/var/lib/naomi-dock/phone-wifi";
    const std::string wifiEnv = env("NAOMI_DOCK_WIFI", "");
    const bool wifiFixed = !wifiEnv.empty();
    if (wifiEnv != "-") {
        if (wifiFixed) {
            wifiPath.host = wifiEnv;
        } else if (FILE *f = fopen(wifiFile, "r")) {
            char line[64] = {};
            if (fgets(line, sizeof line, f))
                wifiPath.host = std::string(line).substr(0, std::string(line).find_first_of("\r\n "));
            fclose(f);
        }
    }
    fprintf(stderr, "пути: кабель — %s, Wi-Fi — %s\n", host.c_str(),
            wifiEnv == "-" ? "выключен" : wifiPath.host.empty() ? "адрес скажет телефон" : wifiPath.host.c_str());
    std::random_device rd;
    uint64_t ticket = (uint64_t(rd()) << 32 | rd()) | 1;   // билет сеанса — на оба пути (телефон пропал — новый)
    uint32_t seq = 0;                                             // номер ввода — общий
    // Трансляция выключена на телефоне (Switch 0): один тихий путь, ждём Switch 1. switchTo — что телефон сказал
    // последним (−1 — ничего нового): выполняется в главном круге, а не посреди разбора сообщений пути
    bool off = false;
    int switchTo = -1;
    auto anyOpen = [&] { return cablePath.link.isOpen() || wifiPath.link.isOpen(); };
    bool streaming = false, waitingKey = true;   // идёт поток стола; кадры — с опорного
    // Стол показан не на мониторе, а на планшете, телевизоре или чужом экране: монитор — на заставке, но клавиатура и
    // мышь приставки столом управляют (inputOff — ввод телефону не идёт: стол не показан нигде или монитор ещё входит
    // в док)
    auto remote = [&] { return !off && streaming && !piLive && (anyRemoteLive() || otherLive); };
    auto inputOff = [&] { return lockView && !remote(); };
    bool guest = false;          // курсор сейчас ведёт чужой экран (CursorAt): своё положение телефону не слать
    bool relayEntering = false;  // стол начинается для планшета или ТВ (монитор на заставке): Reveal — после первого кадра ему
    bool quickBack = false;      // монитор входит в стол, что уже показан другим экранам: меток возврата не будет
    int64_t keyAskAt = 0;        // монитор снова показывает стол: опорный кадр, если новый поток не начнётся сам
    auto inputAll = [&](uint8_t type, const void *b = nullptr, size_t n = 0) {
        if (off)
            return;   // ни мыши, ни клавиш, ни тактов: телефон их не ждёт, а Wi-Fi его будился бы на каждое
        const uint32_t s = ++seq;
        for (Path *p : paths)
            if (p->link.isOpen())
                p->link.input(s, type, b, n);
    };
    auto sendDeskSize = [&] {   // форма стола и для какого экрана она: планшет или ТВ главный — его, иначе — монитора
        std::string size{char(deskH), char(deskH >> 8), char(deskW), char(deskW >> 8), char(scaleMilli), char(scaleMilli >> 8)};
        const Remote *m = remoteOf(mainScreen);
        size += m && m->w > 0 ? m->id : "pi";
        inputAll(stream::DeskSize, size.data(), size.size());
    };
    auto sendMonitor = [&] {   // какой монитор и что умеет — «Настройки» стола показывают его
        std::string rates;
        for (double r : display.rates()) {
            char b[16];
            snprintf(b, sizeof b, "%s%.2f", rates.empty() ? "" : ",", r);
            rates += b;
        }
        char hz[16];
        snprintf(hz, sizeof hz, "%.2f", display.refreshHz());
        const std::string j = "{\"name\":\"" + display.monitor() + "\",\"port\":\"" + display.name() + "\",\"w\":" +
                              std::to_string(display.width()) + ",\"h\":" + std::to_string(display.height()) +
                              ",\"hz\":" + hz + ",\"rates\":[" + rates + "],\"ddc\":" +
                              std::to_string(brightness.supported()) + "}";
        inputAll(stream::Monitor, j.data(), j.size());
    };
    // Экраны приставки для «Экранов» телефона (Screens, docs/SCREENS.md): монитор, планшет и телевизор — те, что
    // подключены и назвали свой экран; "input" — клавиатура и мышь приставки управляют столом, на чьём бы экране он ни
    // был; "wall" — какие обои заставки у нас уже лежат (для планшета и ТВ); "wake" у телевизора — не на связи его
    // включаем мы (ScreenWake). Вторым словом каждого пути, и тихого, и когда удалённый экран пришёл или ушёл
    const char *wallFile = "/var/lib/naomi-dock/wall.jpg", *wallHashFile = "/var/lib/naomi-dock/wall.hash";
    std::string wallHash;                 // 16 знаков; «none» — обоев нет
    std::vector<uint8_t> wallBody;        // как пришло от телефона: хэш (16 байт) + JPEG — так же уйдёт планшету
    if (FILE *f = fopen(wallHashFile, "r")) {
        char h[32] = {};
        if (fgets(h, sizeof h, f))
            wallHash = std::string(h).substr(0, std::string(h).find_first_of("\r\n "));
        fclose(f);
        wallBody.assign(16, 0);
        memcpy(wallBody.data(), wallHash.data(), std::min<size_t>(16, wallHash.size()));
        if (FILE *j = wallHash == "none" ? nullptr : fopen(wallFile, "rb")) {
            uint8_t buf[65536];
            for (size_t r; (r = fread(buf, 1, sizeof buf, j)) > 0;)
                wallBody.insert(wallBody.end(), buf, buf + r);
            fclose(j);
        }
        if (wallHash != "none" && wallBody.size() == 16)   // файла нет — обоев у нас нет
            wallHash.clear(), wallBody.clear();
    }
    auto clean = [](std::string name) {
        for (char &c : name)
            if (c == '"' || c == '\\' || static_cast<unsigned char>(c) < 0x20)
                c = ' ';
        return name;
    };
    auto screensJson = [&] {
        std::string j = "{\"viewer\":\"pi\",\"input\":1,\"wall\":\"" + wallHash + "\",\"screens\":[{\"id\":\"pi\",\"name\":\""
                        + clean(display.monitor()) + "\",\"kind\":\"monitor\",\"w\":" + std::to_string(display.width())
                        + ",\"h\":" + std::to_string(display.height()) + "}";
        for (const Remote *r : remotes)
            if (r->w > 0)
                j += std::string(",{\"id\":\"") + r->id + "\",\"name\":\"" + clean(r->name) + "\",\"kind\":\"" + r->kind
                     + "\"" + (r->light ? ",\"light\":1" : "") + (r->wake ? ",\"wake\":1" : "") + ",\"w\":"
                     + std::to_string(r->w) + ",\"h\":" + std::to_string(r->h) + "}";
        return j + "]}";
    };
    auto sendScreens = [&] {   // экраны сменились — по всем путям, как есть (и по тихому)
        const std::string j = screensJson();
        for (Path *p : paths)
            if (p->link.isOpen())
                p->link.send(stream::Screens, j.data(), j.size());
    };
    // заставка планшету и ТВ (tablet::Splash): часы — как у монитора (настройки стола «Без дока»), размер — по ширине
    // стола
    auto remoteSplash = [&] {
        for (Remote *r : remotes)
            r->link.splash("{\"clock\":" + std::to_string(screen.clock ? 1 : 0) + ",\"clockX\":"
                           + std::to_string(screen.clockX) + ",\"clockY\":" + std::to_string(screen.clockY)
                           + ",\"width\":" + std::to_string(screen.width) + "}");
    };
    remoteSplash();
    int64_t configAt = 0;
    auto sendConfig = [&] {   // кто смотрит, что хочет, форма стола (раньше выбора стола: один Hello нужного размера)
        configAt = monoUs();
        inputAll(stream::Viewer, "pi", 2);
        sendMonitor();
        const uint8_t ru = input.russian();
        inputAll(stream::Layout, &ru, 1);
        inputAll(stream::Want, want.data(), want.size());
        sendDeskSize();
        const uint8_t desk = 1;
        inputAll(stream::Mode, &desk, 1);
    };
    // Настройки экрана со стола (Screen): форма стола — телефону сразу (он ответит новым Hello; масштаб сменился —
    // перезапустит браузер); частота — в главном круге (смена режима — не посреди отдачи кадра); остальное — сразу
    double rateWant = 0;
    auto applyScreen = [&](const ScreenCfg &c) {
        if (!deskEnv) {
            const int w0 = deskW, h0 = deskH, m0 = scaleMilli;
            deskFor(c);
            if (deskW != w0 || deskH != h0 || scaleMilli != m0) {
                fprintf(stderr, "стол: %dx%d точек, масштаб %.3f — кадр %ldx%ld\n", deskW, deskH, scaleMilli / 1000.0,
                        std::lround(deskW * scaleMilli / 2000.0) * 2, std::lround(deskH * scaleMilli / 2000.0) * 2);
                lock.setPt(double(display.width()) / deskW);
                if (anyOpen())
                    sendDeskSize();
            }
        }
        if (!hzEnv)
            rateWant = c.hz;
        if (!sleepEnv)
            sleepUs = int64_t(c.sleepS) * 1000000;
        lock.setClockShown(c.clock);
        lock.setClockPos(c.clockX / 10000.0, c.clockY / 10000.0);
        wakeMove = c.wakeMove;
    };
    // путь жив: телефон по нему уже отвечал, и данные были за 0,4 с (пинг по каждому — 10 раз в секунду); запасной
    // путь в покое пингуется раз в секунду (ниже) — ему и только что вышедшему из покоя срок kRestAliveUs
    auto healthy = [](const Path &p, int64_t now) {
        const int64_t fresh = p.resting || now < p.graceUntil ? kRestAliveUs : 400000;
        return p.link.isOpen() && p.link.lastDataUs() > p.link.openedUs() && now - p.link.lastDataUs() < fresh;
    };
    Path *primary = nullptr;
    // Без дока: вход — первый кадр сеанса на экране → (покров растаял) → Reveal → стол возвращается (метка AwayBack);
    // выход — метки AwayLeave и AwayFinal; awayMarks — метки по захвату кадра (из Away, до его Video)
    std::map<int64_t, uint8_t> awayMarks;
    bool entering = false;                 // сеанс начался без дока: ждём его первый кадр на экране
    int64_t revealAt = 0, revealSent = 0;  // когда сказать Reveal; когда сказали (ждём AwayBack)
    int64_t lostFrom = 0;                  // с какого момента ни один путь не жив (0 — жив)
    int64_t lockIdleFrom = monoUs();       // без дока: с какого момента без клавиш (для сна монитора)
    int64_t wokeAt = 0, wakeSent = 0;      // монитор проснулся; Wake телефону ушёл
    bool dimmed = false;                   // без дела монитор приглушён (вместо сна, настройки «Без дока»)
    int64_t idleTestAt = 0;                // телефон: «Тест» (IdleTest) — без дела сразу после ухода стола
    int brightnessSeq = -1;                // номер последней просьбы Brightness
    int idleTestDim = -1;                  // …и не как в настройках, а приглушить до стольких % («луна»); −1 — как в них
    // проснуться: монитор спит — сигнал снова (стол — не раньше kWakeSettleUs: монитору поймать его); приглушён —
    // прежняя яркость. false — и так не спал
    auto wakeMonitor = [&](int64_t now) {
        idleTestAt = 0;
        if (dimmed) {
            dimmed = false;
            brightness.restore();
            return true;
        }
        if (display.asleep() && display.setAsleep(false)) {
            wokeAt = now;
            return true;
        }
        return false;
    };
    // Телефон пропал, а стол был на экране: обои поверх застывшего стола, часы, курсора нет; билет — новый: телефон
    // начнёт новый сеанс без панели и окон, и стол вернётся, как при входе
    auto lost = [&](int64_t now, const char *why) {
        fprintf(stderr, "без дока: %s — обои и часы\n", why);
        lockView = true;
        lockIdleFrom = now;
        lock.coverIn(now, 500000);
        lock.clockIn(now);
        lock.cursorTo(false, now, 140000);
        showCursor(stream::CursorArrow, now);   // новый сеанс скажет свою форму
        ticket = (uint64_t(rd()) << 32 | rd()) | 1;
    };

    FILE *dump = getenv("NAOMI_DOCK_DUMP") ? fopen(getenv("NAOMI_DOCK_DUMP"), "wb") : nullptr;
    FILE *ptrace = getenv("NAOMI_DOCK_PTRACE") ? fopen(getenv("NAOMI_DOCK_PTRACE"), "w") : nullptr;
    if (ptrace)
        fprintf(ptrace, "kind,t,x,y,gain,sent,value\n");   // у «s»: когда такт на самом деле ушёл (мкс), его значение;
                                                              // «w» — шаг колеса за такт (точки стола)
    FILE *atrace = getenv("NAOMI_DOCK_ATRACE") ? fopen(getenv("NAOMI_DOCK_ATRACE"), "w") : nullptr;
    if (atrace)
        fprintf(atrace, "first,arrive,offset,play,lead_ms,path,copy\n");
    FILE *trace = nullptr;
    if (const char *t = getenv("NAOMI_DOCK_TRACE")) {
        trace = fopen(t, "w");
        if (trace)
            fprintf(trace, "capture,encoded,recv,decoded,shown,offset,target,bytes,key\n");
    }

    // поток
    long long gen = 0;
    int frameW = 1920, frameH = 1080;
    int64_t lastCapture = 0;
    // часы телефона − часы Pi: по лучшему (быстрейшему) из последних пингов
    struct Pong { int64_t rtt, offset; };
    std::vector<Pong> pongs;
    int64_t offset = 0, rtt = 0;
    bool haveOffset = false;

    // мышь: курсор — на экране Pi (сразу), телефону — доля кадра, в такт
    double cx = display.width() / 2.0, cy = display.height() / 2.0;   // последнее положение от мыши
    Pointer pointer;
    Wheel wheel;
    WheelRate wheelRate;
    // настройки мыши со стола телефона (сообщение Mouse); до него — как было: 3 строки (100 точек), плавно за 130 мс
    struct { bool natural = false, smooth = true, wheelAccel = false; int lines = 3, softMs = 130; } wheelCfg;
    std::string mouseJson;   // последнее сообщение Mouse
    int buttons = 0, sentButtons = -1;
    float sentX = -1, sentY = -1;
    bool wasMoving = false;
    int64_t wheelEndAt = 0, lastInputUs = 0, lastFrameUs = 0;
    double wheelX = 0.5, wheelY = 0.5;
    // прокрутка трекпадом (docs/PI.md, «Трекпад»): пальцы на трекпаде — ход за такт целиком; сняли — инерция
    bool padScroll = false, padMomentum = false;
    std::deque<std::tuple<int64_t, double, double>> padHist;   // ход пальцев за последние ~80 мс: время, точки стола
    // настройки трекпада со стола телефона (сообщение Pad, «Настройки» → «Трекпад»; ключи — stream_protocol.h) —
    // запомненные: телефон пришлёт свои, когда подключится; не было ни разу — как трекпад на Mac пользователя
    double padMomentumS = 0.33;   // инерция: остаток тает с такой постоянной (0 — без инерции)
    const char *padFile = "/var/lib/naomi-dock/pad.json";
    std::string padJson = readFile(padFile);
    auto applyPad = [&](const std::string &j, bool show) {
        PadConfig c;
        Input::PadActions a;
        auto num = [&](const char *key, int fallback, int lo, int hi) { return int(std::clamp(jsonInt(j, key, fallback), (long long)lo, (long long)hi)); };
        c.speed = num("speed", c.speed, 0, 100);
        c.accel = num("accel", c.accel, 0, 2);
        c.tap = num("tap", c.tap, 0, 1);
        c.right = num("right", c.right, 0, 2);
        c.drag = num("drag", c.drag, 0, 2);
        c.holdMs = num("hold", c.holdMs, 0, 3000);
        c.natural = num("natural", c.natural, 0, 1);
        c.scrollSpeed = num("sspeed", c.scrollSpeed, 0, 100);
        c.pinch = num("pinch", c.pinch, 0, 1);
        c.jitter = num("jitter", c.jitter, 0, 2);
        c.palm = num("palm", c.palm, 0, 1);
        c.typing = num("typing", c.typing, 0, 1);
        c.pin = num("pin", c.pin, 0, 1);
        a.side = num("swSide", a.side, 0, 2);
        a.up = num("swUp", a.up, 0, 1);
        a.down = num("swDown", a.down, 0, 1);
        const int inertia = num("inertia", 1, 0, 2);
        padMomentumS = inertia == 0 ? 0 : inertia == 2 ? 0.6 : 0.33;
        input.setPad(c, a, show);
        fprintf(stderr, "трекпад: указатель %d %%, ускорение %d; касание %s, правый %d, перетаскивание %d (держит %d мс); "
                        "прокрутка %s, %d %%, инерция %d; щипок %s; дрожь %d, лежащий палец %s, пауза при печати %s, "
                        "замирать при щелчке %s; смахивания: вбок %d, вверх %d, вниз %d%s\n",
                c.speed, c.accel, c.tap ? "вкл" : "выкл", c.right, c.drag, c.holdMs, c.natural ? "за пальцами" : "как колесо",
                c.scrollSpeed, inertia, c.pinch ? "вкл" : "выкл", c.jitter, c.palm ? "не считать" : "считать",
                c.typing ? "вкл" : "выкл", c.pin ? "вкл" : "выкл", a.side, a.up, a.down, j.empty() ? " (как на Mac — настроек стола ещё не было)" : "");
    };
    applyPad(padJson, false);
    // время касания удалённого экрана — в часах Pi (Remote::clock)
    auto remoteTime = [&](Remote &r, int64_t t, int64_t recv) {
        r.clock.emplace_back(recv, recv - t);
        while (r.clock.size() > 1 && recv - r.clock.front().first > 4000000)
            r.clock.pop_front();
        int64_t d = r.clock.front().second;
        for (const auto &c : r.clock)
            d = std::min(d, c.second);
        return std::min(t + d, recv);
    };
    // Телевизор включить и открыть на нём приложение (ScreenWake «tv» от телефона, docs/TV.md): дежурный на связи — ему
    // Launch (экран включит и приложение откроет он, и поверх HDMI-входа); нет — телевизор спит, Wi-Fi у него выключен:
    // Wake-on-LAN раз в секунду по MAC, что запомнили, — дежурный выйдет на связь сам, тогда Launch. Приложение
    // подключилось (экран «tv» на связи) — готово; не вышло за kTvWakeUs — бросить
    constexpr int64_t kTvWakeUs = 45000000;
    int64_t tvWakeUntil = 0, tvWakeNext = 0, tvWakeFrom = 0;
    const std::string tvMacFile = tvDir + "/mac";
    std::string tvMac;
    if (FILE *f = fopen(tvMacFile.c_str(), "r")) {
        char line[64] = {};
        if (fgets(line, sizeof line, f))
            tvMac = std::string(line).substr(0, std::string(line).find_first_of("\r\n "));
        fclose(f);
    }
    auto learnTvMac = [&](const std::string &who) {   // «имя (адрес)» подключившегося — MAC для Wake-on-LAN
        const size_t a = who.rfind('('), b = who.rfind(')');
        if (a == std::string::npos || b == std::string::npos || b < a)
            return;
        const std::string mac = macOf(who.substr(a + 1, b - a - 1));
        if (mac.empty() || mac == tvMac)
            return;
        tvMac = mac;
        writeFile(tvMacFile.c_str(), mac + "\n");
        fprintf(stderr, "ТВ: адрес для пробуждения (Wake-on-LAN) — %s\n", mac.c_str());
    };

    // такт: свой будильник; когда отправлен каждый (по ним — сколько кадр идёт от такта)
    const int tickTimer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    bool tickArmed = false;
    struct Tick { int64_t sent, target; };   // когда отправлен, к какому обновлению монитора
    std::deque<Tick> ticks;
    int64_t armedTarget = 0, armedAt = 0;   // такт: к какому обновлению, его расчётный момент
    std::deque<double> leads;      // от такта до распакованного кадра, мс — кадры движения (без первых после паузы)
    std::deque<double> startLeads; // то же — первые кадры после паузы (docs/PI.md, «Опережение: начало и движение»)
    int64_t leadUs = 40000;        // пока не мерили — с запасом (на старте движения кадр идёт дольше)
    int64_t leadHoldUntil = 0;     // опережение подняли — не опускать до (часы Pi)
    int64_t lastTickDecoded = 0;   // распакован прошлый кадр по такту (часы Pi): кадров нет 0,1 с — пауза
    int startFrames = 0;           // сколько ещё кадров считать «первыми после паузы»
    constexpr int64_t kPauseUs = 100000, kLeadHoldUs = 300000;
    // Кадр шёл от такта дольше kStallUs — это зависание (поток интерфейса телефона занят: открыли папку, окно), а не
    // путь кадра: в опережение и в его статистику не берём — один кадр опоздает. Было: такой кадр поднимал
    // опережение до 80 мс, и оно держалось секундами (кадры ждали в очереди, отклик на щелчок — 90–230 мс; прогон
    // стола 19 ноября 2026, docs/PI.md, «Очередь и опережение»). Опережение — не больше kLeadMaxUs
    constexpr int64_t kStallUs = 65000, kLeadMaxUs = 60000;
    constexpr int kStartFrames = 8;
    // опережение для начала движения: 97% первых кадров после паузы + 2,5 мс (их пока мало — 45 мс). С 90% при
    // ~37 мс опережения 16% анимаций теряли кадр в первых пяти (хвосты пути — 34–40 мс; прогон стола 19 ноября 2026):
    // ровное начало дороже 5–8 мс задержки
    auto startLead = [&]() -> int64_t {
        if (startLeads.size() < 16)
            return 45000;
        return std::clamp<int64_t>(
            int64_t(pct(std::vector<double>(startLeads.begin(), startLeads.end()), 0.97) * 1000) + 2500, 30000,
            kLeadMaxUs);
    };

    // цифры за секунду
    std::vector<double> sPhone, sNet, sDecode, sWait, sTotal;
    int sShown = 0, sDropped0 = 0, sMotion = 0, sTicks = 0;
    size_t sBytes = 0;
    double sStall = 0;
    int64_t lastRecv = 0, lastRecvCapture = 0, statFrom = monoUs();
    // звук: последний взятый кусок (копии по другому пути — прочь); задержка — от метки на телефоне до звучания
    // (docs/PI.md, «Звук на мониторе»): как у картинки (медиана «итого» + 5 мс; первые 3 с потока — без неё: там
    // опорные кадры), но не меньше 70 мс и не меньше, чем нужно кускам, чтобы успеть (второй по раннести кусок за
    // секунду — раньше своего момента на 35 мс: буфер устройства 20 + запас). Растёт, когда куски опоздали или
    // запаса не хватает на 15 мс; меньше — только в тишине: смена задержки посреди звука — щелчок
    int64_t lastAudio = 0, audioDelayUs = 70000, streamFrom = 0;
    double lastVideoMs = 0;

    // где кадр на мониторе; монитор стол не показывает (заставка, стол — на планшете или чужом экране) — где был бы
    auto vrect = [&] { return lockView ? display.fit(frameW, frameH) : display.video(); };
    auto clampToVideo = [&](double &x, double &y) {
        const Rect v = vrect();
        x = std::clamp(x, double(v.x), double(v.x + v.w - 1));
        y = std::clamp(y, double(v.y), double(v.y + v.h - 1));
    };
    auto frac = [&](double x, double y, double &fx, double &fy) {
        const Rect v = vrect();
        fx = std::clamp((x - v.x) / v.w, 0.0, 1.0);
        fy = std::clamp((y - v.y) / v.h, 0.0, 1.0);
    };
    auto pointerAt = [&](int64_t t, double &x, double &y) {
        if (pointer.empty()) {
            x = cx, y = cy;
            return;
        }
        pointer.at(t, x, y);
        clampToVideo(x, y);
    };
    auto sendPointer = [&](double px, double py) {
        double fx, fy;
        frac(px, py, fx, fy);
        const float x = float(fx), y = float(fy);
        if (guest || (x == sentX && y == sentY && buttons == sentButtons))
            return;
        uint8_t b[9];
        b[0] = uint8_t(buttons | input.pointerMods());
        memcpy(b + 1, &x, 4);
        memcpy(b + 5, &y, 4);
        inputAll(stream::Pointer, b, sizeof b);
        sentX = x, sentY = y, sentButtons = buttons;
    };
    auto sendScroll = [&](double dx, double dy, uint8_t phase) {
        uint8_t b[17];
        const float v[4] = {float(wheelX), float(wheelY), float(dx), float(dy)};
        memcpy(b, v, 16);
        b[16] = phase;
        inputAll(stream::Scroll, b, sizeof b);
    };

    display.cursorAt = [&](int64_t t, int &x, int &y) {
        double px, py;
        pointerAt(t, px, py);
        x = int(px), y = int(py);
        if (ptrace)
            fprintf(ptrace, "c,%lld,%.2f,%.2f,\n", (long long)t, px, py);
    };
    input.onRaw = [&](int64_t t, int dx, int dy, double gain) {
        if (ptrace)
            fprintf(ptrace, "m,%lld,%d,%d,%.3f\n", (long long)t, dx, dy, gain);
    };
    display.cursorMoving = [&] { return pointer.moving(monoUs()); };

    // Без дока: клавиша или кнопка мыши — разбудить монитор и док (Wake телефону по тихому пути), сами телефону не идут;
    // движение мыши и колесо — ничего
    auto lockKey = [&] {
        const int64_t now = monoUs();
        lockIdleFrom = now;
        wakeMonitor(now);
        if (off && now - wakeSent > 1000000)
            for (Path *p : paths)
                if (p->link.isOpen() && p->link.quiet()) {
                    p->link.send(stream::Wake, "pi", 2);
                    wakeSent = now;
                    fprintf(stderr, "без дока: клавиша — будим док\n");
                    break;
                }
    };
    input.onMotion = [&](double dx, double dy, int64_t t) {
        if (inputOff()) {   // настройки «Без дока»: движение мыши будит монитор (док — по-прежнему клавиша или щелчок)
            if (wakeMove) {
                const int64_t now = monoUs();
                lockIdleFrom = now;
                if (wakeMonitor(now))
                    fprintf(stderr, "без дока: мышь сдвинули — монитор проснулся\n");
            }
            return;
        }
        ++sMotion;
        cursorsOn();
        guest = false;
        cx += dx, cy += dy;
        clampToVideo(cx, cy);
        pointer.add(t, cx, cy);
        if (ptrace)
            fprintf(ptrace, "r,%lld,%.2f,%.2f,\n", (long long)t, cx, cy);
        display.cursorChanged();
        lastInputUs = monoUs();
    };
    input.onAbsolute = [&](double fx, double fy) {
        if (inputOff())
            return;
        cursorsOn();
        guest = false;
        const Rect v = vrect();
        cx = v.x + fx * (v.w - 1);
        cy = v.y + fy * (v.h - 1);
        pointer.jump(monoUs(), cx, cy);
        display.cursorChanged();
        lastInputUs = monoUs();
    };
    input.onButtons = [&](int b) {   // кнопка — сразу, там, где курсор виден: по нему и целятся
        if (inputOff()) {
            if (b)
                lockKey();
            return;
        }
        guest = false;
        buttons = b;
        cursorsOn();
        lastInputUs = monoUs();
        if (anyOpen()) {
            double px, py;
            pointerAt(monoUs(), px, py);
            sendPointer(px, py);
        }
    };
    input.onWheel = [&](double nx, double ny) {
        if (inputOff())
            return;
        // щелчок колеса — 3 строки, 100 точек стола (как колесо мыши в окне Mac; настройки стола — строк и направление);
        // идёт плавно, по тактам
        if (!wheel.busy() && !wheelEndAt)
            frac(cx, cy, wheelX, wheelY);
        cursorsOn();
        const double boost = wheelCfg.wheelAccel ? wheelRate.add(monoUs(), std::hypot(nx, ny)) : 1;
        const double k = wheelCfg.lines * 100.0 / 3 * boost * (wheelCfg.natural ? -1 : 1);
        padMomentum = false;
        wheel.add(nx * k, ny * k);
        lastInputUs = monoUs();
    };
    // Прокрутка пальцами (трекпад Pi, два пальца на планшете), как у Mac: страница идёт за пальцами (пальцы вверх —
    // страница вверх, то есть прокрутка вниз); положили пальцы — инерция прошлой прокрутки стоп; сняли — инерция по
    // скорости пальцев за последние 80 мс (пальцы перед этим стояли дольше 60 мс — без инерции).
    // px, py — сдвиг содержимого за этот отчёт, точки стола; fingers false — пальцы сняли
    auto fingerScroll = [&](double px, double py, int64_t t, bool fingers) {
        if (fingers) {
            if (!padScroll) {
                padScroll = true;
                padMomentum = false;
                wheel.stop();
                padHist.clear();
                if (!wheelEndAt)
                    frac(cx, cy, wheelX, wheelY);
            }
            wheel.add(px, py);
            padHist.emplace_back(t, px, py);
            while (padHist.size() > 1 && t - std::get<0>(padHist.front()) > 80000)
                padHist.pop_front();
        } else if (padScroll) {
            padScroll = false;
            double sx = 0, sy = 0;
            int64_t from = t, to = 0;
            for (const auto &[ht, hx, hy] : padHist)
                sx += hx, sy += hy, from = std::min(from, ht), to = std::max(to, ht);
            const double span = (to - from + 11000) / 1e6;   // с; первый отчёт окна — ход за его ~11 мс
            if (!padHist.empty() && t - to < 60000 && padMomentumS > 0) {
                wheel.add(sx / span * padMomentumS, sy / span * padMomentumS);
                padMomentum = true;
            }
            padHist.clear();
        }
        lastInputUs = monoUs();
    };
    input.pointScale = [&] { return double(vrect().w) / std::max(1, deskW); };
    input.onPadAction = [&](int action) {   // смахивание трекпада — действие стола (меню N)
        if (inputOff())
            return;
        const uint8_t a = uint8_t(action);
        inputAll(stream::PadAction, &a, 1);
    };
    input.onPadTouch = [&](const uint8_t *b, size_t n) { inputAll(stream::PadTouch, b, n); };   // пальцы — «Проверке»
    input.onScroll = [&](double dx, double dy, int64_t t, bool fingers) {   // ход пальцев трекпада — пиксели монитора
        if (inputOff())
            return;
        cursorsOn();
        const Rect v = vrect();
        fingerScroll(-dx * deskW / std::max(1, v.w), -dy * deskH / std::max(1, v.h), t, fingers);
    };
    input.onKey = [&](bool down, uint32_t qtKey, uint32_t mods, const std::string &text) {
        if (inputOff()) {
            if (down)
                lockKey();
            return;
        }
        std::vector<uint8_t> b(9 + text.size());
        b[0] = down;
        memcpy(b.data() + 1, &qtKey, 4);
        memcpy(b.data() + 5, &mods, 4);
        memcpy(b.data() + 9, text.data(), text.size());
        inputAll(stream::Key, b.data(), b.size());
    };
    // раскладка сменилась сочетанием — столу (подпись EN / RU на панели); при подключении — с настройками (sendConfig)
    input.onLayout = [&](bool ru) {
        fprintf(stderr, "раскладка: %s\n", ru ? "RU" : "EN");
        const uint8_t v = ru;
        inputAll(stream::Layout, &v, 1);
    };

    display.onDropped = [&](const FrameInfo &f) {
        if (trace)
            fprintf(trace, "%lld,%lld,%lld,%lld,0,%lld,%lld,%d,%d\n", (long long)f.capture, (long long)f.encoded,
                    (long long)f.recv, (long long)f.decoded, (long long)offset, (long long)f.target, f.bytes, int(f.key));
    };
    // такт: телефону — мышь к этому моменту, шаг колеса и Tick; следующий — за lead до своего обновления
    auto armTick = [&] {
        const int64_t now = monoUs(), margin = display.marginUs();
        // стол стоит (кадров нет 0,1 с): следующие кадры — первые после паузы, они идут дольше — опережение под
        // них заранее, пока в пути ничего нет (поднять его посреди движения — пропустить обновление монитора)
        if (now - lastTickDecoded > kPauseUs) {   // в пути ничего — опережение заново, под начало движения
            leadUs = startLead();
            leadHoldUntil = std::max(leadHoldUntil, now + kLeadHoldUs);
        }
        int64_t target = display.nextVblank(now + margin + leadUs + 500);
        while (target <= armedTarget)   // опережение опустили — такт к следующему обновлению, а не к тому же
            target += display.periodUs();
        int64_t at = target - margin - leadUs;
        while (at <= now)
            at += display.periodUs(), target += display.periodUs();
        armedTarget = target;
        armedAt = at;
        itimerspec it{};
        it.it_value.tv_sec = at / 1000000;
        it.it_value.tv_nsec = (at % 1000000) * 1000;
        timerfd_settime(tickTimer, TFD_TIMER_ABSTIME, &it, nullptr);
        tickArmed = true;
    };
    auto onTick = [&] {
        uint64_t n;
        if (::read(tickTimer, &n, sizeof n) < 0)
            return;
        tickArmed = false;
        if (!anyOpen() || !streaming)
            return;
        // мышь — дочитать: отчёт, пришедший к такту, но ещё не прочитанный, сдвинул бы окно не туда (−1,5 пикс)
        std::vector<int> devs;
        for (const auto &[fd, name] : input.devices())
            devs.push_back(fd);
        for (int fd : devs)
            input.read(fd);
        const int64_t now = monoUs();
        // мышь — на расчётный момент такта (сетка обновлений монитора), а не на момент будильника (±1 мс): шаги
        // окна от кадра к кадру — одинаковые
        double px, py;
        pointerAt(armedAt ? armedAt : now, px, py);
        sendPointer(px, py);
        if (ptrace)
            fprintf(ptrace, "s,%lld,%.2f,%.2f,,%lld,%lld\n", (long long)(armedAt ? armedAt : now), px, py,
                    (long long)monoUs(), (long long)armedTarget);
        int dx, dy;
        // доля остатка за такт: к «мягкости» щелчок проходит 95 % (e^−3); без плавности — весь сразу
        double share = wheelCfg.smooth ? 1 - std::exp(-3.0 * display.periodUs() / (wheelCfg.softMs * 1000.0)) : 1;
        if (padScroll)   // трекпад: пальцы на нём — ход целиком; сняли — инерция тает с постоянной padMomentumS
            share = 1;
        else if (padMomentum)
            share = 1 - std::exp(-display.periodUs() / (padMomentumS * 1e6));
        if (wheel.step(dx, dy, share)) {
            sendScroll(double(dx) / deskW, double(dy) / deskH, stream::ScrollUpdate);
            if (ptrace)   // шаг колеса этого такта, точки стола (x, y — сдвиг содержимого)
                fprintf(ptrace, "w,%lld,%d,%d,\n", (long long)now, dx, dy);
            wheelEndAt = now + 90000;
        }
        if (padMomentum && !wheel.busy())
            padMomentum = false;
        // в такте — его обновление монитора: телефон вернёт его с кадром (Echo) — кадр встанет ровно туда
        inputAll(stream::Tick, &armedTarget, 8);
        ticks.push_back({now, armedTarget});
        while (ticks.size() > 64)
            ticks.pop_front();
        ++sTicks;
    };
    // кадр — по какому такту: телефон говорит сам (Echo — значение такта, то есть обновление монитора); старый
    // телефон без Echo — по времени снятия (часы телефона → Pi по пингам; бывает, путает соседние такты). Отсюда —
    // обновление монитора кадра (0 — не по такту) и замер «от такта до распакованного»
    std::map<int64_t, int64_t> echoes;   // захват → обновление монитора (из Echo)
    auto tickOf = [&](int64_t capture, int64_t decoded, bool key) -> int64_t {
        Tick tick{0, 0};
        if (auto e = echoes.find(capture); e != echoes.end()) {
            for (const Tick &k : ticks)
                if (k.target == e->second)
                    tick = k;
            if (!tick.sent)
                tick.target = e->second;
            echoes.erase(echoes.begin(), std::next(e));
        } else {
            if (!haveOffset || ticks.empty())
                return 0;
            const int64_t capturedPi = capture - offset;
            auto it = std::upper_bound(ticks.begin(), ticks.end(), capturedPi,
                                       [](int64_t t, const Tick &k) { return t < k.sent; });
            if (it == ticks.begin())
                return 0;
            tick = *std::prev(it);
            if (capturedPi - tick.sent > 12000)
                return 0;   // кадр не по такту (стол сам по себе)
        }
        // первые кадры после паузы (стол стоял ≥ 0,1 с) идут дольше: первый сжимается ~28 мс против 19, окно,
        // появляясь, отстаёт от тактов на 15–20 мс — от такта до распакованного 40–60 мс против 30 в движении
        // (docs/PI.md, «Опережение: начало и движение»)
        if (decoded - lastTickDecoded > kPauseUs)
            startFrames = kStartFrames;
        lastTickDecoded = decoded;
        if (!tick.sent || key)   // опорный кадр идёт дольше (60–90 мс): опережение по нему раздулось бы
            return tick.target;
        const int64_t took = decoded - tick.sent;
        if (took > kStallUs) {   // зависание телефона — не мера пути (kStallUs)
            if (startFrames > 0)
                --startFrames;
            return tick.target;
        }
        auto &into = startFrames > 0 ? startLeads : leads;
        if (startFrames > 0)
            --startFrames;
        into.push_back(took / 1000.0);
        while (into.size() > (&into == &leads ? 150u : 64u))
            into.pop_front();
        // Опережение два: для начала движения (startLead — такты после паузы ставятся с ним заранее, armTick) и для
        // движения — 99% его кадров + 2,5 мс. Кадр опоздал — сразу под него (окно проявляется кадрами по 100–140 КБ:
        // пока опережение догоняло, Pi выбрасывал кадры в начале анимации, «Часы анимаций»). Поднятое держится 0,3 с,
        // потом сходит к движению по 0,5 мс за кадр — шаг окна за кадр меняется не больше чем на 4%. Было: одно
        // опережение по 99% всех кадров — всплески начала держали его на 55–62 мс всю работу. Вверх к 99% — сразу;
        // кадров движения меньше 20 — к 40 мс, как с начала.
        if (took + 2500 > leadUs) {
            leadUs = std::min<int64_t>(took + 2500, kLeadMaxUs);
            leadHoldUntil = decoded + kLeadHoldUs;
        }
        const int64_t want = leads.size() >= 20
            ? std::clamp<int64_t>(int64_t(pct(std::vector<double>(leads.begin(), leads.end()), 0.99) * 1000) + 2500,
                                  8000, kLeadMaxUs)
            : 40000;
        if (want > leadUs)
            leadUs = want;
        else if (decoded >= leadHoldUntil && want < leadUs - 1000)
            leadUs -= std::min<int64_t>(500, leadUs - want);
        return tick.target;
    };

    // метка кадра на экране: выход — часы проявляются, курсор тает (в обновление этого кадра); вход — наоборот
    display.onCommit = [&](const FrameInfo &f, int64_t vblank) {
        if (f.away == 1 + stream::AwayLeave) {
            fprintf(stderr, "без дока: стол уходит\n");
            lockView = true;
            lockIdleFrom = monoUs();
            lock.clockIn(vblank);
            lock.cursorTo(false, vblank, 140000);
        } else if (f.away == 1 + stream::AwayBack && lockView) {
            fprintf(stderr, "без дока: стол вернулся (%.0f мс после Reveal)\n", revealSent ? (monoUs() - revealSent) / 1000.0 : 0.0);
            lockView = false;
            revealSent = 0;
            lock.clockOut(vblank);
            lock.cursorTo(true, vblank, 180000);
        }
    };
    display.onShown = [&](const FrameInfo &f, int64_t shown) {
        if (f.away == 1 + stream::AwayFinal) {   // последний кадр выхода на экране — телефону можно выключаться
            inputAll(stream::AwayShown);
            fprintf(stderr, "без дока: стол ушёл, обои на экране\n");
        }
        if (entering && lockView) {   // первый кадр сеанса без дока: покров (если был) тает, потом — Reveal
            entering = false;
            int64_t at = std::max(shown, wokeAt + kWakeSettleUs);
            if (lock.covered()) {
                lock.coverOut(at, 250000);
                at += 250000;
            }
            revealAt = at;
        }
        ++sShown;
        if (haveOffset) {
            sPhone.push_back((f.encoded - f.capture) / 1000.0);
            sNet.push_back((f.recv - (f.encoded - offset)) / 1000.0);
            sDecode.push_back((f.decoded - f.recv) / 1000.0);
            sWait.push_back((shown - f.decoded) / 1000.0);
            sTotal.push_back((shown - (f.capture - offset)) / 1000.0);
        }
        if (trace)
            fprintf(trace, "%lld,%lld,%lld,%lld,%lld,%lld,%lld,%d,%d\n", (long long)f.capture, (long long)f.encoded,
                    (long long)f.recv, (long long)f.decoded, (long long)shown, (long long)offset, (long long)f.target,
                    f.bytes, int(f.key));
    };

    char msgPath = '?';   // по какому пути пришло сообщение (c — кабель через SSH, d — напрямую, w — Wi-Fi): для ATRACE
    auto onMessage = [&](uint8_t type, const uint8_t *p, size_t n) {
        if (type == stream::Pong && n >= 16) {
            const int64_t mine = get<int64_t>(p), phone = get<int64_t>(p + 8), back = monoUs();
            pongs.push_back({back - mine, phone - (mine + back) / 2});
            if (pongs.size() > 20)
                pongs.erase(pongs.begin());
            const Pong best = *std::min_element(pongs.begin(), pongs.end(),
                                                [](const Pong &a, const Pong &b) { return a.rtt < b.rtt; });
            offset = best.offset;
            rtt = best.rtt;
            haveOffset = true;
        } else if (type == stream::Hello && off) {
            // ждали тихо, а телефон уже ведёт сеанс (включили, пока его оболочка перезапускалась: Switch 1 слать было
            // некому) — Hello значит «включено»: подключиться как следует
            switchTo = 1;
        } else if (type == stream::Hello) {
            const std::string j(reinterpret_cast<const char *>(p), n);
            const long long g = jsonInt(j, "gen");
            if (g && g <= gen)
                return;   // копия по другому пути или опоздавший (номер растёт и у перезапущенной оболочки)
            gen = g;
            const std::string mode = jsonStr(j, "mode"), codec = jsonStr(j, "codec");
            frameW = int(jsonInt(j, "width"));
            frameH = int(jsonInt(j, "height"));
            streaming = mode == "desktop" && codec.rfind("hvc1", 0) == 0;
            fprintf(stderr, "телефон: %s %dx%d, %lld к/с, %s%s\n", mode.c_str(), frameW, frameH, jsonInt(j, "fps"),
                    codec.c_str(), streaming ? "" : " — ждём стол в HEVC");
            if (streaming) {
                decoder.reset();
                display.newStream();
                waitingKey = true;
                streamFrom = monoUs();
                if (lockView && piLive) {   // сеанс без дока: телефон начал с одних обоев — показать, потом Reveal
                    entering = true;
                    revealAt = revealSent = 0;
                    wakeMonitor(monoUs());
                } else if (!piLive && anyRemoteLive()) {   // стол — планшету или ТВ: Reveal — когда первый кадр уйдёт ему
                    relayEntering = true;
                    revealAt = revealSent = 0;
                }
                keyAskAt = 0;   // новый поток и так начнётся с опорного
            } else if (anyOpen() && monoUs() - configAt > 3000000) {
                sendConfig();   // сеанс у телефона новый (перезапустился, путь вернулся первым) — настроить снова
            }
        } else if (type == stream::Away && n >= 9) {   // метка кадра (экран без дока) — до его Video
            awayMarks[get<int64_t>(p)] = uint8_t(1 + p[8]);
            while (awayMarks.size() > 16)
                awayMarks.erase(awayMarks.begin());
        } else if (type == stream::Echo && n >= 16) {
            echoes[get<int64_t>(p)] = get<int64_t>(p + 8);
            while (echoes.size() > 64)
                echoes.erase(echoes.begin());
        } else if (type == stream::Direct && n >= 22) {
            const uint16_t port = uint16_t(p[4] | p[5] << 8);
            if (cablePath.link.openDirect(p, port, p + 6)) {
                fprintf(stderr, "картинка: напрямую по кабелю (%u.%u.%u.%u:%u), ввод — через SSH\n", p[0], p[1], p[2],
                        p[3], port);
            } else {
                fprintf(stderr, "картинка: напрямую не подключилась (%s) — через SSH\n", strerror(errno));
                cablePath.directAskAt = monoUs() + 2000000;
            }
        } else if (type == stream::Address && n >= 4) {   // адрес телефона в Wi-Fi — запасной путь (и без кабеля)
            char ip[20];
            snprintf(ip, sizeof ip, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
            if ((p[0] | p[1] | p[2] | p[3]) && !wifiFixed && wifiEnv != "-" && wifiPath.host != ip) {
                fprintf(stderr, "Wi-Fi телефона: %s\n", ip);
                wifiPath.host = ip;
                mkdir("/var/lib/naomi-dock", 0755);
                if (FILE *f = fopen(wifiFile, "w")) {
                    fprintf(f, "%s\n", ip);
                    fclose(f);
                }
                if (wifiPath.link.isOpen())
                    wifiPath.link.close();
                wifiPath.retryAt = 0;
            }
        } else if (type == stream::Switch && n >= 1) {
            switchTo = p[0] ? 1 : 0;
        } else if (type == stream::Show) {   // какие экраны показывают стол и чей главный (копии по другим путям — те же)
            const std::string j(reinterpret_cast<const char *>(p), n);
            if (j == showSaid)
                return;
            showSaid = j;
            const size_t from = j.find("\"live\":["), to = from == std::string::npos ? from : j.find(']', from);
            const std::string live = to == std::string::npos ? "" : j.substr(from + 8, to - from - 8);
            const bool pi = live.find("\"pi\"") != std::string::npos;
            bool far[kRemotes];   // удалённые экраны приставки, что теперь показывают стол
            int mine = int(pi);
            for (size_t i = 0; i < kRemotes; ++i)
                mine += far[i] = live.find(std::string("\"") + remotes[i]->id + "\"") != std::string::npos;
            const bool other = std::count(live.begin(), live.end(), '"') / 2 > mine;
            const std::string main = jsonStr(j, "main");
            fprintf(stderr, "экраны: стол показывают — %s; главный — %s\n", live.empty() ? "никто" : live.c_str(),
                    main.empty() ? "нет" : main.c_str());
            // стол уже был показан на другом экране и остаётся там — он не «уходил»: монитору его не ждать с Reveal
            bool stays = otherLive && other, fresh = false;
            for (size_t i = 0; i < kRemotes; ++i) {
                stays |= remotes[i]->live && far[i];
                fresh |= far[i] && !remotes[i]->live;
            }
            const int64_t now = monoUs();
            if (pi && !piLive && !off) {   // монитор снова показывает стол: первый кадр — на экран, покров тает
                entering = true;
                quickBack = stays;
                revealAt = revealSent = 0;
                waitingKey = true;
                keyAskAt = now + 150000;
                wakeMonitor(now);
            } else if (!pi && piLive) {   // монитор стол больше не показывает: заставка (стол уходил сам — она уже есть)
                if (!lockView) {
                    fprintf(stderr, "без дока: стол — на других экранах, на мониторе заставка\n");
                    lockView = true;
                    lockIdleFrom = now;
                    lock.coverIn(now, 500000);
                    lock.clockIn(now);
                    lock.cursorTo(false, now, 140000);
                } else if (!lock.covered()) {
                    lock.coverIn(now, 0);
                }
                entering = false;
                revealAt = revealSent = 0;
            }
            if (fresh && !pi)   // стол — на планшет или ТВ: Reveal — когда первый кадр уйдёт ему
                relayEntering = true;
            piLive = pi;
            for (size_t i = 0; i < kRemotes; ++i) {
                Remote &r = *remotes[i];
                // стол экран показывал, а теперь с него снят («нет», выключили, переехал) — Leave: телевизор уходит
                // на то, что было до приложения (планшету хватает Hello без стола — заставка). Hello без стола к этой
                // минуте мог уже уйти (стол выключают: поток кончается раньше, чем приходит Show) — судим по told
                if (r.live && !far[i] && r.told) {
                    r.link.send(tablet::Leave, nullptr, 0);
                    r.told = false;
                }
                r.live = far[i];
            }
            otherLive = other;
            if (main != mainScreen) {   // главный сменился: наш экран — форма стола его
                mainScreen = main;
                if (!deskEnv) {
                    deskFor(screen);
                    fprintf(stderr, "стол под главный экран (%s): %dx%d точек, масштаб %.3f\n", main.c_str(), deskW, deskH,
                            scaleMilli / 1000.0);
                    if (anyOpen() && !off && (main == "pi" || remoteOf(main)))   // чужой главный — форму назовёт он
                        sendDeskSize();
                }
            }
        } else if (type == stream::ScreenLight) {   // яркость экрана со стола («Экраны»): планшету — как есть
            const std::string j(reinterpret_cast<const char *>(p), n);
            if (Remote *r = remoteOf(jsonStr(j, "id")); r && r->light)
                r->link.light(jsonInt(j, "auto", 1) != 0, int(std::clamp(jsonInt(j, "level", 60), 1LL, 100LL)));
        } else if (type == stream::ScreenWake) {   // экран поставили на «стол», а он не на связи: телевизор — включить
            const std::string j(reinterpret_cast<const char *>(p), n);
            const int64_t now = monoUs();
            if (jsonStr(j, "id") != tvScreen.id) {
            } else if (jsonInt(j, "on", 1) == 0) {   // поставили на «нет», пока включался: уже не надо
                if (tvWakeUntil)
                    fprintf(stderr, "ТВ: включать уже не надо\n");
                tvWakeUntil = 0;
            } else if (!tvScreen.link.connected() && now - tvWakeFrom > 1000000) {
                tvWakeFrom = tvWakeNext = now;   // (копии по другим путям — в ту же секунду: прочь)
                tvWakeUntil = now + kTvWakeUs;
                fprintf(stderr, "ТВ: телефон просит включить — %s\n",
                        tvAgent.connected() ? "дежурный на связи: Launch"
                        : tvMac.empty() ? "дежурного нет, адреса для пробуждения тоже (ТВ ещё не подключался)"
                                        : "дежурного нет: бужу по сети (Wake-on-LAN)");
            }
        } else if (type == stream::Wallpaper && n >= 16) {   // обои стола — заставке планшета и ТВ: на диск и им
            const std::string hash = std::string(reinterpret_cast<const char *>(p), 16).c_str();
            if (hash != wallHash) {
                wallHash = hash;
                wallBody.assign(p, p + n);
                mkdir("/var/lib/naomi-dock", 0755);
                if (n > 16)
                    writeFile(wallFile, std::string(reinterpret_cast<const char *>(p + 16), n - 16));
                writeFile(wallHashFile, hash + "\n");
                fprintf(stderr, "обои заставки: %zu КБ\n", (n - 16) / 1024);
                for (Remote *r : remotes)
                    if (r->link.connected() && r->wall != wallHash) {
                        r->link.send(tablet::Wallpaper, wallBody.data(), wallBody.size());
                        r->wall = wallHash;
                    }
            }
        } else if (type == stream::CursorAt && n >= 9) {   // мышь стола ведёт чужой экран (окно Mac): курсор — туда же
            const Rect v = vrect();
            cx = v.x + std::clamp(double(get<float>(p + 1)), 0.0, 1.0) * (v.w - 1);
            cy = v.y + std::clamp(double(get<float>(p + 5)), 0.0, 1.0) * (v.h - 1);
            pointer.jump(monoUs(), cx, cy);
            guest = true;
            cursorsOn();
            display.cursorChanged();
        } else if (type == stream::Mouse) {   // настройки мыши со стола телефона (копии по другим путям — прочь)
            const std::string j(reinterpret_cast<const char *>(p), n);
            if (j == mouseJson)
                return;
            mouseJson = j;
            const int speed = int(std::clamp(jsonInt(j, "speed", 50), 0LL, 100LL));
            const int accel = int(std::clamp(jsonInt(j, "accel", 1), 0LL, 2LL));
            input.setPointer(std::pow(2.0, (speed - 50) / 33.3), accel);   // 0 % — 0,35, 50 — 1, 100 — 2,8
            wheelCfg.natural = jsonInt(j, "natural") != 0;
            wheelCfg.lines = int(std::clamp(jsonInt(j, "lines", 3), 1LL, 10LL));
            wheelCfg.smooth = jsonInt(j, "smooth", 1) != 0;
            wheelCfg.softMs = int(std::clamp(jsonInt(j, "soft", 130), 30LL, 1000LL));
            wheelCfg.wheelAccel = jsonInt(j, "wheelAccel") != 0;
            fprintf(stderr, "мышь: скорость %d %%, ускорение %s; колесо %s, %d стр., %s, разгон %s\n", speed,
                    accel == 0 ? "нет" : accel == 2 ? "сильное" : "мягкое", wheelCfg.natural ? "обратное" : "обычное",
                    wheelCfg.lines, wheelCfg.smooth ? (std::to_string(wheelCfg.softMs) + " мс").c_str() : "сразу",
                    wheelCfg.wheelAccel ? "вкл" : "выкл");
        } else if (type == stream::Pad) {   // настройки трекпада со стола телефона (копии по другим путям — прочь)
            const std::string j(reinterpret_cast<const char *>(p), n);
            if (j == padJson)
                return;
            // запомнить — без show (раздел «Трекпад» открыт сейчас): только он сменился — файл не трогать
            auto stored = [](std::string v) {
                const size_t at = v.find("\"show\":");
                if (at == std::string::npos)
                    return v;
                size_t end = at + 7;
                while (end < v.size() && isdigit(static_cast<unsigned char>(v[end])))
                    ++end;
                if (end < v.size() && v[end] == ',')
                    v.erase(at, end + 1 - at);
                else
                    v.erase(at > 0 && v[at - 1] == ',' ? at - 1 : at, end - (at > 0 && v[at - 1] == ',' ? at - 1 : at));
                return v;
            };
            if (stored(j) != stored(padJson))
                writeFile(padFile, stored(j));
            padJson = j;
            applyPad(j, jsonInt(j, "show") != 0);
        } else if (type == stream::Screen) {   // настройки экрана со стола телефона (копии по другим путям — прочь)
            const std::string j(reinterpret_cast<const char *>(p), n);
            if (j == screenJson)
                return;
            screenJson = j;
            screen = parseScreen(j);
            writeFile(screenFile, j);
            fprintf(stderr, "экран: стол %d точек, резкость %d, %d Гц, часы %s (%.1f %%, %.1f %%), %s %s, движение мыши %s%s%s%s\n",
                    screen.width, screen.sharp, screen.hz, screen.clock ? "да" : "нет", screen.clockX / 100.0,
                    screen.clockY / 100.0,
                    screen.dim ? ("приглушение до " + std::to_string(screen.dimLevel) + " %").c_str() : "сон",
                    screen.sleepS ? ("через " + std::to_string(screen.sleepS / 60) + " мин").c_str() : "никогда",
                    screen.wakeMove ? "будит" : "не будит", deskEnv ? "; стол — NAOMI_DOCK_DESK" : "",
                    hzEnv ? "; частота — NAOMI_DOCK_HZ" : "", sleepEnv ? "; сон — NAOMI_DOCK_SLEEP" : "");
            applyScreen(screen);
            remoteSplash();
            if (dimmed && wakeMonitor(monoUs()))   // приглушён по прежним настройкам — снять, отсчёт заново
                lockIdleFrom = monoUs();
        } else if (type == stream::Layout && n >= 1) {   // щелчок по EN / RU на панели стола (копии по другим путям —
                                                          // та же): поставить и ответить своей
            if (bool(p[0]) != input.russian()) {
                input.setRussian(p[0]);
                fprintf(stderr, "раскладка: %s (со стола)\n", p[0] ? "RU" : "EN");
                const uint8_t v = p[0] ? 1 : 0;
                inputAll(stream::Layout, &v, 1);
            }
        } else if (type == stream::Brightness && n >= 2) {   // клавиши яркости клавиатуры телефона (копия по другому
            if (p[0] != brightnessSeq) {                      // пути — тот же номер); без дока — не трогаем
                brightnessSeq = p[0];
                if (!lockView)
                    brightness.step(int8_t(p[1]));
            }
        } else if (type == stream::IdleTest) {   // «Тест»: стол сейчас уйдёт — без дела не через sleep, а сразу;
            const int level = n >= 1 && p[0] <= 100 ? p[0] : -1;   // с долей («луна») — приглушить до неё
            if (!idleTestAt)
                fprintf(stderr, "без дока: %s — %s сразу после ухода стола\n", level >= 0 ? "«луна»" : "тест",
                        (level >= 0 || screen.dim) && brightness.supported() != 0 ? "приглушение" : "сон");
            idleTestAt = monoUs();
            idleTestDim = level;
        } else if (type == stream::Cursor && n >= 1) {   // форма курсора стола (копии по другим путям — та же)
            if (p[0] != cursorShape) {
                if (ptrace)
                    fprintf(ptrace, "k,%lld,,,,,%d\n", (long long)monoUs(), p[0]);
                showCursor(p[0], monoUs());
            }
        } else if (type == stream::Route && n >= 1) {
            fprintf(stderr, "путь: %s\n", p[0] == stream::RouteCable ? "кабель" : p[0] == stream::RouteWifi ? "Wi-Fi" : "?");
        } else if (type == stream::Audio && n >= 14 && audio && haveOffset) {
            const int64_t first = get<int64_t>(p);
            const bool f32 = p[13] == stream::AudioF32;
            if (get<uint32_t>(p + 8) != 48000 || p[12] != 2 || (!f32 && p[13] != stream::AudioS16))
                return;
            const bool copy = first <= lastAudio && lastAudio - first < 10000000;   // копия по другому пути
            if (atrace) {
                const int64_t now = monoUs(), play = first - offset + audioDelayUs;
                fprintf(atrace, "%lld,%lld,%lld,%lld,%.1f,%c,%d\n", (long long)first, (long long)now, (long long)offset,
                        (long long)play, (play - now) / 1000.0, msgPath, copy);
            }
            if (copy)
                return;
            lastAudio = first;
            audio->push(first, first - offset + audioDelayUs, p + 14, (n - 14) / (f32 ? 8 : 4), f32);
        } else if (type == stream::Video && n >= 17 && streaming) {
            const int64_t capture = get<int64_t>(p), encoded = get<int64_t>(p + 8);
            const bool key = p[16] & 1;
            if (capture <= lastCapture && lastCapture - capture < 10000000)
                return;   // копия по другому пути
            lastCapture = capture;
            for (Remote *r : remotes)   // планшету и ТВ — тот же кадр (с опорного: у каждого своя очередь, tablet.h)
                if (r->live)
                    r->link.video(p, n, haveOffset, offset);
            uint8_t away = 0;
            if (auto m = awayMarks.find(capture); m != awayMarks.end()) {
                away = m->second;
                awayMarks.erase(awayMarks.begin(), std::next(m));
            }
            if (!piLive) {
                // Монитор на заставке, стол — планшету: кадры монитору не нужны, а разговор с телефоном об уходе и
                // приходе стола ведём за планшет — последний кадр ухода ушёл ему: можно менять план; первый кадр
                // нового стола ушёл: показывай панель и окна
                if (away == 1 + stream::AwayFinal)
                    inputAll(stream::AwayShown);
                if (relayEntering && key && anyRemoteLive()) {
                    relayEntering = false;
                    revealAt = monoUs() + 150000;
                }
                waitingKey = true;
                return;
            }
            if (waitingKey && !key)
                return;
            waitingKey = false;
            const int64_t recv = monoUs();
            if (away == 1 + stream::AwayFinal && key)   // чистые обои стола — на диск (экран без дока после перезагрузки)
                lock.saveWallpaper(p + 17, n - 17);
            if (dump)
                fwrite(p + 17, 1, n - 17, dump);
            sBytes += n - 17;
            if (lastRecv && capture - lastRecvCapture < 50000)   // провал связи: пришёл позже, чем был снят после прошлого
                sStall = std::max(sStall, (recv - lastRecv - (capture - lastRecvCapture)) / 1000.0);
            lastRecv = recv;
            lastRecvCapture = capture;
            decoder.push(p + 17, n - 17, key, FrameInfo{capture, encoded, recv, 0, 0, key, away, int(n - 17)});
        }
    };
    // распакованное (поток распаковки) — экрану; сбился — опорный кадр
    auto onDecoded = [&] {
        for (DecodeThread::Out &o : decoder.take()) {
            if (o.failed) {
                fprintf(stderr, "декодер сбился — прошу опорный кадр\n");
                waitingKey = true;
                inputAll(stream::KeyFrame);
                continue;
            }
            lastFrameUs = o.info.decoded;
            o.info.target = tickOf(o.info.capture, o.info.decoded, o.info.key);
            display.show(o.frame, o.info);
        }
    };
    // Планшет и телевизор (tablet.h): касание — мышь стола, как абсолютное устройство Pi: курсор в точку касания, кнопка — сразу
    // там (по ней целятся), движение пальца — по своим меткам времени (досчёт motion.h, как у мыши), без кнопки
    // (перо над экраном) — наведение; два пальца — прокрутка за пальцами, как трекпадом. Без дока касание будит док,
    // как щелчок мыши. Ушёл посреди касания — кнопки отпустить
    auto onRemote = [&](Remote &r) {
        for (const Tablet::Event &e : r.link.take()) {
            const uint8_t *p = e.body.data();
            const size_t n = e.body.size();
            auto at = [&](float fx, float fy) {
                const Rect v = vrect();
                cx = v.x + std::clamp(double(fx), 0.0, 1.0) * (v.w - 1);
                cy = v.y + std::clamp(double(fy), 0.0, 1.0) * (v.h - 1);
            };
            if (e.type == 0) {   // подключился: имя — до адреса в скобках; экран назовёт сам (Screen)
                r.cursorAt = 0;
                r.clock.clear();
                r.name = std::string(e.body.begin(), e.body.end());
                if (r.wake)
                    learnTvMac(r.name);
                if (const size_t at = r.name.find(" ("); at != std::string::npos)
                    r.name.resize(at);
                r.w = r.h = 0;
                r.wall.clear();
                r.told = r.said.find("\"desk\":1") != std::string::npos;
            } else if (e.type == 255) {
                if (r.buttons && !inputOff()) {
                    buttons = 0;
                    if (anyOpen())
                        sendPointer(cx, cy);
                }
                r.buttons = 0;
                r.told = false;
                if (r.scroll && !inputOff())
                    fingerScroll(0, 0, monoUs(), false);
                r.scroll = false;
                if (r.w) {   // экрана больше нет
                    r.w = r.h = 0;
                    sendScreens();
                }
            } else if (e.type == tablet::Screen && n >= 4) {   // экран планшета и какие обои заставки у него есть
                const int w = get<uint16_t>(p), h = get<uint16_t>(p + 2);
                r.wall = n >= 20 ? std::string(reinterpret_cast<const char *>(p + 4), 16).c_str() : "";
                if (w >= 320 && h >= 240 && (w != r.w || h != r.h)) {
                    r.w = w;
                    r.h = h;
                    fprintf(stderr, "%s %s: экран %dx%d\n", r.label, r.name.c_str(), w, h);
                    sendScreens();
                }
                if (!wallHash.empty() && r.wall != wallHash) {
                    r.link.send(tablet::Wallpaper, wallBody.data(), wallBody.size());
                    r.wall = wallHash;
                }
            } else if (e.type == tablet::Wake) {   // двойное касание заставки планшета, кнопка пульта ТВ — «стол сюда»
                fprintf(stderr, "%s: жест на заставке — прошу стол на него\n", r.label);
                if (off) {
                    for (Path *q : paths)
                        if (q->link.isOpen()) {
                            q->link.send(stream::Wake, r.id, strlen(r.id));
                            break;
                        }
                } else {
                    inputAll(stream::Wake, r.id, strlen(r.id));
                }
            } else if (e.type == tablet::View && n >= 4) {
                const int h = get<uint16_t>(p + 2);
                if (h >= 200 && h <= 4320 && h != r.artH) {
                    r.art = std::make_unique<CursorArt>(h);
                    r.artH = h;
                }
                remoteImage(r, monoUs());
            } else if (e.type == tablet::Pointer && n >= 17) {
                const int b = p[0] & 3;
                const int64_t t = remoteTime(r, get<int64_t>(p + 9), e.recv);
                if (!r.live || off || !streaming) {   // на экране заставка: касания — не столу (будит — Wake)
                    r.buttons = b;
                    continue;
                }
                r.cursor = false;
                guest = false;
                at(get<float>(p + 1), get<float>(p + 5));
                if (b != r.buttons)
                    pointer.jump(t, cx, cy);
                else
                    pointer.add(t, cx, cy);
                display.cursorChanged();
                lastInputUs = monoUs();
                if (b != r.buttons) {
                    buttons = b;
                    if (anyOpen())
                        sendPointer(cx, cy);
                }
                r.buttons = b;
            } else if (e.type == tablet::Scroll && n >= 25) {
                if (!r.live || off || !streaming)
                    continue;
                guest = false;
                const bool fingers = p[16];
                const int64_t t = remoteTime(r, get<int64_t>(p + 17), e.recv);
                r.cursor = false;
                if (fingers && !r.scroll) {   // прокрутка — там, где пальцы
                    at(get<float>(p), get<float>(p + 4));
                    pointer.jump(t, cx, cy);
                    display.cursorChanged();
                    if (anyOpen())
                        sendPointer(cx, cy);
                }
                r.scroll = fingers;
                fingerScroll(-get<float>(p + 8) * deskW, -get<float>(p + 12) * deskH, t, fingers);
            }
        }
    };

    // Дежурный телевизора вышел на связь (телевизор проснулся или включён): его адрес — для Wake-on-LAN; ждали его,
    // чтобы открыть приложение, — Launch сразу
    auto onTvAgent = [&] {
        for (const Tablet::Event &e : tvAgent.take())
            if (e.type == 0) {
                learnTvMac(std::string(e.body.begin(), e.body.end()));
                if (tvWakeUntil)
                    tvWakeNext = 0;
            }
    };

    input.scan();
    int64_t nextScan = monoUs() + 2000000;
    while (!g_quit) {
        const int64_t now = monoUs();
        // Телефон: трансляция выключена — все пути закрыть, дальше один тихий (ниже); включена — заново, как с нуля
        if (switchTo >= 0) {
            const bool wasOff = off;
            off = switchTo == 0;
            switchTo = -1;
            if (off && !lockView)   // выключили без ухода стола (телефон его не показывал) — обои и часы самим
                lost(now, "выключили без ухода стола");
            if (!off)
                wakeMonitor(now);
            if (off != wasOff || !off) {
                fprintf(stderr, off ? "телефон: трансляция выключена — жду тихо\n" : "телефон: трансляция включена — подключаюсь\n");
                for (Path *p : paths) {
                    if (p->link.isOpen() && !(off && p->link.quiet()))
                        p->link.close();
                    p->retryAt = now;
                    p->directAskAt = 0;
                }
                streaming = false;
                primary = nullptr;
                wheel = Wheel();
                wheelEndAt = 0;
                padMomentum = false;
                buttons = 0;
                sentButtons = -1;
                sentX = sentY = -1;
            }
        }
        // Пути: нет — подключиться (раз в 1,5 с); молчит 4 с (пинг — 10 раз в секунду) или очередь не уходит —
        // оборвался. Кабель — 4 раза в секунду, вставлен ли (onLink): вынут — путь закрыт сразу, без попыток ssh;
        // вставлен — подключиться тут же. Ни одного пути не было — сеанс заново (новый Hello, настройки). Прямой
        // канал — только у кабеля: попросить (после подключения; оборвался — сразу, просьба и говорит телефону
        // закрыть свой конец; не подключился — через 2 с); молчит 2 с — закрыть
        for (Path *p : paths) {
            Link &l = p->link;
            if (p->cable && now >= p->presentAt) {
                p->presentAt = now + 250000;
                const bool present = onLink(p->host);
                if (present != p->present) {
                    fprintf(stderr, "кабель %s\n", present ? "вставлен" : "вынут");
                    p->present = present;
                    p->retryAt = now;
                    if (!present && l.isOpen())
                        l.close();
                }
            }
            if (l.isOpen() && (now - l.lastDataUs() > (l.quiet() ? stream::kParkedSilentS * 1000000LL : 4000000)
                               || l.broken())) {
                fprintf(stderr, "путь %s молчит — закрыт\n", p->name);
                l.close();
                p->retryAt = now + 1500000;
            }
            // выключено — путь один: кабель, если вставлен, иначе Wi-Fi
            const bool spare = off && (p->cable ? wifiPath.link.isOpen() : cablePath.link.isOpen() || cablePath.present);
            if (!l.isOpen() && !p->host.empty() && p->present && now >= p->retryAt && !spare) {
                const bool fresh = !anyOpen();
                if (fresh && !lockView && !off)   // все пути оборвались (телефон перезагрузился, оболочка перезапустилась)
                    lost(now, "все пути оборвались");
                if (fresh) {
                    streaming = false;
                    gen = 0;
                    lastAudio = 0;
                    pongs.clear();
                    haveOffset = false;
                    primary = nullptr;
                }
                // основным по Join называется только кабель; Wi-Fi — запасной, основным его делает Use, когда кабеля
                // нет (выбор ниже, по ответам): иначе Join по Wi-Fi, пришедший позже кабельного, отнял бы основной
                const bool use = p->cable;
                if (l.open(p->host, keys, ticket, use, off)) {
                    const std::string screens = screensJson();
                    l.send(stream::Screens, screens.data(), screens.size());
                    if (fresh)
                        sendConfig();
                    if (p->cable && direct && !off)
                        p->directAskAt = now;
                    p->nextPing = now;
                } else {
                    p->retryAt = now + 1500000;
                }
            }
            if (p->cable && l.isOpen() && p->directAskAt && now >= p->directAskAt && l.directFd() < 0) {
                l.send(stream::DirectAsk);
                p->directAskAt = 0;
            }
            if (p->cable && l.directFd() >= 0 && now - l.directDataUs() > 2000000) {
                fprintf(stderr, "прямой канал молчит — картинка через SSH\n");
                l.closeDirect();
                p->directAskAt = now;
            }
            if (off && l.isOpen() && !l.quiet()) {   // трансляцию выключили — путь не тихий: закрыть, откроется тихим
                l.close();
                p->retryAt = now;
                continue;
            }
            // Запасной Wi-Fi в покое: стол стоит (ни ввода, ни кадров kRestAfterUs), картинка идёт по живому кабелю —
            // пинг по Wi-Fi раз в секунду, а не десять: десять держали радио телефона проснувшимся, и телефон с
            // неподвижным столом грелся на +0,45 Вт (docs/PI.md, «Запасной путь в покое»). Движение — снова десять
            const bool rest = !p->cable && !off && streaming && primary == &cablePath && healthy(cablePath, now)
                              && now - lastInputUs > kRestAfterUs && now - lastFrameUs > kRestAfterUs && !wheel.busy();
            if (rest != p->resting) {
                p->resting = rest;
                if (!rest) {
                    p->graceUntil = now + kRestAliveUs;
                    p->nextPing = now;
                }
            }
            if (l.isOpen() && now >= p->nextPing) {
                l.send(stream::Ping, &now, 8);
                l.sendDirect(stream::Ping, &now, 8);
                p->nextPing = now + (l.quiet() ? stream::kParkedPingS * 1000000LL : p->resting ? kRestPingUs : 100000);
            }
        }
        // основной путь — кабель, пока жив (данные за 0,4 с), иначе Wi-Fi: телефон шлёт Hello и ставит значок по нему
        // (Join с флагом «основной» телефон уже сделал основным сам — Use ему тогда ничего не меняет)
        if (Path *best = off ? nullptr : healthy(cablePath, now) ? &cablePath : healthy(wifiPath, now) ? &wifiPath : nullptr;
            best && best != primary) {
            best->link.send(stream::Use);
            fprintf(stderr, "основной путь: %s\n", best->name);
            primary = best;
        }
        // Телефон пропал: стол на экране, а ни один путь не жив kLostUs (пути молчат: Wi-Fi пропал) — обои и часы, пути —
        // заново с новым билетом. Пути оборвались все сразу (перезагрузка) — то же, когда подключаемся заново (выше)
        lostFrom = (healthy(cablePath, now) || healthy(wifiPath, now)) ? 0 : lostFrom ? lostFrom : now;
        if (!off && !lockView && lostFrom && now - lostFrom > kLostUs) {
            lost(now, "телефон пропал");
            for (Path *p : paths) {
                if (p->link.isOpen())
                    p->link.close();
                p->retryAt = now;
                p->directAskAt = 0;
            }
            streaming = false;
            primary = nullptr;
        }
        // монитор снова показывает стол, а новый поток сам не начался (форма стола та же) — опорный кадр
        if (keyAskAt && now >= keyAskAt) {
            keyAskAt = 0;
            if (waitingKey && piLive && streaming && anyOpen())
                inputAll(stream::KeyFrame);
        }
        if (revealAt && now >= revealAt) {   // первый кадр без дока на экране (и покров растаял) — телефону: показывай
            revealAt = 0;
            revealSent = now;
            inputAll(stream::Reveal);
            lastInputUs = now;   // такты — сразу: стол вернётся в такт монитору
        }
        if (revealSent && !piLive)   // Reveal говорили за планшет: монитор остаётся на заставке
            revealSent = 0;
        // стол не вернулся с меткой: телефон без экрана без дока — или стол и так уже шёл для других экранов (quickBack:
        // метки и не будет) — часы прочь сами
        if (revealSent && lockView && now - revealSent > (quickBack ? 0 : kRevealWaitUs)) {
            if (!quickBack)
                fprintf(stderr, "без дока: стол не вернулся сам — часы прочь\n");
            quickBack = false;
            revealSent = 0;
            lockView = false;
            lock.clockOut(now);
            lock.cursorTo(true, now, 180000);
            if (lock.covered())
                lock.coverOut(now, 250000);
        }
        // без дока и без дела: монитор спит или приглушается (не отвечает по DDC/CI — спит); стол вернулся — яркость
        // прежняя (будят и клавиша, и щелчок, и сам телефон — wakeMonitor)
        // «Тест» — через kIdleTestUs (просьба живёт 10 с: стол так и не ушёл — забыть)
        if (idleTestAt && now - idleTestAt > 10000000 && !lockView)
            idleTestAt = 0;
        const int64_t idleUs = idleTestAt ? kIdleTestUs : sleepUs;
        if (lockView && idleUs > 0 && !display.asleep() && !dimmed && now - lockIdleFrom > idleUs) {
            const int level = idleTestAt && idleTestDim >= 0 ? idleTestDim : screen.dim ? screen.dimLevel : -1;
            idleTestAt = 0;
            if (level >= 0 && brightness.supported() != 0) {
                dimmed = true;
                brightness.dim(level);
                fprintf(stderr, "без дока: монитор приглушается до %d %%\n", level);
            } else if (display.setAsleep(true)) {
                fprintf(stderr, "без дока: монитор спит\n");
            }
        }
        if (dimmed && !lockView)
            wakeMonitor(now);
        if (const int level = brightness.takeLevel(); level >= 0) {   // яркость клавишей записана — столу плашку
            const uint8_t v = uint8_t(level);
            inputAll(stream::Brightness, &v, 1);
        }
        // частота монитора из настроек: сменилась — телефону Want (стол рисует под неё) и что теперь у монитора
        if (rateWant > 0) {
            const double was = display.refreshHz();
            if (display.setRate(rateWant)) {
                rateWant = 0;
                if (std::fabs(display.refreshHz() - was) > 0.01) {
                    makeWant();
                    inputAll(stream::Want, want.data(), want.size());
                    sendMonitor();
                }
            }
        }
        lock.poll(now);
        if (cursorShape == stream::CursorBusy && now >= busyNext)   // «Морфинг» — ~30 картинок в секунду
            showCursor(stream::CursorBusy, now);
        // Стола всё нет, а путь открыт (телефон перезагружался: настройки ушли в подключение, что тут же оборвалось, —
        // телефон их не получил и показывает свой экран) — настройки снова, раз в 2 с, пока не придёт Hello стола
        if (!streaming && !off && anyOpen() && now - configAt > 2000000) {
            fprintf(stderr, "стола нет — настройки снова\n");
            sendConfig();
        }
        if (wheelEndAt && now >= wheelEndAt && !wheel.busy()) {
            sendScroll(0, 0, stream::ScrollEnd);
            wheelEndAt = 0;
        }
        // Планшет и ТВ: что идёт (новый сеанс, размер кадра, стола нет) — Hello (помнится: новому — сразу); курсор
        // Pi — не чаще раза в 8 мс; опорный кадр для него — у телефона, не чаще раза в 0,5 с
        for (Remote *r : remotes) {
            if (const std::string j = streaming && r->live && !off
                                          ? "{\"desk\":1,\"width\":" + std::to_string(frameW) + ",\"height\":"
                                                + std::to_string(frameH) + ",\"codec\":\"hevc\",\"gen\":" + std::to_string(gen) + "}"
                                          : std::string("{\"desk\":0}");
                j != r->said) {
                r->link.hello(j);
                r->said = j;
                r->told |= r->link.connected() && j.size() > 12;   // со столом (без стола — {"desk":0})
            }
            if (!r->link.connected())
                continue;
            if (now - r->cursorAt >= 8000) {
                double px, py, fx, fy;
                pointerAt(now, px, py);
                frac(px, py, fx, fy);
                r->link.cursor(r->cursor && streaming && r->live && !inputOff() && cursorShape != stream::CursorHidden,
                               float(fx), float(fy));
                r->cursorAt = now;
            }
            if (r->link.takeKeyWanted())
                r->keyPending = true;
            if (r->keyPending && streaming && anyOpen() && now - r->keyAt > 500000) {
                inputAll(stream::KeyFrame);
                r->keyAt = now;
                r->keyPending = false;
            }
        }
        // Телевизор включают (ScreenWake): приложение на экране — готово; дежурный на связи — Launch (и снова через
        // 3 с, пока приложение не подключилось); спит — Wake-on-LAN раз в секунду
        if (tvWakeUntil) {
            if (tvScreen.link.connected()) {
                fprintf(stderr, "ТВ: приложение на экране — через %.1f с после просьбы\n", (now - tvWakeFrom) / 1e6);
                tvWakeUntil = 0;
            } else if (now >= tvWakeUntil) {
                fprintf(stderr, "ТВ: не включился за %lld с — бросаю (%s)\n", (long long)(kTvWakeUs / 1000000),
                        tvAgent.connected() ? "дежурный на связи, приложение не открылось" : "дежурный на связь не вышел");
                tvWakeUntil = 0;
            } else if (now >= tvWakeNext) {
                if (tvAgent.connected()) {
                    tvAgent.send(tablet::Launch, nullptr, 0);
                    tvWakeNext = now + 3000000;
                } else {
                    if (!tvMac.empty())
                        wakeOnLan(tvMac);
                    tvWakeNext = now + 1000000;
                }
            }
        }
        // мышь остановилась — курсору и телефону последнее настоящее положение (без досчёта)
        const bool moving = pointer.moving(now);
        if (wasMoving && !moving) {
            display.cursorChanged();
            if (anyOpen() && streaming)
                sendPointer(cx, cy);
        }
        wasMoving = moving;
        // такты — пока есть движение: ввод за последнюю секунду, кадры за полсекунды, прокрутка не доехала
        // (монитор на заставке, стол на другом экране — такты идут, пока есть ввод: с ними телефону уходят мышь и колесо)
        const bool active = streaming && (now - lastInputUs < 1000000 || now - lastFrameUs < 500000 || wheel.busy());
        if (active && !tickArmed)
            armTick();
        if (now >= nextScan) {
            input.scan();
            nextScan = now + 2000000;
        }
        input.tick(now);   // сроки трекпада: щелчок касания отпустить, три пальца сняли — отпустить через удержание
        if (now - statFrom >= 1000000) {
            const double sec = (now - statFrom) / 1e6;
            if (display.flips)
                fprintf(stderr, "экран: обновлений %.0f/с (отдано: кадром %d, курсором %d; занят %d, опоздали %d; "
                                "будильник позже 1 мс: %d, до %.1f мс), от отдачи до экрана %.1f мс; сдвигов мыши %d\n",
                        display.flips / sec, display.commitsFrame, display.commitsCursor, display.busy, display.late,
                        display.timerLate, display.timerLateMaxUs / 1000.0, display.flipWaitUs / 1000.0 / display.flips,
                        sMotion);
            display.commitsFrame = display.commitsCursor = display.flips = display.busy = display.late = 0;
            display.timerLate = 0;
            display.timerLateMaxUs = 0;
            display.flipWaitUs = 0;
            sMotion = 0;
            if (sTicks)
                fprintf(stderr, "такт: %d/с, опережение %.1f мс (от такта до распакованного: медиана %.1f, 98%% %.1f); "
                                "кадров позже своего обновления %d\n",
                        sTicks, leadUs / 1000.0, pct(std::vector<double>(leads.begin(), leads.end()), 0.5),
                        pct(std::vector<double>(leads.begin(), leads.end()), 0.98), display.framesLate);
            display.framesLate = 0;
            sTicks = 0;
            if (sShown || sBytes)
                fprintf(stderr, "кадров %.0f/с (выброшено %d), %.1f Мбит/с; телефон (кадр → сжат) %.1f мс, связь %.1f мс "
                                "(провал до %.0f, пинг %.1f), распаковка %.1f мс, до обновления монитора %.1f мс; "
                                "итого от кадра на телефоне до экрана %.1f мс (95%%: %.1f)\n",
                        sShown / sec, display.dropped - sDropped0, sBytes * 8 / sec / 1e6, pct(sPhone, 0.5),
                        pct(sNet, 0.5), sStall, rtt / 1000.0, pct(sDecode, 0.5), pct(sWait, 0.5), pct(sTotal, 0.5),
                        pct(sTotal, 0.95));
            if (sTotal.size() >= 10 && streamFrom && now - streamFrom > 3000000)
                lastVideoMs = pct(sTotal, 0.5);
            if (audio) {
                const AudioOut::Stats a = audio->takeStats();
                const int64_t need = std::max<int64_t>(
                    {a.chunks >= 2 ? audioDelayUs - int64_t(a.secondLeadMs * 1000) + 35000 : 0,
                     int64_t(lastVideoMs * 1000) + 5000, 70000});
                const int64_t want = std::min<int64_t>(need, 250000);
                if ((a.late && want > audioDelayUs) || want > audioDelayUs + 15000
                    || (!a.chunks && want < audioDelayUs - 3000))
                    audioDelayUs = want;
                if (a.chunks || a.late || a.underruns)
                    fprintf(stderr, "звук: кусков %d/с (встык %d), громче всего %.0f дБ, задержка %.0f мс (картинка %.1f), "
                                    "раньше срока ≥ %.1f мс; опоздало %d, сбоев %d, поправок %d, перескоков %d\n",
                            a.chunks, a.joined, a.peak > 0 ? 20 * std::log10(a.peak) : -99.0, audioDelayUs / 1000.0,
                            lastVideoMs, a.chunks ? a.secondLeadMs : 0.0, a.late, a.underruns, a.nudges, a.jumps);
            }
            for (Remote *r : remotes)
                if (const Tablet::Stats ts = r->link.takeStats(); ts.frames || ts.dropped)
                    fprintf(stderr, "%s: кадров %.0f/с, %.1f Мбит/с%s\n", r->label, ts.frames / sec, ts.bytes * 8 / sec / 1e6,
                            ts.dropped ? ("; выброшено " + std::to_string(ts.dropped) + " — Wi-Fi не успевал").c_str() : "");
            sPhone.clear();
            sNet.clear();
            sWait.clear();
            sDecode.clear();
            sTotal.clear();
            sShown = 0;
            sDropped0 = display.dropped;
            sBytes = 0;
            sStall = 0;
            statFrom = now;
            if (trace)
                fflush(trace);
            if (atrace)
                fflush(atrace);
        }

        std::vector<pollfd> fds;
        fds.push_back({display.fd(), POLLIN, 0});
        fds.push_back({display.timerFd(), POLLIN, 0});
        fds.push_back({tickTimer, POLLIN, 0});
        fds.push_back({decoder.fd(), POLLIN, 0});
        fds.push_back({lock.fd(), POLLIN, 0});
        for (Remote *r : remotes)
            if (r->link.fd() >= 0)
                fds.push_back({r->link.fd(), POLLIN, 0});
        if (tvAgent.fd() >= 0)
            fds.push_back({tvAgent.fd(), POLLIN, 0});
        for (Path *p : paths) {
            if (p->link.isOpen())
                fds.push_back({p->link.fd(), POLLIN, 0});
            if (p->link.wantsWrite())
                fds.push_back({p->link.inFd(), POLLOUT, 0});
            if (p->link.directFd() >= 0)
                fds.push_back({p->link.directFd(),
                               short(POLLIN | (p->link.directWantsWrite() ? POLLOUT : 0)), 0});
        }
        for (const auto &[fd, name] : input.devices())
            fds.push_back({fd, POLLIN, 0});
        if (poll(fds.data(), fds.size(), 5) < 0)
            continue;
        for (const pollfd &f : fds) {
            if (!(f.revents & (POLLIN | POLLOUT | POLLHUP | POLLERR)))
                continue;
            if (f.fd == display.fd()) {
                display.handleEvents();
            } else if (f.fd == display.timerFd()) {
                display.onTimer();
            } else if (f.fd == tickTimer) {
                onTick();
            } else if (f.fd == decoder.fd()) {
                onDecoded();
            } else if (f.fd == lock.fd()) {
                lock.poll(monoUs());
            } else if (f.fd == tabletScreen.link.fd()) {
                onRemote(tabletScreen);
            } else if (f.fd == tvScreen.link.fd()) {
                onRemote(tvScreen);
            } else if (f.fd == tvAgent.fd()) {
                onTvAgent();
            } else if (Path *pp = [&]() -> Path * {
                           for (Path *p : paths)
                               if (f.fd == p->link.fd() || f.fd == p->link.inFd() || f.fd == p->link.directFd())
                                   return p;
                           return nullptr;
                       }()) {
                Link &l = pp->link;
                if (f.revents & POLLOUT)
                    l.flush();
                msgPath = f.fd == l.directFd() ? 'd' : pp->cable ? 'c' : 'w';
                if (f.fd == l.fd() && (f.revents & (POLLIN | POLLHUP | POLLERR)) && !l.read(onMessage)) {
                    fprintf(stderr, "путь %s оборвался\n", pp->name);
                    l.close();
                    pp->retryAt = monoUs() + 1500000;
                } else if (f.fd == l.directFd() && (f.revents & (POLLIN | POLLHUP | POLLERR)) && !l.readDirect(onMessage)) {
                    fprintf(stderr, "прямой канал оборвался — картинка через SSH\n");
                    l.closeDirect();
                    pp->directAskAt = monoUs();
                }
            } else if (input.devices().count(f.fd)) {   // не закрытый по ходу (связь, прямой канал)
                input.read(f.fd);
            }
        }
    }
    fprintf(stderr, "выход\n");
    if (trace)
        fclose(trace);
    if (atrace)
        fclose(atrace);
    if (dump)
        fclose(dump);
    if (ptrace)
        fclose(ptrace);
    for (Path *p : paths)
        p->link.close();
    lock.close();
    brightness.close();   // приглушён — прежняя яркость
    display.close();
    return 0;
}
