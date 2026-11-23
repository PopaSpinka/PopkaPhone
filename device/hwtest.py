#!/usr/bin/env python3
"""НаомиФон — живые тесты железа на телефоне.

Каждый тест включает устройство по-настоящему и проверяет результат, а не только наличие службы.
Итог — JSON в stdout; ход — строки в stderr. Запуск с Mac: `tools/naomi test [тест …]`.
Нужны: пользователь phablet, sudo без пароля (доступ разработчика), QML-файлы в /tmp/naomi-qml.

Статусы: ok — работает; partial — работает не полностью; fail — не работает;
manual — сделано, результат должен подтвердить человек (звук, свет); skip — нельзя проверить сейчас.

Внимание: в этом порте одновременные запись и воспроизведение звука вешают PulseAudio
(«q overrun»), поэтому звуковые тесты идут строго по очереди.
"""
import array
import json
import math
import os
import re
import subprocess
import sys
import time

UID = os.getuid()
RUNTIME = f"/run/user/{UID}"
QML_DIR = "/tmp/naomi-qml"
QMLSCENE = "/usr/lib/qt5/bin/qmlscene"
RINGTONE = "/usr/share/sounds/lomiri/ringtones/Ubuntu.ogg"
USER_ENV = {"XDG_RUNTIME_DIR": RUNTIME}
# окружение клиента Lomiri (Mir) — нужно камере, ей нужен видоискатель на экране
MIR_ENV = {"XDG_RUNTIME_DIR": RUNTIME, "MIR_SOCKET": f"{RUNTIME}/mir_socket",
           "QT_QPA_PLATFORM": "ubuntumirclient", "QT_OPENGL_VARIANT": "gles", "GRID_UNIT_PX": "21"}


def log(msg):
    print(f"  · {msg}", file=sys.stderr, flush=True)


def sh(cmd, timeout=30, env=None, check=False):
    e = dict(os.environ)
    e.update(env or {})
    try:
        p = subprocess.run(cmd, shell=isinstance(cmd, str), capture_output=True, text=True,
                           timeout=timeout, env=e)
    except subprocess.TimeoutExpired as ex:
        return 124, (ex.stdout or b"").decode() if isinstance(ex.stdout, bytes) else (ex.stdout or "")
    if check and p.returncode != 0:
        raise RuntimeError(f"{cmd}: {p.stderr.strip()[:300]}")
    return p.returncode, p.stdout + p.stderr


def qml_json(path, env, timeout):
    """Запустить QML и вернуть объект из строки «NAOMI_JSON {…}»."""
    code, out = sh([QMLSCENE, path], timeout=timeout, env=env)
    for line in out.splitlines():
        if "NAOMI_JSON " in line:
            return json.loads(line.split("NAOMI_JSON ", 1)[1])
    raise RuntimeError(f"QML не вернул результат (код {code}): {out.strip()[-300:]}")


def result(status, summary, **details):
    return {"status": status, "summary": summary, "details": details}


def dbus_iface(bus_name, path, iface):
    import dbus
    return dbus.Interface(dbus.SystemBus().get_object(bus_name, path), iface)


# ---------- звук ----------

def wav_rms_quarters(path):
    import wave
    w = wave.open(path)
    fr, ch, n = w.getframerate(), w.getnchannels(), w.getnframes()
    a = array.array("h", w.readframes(n))
    step = fr * ch // 4
    return round(n / fr, 2), [round(math.sqrt(sum(x * x for x in a[i:i + step]) / max(len(a[i:i + step]), 1)))
                              for i in range(0, len(a), step)]


def vibrate(ms):
    import dbus
    dbus_iface("com.lomiri.hfd", "/com/lomiri/hfd", "com.lomiri.hfd.Vibrator").vibrate(dbus.Int32(ms))


