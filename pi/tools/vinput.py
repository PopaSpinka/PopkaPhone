#!/usr/bin/env python3
"""НаомиФон, приставка на Pi — проверочная мышь (uinput) для замеров плавности (tools/pi-smooth, docs/PI.md).

  sudo python3 vinput.py [--hz 125] [--log файл.csv] команда …
  sudo python3 vinput.py [--hz 125] [--log файл.csv] --stdin     команды строками из stdin; после каждой строки —
                                                                «ok» в stdout (сценарий tools/deskbench ждёт его)

Мышь — как настоящая: относительный сдвиг с частотой --hz отчётов в секунду (у мыши пользователя ~60), колесо,
кнопка; плюс абсолютная позиция (ABS) — встать в точку экрана без ускорения. Приёмник замечает новое устройство
за ≤ 2 с — первой командой стоит wait 2.5. Команды:
  wait С                 подождать
  abs X Y                встать в точку: доли экрана 0…1
  press / release        левая кнопка
  right                  щелчок правой (меню «Файлов»)
  rel DX DY С            сдвигать на DX, DY отсчётов каждый отчёт, С секунд (перетаскивание, круги — нет)
  circle R С             круги радиусом R отсчётов за отчёт, оборот в секунду
  wheel N В_СЕК          N щелчков колеса (минус — к себе: страница вниз), В_СЕК щелчков в секунду
  key КОД                нажать и отпустить клавишу (113 — без звука, 114 — тише, 115 — громче): у мыши есть
                         клавиши громкости — приёмник видит её и клавиатурой
  down КОД / up КОД      нажать / отпустить клавишу — для сочетаний: Shift 42, Alt 56, Win 125, 3 — 4, 4 — 5,
                         Esc 1, пробел 57, ⌫ 14, D 32, F 33, J 36, стрелки ← 105, → 106, ↑ 103, ↓ 108
                         (скриншоты стола Shift+Alt+3/4, «Просмотр»; D и J в русской раскладке — «в» и «о»,
                         их не спутать с латиницей — печать в Android, docs/ANDROID.md)
  mark ИМЯ               метка в журнал (начало и конец шага сценария — tools/deskbench)
Журнал (--log): время каждого отчёта (CLOCK_MONOTONIC, мкс — те же часы, что у приёмника), что послано.
"""
import fcntl
import math
import os
import struct
import sys
import time

EV_SYN, EV_KEY, EV_REL, EV_ABS = 0, 1, 2, 3
REL_X, REL_Y, REL_WHEEL = 0, 1, 8
ABS_X, ABS_Y = 0, 1
BTN_LEFT, BTN_RIGHT, BTN_MIDDLE = 0x110, 0x111, 0x112
KEY_MUTE, KEY_VOLUMEDOWN, KEY_VOLUMEUP = 113, 114, 115
# клавиши для сочетаний (down / up): Esc, 3, 4, ⌫, D, F, J, Shift, Alt, пробел, стрелки, Win
KEYS = (1, 4, 5, 14, 32, 33, 36, 42, 56, 57, 103, 105, 106, 108, 125)
UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_RELBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045566, 0x40045567
UI_DEV_CREATE, UI_DEV_DESTROY = 0x5501, 0x5502
ABS_MAX = 65535


def now_us():
    return int(time.clock_gettime(time.CLOCK_MONOTONIC) * 1e6)


