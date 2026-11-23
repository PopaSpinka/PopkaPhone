#!/usr/bin/env python3
"""НаомиФон — «чёрный ящик» (docs/XRAY.md, «Чёрный ящик»; служба naomi-blackbox, ставит install-shell.sh): раз в
минуту — строка JSON о состоянии телефона в /userdata/naomi/blackbox/ГГГГММДД.jsonl. Журнал systemd у нас в памяти
и перезагрузку не переживает, а то, что портит телефон за часы и дни, — рост памяти, потоков и открытых файлов,
перезапуски служб, перегрев — видно только на длинной записи. Разбор — `tools/naomi blackbox [часов]`.

Что в строке: время и время работы (меньше прошлого — была перезагрузка); память (доступно, свободно, кэш, ядро, ION);
процессор — занятость за минуту по группам ядер; мощность от кабеля и ток батареи, заряд; датчики тепла и
ограничители, что включены; режим стола (держит ли naomid «стол на полную»); сеть (байты usb0 и wlan0); место на
/userdata; и по главным процессам — pid (сменился — перезапуск), память (RSS), потоков, открытых файлов, тики
процессора: оболочка (у неё ещё — сколько сокетов, dma-buf и потоков захвата звука), naomid, браузер (все его
процессы вместе), PulseAudio, sshd; сколько процессов у контейнеров Android.
Хранится 14 дней; одна запись — ~30 мс процессора.
"""
import glob
import json
import os
import time

DIR = "/userdata/naomi/blackbox"
KEEP_DAYS = 14
STEP = 60
ZONES = {"skin-therm": "skin", "sdm-therm": "sdm", "battery": "bat", "cpu-1-max-step": "big", "gpuss-max-step": "gpu",
         "xo-therm-usr": "xo"}
HZ = os.sysconf("SC_CLK_TCK")
PAGE_KB = os.sysconf("SC_PAGE_SIZE") // 1024


def read(p, default=""):
    try:
        with open(p) as f:
            return f.read()
    except OSError:
        return default


def num(p, default=0):
    try:
        return int(read(p).split()[0])
    except (ValueError, IndexError):
        return default


def procs():
    """pid → (имя, тики, RSS КБ, потоков)"""
    out = {}
    for d in glob.glob("/proc/[0-9]*"):
        st = read(d + "/stat")
        r = st.rfind(")")
        if r < 0:
            continue
        f = st[r + 2:].split()
        out[int(d[6:])] = (st[st.find("(") + 1:r], int(f[11]) + int(f[12]), int(f[21]) * PAGE_KB, int(f[17]))
    return out


def fds(pid):
    kinds = {"all": 0, "socket": 0, "dmabuf": 0}
    try:
        names = os.listdir(f"/proc/{pid}/fd")
    except OSError:
        return kinds
    kinds["all"] = len(names)
    for n in names:
        try:
            t = os.readlink(f"/proc/{pid}/fd/{n}")
        except OSError:
            continue
        if t.startswith("socket:"):
            kinds["socket"] += 1
        elif "dmabuf" in t:
            kinds["dmabuf"] += 1
    return kinds


def threads_named(pid, name):
    n = 0
    for t in glob.glob(f"/proc/{pid}/task/[0-9]*/comm"):
        if read(t).strip() == name:
            n += 1
    return n


def cpu_groups():
    out = {}
    for line in read("/proc/stat").splitlines():
        if line.startswith("cpu") and line[3:4].isdigit():
            p = line.split()
            v = [int(x) for x in p[1:9]]
            out[int(p[0][3:])] = (sum(v) - v[3] - v[4], sum(v))
    return out


