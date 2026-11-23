#!/usr/bin/env python3
"""НаомиФон — «рентген» телефона (tools/xray, docs/XRAY.md): запись на N секунд всего, что делает система.

  sudo python3 xray.py СЕК ПАПКА [idle] [stop=ФАЙЛ]

СЕК — сколько писать; со stop=ФАЙЛ — не дольше СЕК, а кончить, как только появится файл (tools/deskbench).

Пишет в ПАПКУ:
  trace.gz    — трасса ядра (ftrace, часы CLOCK_MONOTONIC): каждое переключение потоков на каждом ядре, пробуждения,
                переезды между ядрами, причины блокировок, прерывания, частоты, видеочип (задания и уровни), кодер,
                тепловые ограничители; со словом idle — ещё и засыпания ядер (cpu_idle, событий много);
  state.json  — два снимка /proc и /sys (до и после): потоки (кто, чей, приоритет, группа планировщика, разрешённые
                ядра, счётчики), ядра, частоты и сон ядер, прерывания, память (PSS по процессам), тепло, ограничители,
                мощность, видеочип, сеть, диск.
Разбор — на Mac (tools/xray): телефон только пишет, чтобы запись сама не мешала замеру.
"""
import glob
import gzip
import json
import os
import shutil
import sys
import time

T = "/sys/kernel/tracing"
EVENTS = [
    "sched/sched_switch", "sched/sched_wakeup", "sched/sched_wakeup_new", "sched/sched_migrate_task",
    "sched/sched_blocked_reason",
    "irq/irq_handler_entry", "irq/irq_handler_exit", "irq/softirq_entry", "irq/softirq_exit",
    "power/cpu_frequency", "power/cpu_frequency_limits",
    "kgsl/kgsl_pwrlevel", "kgsl/kgsl_pwr_set_state", "kgsl/kgsl_buslevel",
    "kgsl/adreno_cmdbatch_queued", "kgsl/adreno_cmdbatch_submitted", "kgsl/adreno_cmdbatch_retired",
    "msm_vidc_events/msm_v4l2_vidc_buffer_event_start", "msm_vidc_events/msm_v4l2_vidc_buffer_event_end",
    "thermal/cdev_update", "thermal/thermal_zone_trip", "dcvsh/dcvsh_freq",
]
BUFFER_KB = 65536   # на ядро; 8 ядер — 512 МБ на время записи (минуты работы стола)


def read(p, default=""):
    try:
        with open(p) as f:
            return f.read()
    except OSError:
        return default


def write(p, v):
    try:
        with open(p, "w") as f:
            f.write(str(v))
        return True
    except OSError:
        return False


def num(p, default=0):
    try:
        return int(read(p).split()[0])
    except (ValueError, IndexError):
        return default


def threads():
    """Все потоки: tid → всё, что о нём знает ядро."""
    stune = {}
    for g in glob.glob("/dev/stune/*/tasks") + ["/dev/stune/tasks"]:
        name = os.path.basename(os.path.dirname(g)) if g != "/dev/stune/tasks" else ""
        for tid in read(g).split():
            stune[tid] = name
    out = {}
    for pdir in glob.glob("/proc/[0-9]*"):
        pid = pdir[6:]
        cmd = read(pdir + "/cmdline").replace("\0", " ").strip()[:160]
        cg = ""
        for line in read(pdir + "/cgroup").splitlines():
            if line.startswith("0::"):
                cg = line[3:]
        for tdir in glob.glob(pdir + "/task/[0-9]*"):
            tid = os.path.basename(tdir)
            st = read(tdir + "/stat")
            r = st.rfind(")")
            if r < 0:
                continue
            f = st[r + 2:].split()
            ss = read(tdir + "/schedstat").split()
            status = read(tdir + "/status")
            allowed = vol = nonvol = ""
            for line in status.splitlines():
                if line.startswith("Cpus_allowed_list:"):
                    allowed = line.split(":")[1].strip()
                elif line.startswith("voluntary_ctxt_switches:"):
                    vol = line.split(":")[1].strip()
                elif line.startswith("nonvoluntary_ctxt_switches:"):
                    nonvol = line.split(":")[1].strip()
            out[tid] = {
                "pid": int(pid), "comm": st[st.find("(") + 1:r], "cmd": cmd, "cg": cg,
                "utime": int(f[11]), "stime": int(f[12]), "prio": int(f[15]), "nice": int(f[16]),
                "cpu": int(f[36]), "rtprio": int(f[37]), "policy": int(f[38]),
                "minflt": int(f[7]), "majflt": int(f[9]),
                "run_ns": int(ss[0]) if len(ss) > 2 else 0, "wait_ns": int(ss[1]) if len(ss) > 2 else 0,
                "slices": int(ss[2]) if len(ss) > 2 else 0,
                "allowed": allowed, "vol": int(vol or 0), "nonvol": int(nonvol or 0),
                "stune": stune.get(tid, ""),
            }
    return out


