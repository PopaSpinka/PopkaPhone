#!/usr/bin/env python3
"""НаомиФон, приставка на Pi — проверочный трекпад (uinput): пальцы по сценарию, без рук (docs/PI.md, «Трекпад»).

  sudo python3 vpad.py команда …       команды через пробел, как у vinput.py

Устройство — как Magic Trackpad для приёмника: касания многими пальцами (ABS_MT_*, до 16), размер пятна, сила,
кнопка-щелчок, 160 × 115 мм, отчёты 90 раз в секунду. Приёмник замечает его за ≤ 2 с — первой командой wait 2.5.
Координаты — миллиметры от левого верхнего угла. Команды:
  wait С                 подождать (пальцы, что лежат, шлют отчёты и стоя — как настоящие)
  down N X Y             поставить палец N (1…16) в точку
  up N / upall           снять палец / все
  glide DX DY МС [N…]    вести пальцы (все лежащие или названные) на DX, DY мм за МС миллисекунд
  spread ММ МС           два пальца: развести на ММ (минус — свести) за МС — щипок
  shake ММ МС            лежащие пальцы дрожат с таким размахом
  click / unclick        нажать / отпустить щелчок трекпада
  tap X Y [ЧИСЛО]        касание: пальцы (1…3, рядом) на 60 мс
  size N MAJOR           размер пятна пальца N (по умолчанию 300; ладонь — от 900)
"""
import fcntl
import math
import os
import struct
import sys
import time

EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
BTN_LEFT, BTN_TOOL_FINGER, BTN_TOUCH = 0x110, 0x145, 0x14a
ABS_MT_SLOT, ABS_MT_TOUCH_MAJOR, ABS_MT_TOUCH_MINOR = 0x2f, 0x30, 0x31
ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID, ABS_MT_PRESSURE = 0x35, 0x36, 0x39, 0x3a
UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT, UI_SET_PROPBIT = 0x40045564, 0x40045565, 0x40045567, 0x4004556e
UI_DEV_SETUP, UI_ABS_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = 0x405c5503, 0x401c5504, 0x5501, 0x5502
RES_X, RES_Y, W, H = 47, 44, 160.0, 115.0   # отсчётов на мм и размер, как у Magic Trackpad
PERIOD = 1 / 90


