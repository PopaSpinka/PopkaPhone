#include "tablet.h"

#include "link.h"   // monoUs
#include "ssl_min.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

// Кадры, ещё не отданные TLS, — не больше стольких: больше — Wi-Fi планшета не успевает, очередь прочь, дальше с
// опорного (он и просится у телефона). Ядру — не больше kNotSent неотправленного: иначе кадры копились бы в сокете
// и шли бы на планшет с опозданием
constexpr int kMaxFrames = 6;
constexpr size_t kMaxFrameBytes = 3u << 20;
constexpr int kNotSent = 128 << 10;
constexpr int64_t kSilentUs = 3000000;      // от планшета ничего столько (пинг — раз в 0,5 с) — закрыть
constexpr int64_t kHandshakeUs = 3000000;   // TLS и ключ — не дольше
constexpr int kMaxHandshakes = 4;

std::vector<uint8_t> message(uint8_t type, const void *body, size_t n)
{
    std::vector<uint8_t> m(5 + n);
    const uint32_t len = uint32_t(n + 1);
    memcpy(m.data(), &len, 4);
    m[4] = type;
    if (n)
        memcpy(m.data() + 5, body, n);
    return m;
}

// Ошибка последнего вызова SSL_* (очередь ошибок потока — до конца: перед следующим вызовом она должна быть пуста)
std::string sslError()
{
    const unsigned long e = ERR_get_error();
    std::string s = !e ? (errno ? strerror(errno) : "связь закрыта") : std::string();
    if (e) {
        char b[256];
        ERR_error_string_n(e, b, sizeof b);
        s = b;
    }
    ERR_clear_error();
    return s;
}

// Перед каждым SSL_read / SSL_write / SSL_accept / SSL_shutdown очередь ошибок потока — пустая: иначе SSL_get_error
// судит по прошлой ошибке. Было: после обрыва прошлого планшета первое чтение нового считалось обрывом — новый
// подключался со второго раза (через 0,5 с)
void sslReady()
{
    ERR_clear_error();
    errno = 0;
}

void kick(int fd)
{
    const uint64_t one = 1;
    if (write(fd, &one, 8) < 0) {
    }
}

std::string peer(int fd)
{
    sockaddr_in a{};
    socklen_t len = sizeof a;
    char ip[INET_ADDRSTRLEN] = "?";
    if (getpeername(fd, reinterpret_cast<sockaddr *>(&a), &len) == 0)
        inet_ntop(AF_INET, &a.sin_addr, ip, sizeof ip);
    return ip;
}

// ждать готовности fd до deadline (мкс, monoUs); false — не дождались
bool waitFd(int fd, short events, int64_t deadline)
{
    const int64_t left = deadline - monoUs();
    if (left <= 0)
        return false;
    pollfd p{fd, events, 0};
    return poll(&p, 1, int(left / 1000) + 1) > 0;
}

bool readToken(const std::string &path, uint8_t out[32])
{
    FILE *f = fopen(path.c_str(), "r");
    if (!f)
        return false;
    char hex[80] = {};
    const bool ok = fgets(hex, sizeof hex, f) != nullptr;
    fclose(f);
    if (!ok)
        return false;
    for (int i = 0; i < 32; ++i) {
        unsigned v;
        if (sscanf(hex + 2 * i, "%2x", &v) != 1)
            return false;
        out[i] = uint8_t(v);
    }
    return true;
}

} // namespace

