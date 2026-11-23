#!/usr/bin/env python3
"""НаомиФон — опись телефона: что есть, что запущено, какие значения сейчас.

Только читает: ничего не включает и не меняет. Работает от обычного пользователя
(phablet), без sudo; что без прав не прочитать — отмечено ошибкой в своём разделе.
Печатает JSON в stdout. Запуск с Mac: `tools/naomi probe`.
"""
import glob
import json
import os
import re
import subprocess
import sys
import time


def read(path, default=None):
    try:
        with open(path, "rb") as f:
            return f.read().decode("utf-8", "replace").strip()
    except OSError:
        return default


def num(path):
    v = read(path)
    try:
        return int(v)
    except (TypeError, ValueError):
        return v


def run(cmd, timeout=8, env=None):
    """Вывод команды или None, если её нет / упала."""
    try:
        e = dict(os.environ)
        if env:
            e.update(env)
        p = subprocess.run(cmd, shell=isinstance(cmd, str), capture_output=True,
                           text=True, timeout=timeout, env=e)
        out = p.stdout.strip()
        if p.returncode != 0 and not out:
            return None
        return out
    except (OSError, subprocess.TimeoutExpired):
        return None


def lines(text):
    return [l for l in (text or "").splitlines() if l.strip()]


def basename_sorted(pattern):
    return sorted(glob.glob(pattern), key=lambda p: [int(t) if t.isdigit() else t
                                                      for t in re.split(r"(\d+)", p)])


# ---------- разделы ----------

def system():
    mem = {}
    for l in lines(read("/proc/meminfo")):
        k, v = l.split(":", 1)
        if k in ("MemTotal", "MemAvailable", "MemFree", "Cached", "SwapTotal", "SwapFree",
                 "Shmem", "Buffers"):
            mem[k] = int(v.split()[0])
    osr = dict(l.split("=", 1) for l in lines(read("/etc/os-release")) if "=" in l)
    return {
        "uname": run(["uname", "-a"]),
        "os": osr.get("PRETTY_NAME", "").strip('"'),
        "uptime_s": float((read("/proc/uptime") or "0").split()[0]),
        "loadavg": read("/proc/loadavg"),
        "cmdline": read("/proc/cmdline"),
        "meminfo_kb": mem,
        "root_readonly": "ro" in (run("findmnt -no OPTIONS /") or "").split(","),
        "df": lines(run(["df", "-h", "/", "/userdata", "/home", "/android/data"])),
        "zram": [read(p) for p in glob.glob("/sys/block/zram*/disksize")],
        "build_fingerprint": run(["getprop", "ro.build.fingerprint"]),
    }


def cpu():
    policies = []
    for p in basename_sorted("/sys/devices/system/cpu/cpufreq/policy*"):
        freqs = read(p + "/scaling_available_frequencies") or ""
        policies.append({
            "policy": os.path.basename(p),
            "cpus": read(p + "/related_cpus"),
            "governor": read(p + "/scaling_governor"),
            "governors": read(p + "/scaling_available_governors"),
            "cur_khz": num(p + "/scaling_cur_freq"),
            "min_khz": num(p + "/scaling_min_freq"),
            "max_khz": num(p + "/scaling_max_freq"),
            "hw_max_khz": num(p + "/cpuinfo_max_freq"),
            "freqs_khz": [int(x) for x in freqs.split()],
        })
    idle = []
    for s in basename_sorted("/sys/devices/system/cpu/cpu0/cpuidle/state*"):
        idle.append({"name": read(s + "/name"), "latency_us": num(s + "/latency"),
                     "usage": num(s + "/usage"), "time_us": num(s + "/time")})
    online = read("/sys/devices/system/cpu/online")
    model = sorted({l.split(":", 1)[1].strip() for l in lines(read("/proc/cpuinfo"))
                    if l.startswith(("CPU part", "Hardware"))})
    return {"online": online, "parts": model, "policies": policies, "cpu0_idle_states": idle}


def gpu():
    k = "/sys/class/kgsl/kgsl-3d0"
    if not os.path.isdir(k):
        return {"error": "нет /sys/class/kgsl/kgsl-3d0"}
    d = {name: read(f"{k}/{name}") for name in (
        "gpu_model", "gpuclk", "max_gpuclk", "min_clock_mhz", "max_clock_mhz",
        "gpu_available_frequencies", "gpubusy", "gpu_busy_percentage", "temp",
        "default_pwrlevel", "thermal_pwrlevel", "num_pwrlevels", "idle_timer",
        "force_clk_on", "force_bus_on", "force_rail_on", "bus_split")}
    dev = glob.glob(k + "/devfreq") or glob.glob("/sys/class/devfreq/*kgsl*")
    if dev:
        dv = os.path.realpath(dev[0])
        d["devfreq"] = {n: read(f"{dv}/{n}") for n in (
            "governor", "available_governors", "cur_freq", "min_freq", "max_freq",
            "available_frequencies")}
    return d


