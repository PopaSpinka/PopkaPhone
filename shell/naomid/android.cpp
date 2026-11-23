#include "android.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <limits>

Q_LOGGING_CATEGORY(lcAndroid, "naomid.android")

namespace {
const QByteArray kDone = "@@naomid-done";
}

Android::Android(QObject *parent)
    : BusObject("/org/naomi/Phone/Android", "org.naomi.Phone.Android", parent)
{
    // оболочке на команду — 3 с: первая после запуска ждёт и сам запуск (1,5 с); дольше — Android стоит (заморожен)
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(3000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { drop("не ответила за 3 с"); });
    set("Tasks", QStringLiteral("[]"));
}

// Строки `cmd activity stack list`: «RootTask id=… bounds=…», под ней « configuration={… mWindowingMode=freeform …}» и
// «  taskId=126: com.android.settings/….Settings bounds=[1037,51][2347,1103] userId=0 visible=true …»
static QJsonArray parseTasks(const QStringList &lines)
{
    static const QRegularExpression task(
        "^\\s*taskId=(\\d+): ([^/\\s]+)/\\S* bounds=\\[(-?\\d+),(-?\\d+)\\]\\[(-?\\d+),(-?\\d+)\\].*visible=(\\w+)");
    static const QRegularExpression mode("mWindowingMode=(\\w+)");
    QJsonArray out;
    QString lastMode;
    for (const QString &line : lines) {
        if (const auto m = mode.match(line); m.hasMatch())
            lastMode = m.captured(1);
        const auto m = task.match(line);
        if (!m.hasMatch() || m.captured(2) == QLatin1String("unknown"))
            continue;
        const int l = m.captured(3).toInt(), t = m.captured(4).toInt();
        out.append(QJsonObject{{"id", m.captured(1).toInt()}, {"pkg", m.captured(2)}, {"x", l}, {"y", t},
                               {"w", m.captured(5).toInt() - l}, {"h", m.captured(6).toInt() - t},
                               {"mode", lastMode}, {"visible", m.captured(7) == QLatin1String("true")}});
    }
    return out;
}

void Android::RefreshTasks()
{
    if (m_reading) {
        m_readAgain = true;
        return;
    }
    m_reading = true;
    run("cmd activity stack list", [this](const QStringList &lines) {
        m_reading = false;
        if (!lines.isEmpty())
            set("Tasks", QString::fromUtf8(QJsonDocument(parseTasks(lines)).toJson(QJsonDocument::Compact)));
        if (m_readAgain) {
            m_readAgain = false;
            RefreshTasks();
        }
    });
}

void Android::MoveTask(const QString &pkg, int x, int y, int width, int height, int toX, int toY)
{
    place(pkg, x, y, width, height, toX, toY, 0, 0);
}

void Android::ResizeTask(const QString &pkg, int x, int y, int width, int height, int toX, int toY, int toWidth,
                         int toHeight)
{
    if (toWidth <= 0 || toHeight <= 0) {
        qCWarning(lcAndroid, "размер задачи: не то — %dx%d", toWidth, toHeight);
        return;
    }
    place(pkg, x, y, width, height, toX, toY, toWidth, toHeight);
}

void Android::Sleep(bool on)
{
    // KEYCODE_SLEEP 223, KEYCODE_WAKEUP 224 (не POWER: тот переключает — уснувший сам Android проснулся бы)
    run(on ? "input keyevent 223" : "input keyevent 224", [on](const QStringList &out) {
        const QString error = out.filter(QStringLiteral("Error")).join(' ');
        if (!error.isEmpty())
            qCWarning(lcAndroid, "Android: %s — %s", on ? "не уснул" : "не проснулся", qPrintable(error));
        else
            qCInfo(lcAndroid, "Android: %s", on ? "спит" : "проснулся");
    });
}

QHash<QString, QList<qint64>> Android::appPids(const QStringList &pkgs)
{
    QHash<QString, QList<qint64>> out;
    for (const QString &entry : QDir(QStringLiteral("/proc")).entryList(QDir::Dirs)) {
        const qint64 pid = entry.toLongLong();
        if (pid <= 1)
            continue;
        QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(pid));
        if (!cmdline.open(QIODevice::ReadOnly))
            continue;
        const QString name = QString::fromUtf8(cmdline.read(256).split('\0').value(0));
        const QString pkg = name.section(':', 0, 0);
        if (name.isEmpty() || !pkgs.contains(pkg))
            continue;
        QFile cgroup(QStringLiteral("/proc/%1/cgroup").arg(pid));   // только процессы контейнера
        if (cgroup.open(QIODevice::ReadOnly) && cgroup.readAll().contains("lxc.payload"))
            out[pkg] << pid;
    }
    return out;
}

