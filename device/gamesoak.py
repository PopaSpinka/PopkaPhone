#!/usr/bin/env python3
"""НаомиФон — долгий замер Android-приложения на столе (docs/ANDROID.md, «Скорость»).

Пока приложение Android (игра) идёт на рабочем столе, раз в `step` секунд пишет строку CSV:
  кадры приложения — сколько в секунду и сколько из них «долгих» (шаг больше 1,5 периода развёртки Android:
    кадр пропущен) — по отметкам SurfaceFlinger Android (dumpsys SurfaceFlinger --latency, последние 127 кадров);
  стол и трансляция — кадров в секунду и время кодера (строки журнала оболочки);
  мощность телефона (что берёт от кабеля + разряд батареи), температуры (корпус, плата, ядра, видеочип, батарея),
    ограничители по нагреву, частоты ядер и видеочипа, занятость видеочипа;
  процессор: всего (из 8 ядер) и по процессам — игра, оболочка, SurfaceFlinger и композитор Android, звук Android
    (его поток writer — тот, что крутил ядро), PulseAudio.
Итог — JSON в stdout (средние по второй половине: телефон прогрет), ход — в stderr, CSV — в файл `out`.
Запуск с Mac: `tools/naomi gamesoak [сек] [пакет] [имя]` (от root: dumpsys в контейнере, частоты).
"""
import glob
import json
import os
import re
import statistics
import subprocess
import sys
import time

LXC = ["lxc-attach", "-P", "/var/lib/waydroid/lxc", "-n", "waydroid", "--clear-env",
       "-v", "PATH=/system/bin:/system/xbin:/vendor/bin", "--", "/system/bin/sh", "-c"]
ZONES = ("skin-therm", "sdm-therm", "cpu-1-max-step", "gpuss-max-step", "battery")
COOLING = ("thermal-cpufreq-0", "thermal-cpufreq-6", "thermal-cpufreq-7", "thermal-devfreq-0")
HZ = os.sysconf("SC_CLK_TCK")


def read(p, default=None):
    try:
        with open(p) as f:
            return f.read().strip()
    except OSError:
        return default


def num(p, default=0):
    v = read(p)
    try:
        return int(v)
    except (TypeError, ValueError):
        return default


def zones():
    out = {}
    for z in glob.glob("/sys/class/thermal/thermal_zone*"):
        t = read(z + "/type")
        if t in ZONES:
            out[t] = num(z + "/temp") / 1000
    out["battery"] = num("/sys/class/power_supply/battery/temp") / 10
    return out


def cooling():
    out = {c: 0 for c in COOLING}
    for c in glob.glob("/sys/class/thermal/cooling_device*"):
        t = read(c + "/type")
        if t in out:
            out[t] = num(c + "/cur_state")
    return out


def power_w():
    usb = num("/sys/class/power_supply/usb/voltage_now") * num("/sys/class/power_supply/usb/input_current_now") / 1e12
    bat = num("/sys/class/power_supply/battery/voltage_now") * num("/sys/class/power_supply/battery/current_now") / 1e12
    return usb + bat   # ток батареи: плюс — разряд, минус — заряд (его мощность телефон не тратит)


def cpu_total():
    f = read("/proc/stat").splitlines()[0].split()[1:]
    v = list(map(int, f))
    return sum(v), v[3] + v[4]   # всего, простой (idle + iowait)


def procs():
    """pid → (имя для отчёта, тики); поток writer звука Android — отдельно"""
    want = {"surfaceflinger": "sf", "naomi-shell": "shell", "pulseaudio": "pulse"}
    out = {}
    for d in os.listdir("/proc"):
        if not d.isdigit():
            continue
        comm = read(f"/proc/{d}/comm", "")
        cmd = read(f"/proc/{d}/cmdline", "").replace("\0", " ")
        name = want.get(comm)
        if "graphics.composer" in cmd:
            name = "composer"
        elif "android.hardware.audio.service" in cmd:
            name = "audiohal"
        elif GAME and cmd.startswith(GAME):
            name = "game"
        if not name:
            continue
        st = read(f"/proc/{d}/stat", "")
        if not st:
            continue
        f = st[st.rfind(")") + 2:].split()
        out[name] = out.get(name, 0) + int(f[11]) + int(f[12])
    return out


def frames():
    """кадры приложения GAME по SurfaceFlinger Android: (кадров в с, доля долгих, период развёртки мс)"""
    if not GAME:
        return None
    script = (f"dumpsys SurfaceFlinger --list | grep '{GAME}' | grep -E 'BLAST|SurfaceView' | "
              "while IFS= read -r l; do echo \"== $l\"; dumpsys SurfaceFlinger --latency \"$l\"; done")
    try:
        out = subprocess.run(LXC + [script], capture_output=True, text=True, timeout=20).stdout
    except subprocess.TimeoutExpired:
        return None
    best = None
    for block in out.split("== ")[1:]:
        lines = block.splitlines()[1:]
        if not lines:
            continue
        period = int(lines[0]) / 1e6 if lines[0].strip().isdigit() else 0
        pres = [int(r.split()[1]) for r in lines[1:] if len(r.split()) == 3 and 0 < int(r.split()[1]) < 2 ** 62]
        if len(pres) < 10 or not period:
            continue
        d = [(b - a) / 1e6 for a, b in zip(pres, pres[1:])]
        span = (pres[-1] - pres[0]) / 1e9
        fps = len(d) / span if span > 0 else 0
        long_ = sum(1 for x in d if x > 1.5 * period) / len(d)
        if not best or fps > best[0]:
            best = (round(fps, 1), round(long_, 3), round(period, 2))
    return best


