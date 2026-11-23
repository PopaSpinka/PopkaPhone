#include "bluetooth.h"

#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QSocketNotifier>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <memory>
#include <utility>

#include <sys/socket.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcBt, "naomid.bluetooth")

namespace {

const char BLUEZ[] = "org.bluez";
const char ADAPTER[] = "/org/bluez/hci0";
const char ADAPTER_IFACE[] = "org.bluez.Adapter1";
const char DEVICE_IFACE[] = "org.bluez.Device1";
const char BATTERY_IFACE[] = "org.bluez.Battery1";
const char PROPS[] = "org.freedesktop.DBus.Properties";
const char OBJECTS[] = "org.freedesktop.DBus.ObjectManager";
const char TRANSPORT_IFACE[] = "org.bluez.MediaTransport1";
const char AGENT_PATH[] = "/org/naomi/Phone/BluetoothAgent";
// A2DP: у устройства «Audio Sink» — его можно слушать (наушники, колонка); у транспорта BlueZ UUID — роль своей точки:
// 110a — телефон играет, 110b — телефон слушает
const char A2DP_SOURCE_UUID[] = "0000110a-0000-1000-8000-00805f9b34fb";
const char A2DP_SINK_UUID[] = "0000110b-0000-1000-8000-00805f9b34fb";
// клавиатуры и мыши (Human Interface Device)
const char HID_UUID[] = "00001124-0000-1000-8000-00805f9b34fb";
// последние подключённые наушники — подключать самим (корень телефона — только для чтения)
const char STATE_FILE[] = "/userdata/naomi/state/bluetooth.json";
// после запуска службы, BlueZ или включения Bluetooth — попытки подключить их: через 3, 15 и 45 с (выключены — каждая
// ~5 с вызова в эфир)
const int kAutoDelays[] = {3000, 12000, 30000};

// управляющий канал ядра Bluetooth (mgmt, как у btmgmt) — спарить по адресу, когда у BlueZ устройства уже нет; узнать,
// что ключ не подошёл (событие Authentication Failed)
constexpr int kAfBluetooth = 31, kBtprotoHci = 1;
constexpr unsigned short kHciDevNone = 0xffff, kHciChannelControl = 3;
constexpr uint16_t kMgmtPairDevice = 0x0019, kMgmtEvComplete = 0x0001, kMgmtEvStatus = 0x0002,
                   kMgmtEvAuthFailed = 0x0011;
constexpr int kMgmtAlreadyPaired = 0x13;
struct SockaddrHci { sa_family_t family; unsigned short dev; unsigned short channel; };

// открытый управляющий канал (неблокирующий) или −1
int openMgmt()
{
    const int fd = ::socket(kAfBluetooth, SOCK_RAW | SOCK_CLOEXEC | SOCK_NONBLOCK, kBtprotoHci);
    SockaddrHci sa{kAfBluetooth, kHciDevNone, kHciChannelControl};
    if (fd >= 0 && ::bind(fd, reinterpret_cast<sockaddr *>(&sa), sizeof sa) == 0)
        return fd;
    if (fd >= 0)
        ::close(fd);
    qCWarning(lcBt, "управляющий канал Bluetooth не открылся");
    return -1;
}

// адрес «AA:BB:…» — байтами, как в сообщениях ядра (младшим вперёд); пустой — не адрес
QByteArray bdaddr(const QString &address)
{
    const QStringList bytes = address.split(':');
    if (bytes.size() != 6)
        return {};
    QByteArray out(6, 0);
    for (int i = 0; i < 6; ++i)
        out[i] = char(bytes.at(5 - i).toUInt(nullptr, 16));
    return out;
}

// что из изменений устройства стоит показать (RSSI и данные рекламы во время поиска — нет)
const QStringList kShown = {"Name", "Alias", "Paired", "Connected", "Icon", "Trusted"};

QString code6(uint n) { return QStringLiteral("%1").arg(n, 6, 10, QLatin1Char('0')); }

// ступень силы сигнала, как её показывает окно Bluetooth стола (desk-bt.c НаомиОС): не слышно, слабый, средний,
// хороший
int rssiStep(const QVariantMap &d)
{
    if (!d.contains("RSSI"))
        return 0;
    const int rssi = d.value("RSSI").toInt();
    return rssi >= -60 ? 3 : rssi >= -75 ? 2 : 1;
}

} // namespace