bool Tablet::start(const std::string &dir, int port, const std::string &label, const std::string &certDir,
                   const std::string &pair)
{
    m_dir = dir;
    m_certDir = certDir.empty() ? dir : certDir;
    m_label = label;
    m_pair = pair;
    m_kick = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    m_events = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    m_listen = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    const int one = 1;
    setsockopt(m_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(uint16_t(port));
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (m_kick < 0 || m_events < 0 || m_listen < 0 || bind(m_listen, reinterpret_cast<sockaddr *>(&a), sizeof a) != 0
        || listen(m_listen, 4) != 0) {
        fprintf(stderr, "%s: порт %d не открыть (%s) — без него\n", m_label.c_str(), port, strerror(errno));
        stop();
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        uint8_t token[32];
        fprintf(stderr, "%s: жду на порту %d%s\n", m_label.c_str(), port,
                loadKeys() && readToken(m_dir + "/token", token) ? ""
                                                                 : (" — ключей нет (" + m_pair + "), подключения не принимаю").c_str());
    }
    m_thread = std::thread([this] { run(); });
    return true;
}

void Tablet::stop()
{
    m_quit = true;
    if (m_thread.joinable()) {
        kick(m_kick);
        m_thread.join();
    }
    while (m_handshakes > 0)   // потоки рукопожатий кончаются сами не позже kHandshakeUs
        usleep(10000);
    for (const Ready &r : m_ready) {
        SSL_free(static_cast<SSL *>(r.ssl));
        ::close(r.fd);
    }
    m_ready.clear();
    for (int *fd : {&m_listen, &m_kick, &m_events})
        if (*fd >= 0) {
            ::close(*fd);
            *fd = -1;
        }
    if (m_ctx) {
        SSL_CTX_free(m_ctx);
        m_ctx = nullptr;
    }
}

// Сертификат и его ключ — с диска; сменились (tools/tablet pair сделал новые) — заново. Под m_mutex
bool Tablet::loadKeys()
{
    const std::string cert = m_certDir + "/cert.pem", key = m_certDir + "/key.pem";
    struct stat st{};
    if (stat(cert.c_str(), &st) != 0)
        return false;
    const long long t = (long long)st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec;
    if (m_ctx && t == m_certTime)
        return true;
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx)
        return false;
    SSL_CTX_ctrl(ctx, kSslCtrlSetMinProto, kTls13, nullptr);
    SSL_CTX_set_num_tickets(ctx, 0);   // сеансы не возобновляем — билеты не нужны
    if (SSL_CTX_use_certificate_chain_file(ctx, cert.c_str()) != 1
        || SSL_CTX_use_PrivateKey_file(ctx, key.c_str(), kSslFiletypePem) != 1 || SSL_CTX_check_private_key(ctx) != 1) {
        fprintf(stderr, "%s: сертификат не загрузить (%s)\n", m_label.c_str(), sslError().c_str());
        SSL_CTX_free(ctx);
        return false;
    }
    if (m_ctx)
        SSL_CTX_free(m_ctx);   // подключения со старым держат его сами (счётчик ссылок OpenSSL)
    m_ctx = ctx;
    m_certTime = t;
    return true;
}

// Рукопожатие — своим потоком (поток планшета не ждёт чужих: сканер портов или медленный Wi-Fi): TLS, потом первое
// слово — Auth с ключом. Не тот ключ или нет за kHandshakeUs — закрыто
void Tablet::handshake(int fd)
{
    const std::string from = peer(fd);
    const int64_t deadline = monoUs() + kHandshakeUs;
    SSL *ssl = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (loadKeys())
            ssl = SSL_new(m_ctx);
    }
    const char *why = nullptr;
    std::string name;
    const std::string noKeys = "ключей нет (" + m_pair + ")";
    if (!ssl) {
        why = noKeys.c_str();
    } else {
        SSL_set_fd(ssl, fd);
        SSL_ctrl(ssl, kSslCtrlMode, kSslModePartialWrite | kSslModeMovingBuffer, nullptr);
        for (;;) {
            sslReady();
            const int r = SSL_accept(ssl);
            if (r == 1)
                break;
            const int e = SSL_get_error(ssl, r);
            if (!(e == kSslErrorWantRead ? waitFd(fd, POLLIN, deadline)
                                         : e == kSslErrorWantWrite && waitFd(fd, POLLOUT, deadline))) {
                why = "TLS не сложился";
                fprintf(stderr, "%s %s: TLS — %s\n", m_label.c_str(), from.c_str(), sslError().c_str());
                break;
            }
        }
    }
    std::vector<uint8_t> in;
    while (!why) {   // первое слово: u32 длина, u8 Auth, ключ, имя
        if (in.size() >= 5) {
            uint32_t len;
            memcpy(&len, in.data(), 4);
            if (len < 33 || len > 1024 || in[4] != tablet::Auth) {
                why = "первое слово — не ключ";
                break;
            }
            if (in.size() >= 4 + len) {
                uint8_t token[32];
                if (!readToken(m_dir + "/token", token)) {
                    why = noKeys.c_str();
                    break;
                }
                uint8_t diff = 0;   // сравнение без раннего выхода
                for (int i = 0; i < 32; ++i)
                    diff |= uint8_t(token[i] ^ in[5 + i]);
                if (diff) {
                    why = "чужой ключ";
                    break;
                }
                name.assign(reinterpret_cast<const char *>(in.data()) + 37, len - 33);
                break;
            }
        }
        uint8_t b[1024];
        sslReady();
        const int r = SSL_read(ssl, b, sizeof b);
        if (r > 0) {
            in.insert(in.end(), b, b + r);
            continue;
        }
        const int e = SSL_get_error(ssl, r);
        if (!(e == kSslErrorWantRead ? waitFd(fd, POLLIN, deadline)
                                     : e == kSslErrorWantWrite && waitFd(fd, POLLOUT, deadline)))
            why = "ключ не сказан";
    }
    if (why) {
        fprintf(stderr, "%s %s: %s — закрыто\n", m_label.c_str(), from.c_str(), why);
        if (ssl)
            SSL_free(ssl);
        ::close(fd);
    } else {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ready.push_back({fd, ssl, name.empty() ? from : name + " (" + from + ")"});
        kick(m_kick);
    }
    --m_handshakes;
}

