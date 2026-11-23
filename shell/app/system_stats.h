// «Настройки» → «нагрузка» и «адреса и трафик»: память, загрузка процессора, частоты ядер, адреса сетей и трафик
// (в QML — naomiSystem). Всё — из /proc и /sys напрямую (прав не нужно, служба naomid тут ни при чём). Загрузка
// и трафик — за время между двумя замерами: экран зовёт sample() раз в 3 с, пока открыт; первый замер их ещё не
// знает (−1).
#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QVariantMap>

class SystemStats : public QObject
{
    Q_OBJECT
public:
    using QObject::QObject;

    // memUsed, memTotal (байты), cpuLoad (0…100 или −1), cores, freqs [ГГц по кластерам: малые, большие, главное],
    // nets [{kind: wifi | cable | mobile, iface, ip}], rxRate, txRate (байт/с по всем внешним, или −1)
    Q_INVOKABLE QVariantMap sample();

private:
    quint64 m_cpuBusy = 0, m_cpuAll = 0;
    quint64 m_rx = 0, m_tx = 0;
    QElapsedTimer m_since;
};
