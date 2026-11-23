#!/usr/bin/env python3
"""НаомиФон — сборщик нагрузки телефона для прогона стола (tools/deskbench, docs/DESKTOP.md, «Прогон стола»).

  sudo python3 desksampler.py ФАЙЛ.jsonl [стоп-файл] [сек]

Раз в 0,25 с — строка JSON: время (CLOCK_MONOTONIC, те же часы, что у трассы стола), занятость каждого ядра (%),
частоты кластеров (МГц: малые 0–5, большие 6, 7), видеочип (занят %, МГц), мощность (Вт: от кабеля + разряд
батареи), температуры, свободная память. Раз в секунду — ещё процессы (кто сколько ядра взял, %; RSS МБ) и потоки
оболочки naomi-shell (%, на каком ядре был последним — малые или большие), ограничители по нагреву.
Пишет, пока не появится стоп-файл (или сек, по умолчанию 900). От root — чужие процессы и частоты видны все.
"""
import glob
import json
import os
import sys
import time

HZ = os.sysconf("SC_CLK_TCK")
PAGE_MB = os.sysconf("SC_PAGE_SIZE") / 1048576
ZONES = {"skin-therm": "skin", "xo-therm-usr": "xo", "cpu-0-0-usr": "little", "cpu-1-0-usr": "big",
         "gpuss-0-usr": "gpu", "battery": "bat", "sdm-therm": "sdm"}


def read(p, default=""):
    try:
        with open(p) as f:
            return f.read()
    except OSError:
        return default


def num(p, default=0):
    try:
        return int(read(p).split()[0])
    except (IndexError, ValueError):
        return default


def cpus():
    out = {}
    for line in read("/proc/stat").splitlines():
        if line.startswith("cpu") and line[3].isdigit():
            f = line.split()
            v = list(map(int, f[1:8]))
            out[int(f[0][3:])] = (sum(v), v[3] + v[4])
    return out


def stat_of(path):
    """(имя, тики utime+stime, rss страниц, последнее ядро) из /proc/…/stat"""
    s = read(path)
    if not s:
        return None
    r = s.rfind(")")
    name = s[s.find("(") + 1:r]
    f = s[r + 2:].split()
    return name, int(f[11]) + int(f[12]), int(f[21]), int(f[36])


def procs():
    out = {}
    for d in os.listdir("/proc"):
        if d.isdigit():
            st = stat_of(f"/proc/{d}/stat")
            if st:
                out[int(d)] = st
    return out


def threads(pid):
    out = {}
    for d in os.listdir(f"/proc/{pid}/task") if os.path.isdir(f"/proc/{pid}/task") else []:
        st = stat_of(f"/proc/{pid}/task/{d}/stat")
        if st:
            out[int(d)] = st
    return out


def shell_pid():
    for d in os.listdir("/proc"):
        if d.isdigit() and read(f"/proc/{d}/comm").strip() == "naomi-shell":
            return int(d)
    return None


def main():
    out = open(sys.argv[1], "w")
    stop = sys.argv[2] if len(sys.argv) > 2 else None
    limit = float(sys.argv[3]) if len(sys.argv) > 3 else 900
    light = "light" in sys.argv[4:]   # с рентгеном (tools/deskbench, DESKBENCH_XRAY): раз в секунду и без процессов
    step = 1.0 if light else 0.25     # и потоков — их до потока видит трасса ядра, а сборщик не шумит в замере
    zones = {}
    for z in glob.glob("/sys/class/thermal/thermal_zone*"):
        t = read(z + "/type").strip()
        if t in ZONES:
            zones[ZONES[t]] = z + "/temp"
    cool = {}
    for c in glob.glob("/sys/class/thermal/cooling_device*"):
        t = read(c + "/type").strip()
        if t.startswith("thermal-cpufreq") or t.startswith("thermal-devfreq"):
            cool[t.replace("thermal-", "")] = c + "/cur_state"
    t0 = time.monotonic()
    prev_cpu, prev_t = cpus(), t0
    prev_p, prev_th, prev_pt = procs(), {}, t0
    spid = shell_pid()
    if spid:
        prev_th = threads(spid)
    n = 0
    while time.monotonic() - t0 < limit and not (stop and os.path.exists(stop)):
        time.sleep(step - (time.monotonic() - t0) % step)
        now = time.monotonic()
        c = cpus()
        busy = [round(100 * (1 - (c[k][1] - prev_cpu[k][1]) / max(1, c[k][0] - prev_cpu[k][0])))
                if k in prev_cpu else 0 for k in sorted(c)]
        prev_cpu = c
        row = {"t": round(now, 4), "cpu": busy,
               "f": [num(f"/sys/devices/system/cpu/cpu{k}/cpufreq/scaling_cur_freq") // 1000 for k in (0, 6, 7)],
               "gpu": num("/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage"),
               "gmhz": num("/sys/class/kgsl/kgsl-3d0/gpuclk") // 1000000,
               "w": round((num("/sys/class/power_supply/usb/voltage_now") * num("/sys/class/power_supply/usb/input_current_now")
                           + num("/sys/class/power_supply/battery/voltage_now") * num("/sys/class/power_supply/battery/current_now")) / 1e12, 2),
               "temp": {k: num(p) / 1000 for k, p in zones.items()}}
        n += 1
        if light:
            if not spid or not os.path.exists(f"/proc/{spid}"):
                spid = shell_pid()
            if spid:
                row["shell_rss"] = round(int((read(f"/proc/{spid}/statm").split() + ["0", "0"])[1]) * PAGE_MB)
            mi = {l.split(":")[0]: int(l.split()[1]) for l in read("/proc/meminfo").splitlines() if ":" in l}
            row["mem_avail"] = mi.get("MemAvailable", 0) // 1024
            row["cool"] = {k: num(v) for k, v in cool.items() if num(v)}
        elif n % 4 == 0:   # раз в секунду — процессы, потоки оболочки, память, ограничители
            dt = now - prev_pt
            p = procs()
            pc = []
            for pid, (name, ticks, rss, cpu) in p.items():
                if pid in prev_p and prev_p[pid][0] == name:
                    d = 100 * (ticks - prev_p[pid][1]) / HZ / dt
                    if d >= 0.5 or name == "naomi-shell":
                        pc.append((round(d, 1), name, pid, round(rss * PAGE_MB)))
            pc.sort(reverse=True)
            row["procs"] = [list(x) for x in pc[:14]]
            prev_p = p
            if not spid or not os.path.exists(f"/proc/{spid}"):
                spid, prev_th = shell_pid(), {}
            if spid:
                th = threads(spid)
                tc = []
                for tid, (name, ticks, rss, cpu) in th.items():
                    if tid in prev_th:
                        d = 100 * (ticks - prev_th[tid][1]) / HZ / dt
                        if d >= 0.5:
                            tc.append((round(d, 1), name, tid, cpu))
                tc.sort(reverse=True)
                row["threads"] = [list(x) for x in tc[:10]]
                row["shell_rss"] = round(num(f"/proc/{spid}/statm", 0) and int(read(f"/proc/{spid}/statm").split()[1]) * PAGE_MB)
                prev_th = th
            mi = {l.split(":")[0]: int(l.split()[1]) for l in read("/proc/meminfo").splitlines() if ":" in l}
            row["mem_avail"] = mi.get("MemAvailable", 0) // 1024
            row["cool"] = {k: num(v) for k, v in cool.items() if num(v)}
            prev_pt = now
        out.write(json.dumps(row, separators=(",", ":")) + "\n")
        out.flush()


if __name__ == "__main__":
    main()