void Tablet::run()
{
    int fd = -1;
    SSL *ssl = nullptr;
    std::string who;
    std::vector<uint8_t> in, out;
    size_t off = 0;
    int64_t lastData = 0;
    auto drop = [&](const std::string &why) {
        if (fd < 0)
            return;
        fprintf(stderr, "%s %s: %s — отключён\n", m_label.c_str(), who.c_str(), why.c_str());
        sslReady();
        SSL_shutdown(ssl);
        ERR_clear_error();
        SSL_free(ssl);
        ::close(fd);
        fd = -1;
        ssl = nullptr;
        m_connected = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_frames.clear();
            m_frameBytes = 0;
            m_ctrl.clear();
        }
        post(255, nullptr, 0);
    };
    while (!m_quit) {
        bool pending;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            pending = off < out.size() || !m_ctrl.empty() || !m_frames.empty() || m_cursorDirty || m_imageDirty;
        }
        pollfd fds[3] = {{m_listen, POLLIN, 0}, {m_kick, POLLIN, 0}, {fd, short(POLLIN | (pending ? POLLOUT : 0)), 0}};
        if (poll(fds, fd >= 0 ? 3 : 2, 200) < 0 && errno != EINTR)
            break;
        const int64_t now = monoUs();
        if (fds[0].revents & POLLIN)
            for (int c; (c = accept4(m_listen, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC)) >= 0;) {
                if (m_handshakes >= kMaxHandshakes) {
                    ::close(c);
                    continue;
                }
                ++m_handshakes;
                std::thread([this, c] { handshake(c); }).detach();
            }
        if (fds[1].revents & POLLIN) {
            uint64_t v;
            if (read(m_kick, &v, 8) < 0) {
            }
        }
        std::vector<Ready> ready;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ready.swap(m_ready);
        }
        for (size_t i = 0; i < ready.size(); ++i) {
            if (i + 1 < ready.size()) {   // сразу несколько — берём последний
                SSL_free(static_cast<SSL *>(ready[i].ssl));
                ::close(ready[i].fd);
                continue;
            }
            drop("пришёл новый");
            fd = ready[i].fd;
            ssl = static_cast<SSL *>(ready[i].ssl);
            who = ready[i].name;
            const int one = 1, lowat = kNotSent;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
            setsockopt(fd, IPPROTO_TCP, TCP_NOTSENT_LOWAT, &lowat, sizeof lowat);
            in.clear();
            out.clear();
            off = 0;
            lastData = now;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_ctrl.clear();
                m_frames.clear();
                m_frameBytes = 0;
                m_waitKey = true;
                if (!m_hello.empty())
                    m_ctrl.push_back(m_hello);
                if (!m_splash.empty())
                    m_ctrl.push_back(m_splash);
                if (!m_light.empty())
                    m_ctrl.push_back(m_light);
                m_imageDirty = !m_image.empty();
                m_cursorDirty = !m_cursorMsg.empty();
                m_stats = Stats();
            }
            m_connected = true;
            m_keyWanted = true;
            fprintf(stderr, "%s %s: подключён\n", m_label.c_str(), who.c_str());
            post(0, reinterpret_cast<const uint8_t *>(who.data()), who.size());
        }
        if (fd < 0)
            continue;
        // от планшета
        for (;;) {
            uint8_t b[16384];
            sslReady();
            const int r = SSL_read(ssl, b, sizeof b);
            if (r > 0) {
                in.insert(in.end(), b, b + r);
                lastData = now;
                continue;
            }
            const int e = SSL_get_error(ssl, r);
            if (e != kSslErrorWantRead && e != kSslErrorWantWrite)
                drop(e == kSslErrorZeroReturn ? "закрыл связь" : "связь оборвалась (" + sslError() + ")");
            break;
        }
        if (fd < 0)
            continue;
        size_t at = 0;
        while (in.size() - at >= 5) {
            uint32_t len;
            memcpy(&len, in.data() + at, 4);
            if (len == 0 || len > (1u << 20)) {
                drop("сообщение не по протоколу");
                break;
            }
            if (in.size() - at < 4 + len)
                break;
            const uint8_t type = in[at + 4], *body = in.data() + at + 5;
            const size_t n = len - 1;
            if (type == tablet::Ping && n >= 8) {
                uint8_t pong[16];
                const int64_t mine = monoUs();
                memcpy(pong, body, 8);
                memcpy(pong + 8, &mine, 8);
                std::lock_guard<std::mutex> lock(m_mutex);
                m_ctrl.push_back(message(tablet::Pong, pong, sizeof pong));
            } else if (type == tablet::KeyFrame) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_frames.clear();
                m_frameBytes = 0;
                m_waitKey = true;
                m_keyWanted = true;
            } else if (type == tablet::Pointer || type == tablet::Scroll || type == tablet::View
                       || type == tablet::Wake || type == tablet::Screen) {
                post(type, body, n);
            }
            at += 4 + len;
        }
        if (fd < 0)
            continue;
        in.erase(in.begin(), in.begin() + long(at));
        if (now - lastData > kSilentUs) {
            drop("молчит 3 с");
            continue;
        }
        // планшету: сообщение за сообщением — сначала служебные (Hello, Pong), потом курсор, потом кадры; ядру — не
        // больше kNotSent неотправленного
        for (;;) {
            if (off == out.size()) {
                int notSent = 0;
                if (ioctl(fd, SIOCOUTQNSD, &notSent) == 0 && notSent > kNotSent)
                    break;
                out.clear();
                off = 0;
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_ctrl.empty()) {
                    out.swap(m_ctrl.front());
                    m_ctrl.pop_front();
                } else if (m_imageDirty) {
                    out = m_image;
                    m_imageDirty = false;
                } else if (m_cursorDirty) {
                    out = m_cursorMsg;
                    m_cursorDirty = false;
                } else if (!m_frames.empty()) {
                    out.swap(m_frames.front());
                    m_frames.pop_front();
                    m_frameBytes -= out.size();
                    ++m_stats.frames;
                    m_stats.bytes += out.size();
                } else {
                    break;
                }
            }
            sslReady();
            const int r = SSL_write(ssl, out.data() + off, int(std::min<size_t>(out.size() - off, INT_MAX)));
            if (r > 0) {
                off += size_t(r);
                continue;
            }
            const int e = SSL_get_error(ssl, r);
            if (e != kSslErrorWantWrite && e != kSslErrorWantRead)
                drop("не отправить (" + sslError() + ")");
            break;
        }
    }
    drop("приставка выходит");
}