QVariantMap Android::Memory(const QStringList &pkgs) const
{
    QVariantMap out;
    const QHash<QString, QList<qint64>> pids = appPids(pkgs);
    for (const QString &pkg : pkgs) {
        qint64 kb = 0;
        for (const qint64 pid : pids.value(pkg)) {
            QFile rollup(QStringLiteral("/proc/%1/smaps_rollup").arg(pid));
            if (rollup.open(QIODevice::ReadOnly))
                for (const QByteArray &line : rollup.readAll().split('\n'))
                    if (line.startsWith("Pss:"))
                        kb += line.mid(4).trimmed().split(' ').value(0).toLongLong();
        }
        out.insert(pkg, kb);
    }
    return out;
}

void Android::ForceStop(const QString &pkg)
{
    static const QRegularExpression name("^[A-Za-z0-9_]+(\\.[A-Za-z0-9_]+)+$");
    if (!name.match(pkg).hasMatch()) {
        qCWarning(lcAndroid, "Android: закрыть принудительно — не пакет: %s", qPrintable(pkg));
        return;
    }
    qCInfo(lcAndroid, "Android: закрыть принудительно %s (am force-stop)", qPrintable(pkg));
    run(QStringLiteral("am force-stop ") + pkg, [](const QStringList &) {});
    QTimer::singleShot(3000, this, [pkg] {
        const QList<qint64> left = appPids({pkg}).value(pkg);
        if (left.isEmpty()) {
            qCInfo(lcAndroid, "Android: %s закрыто", qPrintable(pkg));
            return;
        }
        qCWarning(lcAndroid, "Android: %s после am force-stop живо процессов %lld — SIGKILL", qPrintable(pkg),
                  qlonglong(left.size()));
        for (const qint64 pid : left)
            ::kill(pid_t(pid), SIGKILL);
    });
}