def desk():
    """последние строки журнала стола и трансляции: кадров стола в с, кадров потока, кодер мс"""
    out = subprocess.run(["journalctl", "-u", "naomi-shell", "--since", f"-{STEP + 2}s", "--no-pager", "-o", "cat"],
                         capture_output=True, text=True).stdout
    d = [int(m) for m in re.findall(r"стол: кадров (\d+)", out)]
    s = [int(m) for m in re.findall(r"naomi.stream: кадров (\d+)", out)]
    e = [float(m) for m in re.findall(r"кодер ([\d.]+)", out)]
    avg = lambda v: round(sum(v) / len(v), 1) if v else ""
    return avg(d), avg(s), avg(e)


DUR = int(sys.argv[1]) if len(sys.argv) > 1 else 600
GAME = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] != "-" else ""
OUT = sys.argv[3] if len(sys.argv) > 3 else "/tmp/gamesoak.csv"
STEP = 10

COLS = ["t", "W", "cpu%", "gpu%", "gpuMHz", "f0", "f6", "f7", "fps", "long", "vsync_ms", "desk_fps", "stream_fps",
        "enc_ms"] + list(ZONES) + list(COOLING) + ["game", "shell", "sf", "composer", "audiohal", "pulse"]
rows = []
t0 = time.time()
tot0, idle0 = cpu_total()
p0 = procs()
with open(OUT, "w") as f:
    f.write(",".join(COLS) + "\n")
    while time.time() - t0 < DUR:
        ws = []
        t_end = time.time() + STEP
        while time.time() < t_end - 0.5:   # мощность — среднее за шаг (2 раза в секунду)
            ws.append(power_w())
            time.sleep(0.5)
        tot, idle = cpu_total()
        p = procs()
        dt = tot - tot0
        cpu = 100 * (dt - (idle - idle0)) / dt if dt else 0
        sec = dt / HZ / os.cpu_count()   # прошло секунд (по тикам всех ядер)
        share = {k: round(100 * (p.get(k, 0) - p0.get(k, 0)) / HZ / sec, 1) if sec else 0 for k in
                 ("game", "shell", "sf", "composer", "audiohal", "pulse")}
        tot0, idle0, p0 = tot, idle, p
        fr = frames() or ("", "", "")
        dk = desk()
        z = zones()
        c = cooling()
        k = "/sys/class/kgsl/kgsl-3d0"
        row = {"t": round(time.time() - t0), "W": round(sum(ws) / len(ws), 2), "cpu%": round(cpu, 1),
               "gpu%": int((read(k + "/gpu_busy_percentage") or "0").split()[0]),
               "gpuMHz": num(k + "/devfreq/cur_freq") // 1000000,
               "f0": num("/sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq") // 1000,
               "f6": num("/sys/devices/system/cpu/cpufreq/policy6/scaling_cur_freq") // 1000,
               "f7": num("/sys/devices/system/cpu/cpufreq/policy7/scaling_cur_freq") // 1000,
               "fps": fr[0], "long": fr[1], "vsync_ms": fr[2], "desk_fps": dk[0], "stream_fps": dk[1], "enc_ms": dk[2]}
        row.update(z)
        row.update(c)
        row.update(share)
        rows.append(row)
        f.write(",".join(str(row.get(col, "")) for col in COLS) + "\n")
        f.flush()
        print(f"{row['t']:4d} с  {row['W']:.2f} Вт  корпус {z.get('skin-therm', 0):.1f}  плата {z.get('sdm-therm', 0):.1f}  "
              f"кадров {row['fps']} (долгих {row['long']})  стол {row['desk_fps']}  ядра {row['f0']}/{row['f6']}/"
              f"{row['f7']}  видеочип {row['gpuMHz']} МГц {row['gpu%']}%  игра {share['game']}%  "
              f"звук {share['audiohal']}%", file=sys.stderr, flush=True)

half = rows[len(rows) // 2:]


def mean(col):
    v = [r[col] for r in half if isinstance(r.get(col), (int, float))]
    return round(statistics.mean(v), 2) if v else None


summary = {"duration_s": DUR, "app": GAME, "csv": OUT, "second_half": {c: mean(c) for c in COLS if c != "t"},
           "end": {c: rows[-1].get(c) for c in COLS} if rows else {}}
print(json.dumps(summary, ensure_ascii=False, indent=1))
