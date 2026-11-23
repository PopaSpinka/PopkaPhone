#include "bus_object.h"

#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusConnectionInterface>
#include <QLoggingCategory>
#include <QTimer>

Q_LOGGING_CATEGORY(lcBus, "naomid.bus")

BusObject::BusObject(const QString &path, const QString &iface, QObject *parent)
    : QObject(parent)
    , m_path(path)
    , m_iface(iface)
{
}

bool BusObject::publish()
{
    if (bus().registerObject(m_path, m_iface, this,
                             QDBusConnection::ExportAllProperties | QDBusConnection::ExportScriptableSlots
                                 | QDBusConnection::ExportScriptableSignals))
        return true;
    qCCritical(lcBus, "не зарегистрировать %s: %s", qPrintable(m_path), qPrintable(bus().lastError().message()));
    return false;
}

void BusObject::set(const QString &name, const QVariant &value)
{
    if (m_props.contains(name) && m_props.value(name) == value)
        return;
    m_props[name] = value;
    m_pending[name] = value;
    if (!m_flushQueued) {
        m_flushQueued = true;
        QMetaObject::invokeMethod(this, &BusObject::flush, Qt::QueuedConnection);
    }
}

void BusObject::setLevel(const QString &name, int value, int step)
{
    const int old = prop(name).toInt();
    const bool edge = (value == 0) != (old == 0) || (value >= 100) != (old >= 100);
    if (!m_props.contains(name) || qAbs(value - old) >= step || edge)
        set(name, value);
}

void BusObject::flush()
{
    m_flushQueued = false;
    if (m_pending.isEmpty())
        return;
    QDBusMessage msg = QDBusMessage::createSignal(m_path, "org.freedesktop.DBus.Properties", "PropertiesChanged");
    msg << m_iface << m_pending << QStringList();
    bus().send(msg);
    m_pending.clear();
}

void BusObject::callAsync(const QString &service, const QString &path, const QString &iface,
                          const QString &method, const QVariantList &args,
                          std::function<void(const QDBusMessage &)> done, int timeoutMs)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(service, path, iface, method);
    msg.setArguments(args);
    auto *watcher = new QDBusPendingCallWatcher(bus().asyncCall(msg, timeoutMs), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [watcher, done = std::move(done), method](QDBusPendingCallWatcher *) {
                watcher->deleteLater();
                const QDBusMessage reply = watcher->reply();
                if (reply.type() == QDBusMessage::ErrorMessage)
                    qCDebug(lcBus, "%s: %s", qPrintable(method), qPrintable(reply.errorMessage()));
                if (done)
                    done(reply);
            });
}

void BusObject::follow(const QString &service, std::function<void()> up)
{
    auto *watcher = new QDBusServiceWatcher(service, bus(), QDBusServiceWatcher::WatchForRegistration, this);
    connect(watcher, &QDBusServiceWatcher::serviceRegistered, this, [up] { up(); });
    if (bus().interface()->isServiceRegistered(service))
        QMetaObject::invokeMethod(this, [up] { up(); }, Qt::QueuedConnection);
}
