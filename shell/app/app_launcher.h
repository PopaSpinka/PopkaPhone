// НаомиФон — программы (Wayland-клиенты оболочки): запуск с их окружением и фокус их окон. В QML —
// naomiApps. Программа <имя> — исполняемый файл <сборка>/apps/<имя>/naomi-<имя> (исходники —
// shell/apps/<имя>). Окно программы оболочка узнаёт по pid клиента Wayland (WaylandClient.processId).
// Программы стола (браузер, docs/WEB.md; «приставка» игр, docs/GAMES.md) запускаются со своим масштабом (launch(имя, 1.5)), их окна Main.qml
// отдаёт столу (deskWindow), а не экрану телефона. Живут они в своей подгруппе процессов (cgroup v2 `desk` внутри
// группы службы оболочки — у службы Delegate=yes): стол никто не смотрит — её замораживает ядро целиком (freezeDesk),
// со всеми процессами Chromium: ни процессора, ни пробуждений, память цела (docs/DESKTOP.md, «Выключатель»).
// У каждой программы стола внутри — ещё и своя подгруппа: desk/<имя>-<номер> (freeze = false — apps/<имя>-<номер>):
// в ней все её процессы, даже ушедшие от родителя (crashpad у Claude, wineserver у Wine), — их находит и добивает
// «закрыть принудительно» (force_quit.h, docs/DESKTOP.md, «Закрыть принудительно»).
#pragma once

#include <QHash>
#include <QObject>
#include <QProcessEnvironment>
#include <QSet>
#include <QStringList>

class QProcess;
class QWaylandCompositor;

class AppLauncher : public QObject
{
    Q_OBJECT
public:
    AppLauncher(const QString &waylandSocket, double density);
    // Закрыть все программы мягко (SIGTERM — успеют сохраниться), через 2 с — SIGKILL; затем allClosed().
    // Выход оболочки ждёт этого с живым циклом событий: программе Qt, чтобы выйти, нужен композитор.
    void closeAll();

    void setCompositor(QWaylandCompositor *compositor) { m_compositor = compositor; }

    // pid или -1. scale — плотность, в которой программа рисует: 0 — экрана телефона (2,75), у программ стола — 1,5
    // args — аргументы программы («Игры»: какая игра и чем её запускать — docs/GAMES.md)
    Q_INVOKABLE qint64 launch(const QString &app, double scale = 0, const QStringList &args = {});
    // то же для чужой программы (игра Windows — её запускалка, docs/WINDOWS.md): app — только метка программы стола.
    // freeze = false — программа стола, которую ядро не замораживает, когда стол не смотрят (Claude, docs/CLAUDE.md:
    // его задачи идут и без монитора); подъём на большие ядра — как у всех
    Q_INVOKABLE qint64 run(const QString &app, const QString &program, double scale, const QStringList &args,
                           bool freeze = true);
    Q_INVOKABLE bool onDesk(qint64 pid) const { return !deskApp(pid).isEmpty(); }
    // Чьё окно стола: имя программы (web, game, win, claude) или пусто — не стола. Сам запущенный процесс — по pid; его
    // потомки (у Wine окно создаёт не тот процесс, что запускали, и родство рвётся: wineserver уходит в фон) — по
    // метке NAOMI_DESK_APP в окружении, её получают все процессы программы стола. У Chromium (Electron) метки в
    // /proc/pid/environ нет — он пишет туда имя процесса, — его окно узнаётся только по pid
    Q_INVOKABLE QString deskApp(qint64 pid) const;
    Q_INVOKABLE void stop(qint64 pid);   // закрыть мягко (SIGTERM — успеет сохраниться); потом — exited
    // пауза программе, которая её понимает (naomi-game: окно свернули): SIGUSR1 — встать, SIGUSR2 — дальше
    Q_INVOKABLE void pause(qint64 pid, bool on);
    // Ни одно окно не в фокусе клавиатуры Wayland: программы Qt считают себя неактивными (их окно
    // активно, пока у него этот фокус) — поле теряет фокус, курсор не мигает, ничего не рисуется.
    // Вернуть фокус — ShellSurfaceItem.takeFocus() в QML.
    Q_INVOKABLE void releaseFocus();
    // Заморозить или оттаять программы стола ядром (cgroup.freeze). Нет своей подгруппы (служба без Delegate=yes,
    // оболочка не службой) — ничего (в журнал — один раз, при запуске).
    void freezeDesk(bool on);
    // Подъём программ стола на большие ядра (schedtune.boost группы /dev/stune/desk — её подключает shell-prepare):
    // стол смотрят (приставка, окно Mac) — да, нет — нет. Группы нет — ничего.
    void setDeskBoost(bool on);
    // Своя подгруппа программы стола, в которой процесс pid (её потомок, где бы ни был), или подгруппа самой
    // запущенной программы с этим pid, даже если она из неё ушла (Electron уходит в свою группу systemd); owner — pid
    // запущенной. Не наша программа стола — пусто и -1
    QByteArray groupOf(qint64 pid, qint64 *owner = nullptr) const;
    // все процессы подгруппы cgroup v2 со вложенными
    static QList<qint64> groupPids(const QByteArray &dir);
    // путь группы процесса pid в /sys/fs/cgroup (строка «0::/…» в /proc/pid/cgroup); процесса нет — пусто
    static QByteArray cgroupOf(qint64 pid);

signals:
    void exited(qint64 pid, const QString &app, int exitCode);
    // окно программы стола (Main.qml → Desktop.qml): QWaylandXdgToplevel и QWaylandXdgSurface
    void deskWindow(QObject *toplevel, QObject *xdgSurface);
    void allClosed();

private:
    void finishClosing();
    void releaseGroup(qint64 pid);
    static bool writeFile(const QByteArray &path, const char *value);

    QWaylandCompositor *m_compositor = nullptr;
    QString m_appsDir;
    QProcessEnvironment m_env;
    QHash<qint64, QProcess *> m_running;
    QHash<qint64, QString> m_desk;   // pid программ стола → имя
    QByteArray m_deskGroup;   // каталог подгруппы программ стола в /sys/fs/cgroup; пусто — заморозки нет
    QByteArray m_appsGroup;   // подгруппа программ стола, которых не морозят (apps); пусто — нет
    QHash<qint64, QByteArray> m_groups;   // pid запущенной программы стола → её своя подгруппа
    int m_groupSeq = 0;
    bool m_deskFrozen = false;
    QByteArray m_boostGroup;  // группа schedtune программ стола (/dev/stune/desk); пусто — подъёма нет
    bool m_deskBoost = false;
    // подъём, пока стол смотрят: как top-app у Android — 10 (замер 17 ноября 2026: 30 и «большие ядра первыми» не лучше)
    static constexpr const char *kDeskBoost = "10";
    enum class Closing { No, Waiting, Done } m_closing = Closing::No;
};
