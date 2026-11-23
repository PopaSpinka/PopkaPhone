#!/usr/bin/env python3
"""Kernel modules the phone's kernel lacks, built for this very kernel (docs/BOARDS.md, «Драйверы»).

The port's kernel (4.19.269-g5278bca141e1) has no cdc-acm (USB serial ports: ESP32 boards) and no USB/IP client
(vhci-hcd: boards plugged into the dock). They are built here, on the phone, from the kernel's own source and config:

  1. the source of the port's commit (GitHub tarball, SHA-256 checked) and /proc/config.gz with three modules turned on;
  2. `make modules_prepare` in the build environment (devroot: Ubuntu clang 14.0.6 — the kernel's is Android 14.0.7);
  3. symbol versions (MODVERSIONS): the kernel itself hides its table (kptr_restrict), so they come from the port's
     own modules (/lib/modules/<release>, built with this kernel), and the ones none of them uses — from compiling the
     kernel files that export them; every such file's other versions must match the port's modules, or nothing is built;
  4. the modules as external ones (only then modpost reads the table), each symbol versioned, vermagic = the release.

Result: ~/naomi/kmod/<release>/{cdc-acm,usbip-core,vhci-hcd}.ko + SHA256SUMS. naomi-usbshare loads them.
Runs as phablet; make and llvm-nm — in the devroot (sudo devroot.sh user …). About two minutes; heats nothing.

    python3 kmod.py [WORKDIR]     (default ~/kernel: the source stays there for the next time)
"""
import gzip
import hashlib
import os
import re
import shutil
import struct
import subprocess
import sys
import urllib.request

RELEASE = "4.19.269-g5278bca141e1"
COMMIT = "5278bca141e163e1dfbe3617b9b29bcc8c9f2ec0"
TARBALL = f"https://codeload.github.com/dadrickpistachio/android_kernel_google_redbull/tar.gz/{COMMIT}"
TARBALL_SHA256 = "49b3fa944f7769e0addac5ad38067e0d2e2092e9f9afd16041113bcd4929f72d"
ENABLE = ["USB_ACM", "USBIP_CORE", "USBIP_VHCI_HCD"]
DISABLE = ["RELR"]  # (Ubuntu's lld passes the RELR test, the port's build had it off: the config stays the kernel's)
# config lines allowed to differ from the running kernel's: the compiler, RELR, ours
ALLOWED = re.compile(r"^(# )?CONFIG_(CLANG_VERSION|LLD_VERSION|TOOLS_SUPPORT_RELR|RELR|USB_ACM|USBIP_\w+)[= ]")
MODULES = {  # folder -> (Kbuild, files from the source, the .ko made)
    "acm": ("obj-m := cdc-acm.o\n", ["drivers/usb/class/cdc-acm.c", "drivers/usb/class/cdc-acm.h"], ["cdc-acm"]),
    "usbip": ("obj-m := usbip-core.o vhci-hcd.o\nusbip-core-y := usbip_common.o usbip_event.o\n"
              "vhci-hcd-y := vhci_sysfs.o vhci_tx.o vhci_rx.o vhci_hcd.o\n",
              [f"drivers/usb/usbip/{n}" for n in ("usbip_common.c", "usbip_common.h", "usbip_event.c", "vhci.h",
                                                  "vhci_hcd.c", "vhci_rx.c", "vhci_sysfs.c", "vhci_tx.c")],
              ["usbip-core", "vhci-hcd"]),
}
DEVROOT = "/userdata/naomi/devroot"
DEVROOT_SH = os.path.expanduser("~/naomi/bin/devroot.sh")
LLVM = "/usr/lib/llvm-14/bin"


def say(*a):
    print("kmod:", *a, flush=True)


def die(*a):
    print("kmod: СТОП —", *a, file=sys.stderr, flush=True)
    sys.exit(1)


def in_devroot(cmd):
    """A shell command in the devroot as phablet (PATH with clang 14); its output -> str."""
    r = subprocess.run(["sudo", "-n", "sh", DEVROOT_SH, "user", f"export PATH={LLVM}:$PATH; {cmd}"],
                       capture_output=True, text=True)
    if r.returncode:
        die(f"в среде сборки: {cmd[:120]}…\n{(r.stdout + r.stderr)[-3000:]}")
    return r.stdout


