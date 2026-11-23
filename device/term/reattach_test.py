# НаомиФон — «Терминал» (docs/TERMINAL.md): стол ушёл (перезапуск оболочки) и вернулся — программа во вкладке жива,
# текст и экран приходят повтором. На телефоне: python3 reattach_test.py
import os, socket, struct, subprocess, time
RT = "/tmp/tt2"; os.makedirs(RT, exist_ok=True)
env = dict(os.environ, XDG_RUNTIME_DIR=RT, NAOMI_TERM_DIR=RT + "/logs")
p = subprocess.Popen([os.path.expanduser("~/naomi/build/term/naomi-term")], env=env, stderr=subprocess.DEVNULL)
def conn():
    for _ in range(50):
        try:
            s = socket.socket(socket.AF_UNIX); s.connect(RT + "/naomi-term.sock"); return s
        except OSError: time.sleep(0.05)
def send(s, t, i, pl=b""):
    pl = pl.encode() if isinstance(pl, str) else pl
    s.sendall(t.encode() + struct.pack("<II", i, len(pl)) + pl)
def frames(s, secs):
    buf, out, end = b"", [], time.time() + secs
    s.settimeout(0.05)
    while time.time() < end:
        try: buf += s.recv(65536)
        except socket.timeout: pass
        while len(buf) >= 9:
            t = chr(buf[0]); i, n = struct.unpack("<II", buf[1:9])
            if len(buf) < 9 + n: break
            out.append((t, i, buf[9:9 + n])); buf = buf[9 + n:]
    return out
s = conn()
send(s, "H", 0, "1 7"); send(s, "O", 7, "80 24 1 ~"); frames(s, 0.8)
send(s, "I", 7, "echo метка-до; vi /etc/hostname\r"); f = frames(s, 1.2)
print("vi запущен:", any(t == "P" and pl.endswith(b"\nvi") for t, i, pl in f) or any(t == "P" and b"vi" in pl for t, i, pl in f))
s.close(); time.sleep(1.0)                      # стол ушёл
alive = subprocess.run(["pgrep", "-f", "vi /etc/hostname"], capture_output=True).stdout.strip()
print("vi жив без стола:", bool(alive))
s = conn()
send(s, "H", 0, "1 7"); send(s, "O", 7, "80 24 1 ~"); f = frames(s, 0.8)
rep = b"".join(pl for t, i, pl in f if t == "Y")
print("повтор пришёл, с меткой:", b"\xd0\xbc\xd0\xb5\xd1\x82\xd0\xba\xd0\xb0-\xd0\xb4\xd0\xbe" in rep, "и экраном vi:", b"hostname" in rep and b"\x1b[?1049h" in rep)
print("без черты «заново» (оболочка та же):", "заново".encode() not in rep)
print("программа — vi:", [pl.decode() for t, i, pl in f if t == "P"])
send(s, "I", 7, "\x1b:q!\r"); frames(s, 0.5); send(s, "Q", 0); s.close(); time.sleep(0.3)
print("служба ушла:", p.poll() is not None)
