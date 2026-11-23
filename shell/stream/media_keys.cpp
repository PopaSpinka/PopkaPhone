#include "media_keys.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMedia, "naomi.media")

namespace {

const QString kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kPlayer = QStringLiteral("org.mpris.MediaPlayer2.Player");
constexpr int kTimeoutMs = 150;   // плеер завис — клавиша оболочку не держит

bool playing(const QString &service)
{
    QDBusMessage m = QDBusMessage::createMethodCall(service, kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                    QStringLiteral("Get"));
    m << kPlayer << QStringLiteral("PlaybackStatus");
    const QDBusMessage r = QDBusConnection::sessionBus().call(m, QDBus::Block, kTimeoutMs);
    return r.type() == QDBusMessage::ReplyMessage && !r.arguments().isEmpty()
           && r.arguments().constFirst().value<QDBusVariant>().variant().toString() == QLatin1String("Playing");
}

void send(const QString &service, const QString &method)
{
    qCInfo(lcMedia, "%s → %s", qPrintable(method), qPrintable(service));
    QDBusConnection::sessionBus().send(QDBusMessage::createMethodCall(service, kPath, kPlayer, method));
}

} // namespace

void MediaKeys::press(int qtKey)
{
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus)
        return;
    const QStringList players = bus->registeredServiceNames().value().filter(QStringLiteral("org.mpris.MediaPlayer2."));
    if (players.isEmpty())
        return;
    QStringList active;
    for (const QString &p : players)
        if (playing(p))
            active << p;
    const QString idle = players.contains(m_last) ? m_last : players.constFirst();
    if (qtKey == Qt::Key_MediaTogglePlayPause) {
        for (const QString &p : std::as_const(active))
            send(p, QStringLiteral("Pause"));
        if (active.isEmpty())
            send(idle, QStringLiteral("Play"));
        else
            m_last = active.constLast();
        return;
    }
    m_last = active.isEmpty() ? idle : active.constLast();
    send(m_last, qtKey == Qt::Key_MediaNext ? QStringLiteral("Next") : QStringLiteral("Previous"));
}