def cpus():
    out = {"stat": {}, "freq": {}, "idle": {}, "cur": {}}
    for line in read("/proc/stat").splitlines():
        if line.startswith("cpu"):
            p = line.split()
            out["stat"][p[0]] = [int(x) for x in p[1:9]]
        elif line.startswith(("ctxt", "intr ", "processes", "softirq ")):
            p = line.split()
            out["stat"][p[0]] = int(p[1])
    for pol in sorted(glob.glob("/sys/devices/system/cpu/cpufreq/policy*")):
        name = os.path.basename(pol)
        tis = {}
        for line in read(pol + "/stats/time_in_state").splitlines():
            a = line.split()
            if len(a) == 2:
                tis[a[0]] = int(a[1])
        out["freq"][name] = {
            "time_in_state": tis, "trans": num(pol + "/stats/total_trans"),
            "min": num(pol + "/scaling_min_freq"), "max": num(pol + "/scaling_max_freq"),
            "cur": num(pol + "/scaling_cur_freq"), "gov": read(pol + "/scaling_governor").strip(),
            "cpus": read(pol + "/related_cpus").strip(),
        }
    for c in sorted(glob.glob("/sys/devices/system/cpu/cpu[0-9]*")):
        states = {}
        for s in sorted(glob.glob(c + "/cpuidle/state*")):
            states[read(s + "/name").strip()] = [num(s + "/usage"), num(s + "/time")]
        out["idle"][os.path.basename(c)] = states
    return out


def interrupts():
    out = {}
    lines = read("/proc/interrupts").splitlines()
    ncpu = len(lines[0].split()) if lines else 0
    for line in lines[1:]:
        p = line.split()
        if not p or not p[0].endswith(":"):
            continue
        counts = []
        for x in p[1:1 + ncpu]:
            if not x.isdigit():
                break
            counts.append(int(x))
        out[p[0][:-1]] = {"n": counts, "name": " ".join(p[1 + len(counts):])[-48:]}
    soft = {}
    for line in read("/proc/softirqs").splitlines()[1:]:
        p = line.split()
        soft[p[0][:-1]] = [int(x) for x in p[1:]]
    aff = {}
    for d in glob.glob("/proc/irq/[0-9]*"):
        aff[os.path.basename(d)] = read(d + "/smp_affinity_list").strip()
    return {"irq": out, "softirq": soft, "affinity": aff}


def thermal():
    zones, cool = {}, {}
    for z in glob.glob("/sys/class/thermal/thermal_zone*"):
        t = read(z + "/temp").strip()
        if t.lstrip("-").isdigit():
            zones[read(z + "/type").strip()] = int(t)
    for c in glob.glob("/sys/class/thermal/cooling_device*"):
        cool[read(c + "/type").strip()] = [num(c + "/cur_state"), num(c + "/max_state")]
    return {"zones": zones, "cooling": cool}


