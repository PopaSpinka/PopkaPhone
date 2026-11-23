#include "force_quit.h"

#include "app_launcher.h"
#include "term_link.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QWaylandClient>
#include <QWaylandCompositor>
#include <QWaylandSurface>
#include <QWaylandXdgShell>

#include <algorithm>
#include <csignal>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcForce, "naomi.force")

namespace {
constexpr int kRoundMs = 3000;   // FORCE_PING_MS НаомиОС: пинг и замер так часто, пока открыт
constexpr int kPingMs = 2500;    // XDG_PING_TIMEOUT_MS: не ответило столько — «не отвечает»
constexpr int kTermMs = 2000;    // FORCE_KILL_MS: после SIGTERM столько ждём — потом SIGKILL
constexpr int kKillTickMs = 250;
constexpr int kStuckMs = 15000;  // столько после SIGKILL кто-то жив — в журнал, кто и в каком состоянии
constexpr int kGiveUpMs = 60000; // и столько — перестать
const QString kPhoneService = QStringLiteral("org.naomi.Phone");
const QString kAndroidPath = QStringLiteral("/org/naomi/Phone/Android");
const QString kAndroidIface = QStringLiteral("org.naomi.Phone.Android");

// поле /proc/pid/stat после «(имя)»: 0 — состояние (R, S, D, Z…), 1 — ppid, 2 — pgrp, 3 — session
QByteArray statField(qint64 pid, int field)
{
    QFile file(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray stat = file.readAll();
    const int close = stat.lastIndexOf(')');
    if (close < 0)
        return {};
    const QList<QByteArray> parts = stat.mid(close + 2).split(' ');
    return field < parts.size() ? parts[field] : QByteArray();
}

bool alive(qint64 pid)
{
    const QByteArray state = statField(pid, 0);
    return !state.isEmpty() && state != "Z" && state != "X";
}

QByteArray comm(qint64 pid)
{
    QFile file(QStringLiteral("/proc/%1/comm").arg(pid));
    return file.open(QIODevice::ReadOnly) ? file.readAll().trimmed() : QByteArray("?");
}

QString memoryLabel(qint64 kb)
{
    const double mb = kb / 1024.0;
    if (mb < 1000)
        return QStringLiteral("%1 МБ").arg(qMax(1.0, mb), 0, 'f', 0);
    return QStringLiteral("%1 ГБ").arg(mb / 1024, 0, 'f', 1).replace('.', ',');
}
}

ForceQuit::ForceQuit(AppLauncher *apps, TermLink *term)
    : m_apps(apps)
    , m_term(term)
{
    m_clock.start();
    m_pool.setMaxThreadCount(1);
    m_round.setInterval(kRoundMs);
    connect(&m_round, &QTimer::timeout, this, &ForceQuit::round);
    m_check.setSingleShot(true);
    connect(&m_check, &QTimer::timeout, this, &ForceQuit::checkPings);
    m_killTimer.setInterval(kKillTickMs);
    connect(&m_killTimer, &QTimer::timeout, this, &ForceQuit::tick);
}

ForceQuit::~ForceQuit()
{
    m_pool.waitForDone();
}

QWaylandXdgShell *ForceQuit::shell()
{
    if (!m_shell && m_compositor) {
        m_shell = QWaylandXdgShell::findIn(m_compositor);
        if (m_shell)
            connect(m_shell, &QWaylandXdgShell::pong, this, &ForceQuit::onPong);
    }
    return m_shell;
}

qint64 ForceQuit::owner(qint64 pid) const
{
    qint64 who = -1;
    m_apps->groupOf(pid, &who);
    return who > 0 ? who : pid;
}

ForceQuit::Target ForceQuit::resolve(const QVariantMap &item) const
{
    Target t;
    t.key = item.value(QStringLiteral("key")).toString();
    t.name = item.value(QStringLiteral("name")).toString();
    if (item.contains(QStringLiteral("pkg"))) {
        t.pkg = item.value(QStringLiteral("pkg")).toString();
        return t;
    }
    if (item.value(QStringLiteral("term")).toBool()) {   // «Терминал»: вся группа его службы (systemd --user)
        t.pid = m_term->servicePid();
        const QByteArray group = t.pid > 0 ? AppLauncher::cgroupOf(t.pid) : QByteArray();
        if (group.endsWith("/naomi-term.service"))
            t.groups << group;
        return t;
    }
    const qint64 pid = item.value(QStringLiteral("pid")).toLongLong();
    if (pid <= 1 || pid == getpid())
        return t;
    qint64 who = -1;
    const QByteArray own = m_apps->groupOf(pid, &who);
    t.pid = who > 0 ? who : pid;
    if (!own.isEmpty())
        t.groups << own;
    // ушла в свою группу systemd (Electron: app-…-<pid>.scope) — и её
    const QByteArray at = AppLauncher::cgroupOf(t.pid);
    if (at.endsWith('-' + QByteArray::number(t.pid) + ".scope") && !t.groups.contains(at))
        t.groups << at;
    return t;
}

QList<qint64> ForceQuit::processes(const Target &t)
{
    QSet<qint64> out;
    for (const QByteArray &group : t.groups)
        for (qint64 pid : AppLauncher::groupPids(group))
            out.insert(pid);
    if (t.pid > 1 && alive(t.pid))
        out.insert(t.pid);
    if (t.groups.isEmpty() && t.pid > 1) {
        // не наша программа: потомки процесса и его группа процессов, если она не наша (как killpg у НаомиОС)
        const qint64 pgrp = statField(t.pid, 2).toLongLong();
        const bool ownGroup = pgrp <= 1 || pgrp == getpgrp() || statField(t.pid, 3).toLongLong() == getsid(0);
        QHash<qint64, qint64> parent;
        for (const QString &name : QDir(QStringLiteral("/proc")).entryList(QDir::Dirs)) {
            const qint64 pid = name.toLongLong();
            if (pid <= 1)
                continue;
            parent.insert(pid, statField(pid, 1).toLongLong());
            if (!ownGroup && statField(pid, 2).toLongLong() == pgrp)
                out.insert(pid);
        }
        for (bool grew = true; grew;) {
            grew = false;
            for (auto it = parent.constBegin(); it != parent.constEnd(); ++it)
                if (out.contains(it.value()) && !out.contains(it.key())) {
                    out.insert(it.key());
                    grew = true;
                }
        }
    }
    out.remove(getpid());
    out.remove(0);
    out.remove(1);
    return out.values();
}

qint64 ForceQuit::pss(qint64 pid)
{
    QFile rollup(QStringLiteral("/proc/%1/smaps_rollup").arg(pid));
    if (rollup.open(QIODevice::ReadOnly))
        for (const QByteArray &line : rollup.readAll().split('\n'))
            if (line.startsWith("Pss:"))
                return line.mid(4).trimmed().split(' ').value(0).toLongLong();
    QFile statm(QStringLiteral("/proc/%1/statm").arg(pid));
    if (!statm.open(QIODevice::ReadOnly))
        return 0;
    return statm.readAll().split(' ').value(1).toLongLong() * (sysconf(_SC_PAGESIZE) / 1024);
}

void ForceQuit::watch(const QVariantList &items)
{
    if (items.isEmpty()) {
        if (m_targets.isEmpty() && m_info.isEmpty())
            return;
        m_round.stop();
        m_check.stop();
        m_targets.clear();
        m_surfaces.clear();
        m_pings.clear();
        m_serials.clear();
        m_kb.clear();
        m_logged = false;
        update();
        return;
    }
    QList<Target> targets;
    QHash<QString, QPointer<QWaylandXdgSurface>> surfaces;
    bool fresh = false;
    for (const QVariant &v : items) {
        const QVariantMap item = v.toMap();
        targets << resolve(item);
        if (auto *xdg = qobject_cast<QWaylandXdgSurface *>(item.value(QStringLiteral("xdg")).value<QObject *>()))
            surfaces.insert(targets.last().key, xdg);
        fresh = fresh || !m_kb.contains(targets.last().key);
    }
    m_targets = targets;
    m_surfaces = surfaces;
    for (auto it = m_pings.begin(); it != m_pings.end();)
        it = m_surfaces.contains(it.key()) ? std::next(it) : m_pings.erase(it);
    if (!m_round.isActive()) {
        m_logged = false;
        m_round.start();
        round();
    } else if (fresh) {
        measure();
    }
    update();
}

void ForceQuit::round()
{
    if (QWaylandXdgShell *xdg = shell()) {
        const qint64 now = m_clock.elapsed();
        for (auto it = m_surfaces.constBegin(); it != m_surfaces.constEnd(); ++it) {
            // клиент с живым xdg_surface держит и xdg_wm_base (уничтожить его раньше окон протокол не даёт) — пинговать можно
            QWaylandXdgSurface *surface = it.value();
            QWaylandClient *client = surface && surface->surface() ? surface->surface()->client() : nullptr;
            Ping &p = m_pings[it.key()];
            if (!client || p.waiting)   // прошлый ещё без ответа — ждём его (checkPings)
                continue;
            p.serial = xdg->ping(client);
            p.sent = now;
            p.waiting = true;
            m_serials.insert(p.serial, it.key());
        }
        m_check.start(kPingMs);
    }
    measure();
}

void ForceQuit::checkPings()
{
    const qint64 now = m_clock.elapsed();
    bool changed = false;
    for (auto it = m_pings.begin(); it != m_pings.end(); ++it) {
        Ping &p = it.value();
        if (!p.waiting || p.hung || now - p.sent < kPingMs - 50)
            continue;
        p.hung = true;
        changed = true;
        const auto t = std::find_if(m_targets.cbegin(), m_targets.cend(), [&](const Target &x) { return x.key == it.key(); });
        qCInfo(lcForce, "Не отвечает: %s (pid %lld)", t != m_targets.cend() ? qPrintable(t->name) : "?",
               t != m_targets.cend() ? t->pid : -1);
    }
    if (changed)
        update();
}

void ForceQuit::onPong(uint serial)
{
    const QString key = m_serials.take(serial);
    if (key.isEmpty() || !m_pings.contains(key))
        return;
    Ping &p = m_pings[key];
    if (p.serial != serial)
        return;
    p.waiting = false;
    if (p.hung) {
        p.hung = false;
        qCInfo(lcForce, "Снова отвечает: %s", qPrintable(key));
        update();
    }
}

void ForceQuit::measure()
{
    QStringList pkgs;
    QList<Target> desk;
    for (const Target &t : std::as_const(m_targets))
        if (!t.pkg.isEmpty())
            pkgs << t.pkg;
        else if (t.pid > 1 || !t.groups.isEmpty())
            desk << t;
    if (!pkgs.isEmpty())
        measureAndroid(pkgs);
    if (m_measuring)
        return;
    m_measuring = true;
    m_pool.start([this, desk] {
        QHash<QString, qint64> kb;
        for (const Target &t : desk) {
            qint64 sum = 0;
            for (qint64 pid : processes(t))
                sum += pss(pid);
            kb.insert(t.key, sum);
        }
        QMetaObject::invokeMethod(this, [this, kb] {
            m_measuring = false;
            if (m_targets.isEmpty())
                return;
            for (auto it = kb.constBegin(); it != kb.constEnd(); ++it)
                m_kb.insert(it.key(), it.value());
            update();
            logOpen();
        }, Qt::QueuedConnection);
    });
}

// Для разбора, что съело память: при открытии, как только память известна у всех строк, — строкой в журнал
void ForceQuit::logOpen()
{
    if (m_logged || std::any_of(m_targets.cbegin(), m_targets.cend(), [this](const Target &t) { return !m_kb.contains(t.key); }))
        return;
    m_logged = true;
    QStringList line;
    for (const Target &t : std::as_const(m_targets))
        line << t.name + ' ' + (m_kb.value(t.key) > 0 ? memoryLabel(m_kb.value(t.key)) : QStringLiteral("?"));
    qCInfo(lcForce, "FORCE QUIT: %s", qPrintable(line.join(QStringLiteral(", "))));
}

void ForceQuit::measureAndroid(const QStringList &pkgs)
{
    QDBusMessage m = QDBusMessage::createMethodCall(kPhoneService, kAndroidPath, kAndroidIface, QStringLiteral("Memory"));
    m << pkgs;
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(m, 5000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        if (m_targets.isEmpty())
            return;
        const QVariantMap kb = reply.isError() ? QVariantMap() : reply.value();   // naomid молчит — памяти нет
        for (const Target &t : std::as_const(m_targets))
            if (!t.pkg.isEmpty())
                m_kb.insert(t.key, kb.value(t.pkg).toLongLong());
        update();
        logOpen();
    });
}

