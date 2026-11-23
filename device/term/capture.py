# НаомиФон — «Терминал» (docs/TERMINAL.md): вывод настоящей программы в псевдотерминале 80 × 24 — для screen_test.
#   python3 capture.py 'top -d 1' top.out 2.5 && ./screen_test top.out
import os, pty, select, struct, sys, time, fcntl, termios

cmd, out, secs = sys.argv[1], sys.argv[2], float(sys.argv[3])
keys = sys.argv[4].encode().decode("unicode_escape").encode("latin1") if len(sys.argv) > 4 else b""
pid, fd = pty.fork()
if pid == 0:
    os.environ["TERM"] = "xterm-256color"
    os.environ["LANG"] = "C.UTF-8"
    os.execvp("sh", ["sh", "-c", cmd])
fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 24, 80, 0, 0))
data = b""
end = time.time() + secs
sent = False
while time.time() < end:
    r, _, _ = select.select([fd], [], [], 0.05)
    if r:
        try:
            data += os.read(fd, 65536)
        except OSError:
            break
    if keys and not sent and time.time() > end - secs / 2:
        os.write(fd, keys)
        sent = True
open(out, "wb").write(data)
try:
    os.kill(pid, 9)
except OSError:
    pass
