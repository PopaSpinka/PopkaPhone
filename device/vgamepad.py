#!/usr/bin/env python3
"""НаомиФон — проверочный геймпад на самом телефоне (uinput): кнопки и стики как у геймпада Switch Pro по Bluetooth
(драйвер hid-nintendo: A/B/X/Y по месту, крестовина кнопками, стики −32767…32767). Игра стола (naomi-game) берёт его
тем же путём, что настоящий (docs/GAMES.md, «Геймпад»).

  sudo python3 vgamepad.py wait:2 start down:0.5 south lx:-32767 wait:1 lx:0
Кнопка — нажать на 0,1 с, «кнопка:сек» — подержать: south east west north (нижняя, правая, левая, верхняя), tl tr tl2
tr2 (L, R, ZL, ZR), select start mode (−, +, Home), thumbl thumbr, up down left right (крестовина). Стик — «lx:значение»
(lx ly rx ry), остаётся, пока не сменят. wait:сек — пауза. Геймпад появляется за 1 с до первой команды и уходит после
последней (через 0,5 с).
"""
import fcntl
import os
import struct
import sys
import time

UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045567
UI_DEV_SETUP, UI_ABS_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = 0x405C5503, 0x401C5504, 0x5501, 0x5502
EV_SYN, EV_KEY, EV_ABS = 0, 1, 3
BUTTONS = {"south": 304, "east": 305, "north": 307, "west": 308, "tl": 310, "tr": 311, "tl2": 312, "tr2": 313,
           "select": 314, "start": 315, "mode": 316, "thumbl": 317, "thumbr": 318, "z": 309,
           "up": 544, "down": 545, "left": 546, "right": 547}
AXES = {"lx": 0, "ly": 1, "rx": 3, "ry": 4}


def main():
    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS)
    for code in BUTTONS.values():
        fcntl.ioctl(fd, UI_SET_KEYBIT, code)
    for code in AXES.values():
        fcntl.ioctl(fd, UI_SET_ABSBIT, code)
        # struct uinput_abs_setup: код, выравнивание, input_absinfo (значение, мин, макс, fuzz, flat, разрешение)
        fcntl.ioctl(fd, UI_ABS_SETUP, struct.pack("HH6i", code, 0, 0, -32767, 32767, 250, 500, 0))
    fcntl.ioctl(fd, UI_DEV_SETUP, struct.pack("HHHH80sI", 0x05, 0x057E, 0x2009, 1, b"naomi-virtual-gamepad", 0))
    fcntl.ioctl(fd, UI_DEV_CREATE)

    def emit(kind, code, value):
        os.write(fd, struct.pack("qqHHi", 0, 0, kind, code, value) + struct.pack("qqHHi", 0, 0, EV_SYN, 0, 0))

    time.sleep(1.0)   # игра находит устройство по событию каталога /dev/input
    try:
        for arg in sys.argv[1:]:
            name, _, value = arg.partition(":")
            if name == "wait":
                time.sleep(float(value))
            elif name in AXES:
                emit(EV_ABS, AXES[name], int(value))
            elif name in BUTTONS:
                emit(EV_KEY, BUTTONS[name], 1)
                time.sleep(float(value) if value else 0.1)
                emit(EV_KEY, BUTTONS[name], 0)
                time.sleep(0.1)
            else:
                raise SystemExit(f"не знаю команду {arg!r}")
        time.sleep(0.5)
    finally:
        fcntl.ioctl(fd, UI_DEV_DESTROY)
        os.close(fd)


if __name__ == "__main__":
    main()