// Ключ телефона не подошёл устройству, пока шло подключение: ключа телефона у него нет (HCI «PIN or Key Missing») или
// он другой («Authentication Failure»). У наушников BlueZ так и отвечает (br-connection-key-missing), а у клавиатур —
// «br-connection-create-socket»: не открылся канал управления клавиатурой (в журнале BlueZ — «Invalid exchange (52)»
// или «Permission denied»). Видно это только по событию ядра Authentication Failed на управляющем канале — его и
// слушаем, пока идёт подключение
struct AuthWatch
{
    int fd = -1;
    QByteArray address;           // bdaddr()
    bool failed = false;
    std::unique_ptr<QSocketNotifier> notifier;

    void drain()
    {
        uint8_t ev[512];
        ssize_t n;
        while ((n = ::read(fd, ev, sizeof ev)) >= 6 + 8) {
            // заголовок: событие, адаптер, длина; дальше адрес (6), его вид, статус
            if ((ev[0] | ev[1] << 8) == kMgmtEvAuthFailed && ::memcmp(ev + 6, address.constData(), 6) == 0)
                failed = true;
        }
    }
    ~AuthWatch()
    {
        notifier.reset();
        ::close(fd);
    }
};

Bluetooth::Bluetooth(QObject *parent)
    : BusObject("/org/naomi/Phone/Bluetooth", "org.naomi.Phone.Bluetooth", parent)
    , m_agent(new BtAgent(this))
{
    set("Powered", false);
    set("Discovering", false);
    set("Devices", QVariantList());

    m_publish.setSingleShot(true);
    m_publish.setInterval(300);
    connect(&m_publish, &QTimer::timeout, this, &Bluetooth::publishDevices);
    m_auto.setSingleShot(true);
    connect(&m_auto, &QTimer::timeout, this, &Bluetooth::autoConnect);
    QFile f(QString::fromLatin1(STATE_FILE));
    if (f.open(QIODevice::ReadOnly))
        m_audio = QJsonDocument::fromJson(f.readAll()).object().value("audio").toString();
    m_scanStop.setSingleShot(true);
    m_scanStop.setInterval(30000);
    connect(&m_scanStop, &QTimer::timeout, this, [this] {
        callAsync(BLUEZ, ADAPTER, ADAPTER_IFACE, "StopDiscovery", {});
    });

    connect(m_agent, &BtAgent::request, this, [this](const QString &path, const QString &code, const QString &mode) {
        const QVariantMap d = m_devs.value(path);
        emit Pairing(d.value("Address").toString(), d.value("Alias", d.value("Name")).toString(), code, mode);
    });
    bus().registerObject(AGENT_PATH, m_agent, QDBusConnection::ExportAllSlots);

    bus().connect(BLUEZ, "/", OBJECTS, "InterfacesAdded", this, SLOT(onAdded(QDBusMessage)));
    bus().connect(BLUEZ, "/", OBJECTS, "InterfacesRemoved", this, SLOT(onRemoved(QDBusMessage)));
    bus().connect(BLUEZ, QString(), PROPS, "PropertiesChanged", this, SLOT(onProps(QDBusMessage)));
    follow(BLUEZ, [this] { start(); });
}

void Bluetooth::start()
{
    callAsync(BLUEZ, "/", OBJECTS, "GetManagedObjects", {}, [this](const QDBusMessage &r) {
        if (r.type() != QDBusMessage::ReplyMessage)
            return;
        const QVariantMap objects = plain(r.arguments().value(0)).toMap();
        m_devs.clear();
        m_battery.clear();
        QVariantMap transports;
        for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
            const QVariantMap ifaces = it.value().toMap();
            if (ifaces.contains(DEVICE_IFACE))
                m_devs[it.key()] = ifaces.value(DEVICE_IFACE).toMap();
            if (ifaces.contains(BATTERY_IFACE))
                m_battery[it.key()] = ifaces.value(BATTERY_IFACE).toMap().value("Percentage").toInt();
            if (ifaces.contains(TRANSPORT_IFACE))
                transports.insert(it.key(), ifaces.value(TRANSPORT_IFACE));
            if (it.key() == ADAPTER) {
                const QVariantMap a = ifaces.value(ADAPTER_IFACE).toMap();
                set("Powered", a.value("Powered").toBool());
                set("Discovering", a.value("Discovering").toBool());
            }
        }
        publishDevices();
        for (auto it = transports.cbegin(); it != transports.cend(); ++it)
            checkTransport(it.key(), it.value().toMap());
        // памяти ещё нет (первый запуск, забыли запомненные) — наушники на связи, иначе спаренные
        for (const bool connected : {true, false})
            for (const QVariantMap &d : std::as_const(m_devs))
                if (m_audio.isEmpty() && d.value("Paired").toBool() && (!connected || d.value("Connected").toBool())
                    && d.value("UUIDs").toStringList().contains(A2DP_SINK_UUID))
                    remember(d.value("Address").toString());
        scheduleAuto();
    });
    callAsync(BLUEZ, "/org/bluez", "org.bluez.AgentManager1", "RegisterAgent",
              {QVariant::fromValue(QDBusObjectPath(AGENT_PATH)), "DisplayYesNo"}, [this](const QDBusMessage &r) {
                  if (r.type() == QDBusMessage::ErrorMessage && !r.errorName().endsWith("AlreadyExists"))
                      qCWarning(lcBt, "агент не зарегистрирован: %s", qPrintable(r.errorMessage()));
                  callAsync(BLUEZ, "/org/bluez", "org.bluez.AgentManager1", "RequestDefaultAgent",
                            {QVariant::fromValue(QDBusObjectPath(AGENT_PATH))});
              });
}

