#include "unix_signals.h"

#include <QSocketNotifier>

#include <csignal>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>

thread_local sigjmp_buf *naomiCrashJump = nullptr;

namespace {
int g_pipe[2] = {-1, -1};

// Падение (SIGSEGV и родня): в журнал — стек упавшего потока, адресами и именами (backtrace_symbols_fd не выделяет
// памяти; своя функция без имени — «naomi-shell(+0x…)»: строка — `addr2line -Cfe naomi-shell 0x…` в среде сборки),
// затем сигнал — как обычно: systemd видит падение и перезапускает оболочку. Без этого от падения оставалась одна
// строка systemd «status=11/SEGV». Сам разбор стека тоже может упасть (второе падение в обработчике — сразу конец,
// что успело — в журнале)
void onCrash(int sig)
{
    if (sigjmp_buf *jump = naomiCrashJump) {   // упал охраняемый кусок — вернуться в него
        naomiCrashJump = nullptr;
        siglongjmp(*jump, 1);
    }
    signal(sig, SIG_DFL);
    static const char head[] = "падение, стек потока:\n";
    [[maybe_unused]] ssize_t n = write(STDERR_FILENO, head, sizeof head - 1);
    void *pc[64];
    backtrace_symbols_fd(pc, backtrace(pc, 64), STDERR_FILENO);
    raise(sig);
}

void onSignal(int sig)
{
    const char c = char(sig);
    [[maybe_unused]] ssize_t n = write(g_pipe[1], &c, 1);
}
}

UnixSignals::UnixSignals(QObject *parent)
    : QObject(parent)
{
    if (pipe2(g_pipe, O_CLOEXEC | O_NONBLOCK) != 0)
        return;
    m_notifier = new QSocketNotifier(g_pipe[0], QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] {
        char sig;
        while (read(g_pipe[0], &sig, 1) == 1) {
            if (sig == SIGUSR2)
                emit screenshot();
            else if (sig == SIGUSR1)
                emit pauseRequested();
            else
                emit terminate();
        }
    });
    struct sigaction sa = {};
    sa.sa_handler = onSignal;
    sa.sa_flags = SA_RESTART;
    for (int sig : {SIGTERM, SIGINT, SIGUSR1, SIGUSR2})
        sigaction(sig, &sa, nullptr);
    void *warm[4];
    backtrace(warm, 4);   // первый вызов грузит libgcc — не в обработчике падения
    struct sigaction crash = {};
    crash.sa_handler = onCrash;
    for (int sig : {SIGSEGV, SIGBUS, SIGABRT, SIGFPE, SIGILL})
        sigaction(sig, &crash, nullptr);
}
