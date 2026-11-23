#include "web_link.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QUrl>

Q_LOGGING_CATEGORY(lcWeb, "naomi.web")

namespace {
constexpr qint64 kMaxLine = 1 << 20;   // строка не длиннее 1 МБ — как у bcdesk (NaomiOS web.c)
}

WebLink::WebLink(QObject *parent)
    : QObject(parent)
    , m_server(new QLocalServer(this))
{
    const QString path = qEnvironmentVariable("XDG_RUNTIME_DIR", QDir::tempPath()) + "/naomi-web.sock";
    QLocalServer::removeServer(path);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(path))
        qCWarning(lcWeb, "сокет браузера %s: %s", qPrintable(path), qPrintable(m_server->errorString()));
    connect(m_server, &QLocalServer::newConnection, this, &WebLink::onConnection);
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/web"), this, QDBusConnection::ExportScriptableSlots)
        || !bus.registerService(QStringLiteral("org.naomi.Web")))
        qCWarning(lcWeb, "org.naomi.Web на сессионной шине: не занять (%s) — ссылки снаружи не откроются",
                  qPrintable(bus.lastError().message()));
    bool ok = false;
    const int delay = qEnvironmentVariableIntValue("NAOMI_WEB_FREEZE_S", &ok);
    m_freezeLater.setSingleShot(true);
    m_freezeLater.setInterval((ok && delay >= 0 ? delay : 120) * 1000);
    connect(&m_freezeLater, &QTimer::timeout, this, [this] {
        qCInfo(lcWeb, "зрителя нет %d с — браузер замораживается", m_freezeLater.interval() / 1000);
        freeze(true);
    });
    m_freezeReply.setSingleShot(true);
    m_freezeReply.setInterval(2000);
    connect(&m_freezeReply, &QTimer::timeout, this, [this] {
        qCWarning(lcWeb, "браузер не ответил, что страницы заморожены, — замораживаю ядром так");
        emit freezeDesk(true);
    });
}

void WebLink::Open(const QString &url)
{
    const QUrl u(url, QUrl::StrictMode);
    if (!u.isValid() || (u.scheme() != QLatin1String("http") && u.scheme() != QLatin1String("https"))) {
        qCWarning(lcWeb, "ссылка снаружи не открыта: не http(s) — %s", qPrintable(url.left(200)));
        return;
    }
    qCInfo(lcWeb, "ссылка снаружи — в браузер: %s", qPrintable(u.host()));
    emit openRequested(u.toString());
}

void WebLink::setDesk(bool enabled, bool viewer)
{
    const bool wasViewer = m_viewer, wasEnabled = m_enabled;
    m_enabled = enabled;
    m_viewer = viewer;
    if (viewer) {
        m_freezeLater.stop();
        freeze(false);
    } else if (!enabled) {
        m_freezeLater.stop();
        if (wasEnabled && !m_frozen)
            qCInfo(lcWeb, "трансляцию выключили — браузер замораживается сразу");
        freeze(true);
    } else if (!m_frozen && !m_freezeLater.isActive()) {
        if (wasViewer)
            pause();
        m_freezeLater.start();
    }
}

void WebLink::pause()
{
    if (!m_browser)
        return;
    qCInfo(lcWeb, "зритель ушёл — видео и звук браузера на паузе");
    send({{"type", "pause"}});
}

// Заморозка — сначала страницы (браузер: пауза, Frozen; ответит frozen), потом ядро; оттепель — наоборот: ядро первым,
// иначе браузер не прочтёт «оттаять»
void WebLink::freeze(bool on)
{
    if (on == m_frozen)
        return;
    m_frozen = on;
    if (on) {
        if (m_browser) {
            send({{"type", "freeze"}, {"on", true}});
            m_freezeReply.start();
        } else {
            emit freezeDesk(true);
        }
        return;
    }
    m_freezeReply.stop();
    emit freezeDesk(false);
    if (m_browser) {
        qCInfo(lcWeb, "зритель вернулся — браузер оттаял");
        send({{"type", "freeze"}, {"on", false}});
    }
}

void WebLink::onConnection()
{
    while (QLocalSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QLocalSocket::readyRead, this, [this, socket] { read(socket); });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            if (socket == m_browser) {
                qCInfo(lcWeb, "браузер отключился");
                reset();
            }
            socket->deleteLater();
        });
    }
}

void WebLink::read(QLocalSocket *socket)
{
    while (socket->canReadLine()) {
        const QByteArray line = socket->readLine(kMaxLine);
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(line, &error);
        if (doc.isObject())
            handle(socket, doc.object().toVariantMap());
    }
    if (socket->bytesAvailable() > kMaxLine) {
        qCWarning(lcWeb, "слишком длинная строка — связь закрыта");
        socket->abort();
    }
}

void WebLink::handle(QLocalSocket *socket, const QVariantMap &m)
{
    const QString type = m.value("type").toString();
    if (type == "hello") {
        if (m_browser && m_browser != socket)
            m_browser->disconnectFromServer();
        reset();
        m_browser = socket;
        qCInfo(lcWeb, "браузер на связи (%s)", qPrintable(m.value("version").toString()));
        emit connectedChanged();
        if (!m_viewer && !m_enabled)
            freeze(true);   // браузер (пере)запустился, а трансляция выключена
        else if (!m_viewer && !m_freezeLater.isActive())
            m_freezeLater.start();   // смотреть некому
        return;
    }
    if (socket != m_browser)
        return;
    if (type == "tabs") {
        m_tabs = m.value("tabs").toList();
        m_active = m.value("active", -1).toInt();
        emit tabsChanged();
    } else if (type == "bookmarks") {
        m_bookmarks = m.value("items").toList();
        emit bookmarksChanged();
    } else if (type == "downloads") {
        m_downloads = m.value("items").toList();
        emit downloadsChanged();
    } else if (type == "cursor") {
        const int shape = m.value("shape").toInt();
        if (shape != m_cursorShape) {
            m_cursorShape = shape;
            emit cursorShapeChanged();
        }
    } else if (type == "fullscreen") {
        const bool on = m.value("on").toBool();
        if (on != m_fullscreen) {
            m_fullscreen = on;
            emit fullscreenChanged();
        }
    } else if (type == "contextmenu") {
        emit contextMenu(m.value("tab", -1).toInt());
    } else if (type == "field") {
        emit fieldRequested(m.contains("text") ? m.value("text") : QVariant());
    } else if (type == "frozen") {   // страницы заморожены — теперь ядром весь браузер
        if (m_frozen && m_freezeReply.isActive()) {
            m_freezeReply.stop();
            emit freezeDesk(true);
        }
    }
}

void WebLink::send(const QVariantMap &message)
{
    if (!m_browser)
        return;
    m_browser->write(QJsonDocument(QJsonObject::fromVariantMap(message)).toJson(QJsonDocument::Compact) + '\n');
}

void WebLink::reset()
{
    const bool was = m_browser;
    m_browser = nullptr;
    m_tabs.clear();
    m_bookmarks.clear();
    m_downloads.clear();
    m_active = -1;
    m_fullscreen = false;
    m_cursorShape = Qt::ArrowCursor;
    m_frozen = false;
    m_freezeReply.stop();
    emit tabsChanged();
    emit bookmarksChanged();
    emit cursorShapeChanged();
    emit downloadsChanged();
    emit fullscreenChanged();
    if (was)
        emit connectedChanged();
}
