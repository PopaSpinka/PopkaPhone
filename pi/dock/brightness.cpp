// НаомиФон, приставка на Pi — яркость монитора по DDC/CI (brightness.h). Протокол — VESA DDC/CI: запись хоста —
// 0x51, 0x80 | длина, данные, контрольная сумма (XOR адреса 0x6E и всех байтов); ответ монитора — 0x6E, 0x80 | длина,
// данные, сумма (XOR 0x50 и всех байтов). Прочитать VCP: 0x01 код → ответ 0x02, итог, код, тип, наибольшее, текущее.
// Записать: 0x03 код, значение. Между командами — паузы спецификации: 40 мс до чтения ответа, 50 мс после записи.
#include "brightness.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <dirent.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

constexpr unsigned char kVcpBrightness = 0x10;
constexpr int kStepMs = 20;       // переход: запись раз в столько
constexpr int kDimMs = 700;       // приглушить — за столько
constexpr int kRestoreMs = 400;   // вернуть — быстрее: человек уже здесь
constexpr int kTolerance = 2;     // ярче прежней больше чем на столько — поменяли кнопками монитора
constexpr const char *kSaved = "/var/lib/naomi-dock/brightness";   // «прежняя приглушённая» — служба упала приглушённой

void pause(int ms)
{
    timespec t{ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&t, nullptr);
}

// шина DDC выхода: /sys/class/drm/cardN-HDMI-A-1/ddc → …/i2c-13 → /dev/i2c-13
std::string ddcDevice(const std::string &connector)
{
    DIR *d = opendir("/sys/class/drm");
    if (!d)
        return {};
    std::string dev;
    while (dirent *e = readdir(d)) {
        const std::string n = e->d_name;
        if (n.size() <= connector.size() || n.compare(n.size() - connector.size() - 1, std::string::npos, "-" + connector) != 0)
            continue;
        char link[256];
        const ssize_t k = readlink(("/sys/class/drm/" + n + "/ddc").c_str(), link, sizeof link - 1);
        if (k <= 0)
            continue;
        link[k] = 0;
        const char *slash = strrchr(link, '/');
        dev = std::string("/dev/") + (slash ? slash + 1 : link);
        break;
    }
    closedir(d);
    return dev;
}

}   // namespace

void Brightness::open(const std::string &connector)
{
    m_dev = ddcDevice(connector);
    if (m_dev.empty()) {
        fprintf(stderr, "яркость: у %s нет шины DDC — приглушать нечем\n", connector.c_str());
        m_supported = 0;
        return;
    }
    m_fd = ::open(m_dev.c_str(), O_RDWR | O_CLOEXEC);
    if (m_fd < 0 || ioctl(m_fd, I2C_SLAVE, 0x37) < 0) {
        fprintf(stderr, "яркость: %s не открыть (%s; модуль i2c-dev — tools/pi install) — приглушать нечем\n",
                m_dev.c_str(), strerror(errno));
        if (m_fd >= 0)
            ::close(m_fd);
        m_fd = -1;
        m_supported = 0;
        return;
    }
    m_thread = std::thread([this] { run(); });
}

void Brightness::close()
{
    if (!m_thread.joinable())
        return;
    {
        std::lock_guard l(m_m);
        m_want = -1;
        m_quit = true;
    }
    m_cv.notify_one();
    m_thread.join();
    ::close(m_fd);
    m_fd = -1;
}

void Brightness::dim(int percent)
{
    {
        std::lock_guard l(m_m);
        m_want = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    }
    m_cv.notify_one();
}

void Brightness::step(int tenths)
{
    {
        std::lock_guard l(m_m);
        m_steps += tenths;
    }
    m_cv.notify_one();
}

void Brightness::restore()
{
    {
        std::lock_guard l(m_m);
        m_want = -1;
    }
    m_cv.notify_one();
}

