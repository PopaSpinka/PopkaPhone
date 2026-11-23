#include "term_link.h"

#include "../term/term_protocol.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QLoggingCategory>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <sys/socket.h>

Q_LOGGING_CATEGORY(lcTerm, "naomi.term")

namespace {

constexpr int kBudget = 512 * 1024;   // за раз разобрать не больше — стол не стоит, пока программа сыплет текстом

QString socketPath()
{
    return qEnvironmentVariable("XDG_RUNTIME_DIR", QDir::tempPath()) + QStringLiteral("/naomi-term.sock");
}

QString statePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + QStringLiteral("/naomi/desktop/term.json");
}

QString baseName(const QString &cwd)
{
    if (cwd == QLatin1String("~") || cwd == QLatin1String("/"))
        return cwd;
    return cwd.section(QLatin1Char('/'), -1);
}

}   // namespace

// ---------- вкладка ----------
TermSession::TermSession(int id, TermLink *link, int cols, int rows)
    : QObject(link)
    , m_id(id)
    , m_link(link)
{
    reset(cols, rows);
}

void TermSession::reset(int cols, int rows)
{
    m_screen = std::make_unique<term::Screen>(cols, rows);
    m_screen->reply = [this](const QByteArray &bytes) {
        if (!m_replaying)
            input(bytes);
    };
    anchor = -1;
    emit changed();
}

void TermSession::feed(const QByteArray &data, bool replay)
{
    m_replaying = replay;
    m_screen->feed(data.constData(), size_t(data.size()));
    m_replaying = false;
    emit changed();
}

void TermSession::input(const QByteArray &bytes)
{
    m_link->send(termproto::Input, m_id, bytes);
}

// ---------- терминал ----------
TermLink::TermLink(QObject *parent)
    : QObject(parent)
{
    load();
    m_save.setSingleShot(true);
    m_save.setInterval(500);
    connect(&m_save, &QTimer::timeout, this, &TermLink::save);
    m_retry.setSingleShot(true);
    m_retry.setInterval(100);
    connect(&m_retry, &QTimer::timeout, this, &TermLink::connectService);
    m_more.setSingleShot(true);
    m_more.setInterval(0);
    connect(&m_more, &QTimer::timeout, this, &TermLink::processInput);
}

TermLink::~TermLink()
{
    if (m_save.isActive())
        save();
}

QVariantList TermLink::tabs() const
{
    QVariantList out;
    for (const Tab &t : m_tabs) {
        const QString label = !t.name.isEmpty() ? t.name : !t.proc.isEmpty() ? t.proc : baseName(t.cwd);
        out.append(QVariantMap {{"id", t.id}, {"name", t.name}, {"label", label}, {"pinned", t.pinned},
                                {"cwd", t.cwd}, {"proc", t.proc}, {"running", !t.proc.isEmpty()}});
    }
    return out;
}

QObject *TermLink::activeSession() const
{
    const int i = indexOf(m_active);
    return i >= 0 ? m_tabs[i].session : nullptr;
}

int TermLink::indexOf(int id) const
{
    for (int i = 0; i < m_tabs.size(); ++i)
        if (m_tabs[i].id == id)
            return i;
    return -1;
}

TermLink::Tab *TermLink::tab(int id)
{
    const int i = indexOf(id);
    return i >= 0 ? &m_tabs[i] : nullptr;
}

void TermLink::setActive(int id)
{
    if (id == m_active)
        return;
    m_active = id;
    emit activeChanged();
    emit activeSessionChanged();
    saveLater();
}

// ---------- открыть и закрыть ----------
void TermLink::start()
{
    if (m_running)
        return;
    m_running = true;
    emit runningChanged();
    if (m_tabs.isEmpty())
        newTab();
    if (indexOf(m_active) < 0)
        setActive(m_tabs.first().id);
    for (Tab &t : m_tabs)
        if (!t.session)
            t.session = new TermSession(t.id, this, std::max(m_cols, 20), std::max(m_rows, 5));
    emit activeSessionChanged();
    m_attempts = 0;
    connectService();
}

