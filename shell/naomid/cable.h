// naomid — интернет по кабелю через приставку (docs/PI.md, «Интернет по кабелю»). Маршрут ставит и убирает
// device/naomi-usb (аренда DHCP приставки, проверка, выключатель); здесь — его состояние для стола и выключатель окна
// «Интернет» (docs/DESKTOP.md). Состояние — из ядра: аренда приставки (/run/naomi-usb/gateway), несущая кабеля,
// маршрут в интернет через usb0 (/proc/net/route), выключен ли (/run/naomi-usb/internet-off). Меняется — по событиям
// netlink о маршрутах и интерфейсах, без опроса.
#pragma once

#include "bus_object.h"

#include <QTimer>

class QSocketNotifier;

class Cable : public BusObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Phone.Cable")
    Q_PROPERTY(bool Dock READ dock)          // на кабеле приставка (её аренда, кабель вставлен)
    Q_PROPERTY(bool Enabled READ enabled)    // интернет по кабелю разрешён
    Q_PROPERTY(bool Active READ active)      // интернет сейчас идёт по кабелю

public:
    explicit Cable(QObject *parent = nullptr);
    ~Cable() override;

    bool dock() const { return prop("Dock").toBool(); }
    bool enabled() const { return prop("Enabled").toBool(); }
    bool active() const { return prop("Active").toBool(); }

public slots:
    // разрешить интернет по кабелю или нет (naomi-usb internet on|off); выбор помнит стол — до перезагрузки здесь
    Q_SCRIPTABLE void SetEnabled(bool on);

private:
    void update();

    int m_netlink = -1;
    QSocketNotifier *m_notifier = nullptr;
    QTimer m_settle;   // события netlink идут пачкой (маршруты, адреса) — перечитать один раз после них
};