void Bluetooth::onProps(const QDBusMessage &msg)
{
    const QString iface = msg.arguments().value(0).toString();
    if (iface != DEVICE_IFACE && iface != ADAPTER_IFACE && iface != BATTERY_IFACE)
        return;
    const QVariantMap changed = plainMap(msg.arguments().value(1));
    if (iface == BATTERY_IFACE) {
        if (changed.contains("Percentage")) {
            m_battery[msg.path()] = changed.value("Percentage").toInt();
            m_publish.start();
        }
        return;
    }
    if (msg.path() == ADAPTER) {
        if (changed.contains("Powered")) {
            set("Powered", changed.value("Powered").toBool());
            if (powered())
                scheduleAuto();
        }
        if (changed.contains("Discovering"))
            set("Discovering", changed.value("Discovering").toBool());
        return;
    }
    if (iface != DEVICE_IFACE)
        return;
    QVariantMap &d = m_devs[msg.path()];
    const int step = rssiStep(d);
    for (auto it = changed.cbegin(); it != changed.cend(); ++it)
        d[it.key()] = it.value();
    // пропавшее: поиск кончился — BlueZ убирает силу сигнала (RSSI) у всех
    const QStringList gone = plain(msg.arguments().value(2)).toStringList();
    for (const QString &key : gone)
        d.remove(key);
    // подключились наушники (колонка) — запомнить: их служба подключает сама
    if (changed.value("Connected").toBool() && d.value("UUIDs").toStringList().contains(A2DP_SINK_UUID))
        remember(d.value("Address").toString());
    bool shown = rssiStep(d) != step;
    for (const QString &key : changed.keys() + gone)
        shown = shown || kShown.contains(key);
    if (shown)
        m_publish.start();
}

void Bluetooth::onAdded(const QDBusMessage &msg)
{
    const QString path = plain(msg.arguments().value(0)).toString();
    const QVariantMap ifaces = plain(msg.arguments().value(1)).toMap();
    if (ifaces.contains(BATTERY_IFACE)) {
        m_battery[path] = ifaces.value(BATTERY_IFACE).toMap().value("Percentage").toInt();
        m_publish.start();
    }
    if (ifaces.contains(TRANSPORT_IFACE))
        checkTransport(path, ifaces.value(TRANSPORT_IFACE).toMap());
    if (!ifaces.contains(DEVICE_IFACE))
        return;
    m_devs[path] = ifaces.value(DEVICE_IFACE).toMap();
    m_publish.start();
}

void Bluetooth::onRemoved(const QDBusMessage &msg)
{
    const QString path = plain(msg.arguments().value(0)).toString();
    const QStringList ifaces = plain(msg.arguments().value(1)).toStringList();
    if (ifaces.contains(BATTERY_IFACE) && m_battery.remove(path))
        m_publish.start();
    if (!ifaces.contains(DEVICE_IFACE))
        return;
    m_devs.remove(path);
    m_battery.remove(path);
    m_publish.start();
}

void Bluetooth::publishDevices()
{
    QVariantList list;
    for (auto it = m_devs.cbegin(); it != m_devs.cend(); ++it) {
        const QVariantMap &d = it.value();
        const bool paired = d.value("Paired").toBool();
        if (!paired && !d.contains("Name"))
            continue;
        const QString address = d.value("Address").toString();
        list << QVariantMap{{"address", address},
                            {"name", d.value("Alias", d.value("Name")).toString()},
                            {"icon", d.value("Icon").toString()},
                            {"paired", paired},
                            {"connected", d.value("Connected").toBool()},
                            {"busy", m_busy.contains(address)},
                            {"rssi", d.value("RSSI", -999).toInt()},
                            {"battery", m_battery.value(it.key(), -1)}};
    }
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        const QVariantMap x = a.toMap(), y = b.toMap();
        for (const char *key : {"connected", "paired"})
            if (x.value(key) != y.value(key))
                return x.value(key).toBool();
        if (x.value("rssi") != y.value("rssi"))
            return x.value("rssi").toInt() > y.value("rssi").toInt();
        return x.value("name").toString() < y.value("name").toString();
    });
    set("Devices", list);
}