def sample(prev):
    now = time.time()
    row = {"t": round(now), "up": round(float(read("/proc/uptime", "0 0").split()[0]))}
    mem = {}
    for line in read("/proc/meminfo").splitlines():
        a = line.split()
        mem[a[0][:-1]] = int(a[1])
    row["mem"] = {"avail": mem.get("MemAvailable", 0) // 1024, "free": mem.get("MemFree", 0) // 1024,
                  "cache": mem.get("Cached", 0) // 1024, "slab": mem.get("Slab", 0) // 1024,
                  "ion": num("/sys/kernel/ion/total_heaps_kb") // 1024}
    cg = cpu_groups()
    if prev.get("cpu"):
        def busy(cpus):
            b = sum(cg[c][0] - prev["cpu"][c][0] for c in cpus if c in cg and c in prev["cpu"])
            t = sum(cg[c][1] - prev["cpu"][c][1] for c in cpus if c in cg and c in prev["cpu"])
            return round(100 * b / t, 1) if t else 0
        row["cpu"] = {"little": busy(range(6)), "big": busy((6, 7))}
    prev["cpu"] = cg
    u, b = "/sys/class/power_supply/usb/", "/sys/class/power_supply/battery/"
    row["power"] = {"usb_w": round(num(u + "voltage_now") * num(u + "input_current_now") / 1e12, 2),
                    "bat_ma": num(b + "current_now") // 1000, "pct": num(b + "capacity"),
                    "status": read(b + "status").strip()[:4]}
    temps = {}
    for z in glob.glob("/sys/class/thermal/thermal_zone*"):
        t = read(z + "/type").strip()
        if t in ZONES:
            temps[ZONES[t]] = round(num(z + "/temp") / 1000, 1)
    row["temp"] = temps
    cool = {}
    for c in glob.glob("/sys/class/thermal/cooling_device*"):
        t = read(c + "/type").strip()
        if t.startswith(("thermal-cpufreq", "thermal-devfreq")) and num(c + "/cur_state"):
            cool[t.replace("thermal-", "")] = num(c + "/cur_state")
    if cool:
        row["cool"] = cool
    row["desk"] = num("/dev/stune/shell/schedtune.boost") > 0   # «стол на полную» держат (naomid, Perf)
    net = {}
    for line in read("/proc/net/dev").splitlines()[2:]:
        name, rest = line.split(":", 1)
        if name.strip() in ("usb0", "wlan0"):
            p = rest.split()
            net[name.strip()] = [int(p[0]) >> 20, int(p[8]) >> 20]   # принято, отдано, МБ
    row["net"] = net
    try:
        s = os.statvfs("/userdata")
        row["disk_free_mb"] = s.f_bavail * s.f_frsize >> 20
    except OSError:
        pass
    ps = procs()
    row["n"] = {"procs": len(ps), "threads": sum(p[3] for p in ps.values())}
    watch = {}
    for key, match in (("shell", lambda n: n == "naomi-shell"), ("naomid", lambda n: n == "naomid"),
                       ("web", lambda n: n in ("naomi-web", "QtWebEngineProc")), ("pulse", lambda n: n == "pulseaudio"),
                       ("sshd", lambda n: n == "sshd")):
        pids = sorted(pid for pid, p in ps.items() if match(p[0]))
        if not pids:
            continue
        ticks = sum(ps[p][1] for p in pids)
        rec = {"pid": pids[0], "n": len(pids), "rss": sum(ps[p][2] for p in pids) // 1024,
               "thr": sum(ps[p][3] for p in pids)}
        old = prev.get("ticks", {}).get(key)
        if old and old[0] == pids[0] and prev.get("t"):
            # у sshd процессов несколько и они меняются: ушёл один — тиков стало меньше, не «минус»
            rec["cpu"] = max(0.0, round(100 * (ticks - old[1]) / HZ / (now - prev["t"]), 1))
        prev.setdefault("ticks", {})[key] = (pids[0], ticks)
        if key in ("shell", "web"):
            k = fds(pids[0])
            rec["fd"], rec["sock"], rec["dmabuf"] = k["all"], k["socket"], k["dmabuf"]
        if key == "shell":
            rec["audio"] = threads_named(pids[0], "naomi-audio")
        watch[key] = rec
    row["p"] = watch
    row["android"] = sum(1 for p in ps.values() if p[0] in ("surfaceflinger", "system_server", "zygote64"))
    prev["t"] = now
    return row


def main():
    os.makedirs(DIR, exist_ok=True)
    prev = {}
    first = True
    while True:
        row = sample(prev)
        if first:
            row["start"] = True   # служба запущена: телефон включился или её перезапустили
            first = False
        day = time.strftime("%Y%m%d")
        with open(f"{DIR}/{day}.jsonl", "a") as f:
            f.write(json.dumps(row, separators=(",", ":")) + "\n")
        for old in sorted(glob.glob(DIR + "/*.jsonl"))[:-KEEP_DAYS]:
            os.remove(old)
        time.sleep(STEP - time.time() % STEP)


if __name__ == "__main__":
    main()