void Brightness::run()
{
    // служба прошлый раз ушла приглушённой (упала, выключили Pi) — вернуть, как только монитор ответит
    if (FILE *f = fopen(kSaved, "r")) {
        if (fscanf(f, "%d %d", &m_orig, &m_dimmed) != 2 || m_orig < 0 || m_dimmed < 0)
            m_orig = m_dimmed = -1;
        fclose(f);
    }
    // монитор отвечает не сразу: приёмник только что сменил режим — молчит, пока ловит сигнал (19 ноября: через 0,3 с после
    // смены — нет ответа, через 3 с — есть); спрашивать раз в 3 с, минуту
    int tries = 0;
    auto probe = [&] {
        int cur = 0, max = 0;
        if (get(cur, max)) {
            m_supported = 1;
            fprintf(stderr, "яркость: монитор отвечает по DDC/CI (%s), сейчас %d из %d\n", m_dev.c_str(), cur, max);
            if (m_dimmed >= 0) {   // как бы ни стояла сейчас (могла упасть посреди перехода) — вернуть
                fprintf(stderr, "яркость: прошлый раз осталась приглушённой (%d вместо %d) — возвращаю\n", cur, m_orig);
                m_max = max;
                m_dimmed = cur;
                doRestore();
            }
        } else if (++tries >= 20) {
            m_supported = 0;
            fprintf(stderr, "яркость: монитор не отвечает по DDC/CI (%s) — DDC/CI в его меню? Без дока — сон\n",
                    m_dev.c_str());
        }
    };
    probe();
    for (;;) {
        std::unique_lock l(m_m);
        auto ready = [this] { return m_quit || m_want != m_have || m_steps; };
        if (m_supported < 0) {
            if (!m_cv.wait_for(l, std::chrono::seconds(3), ready)) {
                l.unlock();
                probe();
                continue;
            }
        } else {
            m_cv.wait(l, ready);
        }
        const int want = m_want, steps = m_steps;
        const bool quit = m_quit;
        m_steps = 0;
        l.unlock();
        if (want != m_have) {
            if (want < 0)
                doRestore();
            else
                doDim(want);
            m_have = want;
        }
        if (steps && !quit)
            doStep(steps);
        if (quit)
            break;
    }
}

void Brightness::doDim(int percent)
{
    int cur = 0, max = 0;
    if (!get(cur, max)) {
        fprintf(stderr, "яркость: монитор не ответил — не приглушаю\n");
        return;
    }
    m_supported = 1;
    m_max = max;
    const int to = (percent * max + 50) / 100;
    if (m_dimmed >= 0) {   // уже приглушено — новая доля из настроек, прежняя яркость та же
        if (cur > m_orig + kTolerance)   // ярче прежней — кнопками монитора (сами ярче не пишем): это и есть «прежняя»
            m_orig = cur;
    } else {
        if (to >= cur) {
            fprintf(stderr, "яркость: %d — и так не ярче %d %% (%d), не трогаю\n", cur, percent, to);
            return;
        }
        m_orig = cur;
    }
    if (to >= m_orig) {
        m_dimmed = cur;
        doRestore();
        return;
    }
    if (FILE *f = fopen(kSaved, "w")) {
        fprintf(f, "%d %d\n", m_orig, to);
        fclose(f);
    }
    m_dimmed = fade(cur, to, max, kDimMs, percent);
    if (m_dimmed != to) {
        fprintf(stderr, "яркость: приглушение прервали на %d\n", m_dimmed);
        return;
    }
    if (settle(to, percent))
        fprintf(stderr, "яркость: приглушил %d → %d (%d %%)\n", m_orig, to, percent);
}

// Клавиши яркости: по сетке десятых — с 77 темнее будет 70, ярче — 80. Приглушено (без дока) — не трогаем: «прежняя»
// яркость осталась бы старой.
void Brightness::doStep(int tenths)
{
    if (m_have >= 0 || m_dimmed >= 0)
        return;
    // только что (до 2 с) писали сами — от записанного: монитор сразу после записи молчит или отвечает прежним
    // значением (21 ноября: два нажатия подряд — оба раза «60», ступень терялась)
    const auto now = std::chrono::steady_clock::now();
    int cur = -1, max = 0;
    if (m_stepAt >= 0 && m_max > 0 && now - m_stepTime < std::chrono::seconds(2)) {
        cur = m_stepAt;
        max = m_max;
    } else if (get(cur, max, 1)) {
        m_supported = 1;
        m_max = max;
    } else {
        fprintf(stderr, "яркость: монитор не ответил — клавиша яркости без дела\n");
        return;
    }
    const int at = tenths < 0 ? (cur * 10 + max - 1) / max : cur * 10 / max;   // вниз — от верхней ступени, вверх — от нижней
    const int to = std::clamp(at + tenths, 0, 10) * max / 10;
    if (to != cur)
        set(to);
    m_stepAt = to;
    m_stepTime = std::chrono::steady_clock::now();
    m_level = to * 100 / max;
    fprintf(stderr, "яркость: клавишей %d → %d (%d %%)\n", cur, to, to * 100 / max);
}