QString Bluetooth::pathOf(const QString &address) const
{
    for (auto it = m_devs.cbegin(); it != m_devs.cend(); ++it)
        if (it->value("Address").toString().compare(address, Qt::CaseInsensitive) == 0)
            return it.key();
    return {};
}

void Bluetooth::fail(const QString &address, const QString &reason)
{
    m_busy.remove(address);
    m_repairing.remove(address);
    m_retried.remove(address);
    if (m_quiet.remove(address)) {   // подключали сами (не по щелчку) — интерфейсу не о чем сообщать
        qCInfo(lcBt, "%s: само не подключилось — %s", qPrintable(address), qPrintable(reason));
    } else {
        qCInfo(lcBt, "%s: не вышло — %s", qPrintable(address), qPrintable(reason));
        emit Failed(address, reason);
    }
    publishDevices();
}

void Bluetooth::done(const QString &address)
{
    m_busy.remove(address);
    m_quiet.remove(address);
    m_repairing.remove(address);
    m_retried.remove(address);
    publishDevices();
}

void Bluetooth::SetPowered(bool on)
{
    callAsync(BLUEZ, ADAPTER, PROPS, "Set", {ADAPTER_IFACE, "Powered", QVariant::fromValue(QDBusVariant(on))});
}

void Bluetooth::Scan()
{
    if (!powered())
        return;
    m_scanStop.start();
    if (!discovering())
        callAsync(BLUEZ, ADAPTER, ADAPTER_IFACE, "StartDiscovery", {});
}

void Bluetooth::StopScan()
{
    m_scanStop.stop();
    if (discovering())
        callAsync(BLUEZ, ADAPTER, ADAPTER_IFACE, "StopDiscovery", {});
}

void Bluetooth::Connect(const QString &address)
{
    startConnect(address, false);
}

void Bluetooth::startConnect(const QString &address, bool quiet)
{
    if (m_busy.contains(address)) {   // уже подключается (служба сама) — щелчок ждёт того же итога, но с ответом
        if (!quiet)
            m_quiet.remove(address);
        return;
    }
    const QString path = pathOf(address);
    if (path.isEmpty()) {
        fail(address, "notfound");
        return;
    }
    if (quiet)
        m_quiet.insert(address);
    m_busy.insert(address);
    publishDevices();
    if (m_devs.value(path).value("Paired").toBool()) {
        connectProfiles(address);
        return;
    }
    qCInfo(lcBt, "спариваю %s", qPrintable(address));
    callAsync(BLUEZ, path, DEVICE_IFACE, "Pair", {}, [this, address, path](const QDBusMessage &r) {
        // код спаривания, если его показывали, больше не нужен — ни на экране телефона, ни на столе
        const QVariantMap d = m_devs.value(path);
        emit Pairing(address, d.value("Alias", d.value("Name")).toString(), QString(), "cancel");
        // отказ, неверный код, время вышло, устройство не ответило — всё это «не спарилось»
        if (r.type() == QDBusMessage::ErrorMessage && !r.errorName().endsWith("AlreadyExists")) {
            qCInfo(lcBt, "спаривание %s: %s", qPrintable(address), qPrintable(r.errorName()));
            fail(address, "pairing");
            return;
        }
        // своё устройство — доверенное: дальше подключается без вопросов
        callAsync(BLUEZ, path, PROPS, "Set", {DEVICE_IFACE, "Trusted", QVariant::fromValue(QDBusVariant(true))});
        connectProfiles(address);
    }, 60000);
}

