#include "system.h"

#include <QLoggingCategory>
#include <QProcess>

Q_LOGGING_CATEGORY(lcSystem, "naomid.system")

System::System(QObject *parent)
    : BusObject("/org/naomi/Phone/System", "org.naomi.Phone.System", parent)
{
}

// systemctl — отдельным процессом: ответ на шину уходит сразу, телефон перезагружается следом
void System::Reboot()
{
    qCInfo(lcSystem, "перезагрузка телефона — просьба с шины");
    QProcess::startDetached("systemctl", {"reboot"});
}

void System::PowerOff()
{
    qCInfo(lcSystem, "выключение телефона — просьба с шины");
    QProcess::startDetached("systemctl", {"poweroff"});
}