def thermal():
    zones = []
    for z in basename_sorted("/sys/class/thermal/thermal_zone*"):
        zones.append({"zone": os.path.basename(z), "type": read(z + "/type"),
                      "temp": num(z + "/temp"), "mode": read(z + "/mode")})
    cooling = []
    for c in basename_sorted("/sys/class/thermal/cooling_device*"):
        cooling.append({"dev": os.path.basename(c), "type": read(c + "/type"),
                        "cur": num(c + "/cur_state"), "max": num(c + "/max_state")})
    return {"zones": zones, "cooling": cooling}


def power():
    out = {}
    for s in sorted(glob.glob("/sys/class/power_supply/*")):
        props = {}
        for l in lines(read(s + "/uevent")):
            if "=" in l:
                k, v = l.split("=", 1)
                props[k.replace("POWER_SUPPLY_", "").lower()] = v
        out[os.path.basename(s)] = props
    wl = read("/sys/power/wake_lock")
    if wl is None:  # только для root; с sudo без пароля (доступ разработчика) — читаем так
        wl = run(["sudo", "-n", "cat", "/sys/power/wake_lock"])
    wakeups = (wl or "").split() if wl is not None else ["(нет прав)"]
    return {"supplies": out, "mem_sleep": read("/sys/power/mem_sleep"),
            "state": read("/sys/power/state"), "wake_lock": wakeups,
            "autosleep": read("/sys/power/autosleep"),
            "suspend_stats": {os.path.basename(p): read(p) for p in
                              glob.glob("/sys/power/suspend_stats/*") if os.path.isfile(p)}}


def display():
    drm = []
    for c in sorted(glob.glob("/sys/class/drm/card*-*")):
        drm.append({"connector": os.path.basename(c), "status": read(c + "/status"),
                    "enabled": read(c + "/enabled"), "dpms": read(c + "/dpms"),
                    "modes": lines(read(c + "/modes"))})
    bl = []
    for b in sorted(glob.glob("/sys/class/backlight/*")):
        bl.append({"name": os.path.basename(b), "brightness": num(b + "/brightness"),
                   "max": num(b + "/max_brightness"), "actual": num(b + "/actual_brightness")})
    props = {}
    for l in lines(run("getprop")):
        m = re.match(r"\[(.+?)\]: \[(.*)\]", l)
        if m and re.search(r"display|hwc|composer|refresh|fps|surface_flinger|lcd|panel", m.group(1)):
            props[m.group(1)] = m.group(2)
    return {"drm": drm, "backlight": bl, "android_props": props,
            "fb0": {"virtual_size": read("/sys/class/graphics/fb0/virtual_size"),
                    "modes": read("/sys/class/graphics/fb0/modes")}}


def input_devices():
    devs, cur = [], {}
    for l in (read("/proc/bus/input/devices") or "").splitlines():
        if not l.strip():
            if cur:
                devs.append(cur)
            cur = {}
        elif l.startswith("N: Name="):
            cur["name"] = l.split("=", 1)[1].strip('"')
        elif l.startswith("H: Handlers="):
            cur["handlers"] = l.split("=", 1)[1].strip()
    if cur:
        devs.append(cur)
    return devs


def android():
    svc = {}
    for l in lines(run("getprop")):
        m = re.match(r"\[init\.svc\.(.+?)\]: \[(.*)\]", l)
        if m:
            svc[m.group(1)] = m.group(2)
    keep = ("ro.product.model", "ro.build.version.release", "ro.build.id", "ro.hardware",
            "ro.board.platform", "ro.vendor.build.fingerprint", "ro.boot.slot_suffix",
            "ro.bootloader", "ro.boot.hardware.sku", "persist.sys.timezone")
    props = {k: run(["getprop", k]) for k in keep}
    lxc = run("ls /var/lib/lxc 2>/dev/null")
    return {"props": props, "services_running": sorted(k for k, v in svc.items() if v == "running"),
            "services_other": {k: v for k, v in svc.items() if v != "running"},
            "lxc": lxc}


def services():
    def units(args):
        return lines(run(["systemctl"] + args + ["--no-legend", "--plain", "--no-pager"]))
    user_env = {"XDG_RUNTIME_DIR": f"/run/user/{os.getuid()}"}
    return {
        "running": [l.split()[0] for l in units(["list-units", "--type=service", "--state=running"])],
        "failed": units(["list-units", "--state=failed"]),
        "user_running": [l.split()[0] for l in lines(run(
            "systemctl --user list-units --type=service --state=running --no-legend --plain --no-pager",
            env=user_env))],
        "user_failed": lines(run("systemctl --user list-units --state=failed --no-legend --plain --no-pager",
                                 env=user_env)),
        "boot_time": run(["systemd-analyze"]),
        "boot_blame_top": lines(run("systemd-analyze blame --no-pager 2>/dev/null | head -15")),
    }


