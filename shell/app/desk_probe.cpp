#include "desk_probe.h"

#include "unix_signals.h"

#include "desktop_screen.h"
#include "web_link.h"

#include <QDBusConnection>
#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QPointer>
#include <QQmlContext>
#include <QQuickItem>
#include <QCoreApplication>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QThread>
#include <private/qqmlcontextdata_p.h>
#include <private/qqmldata_p.h>   // файл и строка QML элемента

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <ctime>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <malloc.h>
#include <map>
#include <pthread.h>
#include <sched.h>
#include <thread>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcProbe, "naomi.desktop")

namespace {

// флаги состояния, которые стоит знать сценарию (окно развёрнуто, меню открыто…); строки — все короткие свои
const char *const kFlags[] = {"open", "active", "maximized", "minimized", "shown", "parked", "closing", "checked",
                              "hovered", "pressed", "containsMouse", "busy", "on"};

// получатель события для трассы: тип (без QQuick и _QML), id или «<id» предка-элемента, файл:строка QML
QByteArray describe(QObject *o)
{
    QByteArray cls = o->metaObject()->className();
    if (const int q = cls.indexOf("_QML"); q > 0)
        cls.truncate(q);
    if (cls.startsWith("QQuick"))
        cls.remove(0, 6);
    QByteArray name = cls;
    if (!o->objectName().isEmpty())
        name += "'" + o->objectName().toUtf8() + "'";
    QObject *p = o;
    for (int hops = 0; p && hops < 12; ++hops) {
        const QQmlContext *c = qmlContext(p);
        const QString id = c ? c->nameForObject(p) : QString();
        if (!id.isEmpty()) {
            name += (p == o ? "#" : "<") + id.toUtf8();
            break;
        }
        auto *item = qobject_cast<QQuickItem *>(p);
        p = item ? item->parentItem() : p->parent();
    }
    if (const QQmlData *d = QQmlData::get(o); d && d->outerContext) {
        name += "@" + d->outerContext->url().fileName().toUtf8() + ":" + QByteArray::number(d->lineNumber);
    } else {   // не QML (гнёзда, таймеры служб): чей — классы родителей, до трёх (D-Bus, браузер, композитор)
        int hops = 0;
        for (QObject *q = o->parent(); q && hops < 3; q = q->parent(), ++hops)
            name += "<" + QByteArray(q->metaObject()->className());
    }
    return name;
}

qint64 cpuUs()
{
    timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return qint64(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
}

// сколько поток ждал ядро (готов, но в очереди планировщика), мкс: /proc/thread-self/schedstat, второе число (нс)
qint64 runqueueUs()
{
    static const int fd = ::open("/proc/thread-self/schedstat", O_RDONLY | O_CLOEXEC);
    char buf[96];
    const ssize_t n = fd >= 0 ? ::pread(fd, buf, sizeof buf - 1, 0) : -1;
    if (n <= 0)
        return 0;
    buf[n] = 0;
    unsigned long long run = 0, wait = 0;
    std::sscanf(buf, "%llu %llu", &run, &wait);
    return qint64(wait / 1000);
}

qint64 nowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// ---------- выборки стека потока интерфейса (Sample) ----------
// Обработчик сигнала пишет в кольцо: время и адреса (backtrace — первый вызов прогрет заранее, дальше он не выделяет
// память). Поток выборок шлёт сигнал раз в 1 мс и копит выборки; на остановке — имена по адресам (dladdr) и файл.
struct StackSample
{
    qint64 us;
    int n;
    void *pc[48];
};
constexpr unsigned kRing = 8192;
StackSample g_ring[kRing];
std::atomic<unsigned> g_written{0};
pthread_t g_uiThread;

// Поток выборок и что он накопил. Останавливается и на выходе оболочки (aboutToQuit, а при exit() — деструктором):
// прогон tools/deskbench держит выборки весь прогон, и оболочку, перезапущенную посреди него, разбор статических
// объектов ронял — поток писал в уже освобождённый вектор выборок (SIGSEGV в memcpy), а его std::thread, ещё
// joinable, звал std::terminate (20 ноября 2026, docs/XRAY.md, «Грабли»). Поток стоит раньше, чем разобраны поля
struct Sampler
{
    std::atomic<bool> on{false};
    std::vector<StackSample> samples;
    QString path;
    std::thread thread;

    bool stop()   // false — и так не шли
    {
        if (!thread.joinable())
            return false;
        on = false;
        thread.join();
        signal(SIGPROF, SIG_IGN);
        return true;
    }
    ~Sampler() { stop(); }
} g_sampler;

void onProf(int, siginfo_t *, void *)
{
    const unsigned i = g_written.load(std::memory_order_relaxed);
    StackSample &s = g_ring[i % kRing];
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    s.us = qint64(ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
    // Разбор стека — под охраной (unix_signals.h): сигнал застаёт поток и внутри драйвера видеочипа (библиотеки Android
    // через libhybris), а там таблицы раскрутки верны не в каждой точке — libgcc читал мусор и ронял оболочку (20 ноября
    // 2026: два падения за два прогона с выборками при развёрнутой игре). Упал — выборка пустая, оболочка цела
    sigjmp_buf jump;
    if (sigsetjmp(jump, 1) == 0) {
        naomiCrashJump = &jump;
        s.n = backtrace(s.pc, 48);
    } else {
        s.n = 0;
    }
    naomiCrashJump = nullptr;
    g_written.store(i + 1, std::memory_order_release);
}

QByteArray symbolOf(void *pc)
{
    Dl_info info;
    if (!dladdr(pc, &info) || !info.dli_fname)
        return QByteArrayLiteral("[jit]");
    QByteArray name;
    if (info.dli_sname) {
        int status = 0;
        char *d = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
        name = status == 0 && d ? QByteArray(d) : QByteArray(info.dli_sname);
        free(d);
        if (const int paren = name.indexOf('('); paren > 0)   // без списка аргументов — короче, сливаются перегрузки
            name.truncate(paren);
    } else {
        name = "?";
    }
    QByteArray lib = info.dli_fname;
    lib = lib.mid(lib.lastIndexOf('/') + 1);
    if (lib.startsWith("libQt6"))
        lib = lib.mid(6, lib.indexOf('.') - 6);
    return name.replace(';', ',') + " [" + lib + "]";
}

void samplerLoop()
{
    unsigned read = g_written.load(std::memory_order_acquire);
    while (g_sampler.on.load()) {
        pthread_kill(g_uiThread, SIGPROF);
        std::this_thread::sleep_for(std::chrono::microseconds(1000));
        const unsigned w = g_written.load(std::memory_order_acquire);
        if (w - read > kRing)
            read = w - kRing;   // не успели — старое потеряно
        for (; read != w; ++read)
            g_sampler.samples.push_back(g_ring[read % kRing]);
    }
}

void walk(QQuickItem *item, double opacity, QJsonArray &out)
{
    if (!item->isVisible())
        return;
    opacity *= item->opacity();
    if (opacity < 0.01)
        return;
    if (item->width() > 0 && item->height() > 0) {
        QJsonObject o;
        QByteArray cls = item->metaObject()->className();
        if (const int q = cls.indexOf("_QML"); q > 0)
            cls.truncate(q);
        if (cls.startsWith("QQuick"))
            cls.remove(0, 6);
        o[QStringLiteral("t")] = QString::fromLatin1(cls);
        for (QQuickItem *p = item; p; p = p->parentItem()) {
            const QQmlContext *c = qmlContext(p);
            const QString id = c ? c->nameForObject(p) : QString();
            if (!id.isEmpty()) {
                o[QLatin1String(p == item ? "id" : "in")] = id;
                break;
            }
        }
        if (const QQmlData *d = QQmlData::get(item); d && d->outerContext)
            o[QStringLiteral("f")] = d->outerContext->url().fileName() + QLatin1Char(':') + QString::number(d->lineNumber);
        const QRectF r = item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
        o[QStringLiteral("r")] = QJsonArray{qRound(r.x() * 10) / 10.0, qRound(r.y() * 10) / 10.0,
                                            qRound(r.width() * 10) / 10.0, qRound(r.height() * 10) / 10.0};
        if (opacity < 0.999)
            o[QStringLiteral("o")] = qRound(opacity * 100) / 100.0;
        if (item->z() != 0)
            o[QStringLiteral("z")] = item->z();
        const QMetaObject *mo = item->metaObject();
        QJsonObject props;
        for (int i = QQuickItem::staticMetaObject.propertyCount(); i < mo->propertyCount(); ++i) {
            const QMetaProperty p = mo->property(i);
            if (p.typeId() == QMetaType::QString) {
                const QString v = p.read(item).toString();
                if (!v.isEmpty() && v.size() <= 80)
                    props[QString::fromLatin1(p.name())] = v;
            } else if (p.typeId() == QMetaType::Bool) {
                for (const char *f : kFlags)
                    if (qstrcmp(p.name(), f) == 0 && p.read(item).toBool())
                        props[QString::fromLatin1(p.name())] = true;
            }
        }
        if (!props.isEmpty())
            o[QStringLiteral("p")] = props;
        out.append(o);
    }
    for (QQuickItem *child : item->childItems())
        walk(child, opacity, out);
}

} // namespace

DeskProbe::DeskProbe(DesktopScreen *desk, WebLink *web, QObject *parent)
    : QObject(parent)
    , m_desk(desk)
    , m_web(web)
{
    if (!QDBusConnection::sessionBus().registerObject(QStringLiteral("/probe"), this,
                                                      QDBusConnection::ExportScriptableSlots))
        qCWarning(lcProbe, "зонд стола: не занять /probe на сессионной шине");
    // выход посреди выборок (перезапуск во время прогона) — остановить сразу, до разбора сцены: файл не нужен
    connect(qApp, &QCoreApplication::aboutToQuit, this, [] {
        if (g_sampler.stop())
            qCInfo(lcProbe, "выборки стека: прерваны выходом оболочки, файл не записан");
    });
}

int DeskProbe::Scene(const QString &path)
{
    QQuickWindow *w = m_desk->window();
    if (!w || !w->contentItem())
        return -1;
    QJsonArray items;
    walk(w->contentItem(), 1.0, items);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return -1;
    const QSize s = m_desk->logicalSize();
    f.write(QJsonDocument(QJsonObject{{QStringLiteral("desk"), QJsonArray{s.width(), s.height()}},
                                      {QStringLiteral("items"), items}}).toJson(QJsonDocument::Compact));
    return int(items.size());
}

int DeskProbe::Sample(const QString &path)
{
    if (!path.isEmpty()) {
        if (g_sampler.thread.joinable())
            return -1;
        void *warm[4];
        backtrace(warm, 4);   // первый вызов грузит libgcc — не в обработчике сигнала
        struct sigaction sa = {};
        sa.sa_sigaction = onProf;
        sa.sa_flags = SA_SIGINFO | SA_RESTART;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGPROF, &sa, nullptr);
        g_uiThread = pthread_self();
        g_sampler.samples.clear();
        g_sampler.samples.reserve(200000);
        g_sampler.path = path;
        g_sampler.on = true;
        g_sampler.thread = std::thread(samplerLoop);
        qCInfo(lcProbe, "выборки стека потока интерфейса: %s", qPrintable(path));
        return 0;
    }
    if (!g_sampler.stop())
        return -1;
    std::map<void *, QByteArray> names;
    QFile f(g_sampler.path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return -1;
    for (const StackSample &s : g_sampler.samples) {
        QByteArray line = QByteArray::number(s.us);
        for (int k = s.n - 1; k >= 2; --k) {   // снаружи внутрь; 0–1 — сам обработчик и трамплин сигнала
            auto it = names.find(s.pc[k]);
            if (it == names.end())
                it = names.emplace(s.pc[k], symbolOf(s.pc[k])).first;
            line += ';' + it->second;
        }
        f.write(line + '\n');
    }
    const int n = int(g_sampler.samples.size());
    g_sampler.samples = {};
    qCInfo(lcProbe, "выборки стека: %d в %s", n, qPrintable(g_sampler.path));
    return n;
}

namespace {
long rssKb()
{
    QFile f(QStringLiteral("/proc/self/status"));
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    for (const QByteArray &line : f.readAll().split('\n'))
        if (line.startsWith("VmRSS:"))
            return line.mid(6).trimmed().split(' ').first().toLong();
    return -1;
}
int countObjects(QObject *o)
{
    int n = 1;
    for (QObject *c : o->children())
        n += countObjects(c);
    return n;
}
} // namespace

QString DeskProbe::Web(const QString &command)
{
    if (!command.isEmpty()) {
        const QJsonObject message = QJsonDocument::fromJson(command.toUtf8()).object();
        if (!message.isEmpty())
            m_web->send(message.toVariantMap());
    }
    QJsonArray tabs;
    for (const QVariant &v : m_web->tabs()) {
        const QVariantMap t = v.toMap();
        tabs.append(QJsonObject{{"id", t.value("id").toInt()}, {"url", t.value("url").toString()},
                                {"title", t.value("title").toString()}, {"loading", t.value("loading").toBool()}});
    }
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"connected", m_web->connected()}, {"active", m_web->active()},
                                                       {"tabs", tabs}}).toJson(QJsonDocument::Compact));
}