def power():
    u, b = "/sys/class/power_supply/usb/", "/sys/class/power_supply/battery/"
    return {
        "usb_uv": num(u + "voltage_now"), "usb_ua": num(u + "input_current_now"),
        "bat_uv": num(b + "voltage_now"), "bat_ua": num(b + "current_now"),
        "bat_pct": num(b + "capacity"), "bat_temp": num(b + "temp"),
    }


def gpu():
    g = "/sys/class/kgsl/kgsl-3d0/"
    return {
        "busy": read(g + "gpubusy").split(), "clk": num(g + "gpuclk"),
        "min_mhz": num(g + "min_clock_mhz"), "max_mhz": num(g + "max_clock_mhz"),
        "thermal_level": num(g + "thermal_pwrlevel"), "idle_timer": num(g + "idle_timer"),
        "gov": read(g + "devfreq/governor").strip(),
        "time_in_state": read(g + "devfreq/time_in_state").split(),
        "clock_stats": read(g + "gpu_clock_stats").split(),
    }


def memory():
    mem = {}
    for line in read("/proc/meminfo").splitlines():
        a = line.split()
        mem[a[0][:-1]] = int(a[1])
    vm = {}
    for line in read("/proc/vmstat").splitlines():
        a = line.split()
        if a[0] in ("pgfault", "pgmajfault", "pgpgin", "pgpgout", "pswpin", "pswpout", "allocstall",
                    "compact_stall", "pgscan_kswapd", "pgscan_direct", "pgsteal_kswapd", "pgsteal_direct"):
            vm[a[0]] = int(a[1])
    return {"meminfo": mem, "vmstat": vm, "psi": {k: read("/proc/pressure/" + k).strip()
                                                   for k in ("cpu", "memory", "io")},
            "ion": read("/sys/kernel/ion/total_heaps_kb").strip(), "kgsl": gpu_mem()}


def gpu_mem():
    out = {}
    for d in glob.glob("/sys/class/kgsl/kgsl/proc/[0-9]*"):
        out[os.path.basename(d)] = num(d + "/gpumem_mapped") + num(d + "/gpumem_unmapped")
    return out


def pss():
    """Память по процессам: PSS и чистая своя (Private) — раз за запись, после трассы (чтение дорогое)."""
    out = {}
    for pdir in glob.glob("/proc/[0-9]*"):
        rec = {}
        for line in read(pdir + "/smaps_rollup").splitlines():
            a = line.split()
            if a[0] in ("Pss:", "Pss_Anon:", "Pss_File:", "Private_Dirty:", "Swap:", "Rss:"):
                rec[a[0][:-1]] = int(a[1])
        if rec:
            out[pdir[6:]] = rec
    return out


def net():
    out = {}
    for line in read("/proc/net/dev").splitlines()[2:]:
        name, rest = line.split(":", 1)
        p = rest.split()
        out[name.strip()] = {"rx": int(p[0]), "rx_pk": int(p[1]), "rx_drop": int(p[3]),
                             "tx": int(p[8]), "tx_pk": int(p[9]), "tx_drop": int(p[11])}
    tcp = {}
    lines = read("/proc/net/snmp").splitlines()
    for i in range(0, len(lines) - 1, 2):
        if lines[i].startswith("Tcp:"):
            tcp = dict(zip(lines[i].split()[1:], lines[i + 1].split()[1:]))
    return {"dev": out, "tcp": {k: int(tcp[k]) for k in ("RetransSegs", "OutSegs", "InSegs") if k in tcp}}


def disk():
    out = {}
    for line in read("/proc/diskstats").splitlines():
        p = line.split()
        if p[2].startswith(("sd", "mmcblk", "dm-")) and int(p[3]) + int(p[7]):
            out[p[2]] = {"rd": int(p[3]), "rd_sect": int(p[5]), "wr": int(p[7]), "wr_sect": int(p[9]),
                         "io_ms": int(p[12])}
    return out


