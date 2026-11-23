#!/usr/bin/env python3
"""НаомиФон — виртуальный палец: сенсорный экран через uinput, ведёт касание по заданной траектории.

  sudo python3 touchinject.py circle [секунды] [--hz 180] [--radius 300]
  sudo python3 touchinject.py swipe X1 Y1 X2 Y2 [секунды]
  sudo python3 touchinject.py zigzag X1 Y1 X2 Y2 [секунды]   змейка по полосе: влево-вправо между X1
                                                           и X2 (0,8 с туда-обратно), Y плавно от Y1 к Y2
  sudo python3 touchinject.py tap X Y [X Y …] [--double] [--gap 0.18]   касания по очереди одним
                                                         устройством (набор на клавиатуре)
  sudo python3 touchinject.py replay --file ЗАПИСЬ.json   повторить путь настоящего пальца из
                                                         `touchrec.py --frames` с исходными таймингами
  sudo python3 touchinject.py key КОД                    нажать клавишу (116 — питание, 114/115 —
                                                         громкость) с виртуальной клавиатуры

Координаты — пиксели панели (1080 × 2340), как у настоящего sec_touchscreen. Устройство
«naomi-virtual-touch» живёт, пока идёт жест; libinput оболочки подхватывает его сам (udev).
Для режима разработчика: проверка ввода без человека.
"""
import argparse
import math
import os
import struct
import sys
import time

UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT, UI_SET_PROPBIT = 0x40045564, 0x40045565, 0x40045567, 0x4004556E
UI_DEV_SETUP, UI_ABS_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = 0x405C5503, 0x401C5504, 0x5501, 0x5502
EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
BTN_TOUCH, INPUT_PROP_DIRECT = 0x14A, 1
ABS_X, ABS_Y, ABS_MT_SLOT, ABS_MT_POSITION_X, ABS_MT_POSITION_Y, ABS_MT_TRACKING_ID = 0, 1, 0x2F, 0x35, 0x36, 0x39
W, H = 1080, 2340

import fcntl  # noqa: E402


class Finger:
    def __init__(self):
        self.fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
        for ev in (EV_SYN, EV_KEY, EV_ABS):
            fcntl.ioctl(self.fd, UI_SET_EVBIT, ev)
        fcntl.ioctl(self.fd, UI_SET_KEYBIT, BTN_TOUCH)
        fcntl.ioctl(self.fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT)
        for code, mx in ((ABS_X, W - 1), (ABS_Y, H - 1), (ABS_MT_SLOT, 9), (ABS_MT_POSITION_X, W - 1),
                         (ABS_MT_POSITION_Y, H - 1), (ABS_MT_TRACKING_ID, 65535)):
            fcntl.ioctl(self.fd, UI_SET_ABSBIT, code)
            fcntl.ioctl(self.fd, UI_ABS_SETUP, struct.pack("HH6i", code, 0, 0, 0, mx, 0, 0, 0))
        name = b"naomi-virtual-touch"
        fcntl.ioctl(self.fd, UI_DEV_SETUP, struct.pack("4H80sI", 0x06, 0x1D6B, 0x0104, 1, name, 0))
        fcntl.ioctl(self.fd, UI_DEV_CREATE)
        time.sleep(1.0)  # udev и libinput оболочки должны увидеть устройство
        self.down = False
        self.tracking = 1

    def _ev(self, typ, code, val):
        os.write(self.fd, struct.pack("llHHi", 0, 0, typ, code, val))

    def move(self, x, y):
        x, y = int(max(0, min(W - 1, x))), int(max(0, min(H - 1, y)))
        self._ev(EV_ABS, ABS_MT_SLOT, 0)
        if not self.down:
            self._ev(EV_ABS, ABS_MT_TRACKING_ID, self.tracking)
        self._ev(EV_ABS, ABS_MT_POSITION_X, x)
        self._ev(EV_ABS, ABS_MT_POSITION_Y, y)
        if not self.down:
            self._ev(EV_KEY, BTN_TOUCH, 1)
            self.down = True
        self._ev(EV_ABS, ABS_X, x)
        self._ev(EV_ABS, ABS_Y, y)
        self._ev(EV_SYN, 0, 0)

    def up(self):
        if self.down:
            self._ev(EV_ABS, ABS_MT_SLOT, 0)
            self._ev(EV_ABS, ABS_MT_TRACKING_ID, -1)
            self._ev(EV_KEY, BTN_TOUCH, 0)
            self._ev(EV_SYN, 0, 0)
            self.down = False
            self.tracking += 1

    def close(self):
        self.up()
        time.sleep(0.2)
        fcntl.ioctl(self.fd, UI_DEV_DESTROY)
        os.close(self.fd)