qint64 TermLink::servicePid() const
{
    if (!m_socket || !m_connected)
        return -1;
    ucred cred {};
    socklen_t len = sizeof(cred);
    if (getsockopt(int(m_socket->socketDescriptor()), SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0 || cred.pid <= 0)
        return -1;
    return cred.pid;
}

void TermLink::quit()
{
    if (!m_running)
        return;
    qCInfo(lcTerm, "терминал закрыт: вкладок %lld — текст на диске, оболочки закрыты", qlonglong(m_tabs.size()));
    m_running = false;   // раньше отключения: иначе onDisconnected запустил бы службу снова
    m_retry.stop();
    send(termproto::Quit, 0, QByteArray());
    if (m_socket) {
        m_socket->flush();
        m_socket->disconnectFromServer();
    }
    for (Tab &t : m_tabs) {
        delete t.session;
        t.session = nullptr;
        t.opened = false;
        t.proc.clear();
    }
    emit activeSessionChanged();
    emit tabsChanged();
    emit runningChanged();
    save();
}

int TermLink::newTab(int after, const QString &cwd)
{
    Tab t;
    t.id = m_next++;
    if (!cwd.isEmpty())
        t.cwd = cwd;
    int at = m_tabs.size();
    const int i = indexOf(after);
    if (i >= 0)
        at = i + 1;
    while (at < m_tabs.size() && m_tabs[at].pinned)   // новая — не закреплена: за закреплёнными
        ++at;
    if (m_running) {
        t.session = new TermSession(t.id, this, std::max(m_cols, 20), std::max(m_rows, 5));
    }
    m_tabs.insert(at, t);
    emit tabsChanged();
    setActive(t.id);
    emit activeSessionChanged();
    saveLater();
    if (m_running)
        openSessions();
    return t.id;
}

void TermLink::removeTab(int index)
{
    const int id = m_tabs[index].id;
    delete m_tabs[index].session;
    m_tabs.removeAt(index);
    if (m_active == id) {
        if (m_tabs.isEmpty())
            m_active = -1;
        else
            m_active = m_tabs[std::min(index, int(m_tabs.size()) - 1)].id;
        emit activeChanged();
        emit activeSessionChanged();
    }
    emit tabsChanged();
    saveLater();
    if (m_tabs.isEmpty()) {   // последнюю закрыли — терминал закрывается
        quit();
        emit finished();
    }
}

void TermLink::closeTab(int id)
{
    const int i = indexOf(id);
    if (i < 0)
        return;
    send(termproto::Close, id, QByteArray());
    removeTab(i);
}

void TermLink::activate(int id)
{
    if (indexOf(id) >= 0)
        setActive(id);
}

void TermLink::pin(int id, bool on)
{
    const int i = indexOf(id);
    if (i < 0 || m_tabs[i].pinned == on)
        return;
    Tab t = m_tabs.takeAt(i);
    t.pinned = on;
    // закреплённая — в конец закреплённых, открепили — первой среди прочих
    int at = 0;
    while (at < m_tabs.size() && m_tabs[at].pinned)
        ++at;
    m_tabs.insert(at, t);
    emit tabsChanged();
    saveLater();
}

void TermLink::rename(int id, const QString &name)
{
    Tab *t = tab(id);
    if (!t)
        return;
    const QString n = name.trimmed();
    t->name = n == baseName(t->cwd) ? QString() : n;   // имя как у папки — пусть дальше идёт за папкой
    emit tabsChanged();
    saveLater();
}

void TermLink::duplicate(int id)
{
    if (Tab *t = tab(id))
        newTab(id, t->cwd);
}

void TermLink::cycle(int step)
{
    const int n = m_tabs.size();
    const int i = indexOf(m_active);
    if (n > 1 && i >= 0)
        setActive(m_tabs[((i + step) % n + n) % n].id);
}

void TermLink::select(int index)
{
    if (m_tabs.isEmpty())
        return;
    if (index < 0 || index >= m_tabs.size())
        index = int(m_tabs.size()) - 1;
    setActive(m_tabs[index].id);
}

void TermLink::setSize(int cols, int rows)
{
    cols = std::max(2, cols);
    rows = std::max(1, rows);
    if (cols == m_cols && rows == m_rows)
        return;
    m_cols = cols;
    m_rows = rows;
    const QByteArray size = QByteArray::number(cols) + ' ' + QByteArray::number(rows);
    for (Tab &t : m_tabs) {
        if (!t.session)
            continue;
        t.session->screen().resize(cols, rows);
        emit t.session->changed();
        if (t.opened)
            send(termproto::Resize, t.id, size);
    }
    openSessions();
}

// ---------- служба ----------
void TermLink::connectService()
{
    if (!m_running || m_connected)
        return;
    if (!m_socket) {
        m_socket = new QLocalSocket(this);
        connect(m_socket, &QLocalSocket::connected, this, &TermLink::onConnected);
        connect(m_socket, &QLocalSocket::disconnected, this, &TermLink::onDisconnected);
        connect(m_socket, &QLocalSocket::readyRead, this, &TermLink::onReadyRead);
        connect(m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
            if (m_connected || !m_running)
                return;
            // службы нет — запустить; ждём её до 5 с
            if (m_attempts == 0)
                launchService();
            if (++m_attempts < 50)
                m_retry.start();
            else
                qCWarning(lcTerm, "служба терминала не отвечает: %s", qPrintable(m_socket->errorString()));
        });
    }
    if (m_socket->state() == QLocalSocket::UnconnectedState)
        m_socket->connectToServer(socketPath());
}

