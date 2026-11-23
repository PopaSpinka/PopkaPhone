// НаомиФон — сигналы Unix для оболочки, программ и naomid. SIGTERM/SIGINT — чистый выход (оболочку иначе
// systemd видит с кодом 1, считает это падением и включает безопасный режим; программа успевает
// сохраниться). SIGUSR2 — снимок экрана (tools/naomi shot), его ловит только оболочка; у программ стола SIGUSR1 и
// SIGUSR2 — «пауза» и «дальше» (окно свернули и развернули: naomiApps.pause, как у игр) — pauseRequested и screenshot.
// Обработчик только пишет номер сигнала в self-pipe; разбор — в потоке интерфейса.
// Падение (SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL) — стек упавшего потока в журнал, дальше — как обычно.
#pragma once

#include <QObject>

#include <csetjmp>

// Кусок кода, который вправе упасть (разбор стека в выборках зонда — desk_probe.cpp): пока указатель не пуст,
// обработчик падения не роняет программу, а возвращается в сохранённое место (siglongjmp). У каждого потока — свой
extern thread_local sigjmp_buf *naomiCrashJump;

class QSocketNotifier;

class UnixSignals : public QObject
{
    Q_OBJECT
public:
    explicit UnixSignals(QObject *parent = nullptr);

signals:
    void terminate();
    void screenshot();
    void pauseRequested();   // SIGUSR1 (без обработчика он бы убил программу)

private:
    QSocketNotifier *m_notifier = nullptr;
};