void Bluetooth::connectProfiles(const QString &address)
{
    const QString path = pathOf(address);
    watchAuth(address);
    callAsync(BLUEZ, path, DEVICE_IFACE, "Connect", {}, [this, address](const QDBusMessage &r) {
        const bool badKey = endAuthWatch(address);
        if (r.type() == QDBusMessage::ErrorMessage && !r.errorName().endsWith("AlreadyConnected")) {
            qCInfo(lcBt, "подключение %s: %s (%s)%s", qPrintable(address), qPrintable(r.errorName()),
                   qPrintable(r.errorMessage()), badKey ? ", ключ не подошёл" : "");
            // своё устройство не приняло ключ телефона: спаривание оно забыло (у телефона ключ есть) — спарить
            // заново, раз за подключение. Клавиатуру и мышь — только по щелчку: спаривание с ними спрашивает
            // подтверждение кода, а забыть телефон клавиатура может и потому, что её спарили с другим компьютером, —
            // переманивать без спроса не стоит
            if ((badKey || r.errorMessage() == QLatin1String("br-connection-key-missing"))
                && !m_repairing.contains(address)) {
                if (m_quiet.contains(address) && isInput(address)) {
                    qCInfo(lcBt, "%s: спаривание забыто — спарить заново можно щелчком", qPrintable(address));
                    fail(address, "pairing");
                    return;
                }
                repair(address);
                return;
            }
            // «занято»: устройство в ту же секунду подключается само (наушники зовут телефон в ответ на вызов) или ещё
            // не отошло от отключения (AeroFit 2 после отключения с телефона ~10 с отвечают на вызов отказом — замер
            // 20 ноября 2026). До трёх повторов через 3 с; выключенное отвечает иначе (page timeout) — без повторов
            const bool busy = r.errorName().endsWith("InProgress") || r.errorMessage() == QLatin1String("br-connection-busy")
                              || r.errorMessage() == QLatin1String("br-connection-create-socket");
            if (busy && m_retried.value(address) < 3) {
                ++m_retried[address];
                QTimer::singleShot(3000, this, [this, address] { connectProfiles(address); });
                return;
            }
            // всё ещё «занято», но связь есть: наушники достраивают свой звук (обратный — его переподключит
            // checkTransport) — это не «не вышло»
            if (busy && m_devs.value(pathOf(address)).value("Connected").toBool()) {
                qCInfo(lcBt, "%s: на связи, звук достраивает само устройство", qPrintable(address));
                done(address);
                return;
            }
            fail(address, "connect");
            return;
        }
        qCInfo(lcBt, "подключено: %s", qPrintable(address));
        done(address);
    }, 30000);
}

void Bluetooth::Disconnect(const QString &address)
{
    const QString path = pathOf(address);
    if (path.isEmpty())
        return;
    if (address.compare(m_audio, Qt::CaseInsensitive) == 0)
        m_auto.stop();   // отключили руками — сами не подключаем (до перезапуска службы или включения Bluetooth)
    m_busy.insert(address);
    publishDevices();
    callAsync(BLUEZ, path, DEVICE_IFACE, "Disconnect", {}, [this, address](const QDBusMessage &r) {
        if (r.type() == QDBusMessage::ErrorMessage) {
            qCInfo(lcBt, "отключение %s: %s", qPrintable(address), qPrintable(r.errorName()));
            fail(address, "error");
            return;
        }
        qCInfo(lcBt, "отключено: %s", qPrintable(address));
        done(address);
    }, 30000);
}

void Bluetooth::Forget(const QString &address)
{
    if (address.compare(m_audio, Qt::CaseInsensitive) == 0)
        remember(QString());
    const QString path = pathOf(address);
    if (!path.isEmpty())
        callAsync(BLUEZ, ADAPTER, ADAPTER_IFACE, "RemoveDevice", {QVariant::fromValue(QDBusObjectPath(path))});
}

void Bluetooth::ConfirmPairing(bool accept)
{
    // вопрос ушёл — спрятать его у всех (спросили и на экране телефона, и на столе)
    if (m_agent->answer(accept))
        emit Pairing(QString(), QString(), QString(), "cancel");
}

// --- наушники: спарить заново, направление звука, подключить самим ---

