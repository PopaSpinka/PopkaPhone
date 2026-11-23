// НаомиФон — «закрыть принудительно» (FORCE QUIT) меню N стола, как у НаомиОС (docs/DESKTOP.md, «Закрыть
// принудительно»). В QML — naomiForce.
//   Программа списка (Desktop.qml, forceItems) — {key, name} и чем её найти: pid — процесс окна стола (и xdg — его
// окно, QWaylandXdgSurface: пинг — его клиенту), pkg — приложение Android, term — служба «Терминала».
//   Пока «FORCE QUIT» открыт (watch), раз в 3 с: пинг окон (xdg_wm_base) — не ответило за 2,5 с — «не отвечает»,
// ответило — снова отвечает; и память — сумма PSS всех процессов программы (в своём потоке: smaps_rollup у Chromium
// читается миллисекундами, стол бы дёргался); у Android — от naomid (Android.Memory, процессы контейнера — от root).
//   Закрыть (kill): программе стола — SIGTERM всем процессам её подгруппы ядра (AppLauncher: в ней все её процессы,
// даже ушедшие от родителя), и её группы systemd, если она ушла в свою (Electron); через 2 с — SIGKILL всем, кто жив,
// и так раз в 250 мс, пока не останется ни одного. Не наша программа — процесс, его потомки и его группа процессов,
// если она не наша. «Терминал» — так же вся группа его службы (naomi-term). Приложению Android — naomid
// Android.ForceStop (`am force-stop` от root в контейнере; не вышло — SIGKILL его процессам).
#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QVariantMap>

class AppLauncher;
class TermLink;
class QWaylandClient;
class QWaylandCompositor;
class QWaylandXdgShell;
class QWaylandXdgSurface;

class ForceQuit : public QObject
{
    Q_OBJECT
    // что известно о программах списка: {key: {kb, hung}}; kb — память, КБ (0 — ещё не мерили); hung — окно не
    // ответило на пинг
    Q_PROPERTY(QVariantMap info READ info NOTIFY infoChanged)
public:
    ForceQuit(AppLauncher *apps, TermLink *term);
    ~ForceQuit() override;
    void setCompositor(QWaylandCompositor *compositor) { m_compositor = compositor; }
    QVariantMap info() const { return m_info; }

    // pid запущенной программы стола, которой принадлежит процесс pid (окно Wine — у её запускалки); не наша — pid
    Q_INVOKABLE qint64 owner(qint64 pid) const;
    // «FORCE QUIT» открыт: items — программы списка; пусто — закрыт (пинга и замеров нет)
    Q_INVOKABLE void watch(const QVariantList &items);
    // закрыть программы насовсем (items — как у watch)
    Q_INVOKABLE void kill(const QVariantList &items);

signals:
    void infoChanged();

private:
    // чем найти процессы программы: подгруппы ядра (своя, группа systemd, служба), а нет их — pid и потомки
    struct Target {
        QString key;
        QString name;
        qint64 pid = -1;
        QList<QByteArray> groups;
        QString pkg;
    };
    struct Ping {
        uint serial = 0;
        qint64 sent = 0;
        bool waiting = false;
        bool hung = false;
    };
    struct Kill {
        Target target;
        qint64 deadline = 0;   // мс часов m_clock: до него — SIGTERM, после — SIGKILL
        qint64 started = 0;
        int rounds = 0;        // раз уже послали SIGKILL
    };

    Target resolve(const QVariantMap &item) const;
    static QList<qint64> processes(const Target &t);
    static qint64 pss(qint64 pid);
    void round();
    void checkPings();
    void onPong(uint serial);
    void measure();
    void measureAndroid(const QStringList &pkgs);
    void tick();
    void update();
    void logOpen();
    QWaylandXdgShell *shell();

    AppLauncher *m_apps;
    TermLink *m_term;
    QWaylandCompositor *m_compositor = nullptr;
    QPointer<QWaylandXdgShell> m_shell;
    QElapsedTimer m_clock;
    QList<Target> m_targets;                       // открыт — что в списке
    QHash<QString, QPointer<QWaylandXdgSurface>> m_surfaces;   // ключ → окно (его клиента пингуем)
    QHash<QString, Ping> m_pings;
    QHash<uint, QString> m_serials;                // пинг → ключ
    QHash<QString, qint64> m_kb;                   // замер памяти
    QVariantMap m_info;
    QList<Kill> m_kills;
    QTimer m_round;                                // раз в 3 с, пока открыт
    QTimer m_check;                                // 2,5 с после пинга
    QTimer m_killTimer;                            // 250 мс, пока есть кого добивать
    QThreadPool m_pool;                            // замер памяти — не в потоке интерфейса
    bool m_measuring = false;
    bool m_logged = false;                         // строка «FORCE QUIT: …» за этот заход — в журнале
};
