// naomid — «стол на полную» (docs/NAOMID.md, Perf; docs/DESKTOP.md, «Стол на полную»): пока рабочий стол смотрят
// (приставка, окно Mac), телефон — компьютер на питании, а не телефон: ядра не опускают частоту ниже середины и не
// уходят в глубокий сон, оболочка (стол, захват кадра, трансляция) и приложение Android на переднем плане — в поднятых группах
// планировщика, видеочип — на 625 МГц, защита
// процессора по нагреву корпуса — на 4 °C позже. Держит зовущий,
// пока он на шине; ушёл или упал — служба отпускает сама, при запуске — тоже (могла упасть, держа).
// Зачем: кадр стола — короткий всплеск работы раз в 13–17 мс, и регулятор частоты (schedutil) каждый раз начинает его
// с низкой частоты на малом ядре — отсюда редкие долгие кадры. Ускорителей Android в этом ядре нет (cpu_boost,
// msm_performance, WALT, uclamp не собраны), а его служба питания (libperfmgr) делает то же самое записью в те же
// файлы: пол частоты малых ядер у неё на касание — те же 1363200 (/vendor/etc/powerhint.json, INTERACTION).
#pragma once

#include "bus_object.h"

#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QFile>
#include <QSet>

class Gpu;

class Perf : public BusObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Phone.Perf")
    Q_PROPERTY(bool Desk READ desk)   // режим держат

public:
    explicit Perf(Gpu *gpu, QObject *parent = nullptr);
    bool desk() const { return prop("Desk").toBool(); }

public slots:
    // держать «стол на полную» (true), пока зовущий на шине, или отпустить (false); зовущий — оболочка: её
    // процесс служба кладёт в поднятую группу планировщика (/dev/stune/shell)
    Q_SCRIPTABLE void HoldDesk(bool on);

private:
    void apply();
    void floors(bool on);
    void awake(bool on);       // ядра без глубокого сна (PM QoS)
    void boostShell(bool on);
    void boostApps(bool on);   // приложение Android на переднем плане (top-app)
    void skin(bool on);
    Gpu *m_gpu;
    QSet<QString> m_holders;
    QDBusServiceWatcher m_watch;
    uint m_shellPid = 0;
    QFile m_cpuLatency;        // открыт — запрос действует
};