// Устройство забыло спаривание (на вызов ключом телефона отвечает «PIN or Key Missing»: 20 ноября 2026 так было с
// наушниками AeroFit 2 — ключ у телефона есть, у них нет). Старое спаривание — прочь, новое — по адресу: устройства
// у BlueZ после этого нет, а в поиске оно не видно (наушники, уже бывшие в паре, не в режиме сопряжения), — поэтому
// через управляющий канал ядра, как btmgmt; BlueZ сам заводит устройство и хранит ключ. Наушники соглашаются и вне
// режима сопряжения. Magic Keyboard (21 ноября 2026) — когда ключа телефона у неё нет или её выключили и включили;
// когда у неё свой, другой ключ телефона, — рвёт связь посреди спаривания (тогда — «не спарилось»)
void Bluetooth::repair(const QString &address)
{
    const QString path = pathOf(address);
    if (path.isEmpty()) {
        fail(address, "notfound");
        return;
    }
    m_repairing.insert(address);
    qCInfo(lcBt, "%s: у устройства нет ключа (забыло спаривание) — спариваю заново", qPrintable(address));
    // Связь после отказа ещё рвётся, а пока она есть, BlueZ снимает спаривание не сразу, а по обрыву: ответ на
    // RemoveDevice приходит раньше, и ядро на спаривание отвечает «уже спарено» (0x13; клавиатура, 21 ноября 2026).
    // Поэтому — ждать, пока устройство у BlueZ пропадёт (до 5 с)
    callAsync(BLUEZ, ADAPTER, ADAPTER_IFACE, "RemoveDevice", {QVariant::fromValue(QDBusObjectPath(path))},
              [this, address](const QDBusMessage &) {
        whenGone(address, 33, [this, address] { pairAgain(address, 3); });
    });
}

void Bluetooth::pairAgain(const QString &address, int tries)
{
    mgmtPair(address, [this, address, tries](int status) {
        // «уже спарено»: ядро ещё не сняло старый ключ — через секунду ещё раз
        if (status == kMgmtAlreadyPaired && tries > 1) {
            QTimer::singleShot(1000, this, [this, address, tries] { pairAgain(address, tries - 1); });
            return;
        }
        // код, если его спрашивали (клавиатура), больше не нужен — ни на экране телефона, ни на столе
        emit Pairing(address, QString(), QString(), "cancel");
        if (status != 0) {
            qCInfo(lcBt, "%s: заново не спарилось, ответ ядра 0x%02x", qPrintable(address), status);
            fail(address, "pairing");
            return;
        }
        whenDevice(address, 20, [this, address](const QString &path) {
            if (path.isEmpty()) {
                fail(address, "notfound");
                return;
            }
            qCInfo(lcBt, "%s: спарено заново", qPrintable(address));
            callAsync(BLUEZ, path, PROPS, "Set", {DEVICE_IFACE, "Trusted", QVariant::fromValue(QDBusVariant(true))});
            connectProfiles(address);
        });
    });
}

// MGMT_OP_PAIR_DEVICE на hci0: адрес BR/EDR, способности — как у агента (DisplayYesNo). done(статус ядра; 0 — спарено,
// −1 — канал не открылся, 0xff — время вышло)
void Bluetooth::mgmtPair(const QString &address, std::function<void(int)> done)
{
    const QByteArray bd = bdaddr(address);
    const int fd = bd.isEmpty() ? -1 : openMgmt();
    if (fd < 0) {
        done(-1);
        return;
    }
    // заголовок: команда, номер адаптера (hci0), длина; числа и адрес — младшим байтом вперёд
    uint8_t cmd[6 + 8] = {kMgmtPairDevice & 0xff, kMgmtPairDevice >> 8, 0, 0, 8, 0};
    ::memcpy(cmd + 6, bd.constData(), 6);
    cmd[12] = 0;      // BR/EDR
    cmd[13] = 0x01;   // DisplayYesNo
    if (::write(fd, cmd, sizeof cmd) != ssize_t(sizeof cmd)) {
        ::close(fd);
        done(-1);
        return;
    }
    auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    auto *timeout = new QTimer(this);
    auto finished = std::make_shared<bool>(false);
    auto finish = [=](int status) {
        if (std::exchange(*finished, true))
            return;
        notifier->setEnabled(false);
        notifier->deleteLater();
        timeout->deleteLater();
        ::close(fd);
        done(status);
    };
    QObject::connect(notifier, &QSocketNotifier::activated, this, [=] {
        uint8_t ev[512];
        ssize_t n;
        while ((n = ::read(fd, ev, sizeof ev)) >= 9) {
            const int code = ev[0] | ev[1] << 8, opcode = ev[6] | ev[7] << 8;
            if ((code == kMgmtEvComplete || code == kMgmtEvStatus) && opcode == kMgmtPairDevice) {
                finish(ev[8]);
                return;
            }
        }
    });
    timeout->setSingleShot(true);
    QObject::connect(timeout, &QTimer::timeout, this, [=] { finish(0xff); });
    timeout->start(45000);
}

void Bluetooth::watchAuth(const QString &address)
{
    if (m_authWatch.contains(address))
        return;
    auto w = std::make_shared<AuthWatch>();
    w->address = bdaddr(address);
    w->fd = w->address.isEmpty() ? -1 : openMgmt();
    if (w->fd < 0)
        return;
    w->notifier = std::make_unique<QSocketNotifier>(w->fd, QSocketNotifier::Read);
    AuthWatch *raw = w.get();
    connect(w->notifier.get(), &QSocketNotifier::activated, this, [raw] { raw->drain(); });
    m_authWatch.insert(address, w);
}