void Tablet::post(uint8_t type, const uint8_t *body, size_t n)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_in.push_back({type, std::vector<uint8_t>(body, body + n), monoUs()});
    }
    kick(m_events);
}

std::vector<Tablet::Event> Tablet::take()
{
    uint64_t v;
    if (read(m_events, &v, 8) < 0) {
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Event> out(std::make_move_iterator(m_in.begin()), std::make_move_iterator(m_in.end()));
    m_in.clear();
    return out;
}

void Tablet::hello(const std::string &json)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_hello = message(tablet::Hello, json.data(), json.size());
    if (!m_connected)
        return;
    // кадры прежнего потока новому декодеру не годятся: дальше — с опорного нового (и попросить его: первый опорный
    // нового сеанса мог прийти раньше, чем главный круг сказал Hello, и уйти с прежней очередью)
    m_frames.clear();
    m_frameBytes = 0;
    m_waitKey = true;
    m_keyWanted = true;
    m_ctrl.push_back(m_hello);
    kick(m_kick);
}

void Tablet::splash(const std::string &json)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint8_t> m = message(tablet::Splash, json.data(), json.size());
    if (m == m_splash)
        return;
    m_splash.swap(m);
    if (!m_connected)
        return;
    m_ctrl.push_back(m_splash);
    kick(m_kick);
}

