#include "link.h"

#include "stream_protocol.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <random>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int64_t monoUs()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return int64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}

bool Link::open(const std::string &host, const std::string &keyDir, uint64_t ticket, bool use, bool quiet)
{
    close();
    int toSsh[2], fromSsh[2];
    if (pipe2(toSsh, O_CLOEXEC) || pipe2(fromSsh, O_CLOEXEC))
        return false;
    const std::string key = keyDir + "/naomi_stream", known = "UserKnownHostsFile=" + keyDir + "/known_hosts_phone";
    const std::string target = "phablet@" + host;
    const std::string alive = "ServerAliveInterval=" + std::to_string(quiet ? stream::kParkedPingS : 2);
    m_pid = fork();
    if (m_pid == 0) {
        dup2(toSsh[0], 0);
        dup2(fromSsh[1], 1);
        // обрыв — заметить за ~6 с и на уровне SSH (наш пинг — 10 раз в секунду, его ответы — живость); тихий путь —
        // раз в kParkedPingS (там и наш пинг так же редок).
        // Шифр — AES-GCM (в Pi 5 и телефоне — аппаратный AES): опорный кадр 390 КБ идёт 34–40 мс против 44–48 мс
        // у ChaCha20 по умолчанию (docs/PI.md, «Анимации и след прозрачного»)
        execlp("ssh", "ssh", "-i", key.c_str(), "-o", "IdentitiesOnly=yes", "-o", "BatchMode=yes",
               "-o", "ConnectTimeout=3", "-o", alive.c_str(), "-o", "ServerAliveCountMax=3",
               "-o", known.c_str(), "-o", "HostKeyAlias=naomiphone",
               "-o", "Ciphers=aes128-gcm@openssh.com,chacha20-poly1305@openssh.com",
               "-W", "127.0.0.1:8478", target.c_str(),
               static_cast<char *>(nullptr));
        _exit(127);
    }
    ::close(toSsh[0]);
    ::close(fromSsh[1]);
    if (m_pid < 0) {
        ::close(toSsh[1]);
        ::close(fromSsh[0]);
        return false;
    }
    m_in = toSsh[1];
    m_out = fromSsh[0];
    fcntl(m_out, F_SETFL, fcntl(m_out, F_GETFL) | O_NONBLOCK);
    fcntl(m_in, F_SETFL, fcntl(m_in, F_GETFL) | O_NONBLOCK);
    m_host = host;
    m_buf.clear();
    m_sshOut.clear();
    m_sshOff = 0;
    m_broken = false;
    m_quiet = quiet;
    m_opened = m_lastData = monoUs();
    uint8_t join[9];
    memcpy(join, &ticket, 8);
    join[8] = use ? 1 : 0;
    send(stream::Join, join, sizeof join);
    return true;
}

void Link::close()
{
    closeDirect();
    if (m_in >= 0)
        ::close(m_in);
    if (m_out >= 0)
        ::close(m_out);
    m_in = m_out = -1;
    m_sshOut.clear();
    m_sshOff = 0;
    if (m_pid > 0) {
        kill(m_pid, SIGTERM);
        waitpid(m_pid, nullptr, 0);
    }
    m_pid = -1;
}

// всё, что пришло в fd (неблокирующий), — разобрать на сообщения; false — поток кончился или не наш
bool Link::drain(int fd, std::vector<uint8_t> &buf, int64_t &last, const Handler &on)
{
    uint8_t chunk[1 << 16];
    for (;;) {
        const ssize_t n = ::read(fd, chunk, sizeof chunk);
        if (n > 0) {
            buf.insert(buf.end(), chunk, chunk + n);
            last = monoUs();
            continue;
        }
        if (n == 0)
            return false;   // ssh вышел / прямой закрыт
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            break;
        if (errno != EINTR)
            return false;
    }
    size_t at = 0;
    while (buf.size() - at >= 5) {
        uint32_t len;
        memcpy(&len, buf.data() + at, 4);
        if (len < 1 || len > (64u << 20))
            return false;   // не наш поток
        if (buf.size() - at - 4 < len)
            break;
        on(buf[at + 4], buf.data() + at + 5, len - 1);
        at += 4 + len;
    }
    buf.erase(buf.begin(), buf.begin() + long(at));
    return true;
}

bool Link::read(const Handler &on)
{
    return drain(m_out, m_buf, m_lastData, on);
}

void Link::frame(std::vector<uint8_t> &out, uint8_t type, const void *body, size_t n)
{
    const uint32_t len = uint32_t(1 + n);
    const size_t at = out.size();
    out.resize(at + 5 + n);
    memcpy(out.data() + at, &len, 4);
    out[at + 4] = type;
    if (n)
        memcpy(out.data() + at + 5, body, n);
}

bool Link::push(int fd, std::vector<uint8_t> &out, size_t &off)
{
    while (off < out.size()) {
        const ssize_t w = ::write(fd, out.data() + off, out.size() - off);
        if (w > 0)
            off += size_t(w);
        else if (w < 0 && errno == EINTR)
            continue;
        else if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;   // дошлётся по POLLOUT
        else
            return false;
    }
    if (off == out.size()) {
        out.clear();
        off = 0;
    } else if (off > (1u << 20)) {   // отосланное — прочь, чтобы очередь не росла без нужды
        out.erase(out.begin(), out.begin() + long(off));
        off = 0;
    }
    return true;
}

void Link::flush()
{
    if (m_in >= 0 && !push(m_in, m_sshOut, m_sshOff))
        m_broken = true;
    if (m_direct >= 0 && !push(m_direct, m_directOut, m_directOff))
        m_directData = 0;   // прямой — умер: main закроет по тишине
    if (m_sshOut.size() - m_sshOff > (4u << 20))
        m_broken = true;
}

void Link::send(uint8_t type, const void *body, size_t n)
{
    if (m_in < 0)
        return;
    frame(m_sshOut, type, body, n);
    flush();
}

bool Link::openDirect(const uint8_t addr[4], uint16_t port, const uint8_t pass[16])
{
    closeDirect();
    const int s = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (s < 0)
        return false;
    sockaddr_in a{};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    memcpy(&a.sin_addr.s_addr, addr, 4);
    timeval tv{1, 0};   // кабель — подключение за миллисекунды; нет за секунду — и не будет
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
    if (connect(s, reinterpret_cast<sockaddr *>(&a), sizeof a) < 0) {
        ::close(s);
        return false;
    }
    const int one = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
    m_direct = s;
    m_directBuf.clear();
    m_directOut.clear();
    m_directOff = 0;
    m_directData = monoUs();
    sendDirect(stream::DirectJoin, pass, 16);
    return true;
}

void Link::closeDirect()
{
    if (m_direct >= 0)
        ::close(m_direct);
    m_direct = -1;
    m_directBuf.clear();
    m_directOut.clear();
    m_directOff = 0;
}

bool Link::readDirect(const Handler &on)
{
    return m_direct >= 0 && drain(m_direct, m_directBuf, m_directData, on);
}

void Link::sendDirect(uint8_t type, const void *body, size_t n)
{
    if (m_direct < 0)
        return;
    frame(m_directOut, type, body, n);
    flush();
}

void Link::input(uint32_t seq, uint8_t type, const void *body, size_t n)
{
    std::vector<uint8_t> m(5 + n);
    memcpy(m.data(), &seq, 4);
    m[4] = type;
    if (n)
        memcpy(m.data() + 5, body, n);
    send(stream::Input, m.data(), m.size());
}
