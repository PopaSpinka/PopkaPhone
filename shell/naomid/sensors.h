// naomid — датчики: положение телефона (для автоповорота), освещённость (для автояркости) и движение (для AOD:
// телефон сдвинули — экран выходит из малого питания, docs/SHELL.md, «AOD»). Источник —
// служба датчиков порта sensorfwd (com.nokia.SensorService, датчики Android через HIDL). Датчик работает,
// только пока он кому-то нужен: клиент говорит Watch(["light", "orientation"]), Watch([]) — ничего не нужно;
// клиент пропал с шины — его просьба снимается. Оболочка при погашенном экране просит пустой список —
// датчики стоят, служба спит.
// Движение — акселерометр, 2 замера в секунду: ускорение отошло от опоры больше чем на 50 mG (≈ 3° наклона или
// толчок) — сдвинули; опора медленно идёт за покоем.
//
// Разговор с sensorfwd (своя реализация по его протоколу, docs/NAOMID.md): loadPlugin → requestSensor —
// номер сеанса; сокет /run/sensord.sock — записать номер сеанса (int32), в ответ «\n»; start(сеанс). Дальше
// по сокету приходят пачки: uint32 — сколько замеров, за ним замеры по 16 байт (метка времени uint64 и
// значение uint32; у акселерометра — 24 байта: метка и x, y, z float, mG). Без сокета sensorfwd через 10 с
// считает сеанс потерянным. Начальное значение — свойство датчика по D-Bus: положение и освещённость
// присылаются только при изменении.
#pragma once

#include "bus_object.h"

#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>

class QSocketNotifier;

class Sensors : public BusObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Phone.Sensors")
    // top-up — обычно (верх телефона вверху), left-up, right-up — боком (вверху левый / правый край),
    // bottom-up — вверх ногами, face-up / face-down — лежит, unknown — не знаю или датчик не включён
    Q_PROPERTY(QString Orientation READ orientation)
    Q_PROPERTY(int Light READ light)   // освещённость, люкс; −1 — датчик не включён
    Q_PROPERTY(uint Motion READ motion)   // сколько раз сдвинули с запуска службы (не чаще раза в секунду)

public:
    explicit Sensors(QObject *parent = nullptr);
    ~Sensors() override;

    QString orientation() const { return prop("Orientation").toString(); }
    int light() const { return prop("Light").toInt(); }
    uint motion() const { return prop("Motion").toUInt(); }

public slots:
    // какие датчики нужны тому, кто зовёт: "orientation", "light", "motion"; заменяет его прошлую просьбу
    Q_SCRIPTABLE void Watch(const QStringList &sensors);

private:
    struct Channel {
        QString name;          // orientation, light, motion
        QString id;            // имя датчика в sensorfwd
        QString iface;         // интерфейс его объекта на шине
        QString valueProp;     // свойство с последним значением (метка, значение); пусто — не нужно
        int sampleBytes = 16;  // замер в пачке
        int intervalMs = 0;    // просить такой шаг замеров; 0 — как у sensorfwd
        bool open = false;     // сеанс есть или открывается
        int session = -1;
        int fd = -1;
        QSocketNotifier *notifier = nullptr;
        bool greeted = false;  // «\n» от sensorfwd получен
        QByteArray buf;
        int generation = 0;    // ответы на шаги старого открытия — мимо
    };

    void update();                      // открыть нужные, закрыть ненужные
    void openChannel(Channel &c);
    void closeChannel(Channel &c, bool tellService = true);
    void readChannel(Channel &c);
    void apply(Channel &c, quint32 value);
    void motionSample(float x, float y, float z);
    Channel *channel(const QString &name);

    QList<Channel> m_channels;
    QHash<QString, QSet<QString>> m_wants;   // клиент (уникальное имя на шине) → нужные датчики
    QDBusServiceWatcher m_clients;
    bool m_serviceUp = false;
    bool m_hasBase = false;    // опора движения: ускорение в покое, mG
    float m_base[3] = {};
    QElapsedTimer m_lastMove;
};
