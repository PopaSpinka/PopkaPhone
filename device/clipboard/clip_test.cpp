// НаомиФон — проверка общего буфера обмена (ShellCompositor, shell/app/shell_compositor.*; docs/DESKTOP.md,
// «Копировать и вставить») без экрана и без стола пользователя: свой композитор (QML с тем же корнем, что Main.qml,
// платформа offscreen) и пробные программы Wayland. Команды — строками на вход, ответы — строками на выход (JSON):
//   clip_test server СОКЕТ    copy ТЕКСТ — «копировать» оболочки; focus N — фокус клавиатуры окну N (-1 — никому)
//                             выход: surface N — окно появилось; shell ТЕКСТ — буфер оболочки сменился
//   clip_test client          copy ТЕКСТ — «копировать» программы; paste — ответ paste ТЕКСТ
// Ведёт проверку device/clipboard/run.py; сборка — там же.
#include "shell_compositor.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonValue>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QRasterWindow>
#include <QSocketNotifier>
#include <QWaylandSeat>
#include <QWaylandSurface>

#include <cstdio>
#include <functional>
#include <unistd.h>

static void say(const char *what, const QString &text = {})
{
    const QByteArray json = QJsonDocument::fromVariant(QVariantList{QString::fromLatin1(what), text}).toJson(QJsonDocument::Compact);
    std::printf("%s\n", json.constData());
    std::fflush(stdout);
}

// строка входа: ["команда", "текст"]
static void onLine(const std::function<void(const QString &, const QString &)> &run)
{
    auto *n = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, qApp);
    QObject::connect(n, &QSocketNotifier::activated, qApp, [run] {
        static QByteArray buf;
        char chunk[4096];
        const ssize_t got = ::read(STDIN_FILENO, chunk, sizeof chunk);
        if (got <= 0)
            return QGuiApplication::quit();
        buf.append(chunk, got);
        for (qsizetype at; (at = buf.indexOf('\n')) >= 0;) {
            const QVariantList cmd = QJsonDocument::fromJson(buf.left(at)).toVariant().toList();
            buf.remove(0, at + 1);
            run(cmd.value(0).toString(), cmd.value(1).toString());
        }
    });
}

class Surfaces : public QObject
{
    Q_OBJECT
public:
    QList<QWaylandSurface *> list;
    Q_INVOKABLE void add(QWaylandSurface *s)
    {
        list << s;
        say("surface", QString::number(list.size() - 1));
    }
};

static int server(const QString &socket)
{
    qmlRegisterType<ShellCompositor>("Naomi.Shell", 1, 0, "ShellCompositor");
    Surfaces surfaces;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("surfaces", &surfaces);
    engine.rootContext()->setContextProperty("testSocket", socket);
    engine.loadData(R"(
        import QtQuick
        import QtQuick.Window
        import QtWayland.Compositor
        import QtWayland.Compositor.XdgShell
        import Naomi.Shell
        ShellCompositor {
            socketName: testSocket
            WaylandOutput { sizeFollowsWindow: true; window: Window { visible: true; width: 320; height: 240 } }
            XdgShell { onToplevelCreated: (toplevel, xdgSurface) => surfaces.add(xdgSurface.surface) }
        }
    )");
    auto *compositor = qobject_cast<ShellCompositor *>(engine.rootObjects().value(0));
    if (!compositor)
        return 1;
    QObject::connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, [] {
        say("shell", QGuiApplication::clipboard()->text());
    });
    onLine([&](const QString &cmd, const QString &text) {
        if (cmd == QLatin1String("copy"))
            QGuiApplication::clipboard()->setText(text);
        else if (cmd == QLatin1String("focus"))
            compositor->defaultSeat()->setKeyboardFocus(surfaces.list.value(text.toInt()));
    });
    say("ready");
    return QGuiApplication::exec();
}

static int client()
{
    QRasterWindow window;
    window.resize(100, 100);
    window.show();
    onLine([](const QString &cmd, const QString &text) {
        if (cmd == QLatin1String("copy"))
            QGuiApplication::clipboard()->setText(text);
        else if (cmd == QLatin1String("paste"))
            say("paste", QGuiApplication::clipboard()->text());
    });
    say("ready");
    return QGuiApplication::exec();
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.value(1) == QLatin1String("server"))
        return server(args.value(2));
    if (args.value(1) == QLatin1String("client"))
        return client();
    std::fprintf(stderr, "clip_test server СОКЕТ | client\n");
    return 2;
}

#include "clip_test.moc"
