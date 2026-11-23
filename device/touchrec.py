#!/usr/bin/env python3
"""НаомиФон — запись сырых событий сенсорного экрана (evdev) и разбор: как часто сенсор шлёт кадры
касания, были ли потери (SYN_DROPPED), сколько контактов, где координаты скачут.

  python3 touchrec.py [секунды] [--name sec_touchscreen] [--wait 60]

Ждёт первого касания до --wait с, затем пишет `секунды`. Читает устройство параллельно с оболочкой
(без захвата). Итог — JSON в stdout.
"""
import argparse
import fcntl
import glob
import json
import os
import select
import struct
import sys
import time

EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
SYN_REPORT, SYN_DROPPED = 0, 3
ABS_MT_SLOT, ABS_MT_TOUCH_MAJOR, ABS_MT_POSITION_X, ABS_MT_POSITION_Y = 0x2F, 0x30, 0x35, 0x36
ABS_MT_TRACKING_ID, ABS_MT_PRESSURE = 0x39, 0x3A
EVENT = struct.Struct("llHHi")  # timeval (2 × long), type, code, value


def find_device(name):
    for path in sorted(glob.glob("/sys/class/input/event*/device/name")):
        if open(path).read().strip() == name:
            return "/dev/input/" + path.split("/")[4]
    raise SystemExit(f"нет устройства {name}")


def abs_info(fd, code):
    buf = bytearray(24)  # struct input_absinfo: value, min, max, fuzz, flat, resolution
    fcntl.ioctl(fd, 0x80184540 + code, buf)  # EVIOCGABS(code)
    v, mn, mx, fuzz, flat, res = struct.unpack("6i", buf)
    return {"min": mn, "max": mx, "fuzz": fuzz, "flat": flat, "resolution": res}


def pct(sorted_vals, p):
    return sorted_vals[min(len(sorted_vals) - 1, int(len(sorted_vals) * p))] if sorted_vals else None


def arrival_stats(reads):
    rs = [r for r in reads if r[1]]
    if len(rs) < 3:
        return None
    gaps = sorted((b[0] - a[0]) * 1000 for a, b in zip(rs, rs[1:]))
    per = {}
    for r in rs:
        per[min(r[1], 5)] = per.get(min(r[1], 5), 0) + 1
    return {"reads": len(rs), "gap_ms_median": round(gaps[len(gaps) // 2], 2),
            "gap_ms_p95": round(pct(gaps, 0.95), 2), "gap_ms_max": round(gaps[-1], 2),
            "frames_per_read": {str(k) + ("+" if k == 5 else ""): v for k, v in sorted(per.items())}}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("seconds", type=float, nargs="?", default=10)
    ap.add_argument("--name", default="sec_touchscreen")
    ap.add_argument("--wait", type=float, default=60)
    ap.add_argument("--frames", action="store_true", help="добавить в отчёт все кадры (t, x, y слота 0)")
    a = ap.parse_args()
    dev = find_device(a.name)
    fd = os.open(dev, os.O_RDONLY | os.O_NONBLOCK)
    info = {k: abs_info(fd, c) for k, c in (("x", ABS_MT_POSITION_X), ("y", ABS_MT_POSITION_Y),
                                            ("slot", ABS_MT_SLOT), ("pressure", ABS_MT_PRESSURE))}
    print(f"жду касания до {a.wait:.0f} с, потом пишу {a.seconds:.0f} с ({dev}, {a.name})…",
          file=sys.stderr, flush=True)

    frames = []                 # кадры касания: время, позиции по слотам
    reads = []                  # (момент чтения по CLOCK_MONOTONIC, сколько кадров касания в нём)
    slot, slots, active = 0, {}, set()
    dropped = 0
    pending = {}
    end = time.monotonic() + a.wait
    started = False
    while time.monotonic() < end:
        r, _, _ = select.select([fd], [], [], 0.2)
        if not r:
            continue
        data = os.read(fd, EVENT.size * 256)
        reads.append([time.monotonic(), 0])
        if not started:
            started = True
            end = time.monotonic() + a.seconds
            print("касание есть — пишу", file=sys.stderr, flush=True)
        for off in range(0, len(data), EVENT.size):
            sec, usec, typ, code, val = EVENT.unpack_from(data, off)
            if typ == EV_ABS:
                if code == ABS_MT_SLOT:
                    slot = val
                elif code == ABS_MT_TRACKING_ID:
                    if val < 0:
                        active.discard(slot)
                    else:
                        active.add(slot)
                    pending.setdefault(slot, {})["id"] = val
                elif code in (ABS_MT_POSITION_X, ABS_MT_POSITION_Y):
                    slots.setdefault(slot, [0, 0])[0 if code == ABS_MT_POSITION_X else 1] = val
                    pending.setdefault(slot, {})["xy"] = True
            elif typ == EV_SYN and code == SYN_DROPPED:
                dropped += 1
            elif typ == EV_SYN and code == SYN_REPORT:
                reads[-1][1] += 1
                frames.append({"t": sec + usec / 1e6, "active": sorted(active),
                               "pos": {s: tuple(slots[s]) for s in active if s in slots},
                               "moved": sorted(s for s, p in pending.items() if p.get("xy"))})
                pending = {}
    os.close(fd)

    touching = [f for f in frames if f["active"]]
    iv = sorted((b["t"] - a_["t"]) * 1000 for a_, b in zip(touching, touching[1:])
                if b["t"] - a_["t"] < 0.1)  # интервалы внутри непрерывного ведения
    jumps, steps = [], []
    for a_, b in zip(touching, touching[1:]):
        if 0 in a_["pos"] and 0 in b["pos"] and b["t"] - a_["t"] < 0.1:
            dx = b["pos"][0][0] - a_["pos"][0][0]
            dy = b["pos"][0][1] - a_["pos"][0][1]
            d = (dx * dx + dy * dy) ** 0.5
            steps.append(d)
            if d > 80:
                jumps.append({"t": round(b["t"] - touching[0]["t"], 3), "from": a_["pos"][0],
                              "to": b["pos"][0], "px": round(d)})
    steps.sort()
    multi = sum(1 for f in touching if len(f["active"]) > 1)
    out = {
        "device": dev, "abs": info, "frames": len(frames), "touch_frames": len(touching),
        "syn_dropped": dropped, "frames_with_2plus_contacts": multi,
        "slots_seen": sorted({s for f in touching for s in f["active"]}),
        "report_hz_median": round(1000 / iv[len(iv) // 2], 1) if iv else None,
        "interval_ms": {"median": round(iv[len(iv) // 2], 2) if iv else None,
                        "p95": round(pct(iv, 0.95), 2) if iv else None,
                        "max": round(iv[-1], 2) if iv else None},
        "step_px": {"median": round(steps[len(steps) // 2], 1) if steps else None,
                    "p95": round(pct(steps, 0.95), 1) if steps else None,
                    "max": round(steps[-1], 1) if steps else None},
        "frames_without_motion": sum(1 for f in touching if not f["moved"]),
        # когда события реально доходят до программы (а не метка драйвера): пачки видно здесь
        "arrival": arrival_stats(reads),
        "jumps_over_80px": jumps[:20], "jump_count": len(jumps),
    }
    if a.frames:
        out["frame_list"] = [[round(f["t"], 5), *f["pos"][0], int(0 in f["moved"])]
                             for f in touching if 0 in f["pos"]]
    json.dump(out, sys.stdout, ensure_ascii=False, indent=1)
    print()


if __name__ == "__main__":
    main()
