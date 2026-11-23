// naomid — Bluetooth через BlueZ (адаптер hci0): включить, устройства (свои и рядом), спарить, подключить,
// отключить, забыть. По сигналам BlueZ (ObjectManager, PropertiesChanged); поиск устройств — только по
// Scan() и 30 с после него (как список Wi-Fi: живой, пока смотрят), StopScan() — раньше. Сила сигнала (RSSI)
// во время поиска меняется непрерывно — её служба запоминает, а список пересылает, только когда она перешла
// ступень (хороший / средний / слабый / пропал — так её показывает окно стола). Заряд — от BlueZ (Battery1).
// Спаривание: служба — агент BlueZ (BtAgent, «DisplayYesNo»): код с устройства уходит в интерфейс сигналом
// Pairing, ответ — ConfirmPairing. Наушники и колонки спариваются без кода.
// Наушники (docs/NAOMID.md, «Наушники»): последние подключённые служба помнит и подключает сама — при запуске,
// включении Bluetooth; забыли спаривание (у них нет ключа) — спаривает заново по адресу (управляющий канал ядра,
// как btmgmt); подключились сами и выбрали обратное направление звука (наушники → телефон) — переподключает звук
// телефон → наушники.
// Клавиатуры и мыши (docs/NAOMID.md, «Клавиатура»): спаренные служба подключает сама тогда же, когда наушники; ключ
// не подошёл — спаривает заново, но только по щелчку.
#pragma once

#include "bus_object.h"

#include <QDBusContext>
#include <QDBusObjectPath>
#include <QHash>
#include <QSet>
#include <QTimer>

#include <functional>
#include <memory>

class BtAgent;
struct AuthWatch;

class Bluetooth : public BusObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Phone.Bluetooth")
    Q_PROPERTY(bool Powered READ powered)
    Q_PROPERTY(bool Discovering READ discovering)
    // устройства: address, name, icon (как в BlueZ: audio-headset, phone, computer, input-keyboard…),
    // paired, connected, busy (спаривается, подключается или отключается), rssi (−999 — не слышно), battery
    // (заряд, %; −1 — не сообщает); подключённые первыми, затем свои, затем по силе сигнала. Безымянные чужие
    // не показываются
    Q_PROPERTY(QVariantList Devices READ devices)

public:
    explicit Bluetooth(QObject *parent = nullptr);

    bool powered() const { return prop("Powered").toBool(); }
    bool discovering() const { return prop("Discovering").toBool(); }
    QVariantList devices() const { return prop("Devices").toList(); }

public slots:
    Q_SCRIPTABLE void SetPowered(bool on);
    Q_SCRIPTABLE void Scan();                         // искать устройства 30 с (звать, пока список на экране)
    Q_SCRIPTABLE void StopScan();                     // перестать искать сейчас
    Q_SCRIPTABLE void Connect(const QString &address);   // не спарено — сначала спарить
    Q_SCRIPTABLE void Disconnect(const QString &address);
    Q_SCRIPTABLE void Forget(const QString &address);
    Q_SCRIPTABLE void ConfirmPairing(bool accept);    // ответ на Pairing(…, "confirm")

signals:
    // reason: notfound, pairing (не спарилось: отказ, неверный код, время вышло), connect, error
    Q_SCRIPTABLE void Failed(const QString &address, const QString &reason);
    // mode: confirm — совпадает ли код на устройстве (ответ — ConfirmPairing); display — ввести code на
    // устройстве; cancel — вопрос больше не нужен (ответили, спаривание кончилось или отменено): спрятать;
    // address пустой — у всех
    Q_SCRIPTABLE void Pairing(const QString &address, const QString &name, const QString &code, const QString &mode);

private slots:
    void onProps(const QDBusMessage &msg);
    void onAdded(const QDBusMessage &msg);
    void onRemoved(const QDBusMessage &msg);

private:
    void start();                       // BlueZ появился
    void publishDevices();
    QString pathOf(const QString &address) const;
    void startConnect(const QString &address, bool quiet);   // quiet — сами, не по щелчку
    void connectProfiles(const QString &address);
    void repair(const QString &address);
    void pairAgain(const QString &address, int tries);
    void mgmtPair(const QString &address, std::function<void(int)> done);
    void watchAuth(const QString &address);     // ловить «ключ не подошёл» на время подключения
    bool endAuthWatch(const QString &address);  // true — не подошёл
    void whenDevice(const QString &address, int tries, std::function<void(const QString &)> then);
    void whenGone(const QString &address, int tries, std::function<void()> then);
    void checkTransport(const QString &path, const QVariantMap &props);
    void fixDirection(const QString &devicePath);
    void connectSink(const QString &devicePath, int tries);
    void remember(const QString &address);
    void scheduleAuto();
    void autoConnect();
    void autoConnectInput();
    bool isInput(const QString &address) const;   // клавиатура или мышь (HID)
    void fail(const QString &address, const QString &reason);
    void done(const QString &address);  // дело с устройством кончилось: не занято, список — клиентам

    BtAgent *m_agent;
    QHash<QString, QVariantMap> m_devs; // путь → свойства org.bluez.Device1
    QHash<QString, int> m_battery;      // путь → заряд (org.bluez.Battery1 Percentage)
    QSet<QString> m_busy;               // адреса, которые спариваются, подключаются или отключаются
    QSet<QString> m_quiet;              // подключаются сами (не по щелчку): неудача — без Failed
    QSet<QString> m_repairing;          // спариваются заново (у устройства нет ключа) — второй раз не пробовать
    QHash<QString, int> m_retried;      // сколько раз подключение повторяли после «занято»
    QHash<QString, std::shared_ptr<AuthWatch>> m_authWatch;   // адрес → слежка за «ключ не подошёл»
    QSet<QString> m_fixing;             // пути устройств, у которых переподключается направление звука
    QString m_audio;                    // адрес последних подключённых наушников (колонки) — подключать самим
    int m_autoStep = 0;                 // попытка подключить их после запуска или включения
    QTimer m_auto;
    QTimer m_publish;                   // пачка изменений — одна пересылка
    QTimer m_scanStop;                  // конец поиска
};

// Агент BlueZ: спрашивает подтверждение кода через Bluetooth::Pairing, отвечает по ConfirmPairing
class BtAgent : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")
public:
    explicit BtAgent(QObject *parent = nullptr);
    bool answer(bool accept);   // false — вопроса не было

signals:
    void request(const QString &devicePath, const QString &code, const QString &mode);

public slots:
    void Release();
    QString RequestPinCode(const QDBusObjectPath &device);
    void DisplayPinCode(const QDBusObjectPath &device, const QString &pincode);
    uint RequestPasskey(const QDBusObjectPath &device);
    void DisplayPasskey(const QDBusObjectPath &device, uint passkey, ushort entered);
    void RequestConfirmation(const QDBusObjectPath &device, uint passkey);
    void RequestAuthorization(const QDBusObjectPath &device);
    void AuthorizeService(const QDBusObjectPath &device, const QString &uuid);
    void Cancel();

private:
    QDBusMessage m_pending;             // RequestConfirmation ждёт ответа пользователя
    QTimer m_timeout;
};