// Служба — отдельной программой пользователя (systemd --user): в группе процессов оболочки её убил бы перезапуск
// оболочки (systemd останавливает службу naomi-shell целиком). Нет systemd-run — дочерней, как есть
void TermLink::launchService()
{
    const QString bin = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../term/naomi-term"));
    qCInfo(lcTerm, "запускаю службу терминала %s", qPrintable(bin));
    QProcess run;
    run.start(QStringLiteral("systemd-run"), {QStringLiteral("--user"), QStringLiteral("--unit=naomi-term"),
                                              QStringLiteral("--collect"), QStringLiteral("--quiet"),
                                              QStringLiteral("--description=NaomiPhone terminal"), bin});
    if (run.waitForFinished(3000) && run.exitStatus() == QProcess::NormalExit && run.exitCode() == 0)
        return;
    qCWarning(lcTerm, "systemd-run: %s — служба дочерней", run.readAllStandardError().trimmed().constData());
    QProcess::startDetached(bin, {});
}

void TermLink::onConnected()
{
    m_connected = true;
    m_attempts = 0;
    m_in.clear();
    QByteArray hello = QByteArray::number(termproto::kVersion);
    for (const Tab &t : m_tabs)
        hello += ' ' + QByteArray::number(t.id);
    send(termproto::Hello, 0, hello);
    for (Tab &t : m_tabs)
        t.opened = false;
    qCInfo(lcTerm, "служба терминала на связи, вкладок %lld", qlonglong(m_tabs.size()));
    openSessions();
}

void TermLink::onDisconnected()
{
    const bool was = m_connected;
    m_connected = false;
    for (Tab &t : m_tabs)
        t.opened = false;
    if (was && m_running) {   // служба упала — запустить заново: текст вкладок — с диска
        qCWarning(lcTerm, "служба терминала отключилась — запускаю снова");
        m_attempts = 0;
        m_retry.start();
    }
}

void TermLink::openSessions()
{
    if (!m_connected || m_cols <= 0)
        return;
    for (Tab &t : m_tabs)
        if (!t.opened && t.session)
            openSession(t);
}

void TermLink::openSession(Tab &t)
{
    t.opened = true;
    t.session->reset(m_cols, m_rows);   // служба пришлёт весь текст вкладки заново
    send(termproto::Open, t.id, QByteArray::number(m_cols) + ' ' + QByteArray::number(m_rows) + " 1 " + t.cwd.toUtf8());
}

