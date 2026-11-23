#include "wol.h"

#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

std::string macOf(const std::string &ip)
{
    FILE *f = fopen("/proc/net/arp", "r");
    if (!f)
        return {};
    char line[256], addr[64], mac[64];
    std::string found;
    while (fgets(line, sizeof line, f)) {   // IP address, HW type, Flags, HW address, Mask, Device
        unsigned type, flags;
        if (sscanf(line, "%63s 0x%x 0x%x %63s", addr, &type, &flags, mac) == 4 && ip == addr && (flags & 2)
            && strcmp(mac, "00:00:00:00:00:00") != 0) {
            found = mac;
            break;
        }
    }
    fclose(f);
    return found;
}

bool wakeOnLan(const std::string &mac)
{
    unsigned m[6];
    if (sscanf(mac.c_str(), "%2x:%2x:%2x:%2x:%2x:%2x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6)
        return false;
    unsigned char packet[6 + 16 * 6];
    memset(packet, 0xff, 6);
    for (int i = 0; i < 16; ++i)
        for (int k = 0; k < 6; ++k)
            packet[6 + i * 6 + k] = static_cast<unsigned char>(m[k]);
    const int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
        return false;
    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
    int sent = 0;
    auto to = [&](in_addr_t addr) {
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(9);
        a.sin_addr.s_addr = addr;
        if (sendto(fd, packet, sizeof packet, 0, reinterpret_cast<sockaddr *>(&a), sizeof a) == ssize_t(sizeof packet))
            ++sent;
    };
    ifaddrs *list = nullptr;
    if (getifaddrs(&list) == 0) {
        for (ifaddrs *i = list; i; i = i->ifa_next) {
            if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET || !(i->ifa_flags & IFF_UP)
                || !(i->ifa_flags & IFF_BROADCAST) || (i->ifa_flags & IFF_LOOPBACK) || !i->ifa_broadaddr)
                continue;
            const in_addr_t own = reinterpret_cast<sockaddr_in *>(i->ifa_addr)->sin_addr.s_addr;
            if ((ntohl(own) & 0xffffff00) == 0x0a4d0000)   // 10.77.0.x — кабель телефона: телевизора там нет
                continue;
            to(reinterpret_cast<sockaddr_in *>(i->ifa_broadaddr)->sin_addr.s_addr);
        }
        freeifaddrs(list);
    }
    to(htonl(INADDR_BROADCAST));
    close(fd);
    return sent > 0;
}
