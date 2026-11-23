#include "sensors.h"

#include <QCoreApplication>
#include <QDBusConnectionInterface>
#include <QLoggingCategory>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <cmath>
#include <iterator>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcSensors, "naomid.sensors")

namespace {

const char SVC[] = "com.nokia.SensorService";
const char MANAGER[] = "/SensorManager";
const char MANAGER_IFACE[] = "local.SensorManager";
const char SOCKET_PATH[] = "/run/sensord.sock";
// движение: отход от опоры (mG) — сдвинули; клиентам — не чаще раза в секунду; опора идёт за покоем на 10 % замера
const float MOVE_MG = 50;
const qint64 MOVE_GAP_MS = 1000;
const float BASE_FOLLOW = 0.1f;

// PoseData::Orientation в sensorfw: Undefined, LeftUp, RightUp, BottomUp, BottomDown, FaceDown, FaceUp
QString orientationName(quint32 v)
{
    static const char *names[] = {"unknown", "left-up", "right-up", "bottom-up", "top-up", "face-down", "face-up"};
    return v < std::size(names) ? names[v] : "unknown";
}

qint64 myPid() { return QCoreApplication::applicationPid(); }

} // namespace

Sensors::Sensors(QObject *parent)
    : BusObject("/org/naomi/Phone/Sensors", "org.naomi.Phone.Sensors", parent)
{
    m_clients.setConnection(bus());
    m_clients.setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    m_channels = {
        {"orientation", "orientationsensor", "local.OrientationSensor", "orientation"},
        {"light", "alssensor", "local.ALSSensor", "lux"},
        {"motion", "accelerometersensor", "local.AccelerometerSensor", {}, 24, 500},
    };
    set("Orientation", "unknown");
    set("Light", -1);
    set("Motion", 0u);

    connect(&m_clients, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &who) {
        m_clients.removeWatchedService(who);
        if (m_wants.remove(who))
            update();
    });
    // sensorfwd перезапустился — сеансы пропали (сокет закрылся); открыть заново, что нужно
    follow(SVC, [this] {
        m_serviceUp = true;
        update();
    });
}

Sensors::~Sensors()
{
    for (Channel &c : m_channels)
        closeChannel(c, false);
}

Sensors::Channel *Sensors::channel(const QString &name)
{
    for (Channel &c : m_channels)
        if (c.name == name)
            return &c;
    return nullptr;
}

void Sensors::Watch(const QStringList &sensors)
{
    const QString who = calledFromDBus() ? message().service() : QString();
    QSet<QString> want;
    for (const QString &s : sensors) {
        if (channel(s))
            want.insert(s);
        else
            qCWarning(lcSensors, "нет такого датчика: %s", qPrintable(s));
    }
    if (want.isEmpty()) {
        m_wants.remove(who);
        m_clients.removeWatchedService(who);
    } else {
        m_wants[who] = want;
        if (!who.isEmpty() && !m_clients.watchedServices().contains(who))
            m_clients.addWatchedService(who);
    }
    update();
}

void Sensors::update()
{
    for (Channel &c : m_channels) {
        bool wanted = false;
        for (const QSet<QString> &w : std::as_const(m_wants))
            wanted = wanted || w.contains(c.name);
        if (wanted && !c.open && m_serviceUp)
            openChannel(c);
        else if (!wanted && c.open)
            closeChannel(c);
    }
}