void TermLink::send(char type, int id, const QByteArray &payload)
{
    if (!m_connected || !m_socket)
        return;
    char h[termproto::kHeader];
    h[0] = type;
    termproto::putU32(h + 1, uint32_t(id));
    termproto::putU32(h + 5, uint32_t(payload.size()));
    m_socket->write(h, termproto::kHeader);
    m_socket->write(payload);
}

void TermLink::onReadyRead()
{
    m_in += m_socket->readAll();
    processInput();
}

void TermLink::processInput()
{
    qsizetype off = 0;
    while (m_in.size() - off >= qsizetype(termproto::kHeader)) {
        const char *h = m_in.constData() + off;
        const uint32_t len = termproto::getU32(h + 5);
        if (len > termproto::kMaxPayload) {
            qCWarning(lcTerm, "служба прислала кадр в %u байт — связь закрыта", len);
            m_socket->abort();
            m_in.clear();
            return;
        }
        if (m_in.size() - off < qsizetype(termproto::kHeader + len))
            break;
        const QByteArray payload = m_in.mid(off + termproto::kHeader, len);
        const char type = h[0];
        const int id = int(termproto::getU32(h + 1));
        off += termproto::kHeader + len;
        handle(type, id, payload);
        if (off >= kBudget) {   // остальное — следующим заходом цикла событий
            m_more.start();
            break;
        }
    }
    m_in.remove(0, off);
}

void TermLink::handle(char type, int id, const QByteArray &payload)
{
    switch (type) {
    case termproto::Data:
    case termproto::Replay:
        if (Tab *t = tab(id); t && t->session)
            t->session->feed(payload, type == termproto::Replay);
        break;
    case termproto::Place: {
        Tab *t = tab(id);
        if (!t)
            break;
        const int nl = payload.indexOf('\n');
        const QString cwd = QString::fromUtf8(nl < 0 ? payload : payload.left(nl));
        const QString proc = nl < 0 ? QString() : QString::fromUtf8(payload.mid(nl + 1));
        if (cwd == t->cwd && proc == t->proc)
            break;
        const bool moved = cwd != t->cwd;
        t->cwd = cwd;
        t->proc = proc;
        emit tabsChanged();
        if (moved)
            saveLater();
        break;
    }
    case termproto::Exit: {   // оболочка вкладки ушла (exit, Ctrl+D) — вкладка закрывается
        const int i = indexOf(id);
        if (i >= 0)
            removeTab(i);
        break;
    }
    case termproto::Hello:
        if (payload.toUInt() != termproto::kVersion)
            qCWarning(lcTerm, "служба терминала другой версии (%s) — перезапустите её: systemctl --user stop naomi-term",
                      payload.constData());
        break;
    default:
        break;
    }
}

// ---------- на диск ----------
void TermLink::load()
{
    QFile f(statePath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    m_next = std::max(1, o.value("next").toInt(1));
    for (const QJsonValue &v : o.value("tabs").toArray()) {
        const QJsonObject j = v.toObject();
        Tab t;
        t.id = j.value("id").toInt();
        if (t.id <= 0 || indexOf(t.id) >= 0)
            continue;
        t.name = j.value("name").toString();
        t.pinned = j.value("pinned").toBool();
        t.cwd = j.value("cwd").toString(QStringLiteral("~"));
        m_tabs.append(t);
        m_next = std::max(m_next, t.id + 1);
    }
    std::stable_partition(m_tabs.begin(), m_tabs.end(), [](const Tab &t) { return t.pinned; });
    m_active = o.value("active").toInt(-1);
    if (indexOf(m_active) < 0)
        m_active = m_tabs.isEmpty() ? -1 : m_tabs.first().id;
}

void TermLink::saveLater() { m_save.start(); }

void TermLink::save()
{
    m_save.stop();
    QJsonArray tabs;
    for (const Tab &t : m_tabs)
        tabs.append(QJsonObject {{"id", t.id}, {"name", t.name}, {"pinned", t.pinned}, {"cwd", t.cwd}});
    QDir().mkpath(QFileInfo(statePath()).absolutePath());
    QSaveFile f(statePath());
    if (!f.open(QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(QJsonObject {{"next", m_next}, {"active", m_active}, {"tabs", tabs}}).toJson());
    f.commit();
}