void ForceQuit::update()
{
    QVariantMap info;
    for (const Target &t : std::as_const(m_targets))
        info.insert(t.key, QVariantMap{{QStringLiteral("kb"), m_kb.value(t.key)},
                                       {QStringLiteral("hung"), m_pings.value(t.key).hung}});
    if (info == m_info)
        return;
    m_info = info;
    emit infoChanged();
}

void ForceQuit::kill(const QVariantList &items)
{
    const qint64 now = m_clock.elapsed();
    for (const QVariant &v : items) {
        const Target t = resolve(v.toMap());
        if (!t.pkg.isEmpty()) {
            qCInfo(lcForce, "Закрываем принудительно: %s (Android, %s)", qPrintable(t.name), qPrintable(t.pkg));
            QDBusMessage m = QDBusMessage::createMethodCall(kPhoneService, kAndroidPath, kAndroidIface,
                                                            QStringLiteral("ForceStop"));
            m << t.pkg;
            QDBusConnection::systemBus().asyncCall(m);
            continue;
        }
        if (std::any_of(m_kills.cbegin(), m_kills.cend(), [&](const Kill &k) { return k.target.key == t.key; }))
            continue;
        const QList<qint64> pids = processes(t);
        if (pids.isEmpty()) {
            qCInfo(lcForce, "Закрыть принудительно: %s — процессов уже нет", qPrintable(t.name));
            continue;
        }
        QStringList who;
        for (qint64 pid : pids) {
            ::kill(pid_t(pid), SIGTERM);
            if (who.size() < 8)
                who << QString::fromUtf8(comm(pid));
        }
        qCInfo(lcForce, "Закрываем принудительно: %s (pid %lld, процессов %lld: %s%s) — SIGTERM", qPrintable(t.name),
               t.pid, qlonglong(pids.size()), qPrintable(who.join(QStringLiteral(", "))), pids.size() > 8 ? ", …" : "");
        m_kills << Kill{t, now + kTermMs, now, 0};
    }
    if (!m_kills.isEmpty() && !m_killTimer.isActive())
        m_killTimer.start();
}

