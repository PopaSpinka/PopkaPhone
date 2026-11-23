#include "net.h"

#include <QString>

#include <arpa/inet.h>
#include <dirent.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

Net::Net(QObject *parent)
    : BusObject("/org/naomi/Phone/Net", "org.naomi.Phone.Net", parent)
{
}

namespace {

// Сокеты TCP из /proc/net/tcp{,6}: inode → (адрес телефона, порт), только IPv4 (в tcp6 — вида ::ffff:a.b.c.d).
struct SockInfo { uint32_t ip; int port; };
std::map<unsigned long, SockInfo> tcpSockets()
{
    std::map<unsigned long, SockInfo> out;
    for (const char *file : {"/proc/net/tcp", "/proc/net/tcp6"}) {
        FILE *f = fopen(file, "re");
        if (!f)
            continue;
        char line[512];
        (void)!fgets(line, sizeof line, f);   // заголовок
        while (fgets(line, sizeof line, f)) {
            char local[64];
            unsigned long inode = 0;
            int port = 0;
            if (sscanf(line, "%*d: %63[0-9A-Fa-f]:%x %*s %*x %*s %*s %*s %*d %*d %lu", local, &port, &inode) != 3)
                continue;
            uint32_t ip = 0;
            const size_t len = strlen(local);
            if (len == 8)
                ip = uint32_t(strtoul(local, nullptr, 16));   // порядок байт сети, как в памяти
            else if (len == 32 && strncmp(local, "0000000000000000FFFF0000", 24) == 0)
                ip = uint32_t(strtoul(local + 24, nullptr, 16));
            else
                continue;
            out[inode] = {ip, port};
        }
        fclose(f);
    }
    return out;
}

} // namespace

// Чей сокет 127.0.0.1:port (sshd, ssh -W) → его процесс → его же сокет на порту 22 → на каком адресе телефона
// принят → чей это адрес. Сокеты sshd (он «не для отладки») читает только root — поэтому здесь, а не в оболочке.
QString Net::SshInterface(uint port)
{
    const int sshdPort = int(port);
    const auto socks = tcpSockets();
    unsigned long forward = 0;   // сокет sshd в нашу сторону
    for (const auto &[inode, si] : socks)
        if (si.port == sshdPort && si.ip == htonl(INADDR_LOOPBACK))
            forward = inode;
    if (!forward)
        return {};
    uint32_t local = 0;   // адрес телефона, на который пришёл SSH этого процесса
    if (DIR *proc = opendir("/proc")) {
        while (dirent *e = readdir(proc)) {
            if (!isdigit(static_cast<unsigned char>(e->d_name[0])) || local)
                continue;
            const std::string base = std::string("/proc/") + e->d_name;
            char comm[32] = {};
            if (FILE *c = fopen((base + "/comm").c_str(), "re")) {
                (void)!fgets(comm, sizeof comm, c);
                fclose(c);
            }
            if (strncmp(comm, "sshd", 4) != 0)
                continue;
            std::vector<unsigned long> mine;
            bool has = false;
            if (DIR *fds = opendir((base + "/fd").c_str())) {
                while (dirent *f = readdir(fds)) {
                    char target[64];
                    const ssize_t n = readlink((base + "/fd/" + f->d_name).c_str(), target, sizeof target - 1);
                    unsigned long inode;
                    if (n <= 0)
                        continue;
                    target[n] = 0;
                    if (sscanf(target, "socket:[%lu]", &inode) == 1) {
                        mine.push_back(inode);
                        has |= inode == forward;
                    }
                }
                closedir(fds);
            }
            if (!has)
                continue;
            for (unsigned long inode : mine)
                if (auto it = socks.find(inode); it != socks.end() && it->second.port == 22)
                    local = it->second.ip;
        }
        closedir(proc);
    }
    if (!local)
        return {};
    QString name;
    ifaddrs *list = nullptr;
    if (getifaddrs(&list) == 0) {
        for (ifaddrs *a = list; a; a = a->ifa_next)
            if (a->ifa_addr && a->ifa_addr->sa_family == AF_INET
                && reinterpret_cast<sockaddr_in *>(a->ifa_addr)->sin_addr.s_addr == local)
                name = QString::fromLatin1(a->ifa_name);
        freeifaddrs(list);
    }
    return name;
}