void Tablet::light(bool automatic, int level)
{
    const uint8_t b[2] = {uint8_t(automatic), uint8_t(level)};
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint8_t> m = message(tablet::Light, b, sizeof b);
    if (m == m_light)
        return;
    m_light.swap(m);
    if (!m_connected)
        return;
    m_ctrl.push_back(m_light);
    kick(m_kick);
}

void Tablet::send(uint8_t type, const uint8_t *body, size_t n)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connected)
        return;
    m_ctrl.push_back(message(type, body, n));
    kick(m_kick);
}

void Tablet::video(const uint8_t *body, size_t n, bool known, int64_t offset)
{
    if (!m_connected || n < 18)
        return;
    const bool key = body[16] & 1;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_waitKey && !key)
        return;
    if (int(m_frames.size()) >= kMaxFrames || m_frameBytes > kMaxFrameBytes) {   // Wi-Fi планшета не успевает
        m_stats.dropped += int(m_frames.size());
        m_frames.clear();
        m_frameBytes = 0;
        if (!key) {
            m_waitKey = true;
            m_keyWanted = true;
            ++m_stats.dropped;
            return;
        }
    }
    m_waitKey = false;
    std::vector<uint8_t> m(5 + 17 + (n - 17));
    const uint32_t len = uint32_t(m.size() - 4);
    memcpy(m.data(), &len, 4);
    m[4] = tablet::Video;
    int64_t capture;
    memcpy(&capture, body, 8);
    const int64_t capturePi = known ? capture - offset : 0;
    memcpy(m.data() + 5, &capture, 8);
    memcpy(m.data() + 13, &capturePi, 8);
    m[21] = body[16];
    memcpy(m.data() + 22, body + 17, n - 17);
    m_frameBytes += m.size();
    m_frames.push_back(std::move(m));
    kick(m_kick);
}

void Tablet::cursor(bool shown, float x, float y)
{
    uint8_t b[9];
    b[0] = shown;
    memcpy(b + 1, &x, 4);
    memcpy(b + 5, &y, 4);
    std::vector<uint8_t> m = message(tablet::Cursor, b, sizeof b);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m == m_cursorMsg)
        return;
    m_cursorMsg.swap(m);
    if (m_connected) {
        m_cursorDirty = true;
        kick(m_kick);
    }
}

void Tablet::cursorImage(const std::vector<uint32_t> &argb, int n, int hotX, int hotY)
{
    std::vector<uint8_t> b(6 + argb.size() * 4);
    const uint16_t h[3] = {uint16_t(n), uint16_t(hotX), uint16_t(hotY)};
    memcpy(b.data(), h, 6);
    memcpy(b.data() + 6, argb.data(), argb.size() * 4);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_image = message(tablet::CursorImage, b.data(), b.size());
    if (m_connected) {
        m_imageDirty = true;
        kick(m_kick);
    }
}

Tablet::Stats Tablet::takeStats()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const Stats s = m_stats;
    m_stats = Stats();
    return s;
}
