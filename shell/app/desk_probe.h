// НаомиФон — зонд стола для замеров (docs/DESKTOP.md, «Прогон стола»; tools/deskbench): по SSH, на сессионной шине
// пользователя телефона, org.naomi.DeskTest /probe —
//   Scene(файл)              что сейчас в сцене стола: каждый видимый элемент — тип QML, id (или id предка), файл и
//                            строка, место и размер в точках стола, видимая доля (opacity с предками), строки и флаги
//                            состояния (text, glyph, open, maximized…) — JSON в файл; ответ — сколько элементов.
//                            Сценарий находит по нему окна, их кнопки и края сам, а не по заученным координатам
//   Trace(файл, сцена)       трасса кадров стола (как NAOMI_DESK_TRACE) — включить на ходу, без перезапуска
//                            оболочки; пустой файл — выключить; сцена — со строками d и p (что сменилось к кадру)
//   Sample(файл)             выборки стека потока интерфейса раз в 1 мс (сигнал SIGPROF и backtrace), пустой файл —
//                            остановить и записать: строка на выборку — «мкс;внешняя функция;…;внутренняя» (имена —
//                            dladdr, Qt — полностью, свои — по ближайшему видимому символу; код QML — «[jit]»).
//                            Разбор (tools/deskbench) относит выборки к долгим событиям: что внутри 27 мс.
//                            Выход оболочки посреди выборок их останавливает, файл не пишется
//   Gc()                     сборка мусора JS и malloc_trim: память оболочки до и после (утечка или мусор, что не
//                            отдан системе) — JSON: rss, rssAfterGc, rssAfterTrim (КБ), объектов QObject в сцене стола
//   Web(команда)             команда «Браузеру» стола — строка JSON, как у стола (web_link.h: navigate, new-tab, close…;
//                            пустая — ничего не слать); ответ — вкладки сейчас: JSON {connected, active, tabs[id,
//                            url, title, loading]}. Сценарию — открыть нужную страницу и дождаться загрузки
// Тот же сервис, что у проверочной мыши (org.naomi.DeskTest /input, shell/stream/desk_test_input.h).
//
// ProbedApp — приложение оболочки: пока трасса кадров стола включена, каждое событие потока интерфейса дольше 4 мс —
// строка «s,мкс начала,мкс длилось,мкс на ядре,глубина,мкс в очереди за ядром (у вложенных −1),ядро,тип события,
// получатель» (получатель — тип QML, id, файл и строка): кто держит поток интерфейса, когда такт зрителя ждёт в
// очереди, и чем — работой, ожиданием ядра или сном (tools/deskbench, «долгие события»)
#pragma once

#include <QGuiApplication>
#include <QObject>
#include <QString>

class DesktopScreen;
class WebLink;

class DeskProbe : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.DeskTest")
public:
    explicit DeskProbe(DesktopScreen *desk, WebLink *web, QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE int Scene(const QString &path);
    Q_SCRIPTABLE bool Trace(const QString &path, bool scene);
    Q_SCRIPTABLE int Sample(const QString &path);
    Q_SCRIPTABLE QString Gc();
    Q_SCRIPTABLE QString Web(const QString &command);

private:
    DesktopScreen *m_desk;
    WebLink *m_web;
};

class ProbedApp : public QGuiApplication
{
public:
    ProbedApp(int &argc, char **argv) : QGuiApplication(argc, argv) {}
    bool notify(QObject *receiver, QEvent *event) override;

private:
    int m_depth = 0;
};
