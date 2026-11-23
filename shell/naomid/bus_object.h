// naomid — основа объекта службы на системной шине. Свойства хранятся в словаре; изменения уходят
// клиентам одним сигналом org.freedesktop.DBus.Properties.PropertiesChanged за проход цикла событий.
// Здесь же — помощники для разговора с чужими службами (NetworkManager, UPower, oFono): асинхронный
// вызов и «служба появилась» (в том числе после её перезапуска).
#pragma once

#include "dbus_plain.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QVariantMap>

#include <functional>

class BusObject : public QObject
{
    Q_OBJECT
public:
    BusObject(const QString &path, const QString &iface, QObject *parent = nullptr);
    bool publish();   // зарегистрировать объект на шине (до того, как занять имя службы)

protected:
    static QDBusConnection bus() { return QDBusConnection::systemBus(); }
    QVariant prop(const QString &name) const { return m_props.value(name); }
    void set(const QString &name, const QVariant &value);
    // 0…100, которое дёргается на каждом скане (сила сигнала): клиентам — при изменении на ≥ step или
    // при переходе через край (0 / 100)
    void setLevel(const QString &name, int value, int step = 10);

    // вызов метода чужой службы без ожидания; done получает ответ (или ошибку — msg.type() == ErrorMessage);
    // timeoutMs — для долгих (спаривание Bluetooth), иначе стандартные 25 с
    void callAsync(const QString &service, const QString &path, const QString &iface, const QString &method,
                   const QVariantList &args, std::function<void(const QDBusMessage &)> done = {},
                   int timeoutMs = -1);
    // up() — сейчас, если служба уже есть, и каждый раз, когда она появляется заново
    void follow(const QString &service, std::function<void()> up);

private:
    void flush();

    QString m_path, m_iface;
    QVariantMap m_props, m_pending;
    bool m_flushQueued = false;
};