void Brightness::doRestore()
{
    if (m_dimmed < 0)
        return;
    int cur = -1, max = 0;
    if (get(cur, max, 1)) {   // одна попытка: после череды записей монитор отвечает не сразу, а человек ждёт
        m_max = max;
        if (cur > m_orig + kTolerance) {
            fprintf(stderr, "яркость: пока было приглушено, поставили %d кнопками монитора — оставляю\n", cur);
            unlink(kSaved);
            m_dimmed = -1;
            return;
        }
    } else {
        cur = m_dimmed;   // не ответил — вести от того, что писали сами
    }
    if (m_max <= 0) {
        fprintf(stderr, "яркость: монитор молчит — вернуть %d вернёт следующий запуск\n", m_orig);
        return;
    }
    const int from = cur;
    m_dimmed = fade(cur, m_orig, m_max, kRestoreMs, -1);
    if (m_dimmed != m_orig) {
        fprintf(stderr, "яркость: возврат прервали на %d\n", m_dimmed);
        return;
    }
    if (!settle(m_orig, -1)) {
        std::lock_guard l(m_m);
        if (m_want == -1)
            fprintf(stderr, "яркость: вернуть %d → %d не подтвердилось — вернёт следующий запуск\n", from, m_orig);
        return;
    }
    fprintf(stderr, "яркость: вернул %d → %d\n", from, m_orig);
    unlink(kSaved);
    m_dimmed = -1;
}

// После перехода: монитор берёт не каждую запись (19 ноября, 40 переходов: 1–2 в конце не там — 77, 78, 11, нет
// ответа; посреди перехода обрыв — 64 вместо записанных 54), — читать и дописывать, пока не встанет target. По
// одному чтению за круг, между ними — смотреть, не просят ли другое (клавиша не ждёт проверки)
bool Brightness::settle(int target, int want)
{
    for (int i = 0; i < 8; ++i) {
        {
            std::lock_guard l(m_m);
            if (m_want != want)
                return false;
        }
        int now = -1, max = 0;
        if (get(now, max, 1) && now == target)
            return true;
        set(target);
        pause(i < 2 ? 0 : 100);
    }
    return false;
}

// Плавно from → to за ms: запись каждые kStepMs (монитор берёт каждую — замер: через 16 мс все, через 8 — теряет), по
// кривой «на глаз»: яркость монитора — свет, глаз видит его примерно как корень 2,2, — путь равномерный в корнях,
// начало и конец мягкие (половина косинуса). Просят другое посреди (want сменился) — стоп. Возвращает записанное
// последним
int Brightness::fade(int from, int to, int max, int ms, int want)
{
    const double a = std::pow(double(from) / max, 1 / 2.2), b = std::pow(double(to) / max, 1 / 2.2);
    const int steps = std::max(1, ms / kStepMs);
    const auto t0 = std::chrono::steady_clock::now();
    int last = from, writes = 0, step = 0;
    for (int i = 1; i <= steps; ++i) {
        {
            std::lock_guard l(m_m);
            if (m_want != want)
                break;
        }
        const double e = 0.5 - 0.5 * std::cos(M_PI * i / steps);
        const int v = i == steps ? to : int(std::lround(max * std::pow(a + (b - a) * e, 2.2)));
        if (v != last && put(v)) {
            step = std::max(step, std::abs(v - last));
            last = v;
            ++writes;
        }
        std::this_thread::sleep_until(t0 + std::chrono::milliseconds(kStepMs * i));
    }
    const double took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    fprintf(stderr, "яркость: переход %d → %d — %d записей за %.0f мс, шаг до %d\n", from, last, writes, took, step);
    pause(50);   // после записи — пауза спецификации, дальше можно спрашивать
    return last;
}

bool Brightness::command(const unsigned char *msg, int n)
{
    unsigned char b[8];
    unsigned char sum = 0x6E;
    for (int i = 0; i < n; ++i)
        sum ^= (b[i] = msg[i]);
    b[n] = sum;
    return write(m_fd, b, n + 1) == n + 1;
}

bool Brightness::get(int &cur, int &max, int tries)
{
    const unsigned char req[] = {0x51, 0x82, 0x01, kVcpBrightness};
    for (int i = 0; i < tries; ++i) {
        if (i)
            pause(100);
        if (!command(req, sizeof req))
            continue;
        pause(40);
        unsigned char r[11];
        if (read(m_fd, r, sizeof r) != sizeof r)
            continue;
        unsigned char sum = 0x50;
        for (int k = 0; k < 10; ++k)
            sum ^= r[k];
        // 6E 88 02 итог код тип max_hi max_lo cur_hi cur_lo сумма; «занят» — 6E 80 BE
        if (r[0] != 0x6E || r[1] != 0x88 || r[2] != 0x02 || r[4] != kVcpBrightness || sum != r[10])
            continue;
        if (r[3] != 0)
            return false;   // монитор не знает яркости
        max = r[6] << 8 | r[7];
        cur = r[8] << 8 | r[9];
        pause(10);
        return max > 0;
    }
    return false;
}

bool Brightness::put(int value)
{
    const unsigned char req[] = {0x51, 0x84, 0x03, kVcpBrightness, (unsigned char)(value >> 8), (unsigned char)value};
    return command(req, sizeof req);
}

bool Brightness::set(int value)
{
    const bool ok = put(value);
    pause(50);
    return ok;
}