void ForceQuit::tick()
{
    const qint64 now = m_clock.elapsed();
    for (auto it = m_kills.begin(); it != m_kills.end();) {
        Kill &k = *it;
        const QList<qint64> pids = processes(k.target);
        if (pids.isEmpty()) {
            qCInfo(lcForce, "%s: закрыта за %lld мс%s", qPrintable(k.target.name), now - k.started,
                   k.rounds ? qPrintable(QStringLiteral(" (SIGKILL — %1 раз)").arg(k.rounds)) : " (сама, по SIGTERM)");
            it = m_kills.erase(it);
            continue;
        }
        if (now >= k.deadline) {
            if (k.rounds == 0)
                qCInfo(lcForce, "%s: за 2 с не закрылась — SIGKILL, процессов %lld", qPrintable(k.target.name),
                       qlonglong(pids.size()));
            for (qint64 pid : pids)
                ::kill(pid_t(pid), SIGKILL);
            ++k.rounds;
            if (now - k.started >= kStuckMs && (now - k.started) % 5000 < kKillTickMs) {
                // SIGKILL не берёт только того, кто в ядре (D — ждёт диск или драйвер): умрёт, как только оттуда выйдет
                QStringList states;
                for (qint64 pid : pids)
                    states << QStringLiteral("%1 %2 (%3)").arg(QString::fromUtf8(comm(pid))).arg(pid)
                                  .arg(QString::fromUtf8(statField(pid, 0)));
                qCWarning(lcForce, "%s: после SIGKILL живы %s", qPrintable(k.target.name), qPrintable(states.join(", ")));
            }
            if (now - k.started >= kGiveUpMs) {
                qCWarning(lcForce, "%s: не умирает минуту — бросаю", qPrintable(k.target.name));
                it = m_kills.erase(it);
                continue;
            }
        }
        ++it;
    }
    if (m_kills.isEmpty())
        m_killTimer.stop();
}