def press_key(code):
    """Виртуальная клавиатура с одной клавишей: нажать и отпустить."""
    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    fcntl.ioctl(fd, UI_SET_KEYBIT, code)
    fcntl.ioctl(fd, UI_DEV_SETUP, struct.pack("4H80sI", 0x06, 0x1D6B, 0x0105, 1, b"naomi-virtual-keys", 0))
    fcntl.ioctl(fd, UI_DEV_CREATE)
    time.sleep(1.0)
    for val in (1, 0):
        os.write(fd, struct.pack("llHHi", 0, 0, EV_KEY, code, val))
        os.write(fd, struct.pack("llHHi", 0, 0, EV_SYN, 0, 0))
        time.sleep(0.08)
    time.sleep(0.2)
    fcntl.ioctl(fd, UI_DEV_DESTROY)
    os.close(fd)


def paced(seconds, hz, fn):
    """fn(доля 0..1) каждые 1/hz с, по абсолютному расписанию (без накопления сдвига)."""
    t0 = time.monotonic()
    n = int(seconds * hz)
    for i in range(n + 1):
        fn(i / n)
        delay = t0 + (i + 1) / hz - time.monotonic()
        if delay > 0:
            time.sleep(delay)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("gesture", choices=("circle", "swipe", "zigzag", "tap", "replay", "key"))
    ap.add_argument("args", nargs="*", type=float)
    ap.add_argument("--hz", type=float, default=180)   # как настоящий сенсор Pixel 5
    ap.add_argument("--radius", type=float, default=300)
    ap.add_argument("--double", action="store_true")
    ap.add_argument("--gap", type=float, default=0.12)   # tap: пауза между касаниями, с
    ap.add_argument("--file", help="для replay: JSON от touchrec.py --frames")
    a = ap.parse_args()
    if a.gesture == "key":
        press_key(int(a.args[0]))
        print(f"клавиша {int(a.args[0])} нажата", file=sys.stderr)
        return
    f = Finger()
    try:
        if a.gesture == "circle":
            sec = a.args[0] if a.args else 3
            cx, cy, r = W / 2, H / 2, a.radius
            paced(sec, a.hz, lambda p: f.move(cx + r * math.cos(p * 2 * math.pi * sec / 1.5),
                                              cy + r * math.sin(p * 2 * math.pi * sec / 1.5)))
        elif a.gesture == "replay":
            import json
            frames = json.load(open(a.file))["frame_list"]   # [t, x, y, двигался]
            t0, start, prev_t = frames[0][0], time.monotonic(), None
            for t, x, y, _ in frames:
                if prev_t is not None and t - prev_t > 0.1:   # палец отрывали — отпустить
                    f.up()
                delay = start + (t - t0) - time.monotonic()
                if delay > 0:
                    time.sleep(delay)
                f.move(x, y)
                prev_t = t
        elif a.gesture == "zigzag":
            x1, y1, x2, y2 = a.args[:4]
            sec = a.args[4] if len(a.args) > 4 else 4

            def zz(p):
                phase = (p * sec / 0.8) % 1.0
                k = 0.5 - 0.5 * math.cos(phase * 2 * math.pi)   # 0 → 1 → 0, плавно
                f.move(x1 + (x2 - x1) * k, y1 + (y2 - y1) * p)
            paced(sec, a.hz, zz)
        elif a.gesture == "swipe":
            x1, y1, x2, y2 = a.args[:4]
            sec = a.args[4] if len(a.args) > 4 else 0.3
            paced(sec, a.hz, lambda p: f.move(x1 + (x2 - x1) * p, y1 + (y2 - y1) * p))
        else:
            points = list(zip(a.args[0::2], a.args[1::2]))
            for x, y in points:
                for i in range(2 if a.double else 1):
                    f.move(x, y)
                    time.sleep(0.05)
                    f.up()
                    time.sleep(a.gap)
    finally:
        f.close()
    print(f"жест {a.gesture} выполнен", file=sys.stderr)


if __name__ == "__main__":
    main()
