// naomid — сеть телефона, чего не видно без root: на какой интерфейс пришло SSH-подключение (трансляция на Mac —
// окно приходит через ssh -W, по кабелю или по Wi-Fi: значок пути на столе, docs/STREAM.md).
#pragma once

#include "bus_object.h"

class Net : public BusObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Phone.Net")

public:
    explicit Net(QObject *parent = nullptr);

public slots:
    // SSH-подключение, чей sshd держит сокет 127.0.0.1:port (его сторона канала ssh -W): интерфейс телефона, на
    // который оно пришло, — "usb0" (кабель), "wlan0" (Wi-Fi); "" — такого нет
    Q_SCRIPTABLE QString SshInterface(uint port);
};