class Mouse:
    def __init__(self, hz, log):
        self.period = 1.0 / hz
        self.log = open(log, "w") if log else None
        if self.log:
            self.log.write("t,kind,a,b\n")
        self.fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
        for ev in (EV_KEY, EV_REL, EV_ABS):
            fcntl.ioctl(self.fd, UI_SET_EVBIT, ev)
        for b in (BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, KEY_MUTE, KEY_VOLUMEDOWN, KEY_VOLUMEUP) + KEYS:
            fcntl.ioctl(self.fd, UI_SET_KEYBIT, b)
        for r in (REL_X, REL_Y, REL_WHEEL):
            fcntl.ioctl(self.fd, UI_SET_RELBIT, r)
        for a in (ABS_X, ABS_Y):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, a)
        absmax = [0] * 64
        absmax[ABS_X] = absmax[ABS_Y] = ABS_MAX
        dev = struct.pack("80sHHHHI", b"naomi test mouse", 3, 1, 1, 1, 0)
        dev += struct.pack("64i", *absmax) + struct.pack("64i", *([0] * 64)) * 3
        os.write(self.fd, dev)
        fcntl.ioctl(self.fd, UI_DEV_CREATE)
        self.next = time.monotonic()

    def ev(self, t, c, v):
        os.write(self.fd, struct.pack("llHHi", 0, 0, t, c, v))

    def report(self, kind, *items, a=0, b=0):
        for t, c, v in items:
            self.ev(t, c, v)
        self.ev(EV_SYN, 0, 0)
        if self.log:
            self.log.write(f"{now_us()},{kind},{a},{b}\n")

    def tick(self):   # ровно с частотой отчётов
        self.next += self.period
        d = self.next - time.monotonic()
        if d > 0:
            time.sleep(d)
        else:
            self.next = time.monotonic()

    def run(self, cmds):
        i = 0
        while i < len(cmds):
            c = cmds[i]
            if c == "wait":
                time.sleep(float(cmds[i + 1]))
                self.next = time.monotonic()
                i += 2
            elif c == "abs":
                x, y = int(float(cmds[i + 1]) * ABS_MAX), int(float(cmds[i + 2]) * ABS_MAX)
                self.report("abs", (EV_ABS, ABS_X, x), (EV_ABS, ABS_Y, y), a=x, b=y)
                i += 3
            elif c in ("press", "release"):
                self.report(c, (EV_KEY, BTN_LEFT, 1 if c == "press" else 0))
                i += 1
            elif c == "right":
                self.report(c, (EV_KEY, BTN_RIGHT, 1))
                self.report(c, (EV_KEY, BTN_RIGHT, 0))
                i += 1
            elif c == "rel":
                dx, dy, secs = int(cmds[i + 1]), int(cmds[i + 2]), float(cmds[i + 3])
                end = time.monotonic() + secs
                while time.monotonic() < end:
                    self.report("rel", (EV_REL, REL_X, dx), (EV_REL, REL_Y, dy), a=dx, b=dy)
                    self.tick()
                i += 4
            elif c == "circle":
                r, secs = float(cmds[i + 1]), float(cmds[i + 2])
                n = int(secs / self.period)
                per_turn = 1 / self.period
                for k in range(n):
                    a = k * 2 * math.pi / per_turn
                    dx, dy = int(round(r * math.cos(a))), int(round(r * math.sin(a)))
                    self.report("rel", (EV_REL, REL_X, dx), (EV_REL, REL_Y, dy), a=dx, b=dy)
                    self.tick()
                i += 3
            elif c == "wheel":
                n, rate = int(cmds[i + 1]), float(cmds[i + 2])
                step = -1 if n < 0 else 1
                for _ in range(abs(n)):
                    self.report("wheel", (EV_REL, REL_WHEEL, step), a=step)
                    time.sleep(1 / rate)
                i += 3
            elif c == "key":
                code = int(cmds[i + 1])
                self.report("key", (EV_KEY, code, 1), a=code, b=1)
                time.sleep(0.05)
                self.report("key", (EV_KEY, code, 0), a=code, b=0)
                i += 2
            elif c == "mark":
                if self.log:
                    self.log.write(f"{now_us()},mark,{cmds[i + 1]},\n")
                    self.log.flush()
                i += 2
            elif c in ("down", "up"):
                code = int(cmds[i + 1])
                self.report("key", (EV_KEY, code, 1 if c == "down" else 0), a=code, b=1 if c == "down" else 0)
                time.sleep(0.03)
                i += 2
            else:
                sys.exit(f"не знаю команду: {c}")

    def close(self):
        fcntl.ioctl(self.fd, UI_DEV_DESTROY)
        os.close(self.fd)
        if self.log:
            self.log.close()


def main():
    args = sys.argv[1:]
    hz, log, stdin = 125.0, None, False
    while args and args[0].startswith("--"):
        if args[0] == "--stdin":
            stdin, args = True, args[1:]
            continue
        if args[0] == "--hz":
            hz = float(args[1])
        elif args[0] == "--log":
            log = args[1]
        args = args[2:]
    m = Mouse(hz, log)
    try:
        if stdin:
            for line in sys.stdin:
                if not line.strip():
                    break
                m.run(line.split())
                if m.log:
                    m.log.flush()
                print("ok", flush=True)
        else:
            m.run(args)
    finally:
        m.close()


if __name__ == "__main__":
    main()
