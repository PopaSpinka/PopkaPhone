# НаомиФон — клавиши и мышь столу с самого телефона тем же путём, что от приставки (org.naomi.DeskTest, docs/STREAM.md,
# «Проверочная мышь»): текст — знаками без клавиши (так приставка шлёт буквы), особые — клавишей Qt с модификаторами.
# Печатает в то, что в фокусе стола: перед печатью — снимок (tools/naomi shot desk). docs/TERMINAL.md, «Проверка».
#   python3 deskkeys.py 'text:echo привет' enter 'ctrl:c' 'key:Up' 'ctrlshift:t' 'pt:1,0.3,0.2' 'pt:0,0.3,0.2' 'wait:0.5'
import subprocess, sys, time
QT = {"enter": 0x01000004, "tab": 0x01000001, "backtab": 0x01000002, "esc": 0x01000000, "backspace": 0x01000003,
      "Up": 0x01000013, "Down": 0x01000015, "Left": 0x01000012, "Right": 0x01000014, "PgUp": 0x01000016,
      "PgDown": 0x01000017, "Home": 0x01000010, "End": 0x01000011, "F1": 0x01000030}
SHIFT, CTRL, ALT = 0x02000000, 0x04000000, 0x08000000
calls = []
def key(down, code, mods, text):
    t = text.replace("\\", "\\\\").replace("'", "\\'")
    calls.append(f"gdbus call --session --dest org.naomi.DeskTest --object-path /input --method org.naomi.DeskTest.Key "
                 f"{'true' if down else 'false'} {code} {mods} '{t}' >/dev/null")
for a in sys.argv[1:]:
    kind, _, v = a.partition(":")
    if kind == "text":
        for ch in v:
            key(True, 0, 0, ch); key(False, 0, 0, ch)
    elif kind == "enter":
        key(True, QT["enter"], 0, "\r"); key(False, QT["enter"], 0, "\r")
    elif kind == "key":
        key(True, QT[v], 0, ""); key(False, QT[v], 0, "")
    elif kind in ("ctrl", "ctrlshift", "alt", "shift"):
        mods = {"ctrl": CTRL, "ctrlshift": CTRL | SHIFT, "alt": ALT, "shift": SHIFT}[kind]
        code = QT[v] if v in QT else ord(v.upper())
        key(True, code, mods, ""); key(False, code, mods, "")
    elif kind == "pt":   # мышь: кнопки (1 левая, 2 правая, 4 средняя), x, y — доли стола
        b, x, y = v.split(",")
        calls.append(f"gdbus call --session --dest org.naomi.DeskTest --object-path /input --method "
                     f"org.naomi.DeskTest.Pointer {b} {x} {y} >/dev/null")
    elif kind == "wait":
        calls.append(f"sleep {v}")
subprocess.run(" ; ".join(calls), shell=True)