def network():
    return {
        "ip": lines(run(["ip", "-br", "addr"])),
        "nm_devices": lines(run(["nmcli", "-t", "-f", "DEVICE,TYPE,STATE,CONNECTION", "dev"])),
        "nm_radio": run(["nmcli", "-t", "radio"]),
        "rfkill": [{"name": read(r + "/name"), "type": read(r + "/type"),
                    "soft": read(r + "/soft"), "hard": read(r + "/hard")}
                   for r in sorted(glob.glob("/sys/class/rfkill/rfkill*"))],
        "usb_gadget": {"functions": sorted(os.listdir(g + "/functions"))
                       if os.path.isdir(g + "/functions") else None,
                       "udc": read(g + "/UDC")}
        if (g := (glob.glob("/sys/kernel/config/usb_gadget/*") or [""])[0]) else None,
    }


def bluetooth():
    return {"hci": sorted(os.path.basename(p) for p in glob.glob("/sys/class/bluetooth/hci*")),
            "show": lines(run("timeout 5 bluetoothctl show 2>/dev/null"))}


def dbus_call(dest, path, iface_method, bus="--system"):
    return run(["dbus-send", bus, "--print-reply", f"--dest={dest}", path, iface_method])


def modem():
    out = dbus_call("org.ofono", "/", "org.ofono.Manager.GetModems")
    if out is None:
        return {"error": "oFono не ответил"}
    paths = re.findall(r'object path "([^"]+)"', out)
    strings = re.findall(r'string "([^"]*)"', out)
    keys = [s for s in strings]
    return {"modems": paths, "raw_keys": keys[:200]}


def audio():
    env = {"XDG_RUNTIME_DIR": f"/run/user/{os.getuid()}"}
    return {"pactl_info": lines(run(["pactl", "info"], env=env)),
            "sinks": lines(run(["pactl", "list", "short", "sinks"], env=env)),
            "sources": lines(run(["pactl", "list", "short", "sources"], env=env)),
            "cards": lines(run(["pactl", "list", "short", "cards"], env=env)),
            "alsa_cards": read("/proc/asound/cards")}


def sensors():
    # sensorfw: список через D-Bus; плюс сырые IIO, если есть.
    sfw = dbus_call("com.nokia.SensorService", "/SensorManager",
                    "local.SensorManager.availableSensorPlugins")
    iio = [{"dev": os.path.basename(d), "name": read(d + "/name")}
           for d in sorted(glob.glob("/sys/bus/iio/devices/iio:device*"))]
    return {"sensorfw_plugins": re.findall(r'string "([^"]*)"', sfw or ""),
            "sensorfw_raw": None if sfw else "sensorfw не ответил",
            "iio": iio,
            "hal_service": run("getprop | grep -i sensor | head -10")}


def media():
    return {"gst_droid": run("gst-inspect-1.0 2>/dev/null | grep -i droid"),
            "camera_props": run("getprop | grep -iE 'camera' | head -15"),
            "video_devices": sorted(glob.glob("/dev/video*"))}


def processes(top=25):
    procs = []
    clk = os.sysconf("SC_CLK_TCK")
    up = float((read("/proc/uptime") or "0").split()[0])
    for p in glob.glob("/proc/[0-9]*"):
        stat = read(p + "/stat")
        status = read(p + "/status")
        if not stat or not status:
            continue
        rss = re.search(r"VmRSS:\s+(\d+)", status)
        fields = stat[stat.rfind(")") + 2:].split()
        cpu_s = (int(fields[11]) + int(fields[12])) / clk
        start = int(fields[19]) / clk
        life = max(up - start, 1e-3)
        procs.append({"pid": int(os.path.basename(p)),
                      "name": stat[stat.find("(") + 1:stat.rfind(")")],
                      "user": re.search(r"Uid:\s+(\d+)", status).group(1),
                      "rss_kb": int(rss.group(1)) if rss else 0,
                      "cpu_avg_pct": round(100 * cpu_s / life, 2),
                      "cmd": (read(p + "/cmdline") or "").replace("\0", " ")[:160]})
    procs.sort(key=lambda x: -x["rss_kb"])
    return {"count": len(procs), "top_rss": procs[:top],
            "top_cpu_avg": sorted(procs, key=lambda x: -x["cpu_avg_pct"])[:top]}


SECTIONS = {
    "system": system, "cpu": cpu, "gpu": gpu, "thermal": thermal, "power": power,
    "display": display, "input": input_devices, "android": android, "services": services,
    "network": network, "bluetooth": bluetooth, "modem": modem, "audio": audio,
    "sensors": sensors, "media": media, "processes": processes,
}


def main():
    want = sys.argv[1:] or list(SECTIONS)
    report = {"probe": 1, "time": time.strftime("%Y-%m-%dT%H:%M:%S%z"), "uid": os.getuid()}
    for name in want:
        t = time.monotonic()
        try:
            report[name] = SECTIONS[name]()
        except Exception as e:  # раздел упал — остальные всё равно собираем
            report[name] = {"error": f"{type(e).__name__}: {e}"}
        report.setdefault("_timing_ms", {})[name] = round((time.monotonic() - t) * 1000)
    json.dump(report, sys.stdout, ensure_ascii=False, indent=1)
    print()


if __name__ == "__main__":
    main()
