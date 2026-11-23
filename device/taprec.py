#!/usr/bin/env python3
"""НаомиФон — запись касаний для замера клавиатуры (docs/KEYBOARD.md, «Попадание»).

  python3 taprec.py ФАЙЛ [секунды]

Каждый контакт sec_touchscreen — строка JSON {"down", "up", "pts": [[t, x, y], …]} от касания до отпускания.
Читает устройство параллельно с оболочкой (без захвата). Стоп — по времени (по умолчанию 20 мин) или файлом
/tmp/taprec.stop. Координаты — пиксели панели. Ядро не повторяет координату, совпавшую с прошлой в том же
слоте, — поэтому последние X и Y помнятся по слотам.
"""
import glob
import json
import os
import select
import struct
import sys
import time

EVENT = struct.Struct("llHHi")
EV_SYN, EV_ABS = 0, 3
SLOT, POS_X, POS_Y, TRACKING_ID = 0x2F, 0x35, 0x36, 0x39
STOP = "/tmp/taprec.stop"

dev = next("/dev/input/" + p.split("/")[4] for p in sorted(glob.glob("/sys/class/input/event*/device/name"))
           if open(p).read().strip() == "sec_touchscreen")
out = open(sys.argv[1], "w", buffering=1)
until = time.time() + (float(sys.argv[2]) if len(sys.argv) > 2 else 1200)
fd = os.open(dev, os.O_RDONLY)
slot = 0
pos = {}        # слот → [x, y]: последнее известное
contact = {}    # слот → идущий контакт
changed = set()
while time.time() < until and not os.path.exists(STOP):
    if not select.select([fd], [], [], 0.5)[0]:
        continue
    data = os.read(fd, EVENT.size * 128)
    for i in range(0, len(data), EVENT.size):
        sec, usec, typ, code, value = EVENT.unpack_from(data, i)
        t = round(sec + usec / 1e6, 4)
        if typ == EV_ABS:
            if code == SLOT:
                slot = value
            elif code == TRACKING_ID:
                if value < 0:
                    c = contact.pop(slot, None)
                    if c and c["pts"]:
                        c["up"] = t
                        out.write(json.dumps(c) + "\n")
                else:
                    contact[slot] = {"down": t, "pts": []}
                    changed.add(slot)
            elif code in (POS_X, POS_Y):
                pos.setdefault(slot, [None, None])[code - POS_X] = value
                changed.add(slot)
        elif typ == EV_SYN and code == 0:
            for s in changed:
                if s in contact and None not in pos.get(s, [None]):
                    contact[s]["pts"].append([t] + pos[s])
            changed.clear()
