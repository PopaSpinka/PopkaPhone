#include "web_bridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLocalSocket>
#include <QWindow>

namespace {
constexpr int kRetryMs = 3000;   // RETRY_MS bridge.js НаомиОС
}

WebBridge::WebBridge(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    m_retry.setSingleShot(true);
    m_retry.setInterval(kRetryMs);
    connect(&m_retry, &QTimer::timeout, this, &WebBridge::connectToShell);
    connect(m_socket, &QLocalSocket::connected, this, [this] {
        send({{"type", "hello"}, {"version", "1.0"}});
        emit linked();
    });
    connect(m_socket, &QLocalSocket::disconnected, &m_retry, qOverload<>(&QTimer::start));
    connect(m_socket, &QLocalSocket::errorOccurred, &m_retry, qOverload<>(&QTimer::start));
    connect(m_socket, &QLocalSocket::readyRead, this, &WebBridge::read);
    QCoreApplication::instance()->installEventFilter(this);
    connectToShell();
}

void WebBridge::connectToShell()
{
    m_socket->abort();
    m_socket->connectToServer(qEnvironmentVariable("XDG_RUNTIME_DIR", QDir::tempPath()) + "/naomi-web.sock");
}

void WebBridge::send(const QVariantMap &message)
{
    if (m_socket->state() == QLocalSocket::ConnectedState)
        m_socket->write(QJsonDocument(QJsonObject::fromVariantMap(message)).toJson(QJsonDocument::Compact) + '\n');
}

void WebBridge::read()
{
    while (m_socket->canReadLine()) {
        const QJsonDocument doc = QJsonDocument::fromJson(m_socket->readLine());
        if (doc.isObject())
            emit command(doc.object().toVariantMap());
    }
}

// Сочетания — как у bcdesk (webframe_key, ARCHITECTURE НаомиОС «Браузер»): по латинской раскладке, с ⌘ не берём
// (⌘ Mac окно трансляции и так шлёт столу как Ctrl). Взятую клавишу страница не получает — и её отпускание тоже.
// Плюс то, что в НаомиОС делал сам Chromium: F5 и Ctrl+R, Alt+←/→, Ctrl+плюс/минус/0.
bool WebBridge::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::CursorChange && watched->isWindowType()) {   // форма курсора — столу
        const int shape = static_cast<QWindow *>(watched)->cursor().shape();
        if (shape != m_cursor) {
            m_cursor = shape;
            send({{"type", "cursor"}, {"shape", shape}});
        }
        return QObject::eventFilter(watched, event);
    }
    // клавиша идёт окну, потом элементу в фокусе — берём у элемента (так и та, что модуль ввода шлёт прямо ему)
    if ((event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) || watched->isWindowType())
        return QObject::eventFilter(watched, event);
    auto *e = static_cast<QKeyEvent *>(event);
    const Qt::KeyboardModifiers mods = e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier |
                                                         Qt::MetaModifier);
    const bool ctrl = mods == Qt::ControlModifier, ctrlShift = mods == (Qt::ControlModifier | Qt::ShiftModifier);
    const int key = e->key();
    QString action;
    int index = 0;
    if ((ctrl && key == Qt::Key_L) || (mods == Qt::AltModifier && key == Qt::Key_D) || (!mods && key == Qt::Key_F6))
        action = "field";
    else if (ctrl && (key == Qt::Key_T || key == Qt::Key_N))
        action = "new-tab";
    else if (ctrlShift && key == Qt::Key_T)
        action = "restore-closed";
    else if (ctrl && (key == Qt::Key_W || key == Qt::Key_F4))
        action = "close";
    else if ((ctrl && (key == Qt::Key_Tab || key == Qt::Key_PageDown)))
        action = "next";
    else if ((ctrlShift && (key == Qt::Key_Backtab || key == Qt::Key_Tab)) || (ctrl && key == Qt::Key_PageUp))
        action = "previous";
    else if (ctrl && key >= Qt::Key_1 && key <= Qt::Key_9) {
        action = "select";
        index = key == Qt::Key_9 ? -1 : key - Qt::Key_1;
    } else if ((!mods && key == Qt::Key_F5) || (ctrl && key == Qt::Key_R))
        action = "reload";
    else if (mods == Qt::AltModifier && key == Qt::Key_Left)
        action = "back";
    else if (mods == Qt::AltModifier && key == Qt::Key_Right)
        action = "forward";
    else if ((ctrl || ctrlShift) && (key == Qt::Key_Plus || key == Qt::Key_Equal))
        action = "zoom-in";
    else if (ctrl && key == Qt::Key_Minus)
        action = "zoom-out";
    else if (ctrl && key == Qt::Key_0)
        action = "zoom-reset";
    else if (!mods && key == Qt::Key_Escape && m_fullscreen)
        action = "exit-fullscreen";
    if (action.isEmpty())
        return QObject::eventFilter(watched, event);
    if (event->type() == QEvent::KeyPress && !e->isAutoRepeat())
        emit shortcut(action, index);
    return true;
}
