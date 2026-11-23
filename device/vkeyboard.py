#!/usr/bin/env python3
"""НаомиФон — проверочная клавиатура Apple на самом телефоне (uinput): ядро видит её, как Magic Keyboard по Bluetooth
(004C:029C, без клавиш громкости), и оболочка берёт её тем же путём (hard_keyboard.cpp → key_layout.h →
StreamInput::key; docs/DESKTOP.md, «Клавиатура Bluetooth»). Буквы идут по раскладке стола (EN / RU) — в то, что в фокусе:
перед печатью — снимок (tools/naomi shot desk).

  sudo python3 vkeyboard.py meta+c wait:0.3 meta+shift+z ctrl+c enter code:46
Сочетание — через «+»: meta (⌘), shift, ctrl, alt и клавиша (буква, цифра, enter, esc, tab, space, backspace,
left/right/up/down, code:N — код evdev). Клавиатура появляется на 1 с раньше первой клавиши и уходит после последней.
"""
import fcntl
import os
import struct
import sys
import time

UI_SET_EVBIT, UI_SET_KEYBIT, UI_DEV_SETUP, UI_DEV_CREATE, UI_DEV_DESTROY = 0x40045564, 0x40045565, 0x405C5503, 0x5501, 0x5502
EV_SYN, EV_KEY = 0, 1
MODS = {"meta": 125, "shift": 42, "ctrl": 29, "alt": 56}
NAMED = {"enter": 28, "esc": 1, "tab": 15, "space": 57, "backspace": 14, "left": 105, "right": 106, "up": 103,
         "down": 108}
ROWS = {16: "qwertyuiop", 30: "asdfghjkl", 44: "zxcvbnm"}


def code_of(name):
    if name in NAMED:
        return NAMED[name]
    if name.startswith("code:"):
        return int(name[5:])
    if len(name) == 1 and name.isdigit():
        return 11 if name == "0" else 1 + int(name)
    for start, row in ROWS.items():
        if len(name) == 1 and name in row:
            return start + row.index(name)
    raise SystemExit(f"не знаю клавишу {name!r}")


def main():
    fd = os.open("/dev/uinput", os.O_WRONLY | os.O_NONBLOCK)
    fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY)
    for k in range(1, 256):
        if k not in (113, 114, 115):   # без громкости: так ядро описывает Magic Keyboard (hid-generic)
            fcntl.ioctl(fd, UI_SET_KEYBIT, k)
    fcntl.ioctl(fd, UI_DEV_SETUP, struct.pack("HHHH80sI", 0x05, 0x004C, 0x029C, 1, b"naomi-virtual-keyboard", 0))
    fcntl.ioctl(fd, UI_DEV_CREATE)

    def emit(code, value):
        os.write(fd, struct.pack("qqHHi", 0, 0, EV_KEY, code, value) + struct.pack("qqHHi", 0, 0, EV_SYN, 0, 0))

    time.sleep(1.0)   # оболочка находит устройство по событию каталога /dev/input
    try:
        for arg in sys.argv[1:]:
            if arg.startswith("wait:"):
                time.sleep(float(arg[5:]))
                continue
            parts = arg.split("+")
            mods = [MODS[p] for p in parts[:-1]]
            key = code_of(parts[-1])
            for m in mods:
                emit(m, 1)
            emit(key, 1)
            time.sleep(0.03)
            emit(key, 0)
            for m in reversed(mods):
                emit(m, 0)
            time.sleep(0.12)
        time.sleep(0.3)
    finally:
        fcntl.ioctl(fd, UI_DEV_DESTROY)
        os.close(fd)


if __name__ == "__main__":
    main()