bool Bluetooth::endAuthWatch(const QString &address)
{
    const std::shared_ptr<AuthWatch> w = m_authWatch.take(address);
    if (!w)
        return false;
    w->drain();   // событие ядра приходит раньше ответа BlueZ, но могло ещё не быть прочитано
    return w->failed;
}

// устройство у BlueZ (после спаривания по адресу BlueZ заводит его сам, сигналом чуть позже ответа ядра)
void Bluetooth::whenDevice(const QString &address, int tries, std::function<void(const QString &)> then)
{
    const QString path = pathOf(address);
    if (!path.isEmpty() || tries <= 0) {
        then(path);
        return;
    }
    QTimer::singleShot(150, this, [this, address, tries, then] { whenDevice(address, tries - 1, then); });
}

// устройства у BlueZ больше нет (или время вышло — тогда всё равно дальше: ответ ядра скажет)
void Bluetooth::whenGone(const QString &address, int tries, std::function<void()> then)
{
    if (pathOf(address).isEmpty() || tries <= 0) {
        then();
        return;
    }
    QTimer::singleShot(150, this, [this, address, tries, then] { whenGone(address, tries - 1, then); });
}

// Звук по Bluetooth пошёл «наушники → телефон» (точка телефона — слушает, 110b), а устройство умеет слушать само
// (у него «Audio Sink»). Так делают AeroFit 2, когда подключаются сами: из точек телефона (PulseAudio объявляет и
// играющие, и слушающие) выбирают слушающую, — у PulseAudio карта «выкл», выхода нет, тишина. Переподключить:
// направление «телефон → наушники», как при подключении с телефона
void Bluetooth::checkTransport(const QString &path, const QVariantMap &props)
{
    if (props.value("UUID").toString() != QLatin1String(A2DP_SINK_UUID))
        return;
    const QString device = props.value("Device").toString();
    if (!m_devs.value(device).value("UUIDs").toStringList().contains(A2DP_SINK_UUID))
        return;   // устройство само не слушает (телефон, плеер) — пусть играет телефону
    qCInfo(lcBt, "%s: звук пошёл обратно (устройство → телефон, %s) — переподключаю телефон → устройство",
           qPrintable(m_devs.value(device).value("Address").toString()), qPrintable(path));
    fixDirection(device);
}

void Bluetooth::fixDirection(const QString &devicePath)
{
    if (m_fixing.contains(devicePath))
        return;
    m_fixing.insert(devicePath);
    // секунда — устройство доделывает своё (открыть поток), потом снять его и подключить своё
    QTimer::singleShot(1000, this, [this, devicePath] {
        callAsync(BLUEZ, devicePath, DEVICE_IFACE, "DisconnectProfile", {A2DP_SOURCE_UUID},
                  [this, devicePath](const QDBusMessage &) { connectSink(devicePath, 3); });
    });
}

void Bluetooth::connectSink(const QString &devicePath, int tries)
{
    callAsync(BLUEZ, devicePath, DEVICE_IFACE, "ConnectProfile", {A2DP_SINK_UUID},
              [this, devicePath, tries](const QDBusMessage &r) {
        const QString address = m_devs.value(devicePath).value("Address").toString();
        if (r.type() == QDBusMessage::ErrorMessage && !r.errorName().endsWith("AlreadyConnected")) {
            if (tries > 1) {   // «занято» — устройство ещё закрывает свой поток
                QTimer::singleShot(1500, this, [this, devicePath, tries] { connectSink(devicePath, tries - 1); });
                return;
            }
            qCInfo(lcBt, "%s: звук телефон → устройство не подключился: %s (%s)", qPrintable(address),
                   qPrintable(r.errorName()), qPrintable(r.errorMessage()));
        } else {
            qCInfo(lcBt, "%s: звук — телефон → устройство", qPrintable(address));
        }
        m_fixing.remove(devicePath);
    }, 30000);
}