def versions(path):
    """__versions of a module: [(crc, symbol)]."""
    data = open(path, "rb").read()
    shoff, = struct.unpack_from("<Q", data, 0x28)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    secs = [struct.unpack_from("<IIQQQQIIQQ", data, shoff + i * shentsize) for i in range(shnum)]
    names = secs[shstrndx][4]
    for name, _, _, _, off, size, *_ in secs:
        if data[names + name:data.index(b"\0", names + name)] == b"__versions":
            return [(struct.unpack_from("<Q", data, p)[0], data[p + 8:p + 64].split(b"\0")[0].decode())
                    for p in range(off, off + size, 64)]
    return []


def vermagic(path):
    m = re.search(rb"vermagic=([^\0]*)", open(path, "rb").read())
    return m.group(1).decode() if m else ""


def main():
    work = os.path.abspath(os.path.expanduser(sys.argv[1] if len(sys.argv) > 1 else "~/kernel"))
    if os.uname().release != RELEASE:
        die(f"ядро {os.uname().release}, а скрипт — для {RELEASE}: новое ядро — новый коммит и SHA-256 в kmod.py")
    src, out, mods = f"{work}/redbull", f"{work}/out", f"{work}/mods"
    make = (f"make -C {src} ARCH=arm64 LLVM=1 CC=clang LD=ld.lld HOSTCC=clang HOSTCXX=clang++ O={out} "
            f"LOCALVERSION=-{RELEASE.split('-', 1)[1]}")
    os.makedirs(work, exist_ok=True)

    # 1. the source and the config
    tar = f"{work}/redbull-{COMMIT[:7]}.tar.gz"
    if not os.path.exists(tar):
        say(f"скачиваю исходники ядра ({COMMIT[:12]}, ≈ 190 МБ)")
        with urllib.request.urlopen(TARBALL, timeout=60) as r, open(tar + ".part", "wb") as f:
            shutil.copyfileobj(r, f, 1 << 20)
        os.replace(tar + ".part", tar)
    h = hashlib.sha256()
    with open(tar, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    if h.hexdigest() != TARBALL_SHA256:
        die(f"SHA-256 архива {h.hexdigest()} — не тот, что записан ({TARBALL_SHA256})")
    if not os.path.exists(f"{src}/Makefile"):
        say("распаковываю")
        os.makedirs(src, exist_ok=True)
        subprocess.run(["tar", "-xzf", tar, "-C", src, "--strip-components=1"], check=True)
    os.makedirs(out, exist_ok=True)
    running = gzip.open("/proc/config.gz").read().decode()
    open(f"{out}/config.running", "w").write(running)
    open(f"{out}/.config", "w").write(running)
    flags = " ".join([f"-m {o}" for o in ENABLE] + [f"-d {o}" for o in DISABLE])
    in_devroot(f"cd {src} && scripts/config --file {out}/.config {flags} && {make} -s olddefconfig")
    ours = open(f"{out}/.config").read().splitlines()
    diff = set(running.splitlines()) ^ set(ours)
    odd = sorted(line for line in diff if line.startswith(("CONFIG_", "# CONFIG_")) and not ALLOWED.match(line))
    if odd:
        die("конфиг разошёлся с ядром телефона не только в наших строках:\n  " + "\n  ".join(odd))
    for o in ENABLE:
        if f"CONFIG_{o}=m" not in ours:
            die(f"CONFIG_{o} не включился")

    # 2. the tree for external modules
    say("modules_prepare")
    in_devroot(f"nice -n 19 {make} -j4 modules_prepare")
    release = open(f"{out}/include/config/kernel.release").read().strip()
    if release != RELEASE:
        die(f"сборка назвалась {release}, нужно {RELEASE}")

    # 3. symbol versions: the port's modules first
    port = {}
    for root, _, names in os.walk(f"/lib/modules/{RELEASE}"):
        for n in names:
            if n.endswith(".ko"):
                for crc, sym in versions(os.path.join(root, n)):
                    if port.setdefault(sym, crc) != crc:
                        die(f"модули порта не согласны о {sym}")
    say(f"версий из модулей порта: {len(port)}")

    def build_modules():
        for folder, (kbuild, files, _) in MODULES.items():
            d = f"{mods}/{folder}"
            shutil.rmtree(d, ignore_errors=True)
            os.makedirs(d)
            for f in files:
                shutil.copy(f"{src}/{f}", d)
            open(f"{d}/Kbuild", "w").write(kbuild)
            in_devroot(f"{make} M={d} -j4 modules")
        return {name: f"{mods}/{folder}/{name}.ko" for folder, (_, _, names) in MODULES.items() for name in names}

    open(f"{out}/Module.symvers", "w").write("")
    kos = build_modules()
    undefined, defined = set(), set()
    for ko in kos.values():
        nm = in_devroot(f"llvm-nm {ko}")
        for line in nm.splitlines():
            p = line.split()
            if len(p) == 2 and p[0] == "U":
                undefined.add(p[1])
            elif len(p) == 3 and p[1] in "TDBR":
                defined.add(p[2])
    missing = sorted(undefined - defined - set(port))
    say(f"нужно функций ядра: {len(undefined - defined)}, нет в модулях порта: {len(missing)}")

    # 3b. the rest — from compiling the files that export them
    built = {}
    if missing:
        pattern = r"EXPORT_SYMBOL(_GPL)?\((" + "|".join(missing) + r")\)"
        r = subprocess.run(["grep", "-rnoE", "--include=*.c", "--include=*.S", pattern, "."], cwd=src,
                           capture_output=True, text=True)
        where = {}
        for line in r.stdout.splitlines():
            path, _, text = line.split(":", 2)
            path = path[2:]
            if path.startswith("arch/") and not path.startswith("arch/arm64/"):
                continue
            where.setdefault(re.search(r"\((\w+)\)", text).group(1), path)
        for sym in missing:
            if sym not in where and re.match(r"param_(ops|get|set)_", sym):
                where[sym] = "kernel/params.c"  # (STANDARD_PARAM_DEF exports them)
        lost = [s for s in missing if s not in where]
        if lost:
            die("не нашёл, где ядро объявляет: " + " ".join(lost))
        objs = sorted({re.sub(r"\.[cS]$", ".o", p) for p in where.values()})
        say(f"собираю файлы ядра с ними: {len(objs)}")
        in_devroot(f"nice -n 19 {make} -j4 {' '.join(objs)}")
        checked = 0
        for o in objs:
            for line in in_devroot(f"llvm-nm {out}/{o}").splitlines():
                p = line.split()
                if len(p) == 3 and p[2].startswith("__crc_"):
                    sym, crc = p[2][6:], int(p[0], 16) & 0xFFFFFFFF
                    if sym in port:
                        if port[sym] != crc:
                            die(f"{o}: версия {sym} {crc:#010x}, у ядра {port[sym]:#010x} — исходник или конфиг не те")
                        checked += 1
                    built[sym] = crc
        lost = [s for s in missing if s not in built]
        if lost:
            die("файлы собраны, но версий нет: " + " ".join(lost))
        say(f"сверено с модулями порта: {checked} версий из тех же файлов — все совпали")

    # 4. the modules with versions
    with open(f"{out}/Module.symvers", "w") as f:
        for sym, crc in sorted({**port, **built}.items()):
            f.write(f"0x{crc:08x}\t{sym}\tvmlinux\tEXPORT_SYMBOL\n")
    kos = build_modules()
    dest = os.path.expanduser(f"~/naomi/kmod/{RELEASE}")
    os.makedirs(dest, exist_ok=True)
    sums = []
    for name, ko in kos.items():
        nm = in_devroot(f"llvm-nm -u {ko}").split()
        need = {s for s in nm if s != "U"}
        have = {s for _, s in versions(ko)}
        if need - have:
            die(f"{name}: без версии — {' '.join(sorted(need - have))}")
        if not vermagic(ko).startswith(RELEASE + " "):
            die(f"{name}: vermagic «{vermagic(ko)}»")
        shutil.copy(ko, dest)
        sums.append(f"{hashlib.sha256(open(ko, 'rb').read()).hexdigest()}  {name}.ko")
        say(f"{name}.ko: {len(need)} функций ядра, у всех версия")
    open(f"{dest}/SHA256SUMS", "w").write("\n".join(sums) + "\n")
    print("\n".join(sums))
    say(f"готово → {dest}")


if __name__ == "__main__":
    main()
