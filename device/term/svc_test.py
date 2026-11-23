# НаомиФон — проверка службы «Терминала» naomi-term (docs/TERMINAL.md): свой сокет и своя папка текста (/tmp/tt),
# оболочки — настоящие. На телефоне: python3 svc_test.py
import os, socket, struct, subprocess, sys, time

RT = "/tmp/tt"
BIN = os.path.expanduser("~/naomi/build/term/naomi-term")
LOGS = RT + "/logs"   # своя папка текста: настоящие вкладки не трогать
os.makedirs(RT, exist_ok=True)
env = dict(os.environ, XDG_RUNTIME_DIR=RT, NAOMI_TERM_DIR=LOGS)

def start():
    p = subprocess.Popen([BIN], env=env, stderr=open(RT + "/svc.log", "a"))
    for _ in range(50):
        try:
            s = socket.socket(socket.AF_UNIX); s.connect(RT + "/naomi-term.sock"); return p, s
        except OSError:
            time.sleep(0.05)
    raise SystemExit("служба не поднялась")

def send(s, t, i, payload=b""):
    if isinstance(payload, str): payload = payload.encode()
    s.sendall(t.encode() + struct.pack("<II", i, len(payload)) + payload)

buf = b""
def frames(s, secs):
    global buf
    out = []
    s.settimeout(0.05)
    end = time.time() + secs
    while time.time() < end:
        try:
            d = s.recv(65536)
            if not d: break
            buf += d
        except socket.timeout:
            pass
        while len(buf) >= 9:
            t = chr(buf[0]); i, n = struct.unpack("<II", buf[1:9])
            if len(buf) < 9 + n: break
            out.append((t, i, buf[9:9 + n])); buf = buf[9 + n:]
    return out

def text(fr, ids=None, types="DY"):
    return b"".join(p for t, i, p in fr if t in types and (ids is None or i == ids)).decode("utf-8", "replace")

ok = True
def check(name, cond):
    global ok
    print(("✓ " if cond else "✗ ") + name)
    ok &= bool(cond)

p, s = start()
send(s, "H", 0, "1 101 102")
send(s, "O", 101, "80 24 1 ~")
send(s, "O", 102, "80 24 1 /tmp")
fr = frames(s, 1.2)
check("привет в ответ", any(t == "H" for t, i, _ in fr))
check("обе вкладки — приглашение оболочки", "$" in text(fr, 101) and "$" in text(fr, 102))
places = {i: pl for t, i, pl in fr if t == "P"}
check("вкладка 102 в /tmp: " + repr(places.get(102)), places.get(102, b"").startswith(b"/tmp\n"))
check("вкладка 101 дома: " + repr(places.get(101)), places.get(101, b"").startswith(b"~\n"))
t0 = time.time()
send(s, "I", 101, "echo hello-$((40+2))\r")
fr = frames(s, 0.6)
check("ввод и вывод: hello-42", "hello-42" in text(fr, 101))
send(s, "I", 101, "cd /usr/share\r")
fr = frames(s, 0.8)
check("папка сменилась: " + repr([pl for t, i, pl in fr if t == "P"]), any(t == "P" and pl.startswith(b"/usr/share\n") for t, i, pl in fr))
send(s, "I", 101, "sleep 2\r")
fr = frames(s, 0.8)
check("программа видна: " + repr([pl for t, i, pl in fr if t == "P"]), any(t == "P" and pl.endswith(b"\nsleep") for t, i, pl in fr))
fr = frames(s, 2.0)
check("программа ушла", any(t == "P" and pl.endswith(b"\n") for t, i, pl in fr))
send(s, "R", 101, "100 30")
send(s, "I", 101, "stty size\r")
fr = frames(s, 0.6)
check("размер 30 100: " + repr(text(fr, 101)[-40:]), "30 100" in text(fr, 101))
send(s, "I", 101, "printf '\\033[6n'; read -s -t1 -d R x; echo; echo got:${#x}\r")
fr = frames(s, 1.5)
check("запрос положения курсора без ответа стола — пусто", "got:0" in text(fr, 101))
# текст на диск при закрытии терминала: Q — и связь сразу закрыта (так делает стол)
send(s, "Q", 0)
s.close()
time.sleep(0.4)
check("служба ушла по Q", p.poll() is not None)
check("текст вкладки 101 на диске", os.path.exists(LOGS + "/101.log") and b"hello-42" in open(LOGS + "/101.log", "rb").read())
# снова: восстановление
buf = b""
p, s = start()
send(s, "H", 0, "1 101")
send(s, "O", 101, "80 24 1 /tmp")
fr = frames(s, 1.0)
rep = text(fr, 101, "Y")
check("прошлый текст пришёл повтором (Y)", "hello-42" in rep)
check("черта «оболочка запущена заново»", "оболочка запущена заново" in rep)
check("вкладка 102 у стола не числится — её текст убран", not os.path.exists(LOGS + "/102.log"))
send(s, "I", 101, "pwd\r")
fr = frames(s, 0.6)
check("новая оболочка в /tmp", "/tmp" in text(fr, 101, "D"))
# поток: служба не захлёбывается
t0 = time.time()
send(s, "I", 101, "head -c 3000000 /dev/zero | tr '\\0' 'x' | fold -w 100; echo END-OF-FLOOD\r")
got = 0; done = False
while time.time() - t0 < 15 and not done:
    for t, i, pl in frames(s, 0.2):
        if t == "D": got += len(pl)
        if b"END-OF-FLOOD" in pl and b"echo" not in pl: done = True
check(f"3 МБ вывода дошли за {time.time()-t0:.2f} с ({got/1e6:.1f} МБ)", done)
# ограничение текста на диске и в памяти
send(s, "I", 101, "exit\r")
fr = frames(s, 1.0)
check("exit — кадр X", any(t == "X" and i == 101 for t, i, _ in fr))
time.sleep(0.2)
check("текст вкладки с диска убран", not os.path.exists(LOGS + "/101.log"))
s.close()
time.sleep(10.6)
check("ни вкладок, ни стола 10 с — служба ушла сама", p.poll() is not None)
print("ИТОГ:", "всё прошло" if ok else "есть ошибки")