void Bluetooth::remember(const QString &address)
{
    if (address == m_audio)
        return;
    m_audio = address;
    qCInfo(lcBt, "наушники для подключения самим: %s", address.isEmpty() ? "нет" : qPrintable(address));
    QDir().mkpath(QFileInfo(QString::fromLatin1(STATE_FILE)).path());
    QSaveFile f(QString::fromLatin1(STATE_FILE));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(QJsonObject{{"audio", m_audio}}).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

// Подключить запомненные наушники самим: после запуска службы (и перезагрузки телефона), перезапуска BlueZ, включения
// Bluetooth. Дальше наушники подключаются сами (достали из кейса — зовут телефон; телефон их принимает: доверенные), а
// обрыв связи BlueZ переподключает сам (ReconnectAttempts). Постоянно звать выключенные наушники не стоит: каждый
// вызов — ~5 с передатчика
void Bluetooth::scheduleAuto()
{
    m_autoStep = 0;
    m_auto.start(kAutoDelays[0]);
}

void Bluetooth::autoConnect()
{
    const QString path = pathOf(m_audio);
    const QVariantMap d = m_devs.value(path);
    bool calling = false;
    if (powered() && !path.isEmpty() && d.value("Paired").toBool() && !d.value("Connected").toBool()
        && !m_busy.contains(m_audio)) {
        qCInfo(lcBt, "подключаю наушники сам: %s (попытка %d)", qPrintable(m_audio), m_autoStep + 1);
        startConnect(m_audio, true);
        calling = true;
    }
    // клавиатуры — после вызова наушников: выключенные наушники занимают вызов ~5 с, а два вызова сразу передатчик
    // телефона не обещает
    QTimer::singleShot(calling ? 6000 : 0, this, &Bluetooth::autoConnectInput);
    if (++m_autoStep < int(std::size(kAutoDelays)))
        m_auto.start(kAutoDelays[m_autoStep]);
}

// Спаренные клавиатуры и мыши — подключить самим. Сами они зовут телефон по нажатию клавиши (доверенные — телефон
// принимает), но после перезагрузки или перезапуска BlueZ телефон им сказал «выключаюсь» (HCI 0x15), и клавиатура не
// зовёт, пока её не тронут, хотя ещё не спит и вызов принимает (Magic Keyboard — больше минуты)
void Bluetooth::autoConnectInput()
{
    if (!powered())
        return;
    for (auto it = m_devs.cbegin(); it != m_devs.cend(); ++it) {
        const QVariantMap &d = it.value();
        const QString address = d.value("Address").toString();
        if (d.value("Paired").toBool() && !d.value("Connected").toBool() && !m_busy.contains(address)
            && d.value("UUIDs").toStringList().contains(HID_UUID)) {
            qCInfo(lcBt, "подключаю клавиатуру сам: %s (попытка %d)", qPrintable(address), m_autoStep);
            startConnect(address, true);
        }
    }
}

bool Bluetooth::isInput(const QString &address) const
{
    return m_devs.value(pathOf(address)).value("UUIDs").toStringList().contains(HID_UUID);
}

// --- агент ---

BtAgent::BtAgent(QObject *parent)
    : QObject(parent)
{
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(30000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { answer(false); });
}

bool BtAgent::answer(bool accept)
{
    if (m_pending.type() != QDBusMessage::MethodCallMessage)
        return false;
    QDBusConnection::systemBus().send(accept ? m_pending.createReply()
                                             : m_pending.createErrorReply("org.bluez.Error.Rejected", "отклонено"));
    m_pending = QDBusMessage();
    m_timeout.stop();
    return true;
}

void BtAgent::Release() {}

QString BtAgent::RequestPinCode(const QDBusObjectPath &)
{
    return QStringLiteral("0000");   // старые устройства без экрана (гарнитуры до Bluetooth 2.1)
}

void BtAgent::DisplayPinCode(const QDBusObjectPath &device, const QString &pincode)
{
    emit request(device.path(), pincode, "display");
}

uint BtAgent::RequestPasskey(const QDBusObjectPath &)
{
    sendErrorReply("org.bluez.Error.Rejected", "ввод кода с телефона не поддерживается");
    return 0;
}

void BtAgent::DisplayPasskey(const QDBusObjectPath &device, uint passkey, ushort)
{
    emit request(device.path(), code6(passkey), "display");
}

void BtAgent::RequestConfirmation(const QDBusObjectPath &device, uint passkey)
{
    answer(false);   // прошлый вопрос, если висит, — отказ
    setDelayedReply(true);
    m_pending = message();
    m_timeout.start();
    emit request(device.path(), code6(passkey), "confirm");
}

void BtAgent::RequestAuthorization(const QDBusObjectPath &) {}

void BtAgent::AuthorizeService(const QDBusObjectPath &, const QString &) {}

void BtAgent::Cancel()
{
    m_pending = QDBusMessage();
    m_timeout.stop();
    emit request(QString(), QString(), "cancel");
}
