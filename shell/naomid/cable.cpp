#include "cable.h"

#include <QFile>
#include <QLoggingCategory>
#include <QProcess>
#include <QSocketNotifier>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcCable, "naomid.cable")

namespace {

const char IFACE[] = "usb0";                              // сеть по кабелю (device/naomi-usb, функция ncm.usb0)
const char GATEWAY[] = "/run/naomi-usb/gateway";          // адрес приставки, пока у неё аренда
const char OFF[] = "/run/naomi-usb/internet-off";         // интернет по кабелю выключен
const char TOOL[] = "/usr/local/sbin/naomi-usb";

QByteArray readFile(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll().trimmed() : QByteArray();
}

// маршрут в интернет через кабель: строка usb0 с нулевыми назначением и маской в /proc/net/route (главная таблица)
bool defaultViaCable()
{
    const QList<QByteArray> lines = readFile("/proc/net/route").split('\n');
    for (const QByteArray &line : lines) {
        const QList<QByteArray> f = line.simplified().split(' ');
        if (f.size() >= 8 && f[0] == IFACE && f[1] == "00000000" && f[7] == "00000000")
            return true;
    }
    return false;
}

} // namespace

Cable::Cable(QObject *parent)
    : BusObject("/org/naomi/Phone/Cable", "org.naomi.Phone.Cable", parent)
{
    set("Dock", false);
    set("Enabled", true);
    set("Active", false);

    m_settle.setSingleShot(true);
    m_settle.setInterval(100);
    connect(&m_settle, &QTimer::timeout, this, &Cable::update);

    m_netlink = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, NETLINK_ROUTE);
    sockaddr_nl addr = {};
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_ROUTE;
    if (m_netlink < 0 || bind(m_netlink, reinterpret_cast<sockaddr *>(&addr), sizeof addr) < 0) {
        qCWarning(lcCable, "netlink не открыть — состояние кабеля только при вызовах");
        if (m_netlink >= 0)
            close(m_netlink);
        m_netlink = -1;
    } else {
        m_notifier = new QSocketNotifier(m_netlink, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, [this] {
            char buf[8192];
            while (recv(m_netlink, buf, sizeof buf, 0) > 0) {
            }
            m_settle.start();
        });
    }
    update();
}

Cable::~Cable()
{
    if (m_netlink >= 0)
        close(m_netlink);
}

void Cable::update()
{
    const bool carrier = readFile(QStringLiteral("/sys/class/net/%1/carrier").arg(IFACE)) == "1";
    const bool dock = carrier && !readFile(GATEWAY).isEmpty();
    const bool enabled = !QFile::exists(OFF);
    const bool active = carrier && defaultViaCable();
    if (active != this->active())
        qCInfo(lcCable, "интернет — %s", active ? "по кабелю, через приставку" : "не по кабелю");
    set("Dock", dock);
    set("Enabled", enabled);
    set("Active", active);
}

void Cable::SetEnabled(bool on)
{
    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this, [this, p] {
        p->deleteLater();
        update();
    });
    p->start(TOOL, {"internet", on ? "on" : "off"});
}