// Шаги по очереди, все без ожидания. Если пока шли ответы, датчик стал не нужен или открытие началось
// заново, generation уже другое — ответ выбрасывается.
void Sensors::openChannel(Channel &c)
{
    c.open = true;
    const int gen = ++c.generation;
    const QString name = c.name;
    auto alive = [this, name, gen]() -> Channel * {
        Channel *ch = channel(name);
        return ch && ch->open && ch->generation == gen ? ch : nullptr;
    };
    auto fail = [this, name](const char *step, const QDBusMessage &r) {
        qCWarning(lcSensors, "датчик %s: %s: %s", qPrintable(name), step, qPrintable(r.errorMessage()));
        if (Channel *ch = channel(name))
            closeChannel(*ch);
    };

    callAsync(SVC, MANAGER, MANAGER_IFACE, "loadPlugin", {c.id}, [=, this](const QDBusMessage &r) {
        Channel *ch = alive();
        if (!ch)
            return;
        if (r.type() == QDBusMessage::ErrorMessage || !r.arguments().value(0).toBool())
            return fail("loadPlugin", r);
        callAsync(SVC, MANAGER, MANAGER_IFACE, "requestSensor", {ch->id, myPid()}, [=, this](const QDBusMessage &r) {
            Channel *ch = alive();
            const int session = r.arguments().value(0).toInt();
            if (!ch) {   // пока ждали, стал не нужен — вернуть сеанс
                if (session >= 0)
                    callAsync(SVC, MANAGER, MANAGER_IFACE, "releaseSensor", {channel(name)->id, session, myPid()});
                return;
            }
            if (r.type() == QDBusMessage::ErrorMessage || session < 0)
                return fail("requestSensor", r);
            ch->session = session;

            ch->fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
            sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
            const qint32 id = session;
            if (ch->fd < 0 || ::connect(ch->fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0
                || ::write(ch->fd, &id, sizeof(id)) != sizeof(id)) {
                qCWarning(lcSensors, "датчик %s: сокет %s: %s", qPrintable(name), SOCKET_PATH, std::strerror(errno));
                closeChannel(*ch);
                return;
            }
            ch->notifier = new QSocketNotifier(ch->fd, QSocketNotifier::Read, this);
            connect(ch->notifier, &QSocketNotifier::activated, this, [this, name] {
                if (Channel *ch = channel(name))
                    readChannel(*ch);
            });

            const QString path = QString(MANAGER) + "/" + ch->id;
            if (ch->intervalMs > 0)   // сообщения одной службе идут по порядку: шаг ляжет до start
                callAsync(SVC, path, ch->iface, "setInterval", {session, ch->intervalMs}, [name](const QDBusMessage &r) {
                    if (r.type() == QDBusMessage::ErrorMessage)
                        qCWarning(lcSensors, "датчик %s: setInterval: %s", qPrintable(name), qPrintable(r.errorMessage()));
                });
            callAsync(SVC, path, ch->iface, "start", {session}, [=, this](const QDBusMessage &r) {
                Channel *ch = alive();
                if (!ch)
                    return;
                if (r.type() == QDBusMessage::ErrorMessage)
                    return fail("start", r);
                qCDebug(lcSensors, "датчик %s: включён, сеанс %d", qPrintable(name), ch->session);
                if (ch->valueProp.isEmpty())
                    return;
                callAsync(SVC, path, "org.freedesktop.DBus.Properties", "Get", {ch->iface, ch->valueProp},
                          [=, this](const QDBusMessage &r) {
                              Channel *ch = alive();
                              const QVariantList v = plain(r.arguments().value(0)).toList();   // (метка, значение)
                              if (ch && v.size() == 2)
                                  apply(*ch, v.at(1).toUInt());
                          });
            });
        });
    });
}

// Сокет закрывается последним: sensorfwd, увидев закрытый сокет, сам снимает сеанс, и тогда releaseSensor
// уже не находит его. Канал при этом свободен сразу — новое открытие идёт со своим сокетом.
void Sensors::closeChannel(Channel &c, bool tellService)
{
    if (c.open)
        qCDebug(lcSensors, "датчик %s: выключен", qPrintable(c.name));
    delete c.notifier;
    c.notifier = nullptr;
    if (c.open && c.session >= 0 && c.fd >= 0 && tellService) {
        const int fd = c.fd, session = c.session;
        const QString id = c.id;
        callAsync(SVC, QString(MANAGER) + "/" + id, c.iface, "stop", {session}, [this, fd, session, id](const QDBusMessage &) {
            callAsync(SVC, MANAGER, MANAGER_IFACE, "releaseSensor", {id, session, myPid()},
                      [fd](const QDBusMessage &) { ::close(fd); });
        });
    } else if (c.fd >= 0) {
        ::close(c.fd);
    }
    c.fd = -1;
    c.session = -1;
    c.open = false;
    c.greeted = false;
    c.buf.clear();
    ++c.generation;
    if (c.name == "orientation")
        set("Orientation", "unknown");
    else if (c.name == "light")
        set("Light", -1);
    else
        m_hasBase = false;   // включат снова — опора с первого замера
}

void Sensors::readChannel(Channel &c)
{
    char chunk[512];
    for (;;) {
        const ssize_t n = ::read(c.fd, chunk, sizeof(chunk));
        if (n > 0) {
            c.buf.append(chunk, n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EINTR))
            break;
        // sensorfwd закрыл сокет (перезапуск) — открыть снова, если ещё нужно
        qCWarning(lcSensors, "датчик %s: sensorfwd закрыл сокет", qPrintable(c.name));
        closeChannel(c, false);
        m_serviceUp = bus().interface()->isServiceRegistered(SVC);
        QTimer::singleShot(2000, this, &Sensors::update);
        return;
    }
    if (!c.greeted && !c.buf.isEmpty()) {
        if (c.buf.at(0) != '\n')
            qCWarning(lcSensors, "датчик %s: странное приветствие 0x%02x", qPrintable(c.name), uchar(c.buf.at(0)));
        c.buf.remove(0, 1);
        c.greeted = true;
    }
    // пачки: uint32 сколько, за ним замеры; берётся последний (движению — каждый)
    while (c.buf.size() >= 4) {
        quint32 count;
        std::memcpy(&count, c.buf.constData(), 4);
        if (count == 0 || count > 1000) {
            qCWarning(lcSensors, "датчик %s: битая пачка (%u замеров) — сброс", qPrintable(c.name), count);
            c.buf.clear();
            return;
        }
        const qsizetype need = 4 + qsizetype(count) * c.sampleBytes;
        if (c.buf.size() < need)
            return;
        if (c.name == "motion") {
            for (quint32 i = 0; i < count; ++i) {
                float xyz[3];
                std::memcpy(xyz, c.buf.constData() + 4 + qsizetype(i) * c.sampleBytes + 8, sizeof(xyz));
                motionSample(xyz[0], xyz[1], xyz[2]);
            }
        } else {
            quint32 value;
            std::memcpy(&value, c.buf.constData() + need - c.sampleBytes + 8, 4);
            apply(c, value);
        }
        c.buf.remove(0, need);
    }
}

void Sensors::apply(Channel &c, quint32 value)
{
    if (c.name == "orientation") {
        set("Orientation", orientationName(value));
        return;
    }
    // освещённость дрожит (лампы, тень руки): клиентам — изменение от 5 % (в темноте — от 1 люкса)
    const int lux = int(qMin<quint32>(value, 1000000));
    const int old = light();
    if (old < 0 || qAbs(lux - old) >= qMax(1, old / 20))
        set("Light", lux);
}

void Sensors::motionSample(float x, float y, float z)
{
    const float a[3] = {x, y, z};
    if (!m_hasBase) {
        std::memcpy(m_base, a, sizeof(a));
        m_hasBase = true;
        return;
    }
    const float d = std::hypot(a[0] - m_base[0], a[1] - m_base[1], a[2] - m_base[2]);
    if (d < MOVE_MG) {
        for (int i = 0; i < 3; ++i)
            m_base[i] += (a[i] - m_base[i]) * BASE_FOLLOW;
        return;
    }
    std::memcpy(m_base, a, sizeof(a));   // движется — опора за ним: следующий отход — новое движение
    if (m_lastMove.isValid() && m_lastMove.elapsed() < MOVE_GAP_MS)
        return;
    m_lastMove.start();
    qCDebug(lcSensors, "движение: %.0f mG", d);
    set("Motion", motion() + 1);
}
