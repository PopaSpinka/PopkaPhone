// naomid — сотовая сеть через oFono (модем /ril_0 — в порте он один, вторая SIM не работает): состояние,
// сила сигнала, оператор. По сигналам PropertyChanged модема, SIM и регистрации в сети.
// Выключатель модема (плитка «модем» в шторке): выключен — модем не в сети (oFono Online = false): не ищет сеть, не
// мешает Wi-Fi (docs/STREAM.md, «Сканы wlan1» — модем в вечном поиске задевал радио); Wi-Fi и Bluetooth работают.
// Выбор помнится (/userdata/naomi/state/cell.conf) и ставится модему заново, когда он появился (перезагрузка, oFono).
#pragma once

#include "bus_object.h"

#include <QDBusVariant>
#include <QTimer>

class Cell : public BusObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Phone.Cell")
    // off (модем выключен), nosim, searching, registered, roaming, unregistered, denied, unknown
    Q_PROPERTY(QString State READ state)
    Q_PROPERTY(int Strength READ strength)    // 0…100
    Q_PROPERTY(QString Operator READ op)
    Q_PROPERTY(bool Enabled READ enabled)     // выбор пользователя: модем включён

public:
    explicit Cell(QObject *parent = nullptr);

    QString state() const { return prop("State").toString(); }
    int strength() const { return prop("Strength").toInt(); }
    QString op() const { return prop("Operator").toString(); }
    bool enabled() const { return prop("Enabled").toBool(); }

public slots:
    Q_SCRIPTABLE void SetEnabled(bool on);   // включить или выключить модем; помнится

private slots:
    void onChanged(const QString &name, const QDBusVariant &value);

private:
    void refresh();
    void apply();   // выбор — модему (Online)

    QTimer m_refresh;   // несколько изменений подряд — одно перечитывание
};
