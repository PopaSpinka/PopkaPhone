#!/usr/bin/env python3
"""НаомиФон — проверка общего буфера обмена (docs/DESKTOP.md, «Копировать и вставить») на телефоне, без экрана:
свой композитор с корнем ShellCompositor (clip_test server, платформа offscreen) и программы Wayland (clip_test client)
со своим сокетом — стол пользователя не трогается.

  python3 run.py [путь к clip_test]      (по умолчанию ~/naomi/cliptest/clip_test; сборка — CMakeLists.txt)
"""
import json
import os
import selectors
import subprocess
import sys
import tempfile
import time

BIN = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser("~/naomi/cliptest/clip_test")
RUN = tempfile.mkdtemp(prefix="cliptest-")
os.chmod(RUN, 0o700)
SOCKET = "naomi-cliptest"
fails = 0


class Proc:
    def __init__(self, name, args, env):
        self.name = name
        self.p = subprocess.Popen([BIN] + args, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                  env={**os.environ, "XDG_RUNTIME_DIR": RUN, **env}, text=True, bufsize=1)
        self.sel = selectors.DefaultSelector()
        self.sel.register(self.p.stdout, selectors.EVENT_READ)

    def send(self, cmd, text=""):
        self.p.stdin.write(json.dumps([cmd, text], ensure_ascii=False) + "\n")
        self.p.stdin.flush()

    def wait(self, what, timeout=5.0):
        """следующая строка с этим словом: её текст; None — не дождались"""
        end = time.time() + timeout
        while time.time() < end:
            if not self.sel.select(end - time.time()):
                break
            line = self.p.stdout.readline()
            if not line:
                break
            got = json.loads(line)
            if got[0] == what:
                return got[1]
        return None

    def drain(self, wait=0.3):
        while self.sel.select(wait):
            if not self.p.stdout.readline():
                return

    def alive(self):
        return self.p.poll() is None


def check(name, ok, got=None):
    global fails
    fails += 0 if ok else 1
    print(("ок   " if ok else "НЕТ  ") + name + ("" if ok else f" — получено {got!r}"))


def client(name, **env):
    c = Proc(name, ["client"], {"QT_QPA_PLATFORM": "wayland", "WAYLAND_DISPLAY": SOCKET,
                                "QT_WAYLAND_SHELL_INTEGRATION": "xdg-shell", **env})
    check(f"{name}: программа запустилась", c.wait("ready") is not None)
    return c


def paste(c, want, name):
    c.drain(0.2)
    c.send("paste")
    got = c.wait("paste")
    check(name, got == want, got)


server = Proc("оболочка", ["server", SOCKET], {"QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"})
check("композитор ShellCompositor загрузился из QML", server.wait("ready") is not None)
a = client("A")
check("окно A", server.wait("surface") == "0")
b = client("B")
check("окно B", server.wait("surface") == "1")
c = client("C (без wl_data_device)", QT_WAYLAND_DISABLED_INTERFACES="wl_data_device_manager")
check("окно C", server.wait("surface") == "2")

server.send("focus", "0")
a.send("copy", "из браузера A")
got = server.wait("shell")
check("программа → оболочка: A скопировала, у оболочки то же", got == "из браузера A", got)

server.send("copy", "из Терминала: строка 1\nстрока 2 — ⌘ ёЖ")
server.drain()
paste(a, "из Терминала: строка 1\nстрока 2 — ⌘ ёЖ", "оболочка → программа в фокусе (A), русский и перевод строки")

server.send("focus", "1")
paste(b, "из Терминала: строка 1\nстрока 2 — ⌘ ёЖ", "фокус перешёл к B — у B буфер оболочки")

b.send("copy", "B")
got = server.wait("shell")
check("B скопировала — у оболочки «B»", got == "B", got)
server.send("focus", "0")
paste(a, "B", "фокус снова у A — у A «B»")

server.send("focus", "-1")
server.send("copy", "без фокуса")
server.drain()
server.send("focus", "0")
paste(a, "без фокуса", "оболочка скопировала без фокуса — A получила, когда фокус пришёл")

server.send("focus", "2")
server.send("copy", "при C")
server.drain()
check("фокус у программы без wl_data_device, оболочка копирует — композитор жив", server.alive())
server.send("focus", "0")
paste(a, "при C", "фокус ушёл к A — у A «при C»")

a.send("copy", "A снова")
got = server.wait("shell")
check("A скопировала снова — у оболочки «A снова»", got == "A снова", got)
server.send("focus", "1")
paste(b, "A снова", "фокус у B — у B «A снова»")

for p in (a, b, c, server):
    p.p.terminate()
print(f"\nпровалов: {fails}")
sys.exit(1 if fails else 0)