def snapshot():
    return {"mono": time.monotonic(), "wall": time.time(), "threads": threads(), "cpus": cpus(),
            "interrupts": interrupts(), "thermal": thermal(), "power": power(), "gpu": gpu(),
            "memory": memory(), "net": net(), "disk": disk()}


def static():
    cap = {os.path.basename(c): num(c + "/cpu_capacity") for c in glob.glob("/sys/devices/system/cpu/cpu[0-9]*")}
    stune = {}
    for g in ["/dev/stune"] + glob.glob("/dev/stune/*/"):
        stune[os.path.basename(g.rstrip("/")) if g != "/dev/stune" else ""] = {
            k: num(os.path.join(g, "schedtune." + k)) for k in ("boost", "prefer_idle", "prefer_high_cap")}
    return {"kernel": read("/proc/version").strip(), "uptime": float(read("/proc/uptime").split()[0]),
            "capacity": cap, "stune": stune, "hz": os.sysconf("SC_CLK_TCK"),
            "sched": {os.path.basename(p): read(p).strip() for p in glob.glob("/proc/sys/kernel/sched_*")}}


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    secs, out = float(sys.argv[1]), sys.argv[2]
    events = EVENTS + (["power/cpu_idle"] if "idle" in sys.argv[3:] else [])
    stop = next((a[5:] for a in sys.argv[3:] if a.startswith("stop=")), None)
    os.makedirs(out, exist_ok=True)
    old_kb = num(T + "/buffer_size_kb", 1408)
    old_clock = read(T + "/trace_clock")
    old_clock = old_clock[old_clock.find("[") + 1:old_clock.find("]")] if "[" in old_clock else "local"
    write(T + "/tracing_on", 0)
    write(T + "/current_tracer", "nop")
    write(T + "/set_event", "")
    write(T + "/trace_clock", "mono")
    write(T + "/buffer_size_kb", BUFFER_KB)
    write(T + "/saved_cmdlines_size", 8192)
    write(T + "/options/record-tgid", 1)
    write(T + "/options/overwrite", 0)   # переполнилось — теряем конец, а не начало: начало привязано к снимку
    missing = [e for e in events if not write(f"{T}/events/{e}/enable", 1)]
    write(T + "/trace", "")
    try:
        a = snapshot()
        write(T + "/tracing_on", 1)
        t0 = time.monotonic()
        write(T + "/trace_marker", "xray start")
        while time.monotonic() - t0 < secs and not (stop and os.path.exists(stop)):
            time.sleep(min(0.2, secs) if stop else secs)
        write(T + "/trace_marker", "xray stop")
        t1 = time.monotonic()
        write(T + "/tracing_on", 0)
        b = snapshot()
        b["pss"] = pss()
        lost = {os.path.basename(os.path.dirname(p)): [l for l in read(p).splitlines()
                                                         if l.startswith(("overrun", "dropped", "entries"))]
                for p in glob.glob(T + "/per_cpu/cpu*/stats")}
        with open(T + "/trace", "rb") as src, gzip.open(os.path.join(out, "trace.gz"), "wb", 1) as dst:
            shutil.copyfileobj(src, dst, 1 << 20)
        with open(os.path.join(out, "state.json"), "w") as f:
            json.dump({"secs": secs, "t0": t0, "t1": t1, "static": static(), "a": a, "b": b,
                       "missing_events": missing, "buffers": lost}, f)
    finally:
        write(T + "/tracing_on", 0)
        write(T + "/set_event", "")
        write(T + "/trace", "")
        write(T + "/buffer_size_kb", old_kb)
        write(T + "/trace_clock", old_clock)
        write(T + "/options/overwrite", 1)
    os.system(f"chmod -R a+rX {out}")


if __name__ == "__main__":
    main()