class Pad:
    def __init__(self):
        self.fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
        for ev in (EV_KEY, EV_ABS):
            fcntl.ioctl(self.fd, UI_SET_EVBIT, ev)
        for k in (BTN_LEFT, BTN_TOOL_FINGER, BTN_TOUCH):
            fcntl.ioctl(self.fd, UI_SET_KEYBIT, k)
        fcntl.ioctl(self.fd, UI_SET_PROPBIT, 0)   # INPUT_PROP_POINTER
        fcntl.ioctl(self.fd, UI_SET_PROPBIT, 2)   # INPUT_PROP_BUTTONPAD
        for code, lo, hi, res in ((ABS_MT_SLOT, 0, 15, 0), (ABS_MT_TRACKING_ID, 0, 65535, 0),
                                  (ABS_MT_POSITION_X, 0, int(W * RES_X), RES_X), (ABS_MT_POSITION_Y, 0, int(H * RES_Y), RES_Y),
                                  (ABS_MT_TOUCH_MAJOR, 0, 1020, 0), (ABS_MT_TOUCH_MINOR, 0, 1020, 0), (ABS_MT_PRESSURE, 0, 253, 0)):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, code)
            fcntl.ioctl(self.fd, UI_ABS_SETUP, struct.pack("HHiiiiii", code, 0, 0, lo, hi, 0, 0, res))
        fcntl.ioctl(self.fd, UI_DEV_SETUP, struct.pack("HHHH80sI", 0x06, 0x1, 0x1, 1, b"naomi test trackpad", 0))
        fcntl.ioctl(self.fd, UI_DEV_CREATE)
        self.f = {}          # палец → [x, y, major, pressure]
        self.slot = {}       # палец → слот
        self.sent = {}       # слот → что послано
        self.click = self.sent_click = False
        self.next_id = 1
        self.next = time.monotonic()

    def ev(self, t, c, v):
        os.write(self.fd, struct.pack("llHHi", 0, 0, t, c, v))

    def report(self):   # отчёт: что сменилось — по слотам, потом кнопка, потом SYN (как hid-magicmouse)
        for n, slot in list(self.slot.items()):
            if n not in self.f:
                self.ev(EV_ABS, ABS_MT_SLOT, slot)
                self.ev(EV_ABS, ABS_MT_TRACKING_ID, -1)
                del self.slot[n]
                del self.sent[slot]
        for n, (x, y, major, pressure) in self.f.items():
            if n not in self.slot:
                self.slot[n] = min(set(range(16)) - set(self.slot.values()))
                self.ev(EV_ABS, ABS_MT_SLOT, self.slot[n])
                self.ev(EV_ABS, ABS_MT_TRACKING_ID, self.next_id)
                self.next_id += 1
                self.sent[self.slot[n]] = None
            now = (round(x * RES_X), round(y * RES_Y), major, major * 3 // 4, pressure)
            if self.sent[self.slot[n]] != now:
                self.ev(EV_ABS, ABS_MT_SLOT, self.slot[n])
                for code, v in zip((ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TOUCH_MAJOR, ABS_MT_TOUCH_MINOR, ABS_MT_PRESSURE), now):
                    self.ev(EV_ABS, code, v)
                self.sent[self.slot[n]] = now
        if self.click != self.sent_click:
            self.ev(EV_KEY, BTN_LEFT, int(self.click))
            self.sent_click = self.click
        self.ev(EV_KEY, BTN_TOUCH, int(bool(self.f)))
        self.ev(EV_SYN, 0, 0)

    def tick(self):
        self.next += PERIOD
        d = self.next - time.monotonic()
        if d > 0:
            time.sleep(d)
        else:
            self.next = time.monotonic()
        self.report()

    def hold(self, sec):
        end = time.monotonic() + sec
        if not self.f:
            self.report()
            time.sleep(sec)
            self.next = time.monotonic()
            return
        while time.monotonic() < end:
            self.tick()

    def glide(self, dx, dy, ms, ids):
        steps = max(1, round(ms / 1000 / PERIOD))
        for _ in range(steps):
            for n in ids:
                self.f[n][0] += dx / steps
                self.f[n][1] += dy / steps
            self.tick()

    def run(self, a):
        i = 0
        while i < len(a):
            c = a[i]
            if c == "wait":
                self.hold(float(a[i + 1])); i += 2
            elif c == "down":
                self.f[int(a[i + 1])] = [float(a[i + 2]), float(a[i + 3]), 300, 60]; self.tick(); i += 4
            elif c == "up":
                self.f.pop(int(a[i + 1]), None); self.tick(); i += 2
            elif c == "upall":
                self.f.clear(); self.tick(); i += 1
            elif c == "glide":
                j = i + 4
                ids = []
                while j < len(a) and a[j].isdigit():
                    ids.append(int(a[j])); j += 1
                self.glide(float(a[i + 1]), float(a[i + 2]), float(a[i + 3]), ids or list(self.f)); i = j
            elif c == "spread":
                (p, q), mm, ms = sorted(self.f)[:2], float(a[i + 1]), float(a[i + 2])
                ax, ay, bx, by = self.f[p][0], self.f[p][1], self.f[q][0], self.f[q][1]
                d = math.hypot(bx - ax, by - ay) or 1
                ux, uy, steps = (bx - ax) / d, (by - ay) / d, max(1, round(ms / 1000 / PERIOD))
                for _ in range(steps):
                    self.f[p][0] -= ux * mm / 2 / steps; self.f[p][1] -= uy * mm / 2 / steps
                    self.f[q][0] += ux * mm / 2 / steps; self.f[q][1] += uy * mm / 2 / steps
                    self.tick()
                i += 3
            elif c == "shake":
                mm, steps = float(a[i + 1]), max(1, round(float(a[i + 2]) / 1000 / PERIOD))
                base = {n: v[:2] for n, v in self.f.items()}
                for k in range(steps):
                    for n, (x, y) in base.items():
                        self.f[n][0], self.f[n][1] = x + mm * math.sin(k * 1.7 + n), y + mm * math.cos(k * 2.3 + n)
                    self.tick()
                i += 3
            elif c == "click":
                self.click = True; self.tick(); i += 1
            elif c == "unclick":
                self.click = False; self.tick(); i += 1
            elif c == "tap":
                x, y = float(a[i + 1]), float(a[i + 2])
                n = int(a[i + 3]) if i + 3 < len(a) and a[i + 3].isdigit() else 0
                for k in range(n or 1):
                    self.f[k + 1] = [x + 21 * k, y - 4 * (k % 2), 300, 60]
                self.hold(0.06)
                self.f.clear(); self.tick(); i += 4 if n else 3
            elif c == "size":
                self.f[int(a[i + 1])][2] = int(a[i + 2]); i += 3
            else:
                sys.exit("не понял: " + c)

    def close(self):
        self.f.clear()
        self.click = False
        self.report()
        time.sleep(0.05)
        fcntl.ioctl(self.fd, UI_DEV_DESTROY)
        os.close(self.fd)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    pad = Pad()
    try:
        pad.run(sys.argv[1:])
    finally:
        pad.close()