void Android::place(const QString &pkg, int x, int y, int width, int height, int toX, int toY, int toWidth,
                    int toHeight)
{
    static const QRegularExpression name("^[A-Za-z0-9_]+(\\.[A-Za-z0-9_]+)+$");
    constexpr int kFar = 100000;
    if (!name.match(pkg).hasMatch() || width <= 0 || height <= 0 || std::abs(x) > kFar || std::abs(y) > kFar
        || std::abs(toX) > kFar || std::abs(toY) > kFar || toWidth > kFar || toHeight > kFar) {
        qCWarning(lcAndroid, "перенос задачи: не то — %s %d,%d %dx%d → %d,%d", qPrintable(pkg), x, y, width, height,
                  toX, toY);
        return;
    }
    // строка задачи: «  taskId=79: com.kiloo.subwaysurf/… bounds=[856,27][1474,1125] userId=0 …»; перед ней — настройки
    // её корневой задачи с режимом окна (mWindowingMode=freeform | fullscreen)
    run("cmd activity stack list", [=](const QStringList &lines) {
        static const QRegularExpression task(
            "^\\s*taskId=(\\d+): ([^/\\s]+)/\\S* bounds=\\[(-?\\d+),(-?\\d+)\\]\\[(-?\\d+),(-?\\d+)\\]");
        static const QRegularExpression mode("mWindowingMode=(\\w+)");
        int id = -1, best = std::numeric_limits<int>::max(), w = 0, h = 0;
        QString windowing, lastMode;
        for (const QString &line : lines) {
            if (const auto m = mode.match(line); m.hasMatch())
                lastMode = m.captured(1);
            const auto m = task.match(line);
            if (!m.hasMatch() || m.captured(2) != pkg)
                continue;
            const int l = m.captured(3).toInt(), t = m.captured(4).toInt();
            const int r = m.captured(5).toInt(), b = m.captured(6).toInt();
            const int d = std::abs(l - x) + std::abs(t - y) + std::abs(r - l - width) + std::abs(b - t - height);
            if (d < best) {
                best = d;
                id = m.captured(1).toInt();
                w = r - l;
                h = b - t;
                windowing = lastMode;
            }
        }
        if (id < 0) {
            qCWarning(lcAndroid, "перенос задачи: %s", lines.isEmpty() ? "оболочка Android не ответила"
                                                                      : qPrintable("у Android нет задачи " + pkg));
            return;
        }
        // Задача «во весь экран» Android (её так развернула кнопка Android): размер ей не задать — её режим окна меняет
        // только F11 (у Waydroid — как «развернуть / вернуть» Android), а он идёт окну в фокусе у Android. Только если
        // в фокусе это приложение (по нему щёлкнули): иначе F11 развернул бы чужое окно (было: «Настройки» вместо Subway)
        if (toWidth > 0 && windowing == QLatin1String("fullscreen")) {
            run("dumpsys window | grep -m1 mCurrentFocus", [=](const QStringList &focus) {
                if (focus.isEmpty() || !focus.first().contains(pkg + '/')) {
                    qCInfo(lcAndroid, "задача %d (%s) — во весь экран, но в фокусе не она: оставляю", id, qPrintable(pkg));
                    return;
                }
                run("input keyevent 141", [=](const QStringList &) {   // KEYCODE_F11
                    qCInfo(lcAndroid, "задача %d (%s): из «во весь экран» — окном (F11)", id, qPrintable(pkg));
                    RefreshTasks();
                });
            });
            return;
        }
        if (toWidth > 0) {
            w = toWidth;
            h = toHeight;
        }
        // левее и выше экрана Android не ставит: cmd activity task resize отвечает «bad left arg» и не делает ничего
        // (было: окно, отпущенное частью за левым краем, у Android оставалось на месте, а в журнале — «перенесено»)
        const int tx = std::max(0, toX), ty = std::max(0, toY);
        run(QStringLiteral("cmd activity task resize %1 %2 %3 %4 %5").arg(id).arg(tx).arg(ty).arg(tx + w).arg(ty + h),
            [=](const QStringList &out) {
                const QString error = out.filter(QStringLiteral("Error")).join(' ');
                if (!error.isEmpty())
                    qCWarning(lcAndroid, "задача %d (%s): Android не перенёс — %s", id, qPrintable(pkg), qPrintable(error));
                else
                    qCInfo(lcAndroid, "задача %d (%s) %d,%d %dx%d → %d,%d %dx%d", id, qPrintable(pkg), x, y, width,
                           height, tx, ty, w, h);
                RefreshTasks();   // где задача встала на самом деле (Android мог ограничить)
            });
    });
}

void Android::run(const QString &script, std::function<void(const QStringList &)> done)
{
    m_jobs.append({script, std::move(done)});
    next();
}

void Android::next()
{
    if (m_busy || m_jobs.isEmpty())
        return;
    if (!m_sh) {
        m_sh = new QProcess(this);
        m_sh->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_sh, &QProcess::readyRead, this, &Android::read);
        connect(m_sh, &QProcess::finished, this, [this] { drop("закрылась"); });
        m_sh->start("/usr/bin/waydroid", {"shell"});
        qCInfo(lcAndroid, "оболочка Android: открываю (waydroid shell)");
    }
    m_busy = true;
    m_out.clear();
    m_sh->write(m_jobs.first().script.toUtf8() + "\necho " + kDone + "\n");
    m_timeout.start();
}

void Android::read()
{
    m_buf += m_sh->readAll();
    for (qsizetype nl; (nl = m_buf.indexOf('\n')) >= 0;) {
        const QByteArray line = m_buf.left(nl).trimmed();
        m_buf.remove(0, nl + 1);
        if (!m_busy)
            continue;
        if (line != kDone) {
            m_out << QString::fromUtf8(line);
            continue;
        }
        m_timeout.stop();
        m_busy = false;
        const Job job = m_jobs.takeFirst();
        job.done(m_out);
        next();
    }
}

void Android::drop(const char *why)
{
    if (!m_sh)
        return;
    qCInfo(lcAndroid, "оболочка Android %s — закрываю", why);
    QProcess *sh = m_sh;
    m_sh = nullptr;
    sh->disconnect(this);
    sh->kill();
    sh->deleteLater();
    m_timeout.stop();
    m_buf.clear();
    if (m_busy) {
        m_busy = false;
        const Job job = m_jobs.takeFirst();
        job.done({});
    }
    next();
}
