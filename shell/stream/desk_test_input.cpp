#include "desk_test_input.h"

#include "stream_input.h"
#include "stream_service.h"

#include <QDBusConnection>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDeskTest, "naomi.stream")

DeskTestInput::DeskTestInput(StreamInput *input, StreamService *service)
    : QObject(service)
    , m_input(input)
    , m_service(service)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/input"), this, QDBusConnection::ExportScriptableSlots)
        || !bus.registerService(QStringLiteral("org.naomi.DeskTest")))
        qCWarning(lcDeskTest, "проверочная мышь стола: не занять org.naomi.DeskTest на сессионной шине");
}

void DeskTestInput::Pointer(int buttons, double x, double y)
{
    m_input->pointer(buttons, x, y);
}

void DeskTestInput::Key(bool down, int qtKey, int modifiers, const QString &text)
{
    m_input->key(down, qtKey, modifiers, text);
}

QString DeskTestInput::Screens()
{
    const QJsonObject o{{"desk", m_service->enabled()}, {"connected", m_service->connected()},
                        {"viewer", m_service->viewer()}, {"away", m_service->deskAway()},
                        {"screens", QJsonArray::fromVariantList(m_service->screens())}};
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

void DeskTestInput::ScreenPosition(const QString &id, int position)
{
    m_service->setScreenPosition(id, position);
}

void DeskTestInput::Desk(bool on)
{
    m_service->setEnabled(on);
}

void DeskTestInput::Wake(const QString &id)
{
    m_service->wakeScreen(id);
}

void DeskTestInput::ScreenLight(const QString &id, bool automatic, int level)
{
    m_service->setScreenLight(id, automatic, level);
}