QString DeskProbe::Gc()
{
    const long before = rssKb();
    QQmlEngine *engine = m_desk->window() ? qmlEngine(m_desk->window()->contentItem()->childItems().value(0)) : nullptr;
    if (engine)
        engine->collectGarbage();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const long afterGc = rssKb();
    malloc_trim(0);
    const long afterTrim = rssKb();
    const int objects = m_desk->window() ? countObjects(m_desk->window()->contentItem()) : 0;
    return QStringLiteral("{\"rss\":%1,\"rssAfterGc\":%2,\"rssAfterTrim\":%3,\"objects\":%4}")
        .arg(before).arg(afterGc).arg(afterTrim).arg(objects);
}

bool DeskProbe::Trace(const QString &path, bool scene)
{
    const bool ok = DesktopScreen::setTrace(path, scene);
    qCInfo(lcProbe, "трасса кадров стола: %s", path.isEmpty() ? "выключена" : ok ? qPrintable(path) : "не открыть файл");
    return ok;
}

bool ProbedApp::notify(QObject *receiver, QEvent *event)
{
    FILE *f = DesktopScreen::trace();
    if (!f || QThread::currentThread() != thread())
        return QGuiApplication::notify(receiver, event);
    const int type = event->type();
    // получатель может удалиться внутри (DeferredDelete, закрытое окно): описываем только живого — страж QPointer
    const QPointer<QObject> alive(receiver);
    const qint64 t0 = nowUs(), c0 = cpuUs(), q0 = m_depth ? 0 : runqueueUs();
    ++m_depth;
    const bool r = QGuiApplication::notify(receiver, event);
    --m_depth;
    const qint64 dt = nowUs() - t0;
    if (dt >= 4000 && DesktopScreen::trace() == f) {
        // время: на ядре (cpu), в очереди за ядром (wait — только у внешних), остальное — спал (диск, блокировки,
        // видеочип); ядро, на котором кончилось (0–5 — малые, 6–7 — большие)
        const char *key = QMetaEnum::fromType<QEvent::Type>().valueToKey(type);
        fprintf(f, "s,%lld,%lld,%lld,%d,%lld,%d,%s,%s\n", (long long)t0, (long long)dt, (long long)(cpuUs() - c0),
                m_depth + 1, (long long)(m_depth ? -1 : runqueueUs() - q0), sched_getcpu(),
                key ? key : QByteArray::number(type).constData(),
                alive && type != QEvent::DeferredDelete ? describe(alive.data()).constData() : "(удалён)");
        fflush(f);
    }
    return r;
}
