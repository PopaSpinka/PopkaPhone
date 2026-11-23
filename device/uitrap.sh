#!/bin/sh
# НаомиФон — «ловушка» (docs/XRAY.md): стек ядра на каждое засыпание потока интерфейса оболочки не по своей воле
# (состояние D: диск, замок, память). Своя запись ftrace (instances/naomi_trap) — рентгену не мешает и почти ничего
# не стоит: событий — сотни в секунду, только одного потока. От root: uitrap.sh start | stop ФАЙЛ
I=/sys/kernel/tracing/instances/naomi_trap
case "${1:-}" in
start)
    P=$(pgrep -x naomi-shell | head -1)
    [ -n "$P" ] || { echo "оболочка не работает" >&2; exit 1; }
    mkdir -p $I; echo 0 > $I/tracing_on; echo > $I/trace
    echo mono > $I/trace_clock; echo 16384 > $I/buffer_size_kb
    echo "prev_pid == $P || next_pid == $P" > $I/events/sched/sched_switch/filter
    echo "stacktrace if prev_pid == $P && prev_state == 2" > $I/events/sched/sched_switch/trigger
    echo 1 > $I/events/sched/sched_switch/enable
    echo "pid == $P" > $I/events/sched/sched_blocked_reason/filter
    echo 1 > $I/events/sched/sched_blocked_reason/enable
    echo 1 > $I/tracing_on ;;
stop)
    echo 0 > $I/tracing_on; cat $I/trace > "$2"; chmod 644 "$2"
    echo '!stacktrace' > $I/events/sched/sched_switch/trigger 2>/dev/null
    echo 0 > $I/events/sched/sched_switch/enable; echo 0 > $I/events/sched/sched_blocked_reason/enable
    rmdir $I 2>/dev/null; true ;;
*)  echo "uitrap.sh start | stop ФАЙЛ" >&2; exit 1 ;;
esac
