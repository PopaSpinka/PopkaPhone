# НаомиФон — зритель стола без монитора (docs/STREAM.md, «Протокол»): подключается к трансляции на самом телефоне
# (127.0.0.1:8478), просит рабочий стол и выбрасывает картинку. Нужен для ночных проверок: стол рисуется, окна
# программ живут, `tools/naomi shot desk` и проверочная мышь работают — а монитор приставки не включается.
# Приставку на это время замораживают (на Pi: sudo systemctl kill -s STOP naomi-dock), иначе она войдёт сама, как
# только трансляция включится; потом — kill -s CONT.
#   python3 deskwatch.py [ширина высота масштаб×1000]     # по умолчанию 1600 900 1500 — как у приставки
# Трансляция выключена (стол ушёл без дела) — просит включить (Wake), как клавиша у приставки. Себя зрителем с
# «экраном без дока» не называет — стол без дела от него не уходит. Остановить — SIGTERM.
import json, os, socket, struct, sys, time

HELLO, VIDEO, SWITCH, PING, VIEWER, MODE, JOIN, DESK_SIZE, WANT, WAKE = 1, 2, 9, 10, 16, 17, 19, 22, 23, 30
width, height, scale = (int(a) for a in sys.argv[1:4]) if len(sys.argv) >= 4 else (1600, 900, 1500)
ticket = int.from_bytes(os.urandom(8), "little") | 1


def send(sock, kind, body=b""):
    sock.sendall(struct.pack("<IB", 1 + len(body), kind) + body)


last_ping = 0.0


def read(sock, count):   # и пинг раз в секунду: молчащий путь телефон закрывает
    global last_ping
    data = b""
    while len(data) < count:
        now = time.time()
        if now - last_ping > 1:
            send(sock, PING, struct.pack("<Q", int(now * 1e6)))
            last_ping = now
        try:
            chunk = sock.recv(count - len(data))
        except socket.timeout:
            continue
        if not chunk:
            raise ConnectionError("телефон закрыл путь")
        data += chunk
    return data


def session():
    sock = socket.create_connection(("127.0.0.1", 8478), timeout=10)
    sock.settimeout(1)
    send(sock, JOIN, struct.pack("<QB", ticket, 1))
    send(sock, VIEWER, b"watch")
    send(sock, WANT, json.dumps({"codec": "h264", "fps": 60}).encode())
    send(sock, DESK_SIZE, struct.pack("<HHH", height, width, scale))
    send(sock, MODE, b"\x01")
    frames, last_note = 0, time.time()
    while True:
        size, kind = struct.unpack("<IB", read(sock, 5))
        body = read(sock, size - 1)
        if kind == SWITCH:
            if body[:1] == b"\x00":
                print("трансляция выключена — прошу включить", flush=True)
                send(sock, WAKE)
            else:
                print("трансляцию включили — подключаюсь заново", flush=True)
                sock.close()
                return
        elif kind == HELLO:
            print("стол:", body.decode(errors="replace")[:160], flush=True)
        elif kind == VIDEO:
            frames += 1
        now = time.time()
        if now - last_note > 60:
            print(f"кадров за минуту: {frames}", flush=True)
            frames, last_note = 0, now


while True:
    try:
        session()
    except (OSError, ConnectionError, struct.error) as e:
        print("путь оборвался:", e, flush=True)
        time.sleep(2)
