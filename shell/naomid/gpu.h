// naomid — видеочип (Adreno 620, драйвер kgsl): нижняя граница частоты по просьбе (HoldFast), пока хоть один
// попросивший жив на шине; ушёл или упал — служба отпускает сама.
// Зачем: стол для приставки на Pi (docs/PI.md, «Анимации и след прозрачного»). Сам видеочип в покое стоит на 275 МГц
// из 625 и поднимает частоту, только когда занят долго, — анимация окна или меню (140–180 мс) кончается раньше: её
// первые кадры рисуются и упаковываются для кодера на самой низкой частоте (упаковка 5,5 мс против 3,0 на 500 МГц).
#pragma once

#include "bus_object.h"

#include <QDBusContext>
#include <QDBusServiceWatcher>
#include <QSet>

class Gpu : public BusObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Phone.Gpu")
    Q_PROPERTY(bool Fast READ fast)   // нижняя граница поднята (её держат)

public:
    explicit Gpu(QObject *parent = nullptr);
    bool fast() const { return prop("Fast").toBool(); }

public slots:
    // держать частоту не ниже 500 МГц (true), пока зовущий на шине, или отпустить (false)
    Q_SCRIPTABLE void HoldFast(bool on);

public:
    // «стол на полную» (Perf): наибольшая частота, 625 МГц, — поверх HoldFast
    void holdTop(bool on);

private:
    void apply();
    QSet<QString> m_holders;
    bool m_top = false, m_topSet = false;
    QDBusServiceWatcher m_watch;
    int m_lowest = -1;   // свой уровень без просьб — самый низкий (у kgsl уровни от быстрого 0 к медленному)
};
