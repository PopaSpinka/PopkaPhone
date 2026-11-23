#!/usr/bin/env python3
"""НаомиФон — нагрев под нагрузкой.

Грузит все 8 ядер процессора на `load` секунд, потом `cool` секунд остывает; раз в секунду пишет
частоты ядер, температуры (корпус, CPU, GPU, батарея), ток батареи и включённые ограничители
(thermal cooling devices). Итог — JSON в stdout, ход — в stderr.
Запуск с Mac: `tools/naomi stress [load] [cool]`. Если телефон на кабеле — он заряжается, это
добавляет тепла; в отчёте поле `charging`.
"""
import glob
import json
import multiprocessing as mp
import os
import sys
import time

THERMAL = ("skin-therm", "cpu-0-max-step", "cpu-1-max-step", "gpuss-max-step", "battery", "disp-therm")


def read(p):
    try:
        with open(p) as f:
            return f.read().strip()
    except OSError:
        return None


def zones():
    out = {}
    for z in glob.glob("/sys/class/thermal/thermal_zone*"):
        t = read(z + "/type")
        if t in THERMAL:
            v = read(z + "/temp")
            out[t] = int(v) / 1000 if v and v.lstrip("-").isdigit() else None
    return out


def freqs():
    return {os.path.basename(p): int(read(p + "/scaling_cur_freq") or 0) // 1000
            for p in sorted(glob.glob("/sys/devices/system/cpu/cpufreq/policy*"))}


def cooling():
    act = {}
    for c in glob.glob("/sys/class/thermal/cooling_device*"):
        cur = read(c + "/cur_state")
        if cur and cur.isdigit() and int(cur) > 0:
            act[read(c + "/type")] = int(cur)
    return act


def battery():
    b = "/sys/class/power_supply/battery"
    return {"ma": int(read(b + "/current_now") or 0) / 1000, "status": read(b + "/status"),
            "pct": int(read(b + "/capacity") or 0)}


def burn(stop_at):
    x = 0
    while time.time() < stop_at:
        for i in range(20000):
            x = (x * 1103515245 + 12345) & 0x7FFFFFFF


def main():
    load = int(sys.argv[1]) if len(sys.argv) > 1 else 60
    cool = int(sys.argv[2]) if len(sys.argv) > 2 else 30
    samples = []
    start = time.time()
    stop_at = start + load
    procs = [mp.Process(target=burn, args=(stop_at,)) for _ in range(os.cpu_count() or 8)]
    for p in procs:
        p.start()
    while time.time() < stop_at + cool:
        t = round(time.time() - start, 1)
        s = {"t": t, "phase": "load" if t < load else "cool", "temp": zones(), "mhz": freqs(),
             "cooling": cooling(), "bat": battery()}
        samples.append(s)
        if int(t) % 10 == 0:
            print(f"  {t:>5} с {s['phase']}: корпус {s['temp'].get('skin-therm')}°, "
                  f"CPU {s['temp'].get('cpu-1-max-step')}°, МГц {list(s['mhz'].values())}, "
                  f"ограничители {s['cooling'] or '—'}", file=sys.stderr, flush=True)
        time.sleep(1)
    for p in procs:
        p.join()

    def peak(key, phase=None):
        vals = [s["temp"].get(key) for s in samples if (phase is None or s["phase"] == phase)]
        vals = [v for v in vals if v is not None]
        return max(vals) if vals else None

    loadset = [s for s in samples if s["phase"] == "load"]
    hw_max = {os.path.basename(p): int(read(p + "/cpuinfo_max_freq") or 0) // 1000
              for p in sorted(glob.glob("/sys/devices/system/cpu/cpufreq/policy*"))}
    min_under_load = {k: min(s["mhz"][k] for s in loadset[3:]) for k in hw_max} if len(loadset) > 3 else {}
    summary = {
        "load_s": load, "cool_s": cool, "charging": samples[0]["bat"]["status"],
        "start": samples[0]["temp"], "peak": {k: peak(k) for k in THERMAL},
        "end": samples[-1]["temp"],
        "cpu_mhz_hw_max": hw_max, "cpu_mhz_min_under_load": min_under_load,
        "throttled": {k: v for k, v in min_under_load.items() if v < hw_max[k] * 0.9},
        "cooling_seen": sorted({k for s in samples for k in s["cooling"]}),
    }
    json.dump({"stress": 1, "summary": summary, "samples": samples}, sys.stdout, ensure_ascii=False)
    print()


if __name__ == "__main__":
    main()