def t_mic():
    """Микрофон и вибромотор: запись 4 с, на 1,5 с включается вибрация — её гул виден в записи."""
    if sh("systemctl --user is-active pulseaudio", env=USER_ENV)[1].strip() != "active":
        return result("fail", "PulseAudio не работает")
    path = "/tmp/naomi-mic.wav"
    rec = subprocess.Popen(["timeout", "4", "parecord", "--device=source.primary_input",
                            "--file-format=wav", path], env={**os.environ, **USER_ENV},
                           stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    time.sleep(1.5)
    vibrate(1200)
    rec.wait(timeout=10)
    secs, rms = wav_rms_quarters(path)
    if secs < 3:
        return result("fail", f"запись оборвалась: {secs} с", rms=rms)
    before = sorted(rms[1:6])[len(rms[1:6]) // 2]
    during = max(rms[6:11])
    ratio = round(during / max(before, 1), 1)
    st = "ok" if ratio >= 1.8 else "partial"
    return result(st, f"записано {secs} с; во время вибрации громче в {ratio} раза",
                  rms_quarter_s=rms, ratio=ratio)


def t_speaker():
    """Динамик: мелодия 2,5 с. Слышно ли — подтверждает человек (запись одновременно нельзя)."""
    code, out = sh(f"timeout 2.5 paplay '{RINGTONE}'", env=USER_ENV, timeout=10)
    if code not in (0, 124):
        return result("fail", f"paplay: {out.strip()[:200]}")
    return result("manual", "играла мелодия 2,5 с — было слышно?")


def t_vibration():
    """Вибрация 600 мс через hfd. Объективно вибромотор проверяет тест mic."""
    vibrate(600)
    return result("manual", "вибрация 0,6 с — почувствовал?")


def t_torch():
    """Фонарик: led:torch_0 + led:switch_0 на 2,5 с."""
    L = "/sys/class/leds"
    code, out = sh(f"sudo -n sh -c 'echo 100 > {L}/led:torch_0/brightness; echo 1 > {L}/led:switch_0/brightness'")
    if code != 0:
        return result("fail", f"не включить: {out.strip()[:200]}")
    on = open(f"{L}/led:torch_0/brightness").read().strip()
    time.sleep(2.5)
    sh(f"sudo -n sh -c 'echo 0 > {L}/led:torch_0/brightness; echo 0 > {L}/led:switch_0/brightness'")
    return result("manual", "фонарик горел 2,5 с — видел?", brightness_when_on=on)


# ---------- датчики и камера ----------

SENSORS = ("accelerometer", "gyroscope", "magnetometer", "compass", "rotation", "light",
           "proximity", "pressure", "orientation")


def t_sensors():
    """Все датчики через QtSensors (sensorfw) 4 с. Освещённость и приближение шлют данные только при изменении."""
    data = qml_json(f"{QML_DIR}/sensors.qml",
                    {**USER_ENV, "QT_QPA_PLATFORM": "offscreen", "QT_QUICK_BACKEND": "software"}, 20)
    got = [s for s in SENSORS if data.get(s, {}).get("count", 0) > 0]
    silent = [s for s in SENSORS if s not in got]
    st = "ok" if not silent else ("partial" if len(got) >= 6 else "fail")
    note = f"отвечают {len(got)} из {len(SENSORS)}" + (f"; молчат: {', '.join(silent)}" if silent else "")
    return result(st, note, readings={k: {"count": v["count"], "last": v["last"]} for k, v in data.items()})


def jpeg_size(path):
    with open(path, "rb") as f:
        b = f.read()
    i = 2
    while i < len(b) - 9:
        if b[i] != 0xFF:
            i += 1
            continue
        marker, seg = b[i + 1], int.from_bytes(b[i + 2:i + 4], "big")
        if marker in (0xC0, 0xC1, 0xC2):
            return int.from_bytes(b[i + 7:i + 9], "big"), int.from_bytes(b[i + 5:i + 7], "big")
        i += 2 + seg
    return None


def t_camera():
    """Снимок с каждой камеры через QtMultimedia; окно видоискателя на секунды появляется на экране."""
    shots, errors = {}, {}
    template = open(f"{QML_DIR}/camera.qml").read()
    for idx in (0, 1):
        out = f"/tmp/naomi-cam{idx}.jpg"
        if os.path.exists(out):
            os.remove(out)
        qml = f"/tmp/naomi-cam{idx}.qml"
        with open(qml, "w") as f:
            f.write(template.replace("@INDEX@", str(idx)).replace("@OUT@", out))
        try:
            r = qml_json(qml, MIR_ENV, 40)
        except RuntimeError as e:
            errors[idx] = str(e)[:300]
            continue
        if r.get("saved") and os.path.exists(out):
            w_h = jpeg_size(out)
            shots[idx] = {"file": out, "bytes": os.path.getsize(out), "size": w_h,
                          "name": (r.get("cameras") or [{}] * 2)[idx].get("name")}
        else:
            errors[idx] = r.get("error", "нет снимка")
    if len(shots) == 2:
        st = "ok"
    elif shots:
        st = "partial"
    else:
        st = "skip" if all("timeout" in e or "Mir" in e for e in errors.values()) else "fail"
    desc = "; ".join(f"{v['name']}: {v['size'][0]}×{v['size'][1]}" if v["size"] else v["name"]
                     for v in shots.values())
    return result(st, desc or f"снимков нет: {errors}", shots=shots, errors=errors)


# ---------- связь ----------

def t_wifi():
    """Wi-Fi: подключение, частота, скорость канала, пинг в интернет."""
    _, conn = sh("nmcli -t -f GENERAL.CONNECTION dev show wlan0")
    _, link = sh("iw dev wlan0 link")
    _, ping = sh("ping -c 5 -q 1.1.1.1", timeout=15)
    freq = re.search(r"freq: ([\d.]+)", link)
    rx = re.search(r"rx bitrate: ([\d.]+ MBit/s)", link)
    sig = re.search(r"signal: (-?\d+) dBm", link)
    rtt = re.search(r"= [\d.]+/([\d.]+)/", ping)
    loss = re.search(r"(\d+)% packet loss", ping)
    ok = rtt and loss and loss.group(1) == "0"
    name = conn.split(":", 1)[-1].strip()
    return result("ok" if ok else "fail",
                  f"{name}, {float(freq.group(1)) / 1000 if freq else '?'} ГГц, {sig.group(1) if sig else '?'} дБм, "
                  f"приём {rx.group(1) if rx else '?'}, пинг {rtt.group(1) if rtt else '—'} мс",
                  link=link.strip().splitlines())


def t_bluetooth():
    """Bluetooth: поиск устройств 12 с."""
    sh("timeout 15 bluetoothctl --timeout 12 scan on", timeout=20)
    _, out = sh("bluetoothctl devices")
    n = len([l for l in out.splitlines() if l.startswith("Device ")])
    return result("ok" if n else "fail", f"найдено устройств: {n}", count=n)


def t_modem():
    """Модем: SIM, регистрация в сети, VoLTE, мобильные данные (без звонка)."""
    import dbus

    def props(iface):
        try:
            return {str(k): v for k, v in dbus_iface("org.ofono", "/ril_0", iface).GetProperties().items()}
        except dbus.DBusException as e:
            return {"error": str(e).splitlines()[0][:200]}

    m, sim = props("org.ofono.Modem"), props("org.ofono.SimManager")
    nr, ims = props("org.ofono.NetworkRegistration"), props("org.ofono.IpMultimediaSystem")
    cm, rs = props("org.ofono.ConnectionManager"), props("org.ofono.RadioSettings")
    try:
        cells = len(dbus_iface("org.ofono", "/ril_0", "org.nemomobile.ofono.CellInfo").GetCells())
    except dbus.DBusException:
        cells = None
    info = {
        "powered": bool(m.get("Powered")), "online": bool(m.get("Online")),
        "sim_present": bool(sim.get("Present")),
        "sim_operator": f"{sim.get('MobileCountryCode', '')}/{sim.get('MobileNetworkCode', '')}",
        "registration": str(nr.get("Status", "")), "technology": str(nr.get("Technology", "")),
        "operator": str(nr.get("Name", "")), "strength": int(nr.get("Strength", -1)) if "Strength" in nr else None,
        "cells_visible": cells, "volte_registered": bool(ims.get("Registered")),
        "data_attached": bool(cm.get("Attached")), "tech_preference": str(rs.get("TechnologyPreference", "")),
    }
    if not info["sim_present"]:
        return result("skip", "нет SIM-карты", **info)
    if info["registration"] in ("registered", "roaming"):
        st = "ok" if info["data_attached"] else "partial"
        return result(st, f"в сети {info['operator']} ({info['technology']}), VoLTE "
                          f"{'да' if info['volte_registered'] else 'нет'}, данные "
                          f"{'да' if info['data_attached'] else 'нет'}", **info)
    return result("fail", f"SIM {info['sim_operator']} есть, но в сеть не входит: {info['registration']}, "
                          f"вышек видно {cells}", **info)


def t_nfc():
    """NFC: адаптер nfcd есть и включён."""
    d = dbus_iface("org.sailfishos.nfc.daemon", "/", "org.sailfishos.nfc.Daemon")
    ads = [str(a) for a in d.GetAdapters()]
    if not ads:
        return result("fail", "адаптеров NFC нет")
    a = dbus_iface("org.sailfishos.nfc.daemon", ads[0], "org.sailfishos.nfc.Adapter")
    en, pw = bool(a.GetEnabled()), bool(a.GetPowered())
    return result("ok" if en else "partial", f"{ads[0]}: включён={en}, питание={pw} (метку не читали)",
                  adapters=ads)


def t_gps(seconds=60):
    """GNSS через службу геолокации: спутники и фикс за `seconds` с (в помещении фикса обычно нет)."""
    _, out = sh(f"sudo -n timeout {seconds} lomiri-location-serviced-cli --bus system --test",
                timeout=seconds + 15)
    blocks, cur = [], None
    for l in out.splitlines():
        if l.startswith("key snr"):
            if cur is not None:
                blocks.append(cur)
            cur = []
        elif cur is not None and re.match(r"^\d+ ", l):
            cur.append(l.split())
    if cur:
        blocks.append(cur)
    snr = [float(r[1]) for b in blocks for r in b]
    in_fix = max((sum(1 for r in b if r[4] == "1") for b in blocks), default=0)
    fix = in_fix > 0 or bool(re.search(r"latitude|Position", out, re.I))
    most = max((len(b) for b in blocks), default=0)
    st = "ok" if fix else ("partial" if blocks else "fail")
    return result(st, f"обновлений спутников {len(blocks)}, видно сразу до {most}, "
                      f"SNR {min(snr):.0f}–{max(snr):.0f} дБ, фикс {'есть' if fix else 'нет'}" if snr else
                  "данных о спутниках нет", updates=len(blocks), max_visible=most, fix=fix)


# ---------- экран ----------

def t_display():
    """Режимы панели и реальная частота vblank за 2 с (ftrace drm_vblank_event; в покое кадров может не быть)."""
    modes = open("/sys/class/drm/card0-DSI-1/modes").read().split()
    tr = "/sys/kernel/tracing"
    ev = f"{tr}/events/drm/drm_vblank_event/enable"
    sh(f"sudo -n sh -c 'echo > {tr}/trace; echo 1 > {ev}; echo 1 > {tr}/tracing_on'")
    time.sleep(2)
    _, trace = sh(f"sudo -n sh -c 'echo 0 > {ev}; cat {tr}/trace'")
    stamps = [float(m) for m in re.findall(r"\s([\d.]+): drm_vblank_event", trace)]
    hz = round((len(stamps) - 1) / (stamps[-1] - stamps[0]), 1) if len(stamps) > 2 else None
    has90 = any("x90x" in m for m in modes)
    return result("ok", f"режимы: {', '.join(modes)}; vblank за 2 с: "
                        + (f"{len(stamps)} событий ≈ {hz} Гц" if hz else "нет (экран в покое)"),
                  modes=modes, vblank_events=len(stamps), vblank_hz=hz, has_90hz_mode=has90)


TESTS = {
    "display": t_display, "sensors": t_sensors, "camera": t_camera, "mic": t_mic,
    "speaker": t_speaker, "vibration": t_vibration, "torch": t_torch, "wifi": t_wifi,
    "bluetooth": t_bluetooth, "modem": t_modem, "nfc": t_nfc, "gps": t_gps,
}


def main():
    names = sys.argv[1:] or list(TESTS)
    unknown = [n for n in names if n not in TESTS]
    if unknown:
        print(f"нет тестов: {unknown}; есть: {list(TESTS)}", file=sys.stderr)
        sys.exit(2)
    report = {"hwtest": 1, "time": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "results": {}}
    for n in names:
        print(f"» {n}: {TESTS[n].__doc__.strip().splitlines()[0]}", file=sys.stderr, flush=True)
        t = time.monotonic()
        try:
            r = TESTS[n]()
        except Exception as e:  # тест упал — это тоже результат
            r = result("fail", f"тест упал: {type(e).__name__}: {str(e)[:300]}")
        r["seconds"] = round(time.monotonic() - t, 1)
        report["results"][n] = r
        log(f"{r['status']}: {r['summary']}")
    json.dump(report, sys.stdout, ensure_ascii=False, indent=1)
    print()


if __name__ == "__main__":
    main()
