#!/usr/bin/env python3
"""НаомиФон — первый свой кадр: экран напрямую через HWC2 (libhybris), без Lomiri и Mir.

Что делает:
  1. открывает дисплей через hwc2_compat (тот же путь, что у Mir/Lomiri, но своими руками);
  2. EGL на платформе hwcomposer + GLES: тёмный фон и янтарная полоса, которая едет по экрану;
  3. меряет реальную частоту: метки vsync от дисплея (HWC2) и время срабатывания present-fence —
     момент, когда кадр реально ушёл на панель (ioctl SYNC_IOC_FILE_INFO);
  4. переключает режим панели (`--mode N`) внутренней функцией HWC2::impl::Display::
     setActiveConfigWithConstraints из libhwc2_compat_layer.so — в API libhybris её нет.

Запуск (Lomiri должен быть остановлен — дисплеем владеет один клиент):
  sudo python3 hwcspike.py [--seconds 4] [--mode 1]
Итог — JSON в stdout, ход — в stderr. Это разведка для этапа 4, не оболочка.
"""
import os
os.environ.setdefault("EGL_PLATFORM", "hwcomposer")  # до загрузки libEGL

import argparse
import ctypes as C
import fcntl
import json
import select
import struct
import sys
import threading
import time

L = "/usr/lib/aarch64-linux-gnu/"
hwc = C.CDLL(L + "libhwc2.so.1")
hwin = C.CDLL(L + "libhybris-hwcomposerwindow.so.1")
hyb = C.CDLL(L + "libhybris-common.so.1")
egl = C.CDLL(L + "libEGL.so.1")
gl = C.CDLL(L + "libGLESv2.so.2")


def log(*a):
    print(" ", *a, file=sys.stderr, flush=True)


# ---------- типы HWC2 ----------

VSYNC_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_int32, C.c_uint64, C.c_int64)
HOTPLUG_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_int32, C.c_uint64, C.c_bool, C.c_bool)
REFRESH_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_int32, C.c_uint64)
PRESENT_CB = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_void_p)


class Listener(C.Structure):
    _fields_ = [("vsync", VSYNC_CB), ("hotplug", HOTPLUG_CB), ("refresh", REFRESH_CB)]


class DisplayConfig(C.Structure):
    _fields_ = [("id", C.c_uint32), ("display", C.c_uint64), ("width", C.c_int32),
                ("height", C.c_int32), ("vsyncPeriod", C.c_int64), ("dpiX", C.c_float),
                ("dpiY", C.c_float)]


class Constraints(C.Structure):      # V2_4::IComposerClient::VsyncPeriodChangeConstraints
    _fields_ = [("desiredTimeNanos", C.c_int64), ("seamlessRequired", C.c_bool)]


class Timeline(C.Structure):         # V2_4::VsyncPeriodChangeTimeline
    _fields_ = [("newVsyncAppliedTimeNanos", C.c_int64), ("refreshRequired", C.c_bool),
                ("refreshTimeNanos", C.c_int64)]


hwc.hwc2_compat_device_new.restype = C.c_void_p
hwc.hwc2_compat_device_new.argtypes = [C.c_bool]
hwc.hwc2_compat_device_register_callback.argtypes = [C.c_void_p, C.POINTER(Listener), C.c_int]
hwc.hwc2_compat_device_on_hotplug.argtypes = [C.c_void_p, C.c_uint64, C.c_bool]
hwc.hwc2_compat_device_get_display_by_id.restype = C.c_void_p
hwc.hwc2_compat_device_get_display_by_id.argtypes = [C.c_void_p, C.c_uint64]
hwc.hwc2_compat_display_get_active_config.restype = C.POINTER(DisplayConfig)
hwc.hwc2_compat_display_get_active_config.argtypes = [C.c_void_p]
hwc.hwc2_compat_display_set_power_mode.argtypes = [C.c_void_p, C.c_int]
hwc.hwc2_compat_display_set_vsync_enabled.argtypes = [C.c_void_p, C.c_int]
hwc.hwc2_compat_display_create_layer.restype = C.c_void_p
hwc.hwc2_compat_display_create_layer.argtypes = [C.c_void_p]
for name, args in {
    "hwc2_compat_layer_set_composition_type": [C.c_void_p, C.c_int],
    "hwc2_compat_layer_set_blend_mode": [C.c_void_p, C.c_int],
    "hwc2_compat_layer_set_source_crop": [C.c_void_p, C.c_float, C.c_float, C.c_float, C.c_float],
    "hwc2_compat_layer_set_display_frame": [C.c_void_p, C.c_int32, C.c_int32, C.c_int32, C.c_int32],
    "hwc2_compat_layer_set_visible_region": [C.c_void_p, C.c_int32, C.c_int32, C.c_int32, C.c_int32],
    "hwc2_compat_display_validate": [C.c_void_p, C.POINTER(C.c_uint32), C.POINTER(C.c_uint32)],
    "hwc2_compat_display_accept_changes": [C.c_void_p],
    "hwc2_compat_display_set_client_target": [C.c_void_p, C.c_uint32, C.c_void_p, C.c_int32, C.c_int],
    "hwc2_compat_display_present": [C.c_void_p, C.POINTER(C.c_int32)],
    "hwc2_compat_display_get_release_fences": [C.c_void_p, C.POINTER(C.c_void_p)],
    "hwc2_compat_out_fences_destroy": [C.c_void_p],
}.items():
    getattr(hwc, name).argtypes = args
hwc.hwc2_compat_out_fences_get_fence.restype = C.c_int32
hwc.hwc2_compat_out_fences_get_fence.argtypes = [C.c_void_p, C.c_void_p]
hwin.HWCNativeWindowCreate.restype = C.c_void_p
hwin.HWCNativeWindowCreate.argtypes = [C.c_uint, C.c_uint, C.c_uint, PRESENT_CB, C.c_void_p]
hwin.HWCNativeBufferGetFence.restype = C.c_int
hwin.HWCNativeBufferGetFence.argtypes = [C.c_void_p]
hwin.HWCNativeBufferSetFence.argtypes = [C.c_void_p, C.c_int]
hyb.android_dlopen.restype = C.c_void_p
hyb.android_dlopen.argtypes = [C.c_char_p, C.c_int]
hyb.android_dlsym.restype = C.c_void_p
hyb.android_dlsym.argtypes = [C.c_void_p, C.c_char_p]

HWC2_POWER_MODE_ON, HWC2_COMPOSITION_CLIENT, HWC2_BLEND_MODE_NONE = 2, 1, 1
HWC2_ERROR_HAS_CHANGES = 5
HAL_PIXEL_FORMAT_RGBA_8888 = 1

SYM_SET_CONFIG = (b"_ZN7android4HWC24impl7Display30setActiveConfigWithConstraintsEjRKNS_8hardware8"
                  b"graphics8composer4V2_415IComposerClient28VsyncPeriodChangeConstraintsEPNS6_"
                  b"25VsyncPeriodChangeTimelineE")
SYM_VSYNC_PERIOD = b"_ZNK7android4HWC24impl7Display21getDisplayVsyncPeriodEPl"
SYM_SWITCH_OK = b"_ZNK7android4HWC24impl7Display28isVsyncPeriodSwitchSupportedEv"


# ---------- время срабатывания fence (когда кадр реально ушёл на панель) ----------

SYNC_IOC_FILE_INFO = 0xC0383E04  # _IOWR('>', 4, struct sync_file_info), 56 байт


def fence_timestamp(fd):
    """Метка CLOCK_MONOTONIC (нс), когда fence сработал; None, если нельзя узнать."""
    info = bytearray(56)
    try:
        fcntl.ioctl(fd, SYNC_IOC_FILE_INFO, info)
        n = struct.unpack_from("I", info, 40)[0]
        if not n:
            return None
        arr = (C.c_char * (80 * n))()
        struct.pack_into("IIQ", info, 40, n, 0, C.addressof(arr))
        fcntl.ioctl(fd, SYNC_IOC_FILE_INFO, info)
        return max(struct.unpack_from("Q", bytes(arr), 80 * i + 72)[0] for i in range(n))
    except OSError:
        return None


class FenceWatcher(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.q, self.cv, self.stamps, self.stop = [], threading.Condition(), [], False

    def add(self, fd):
        with self.cv:
            self.q.append(fd)
            self.cv.notify()

    def run(self):
        while True:
            with self.cv:
                while not self.q and not self.stop:
                    self.cv.wait()
                if not self.q:
                    return
                fd = self.q.pop(0)
            select.select([fd], [], [], 1.0)
            ts = fence_timestamp(fd)
            os.close(fd)
            if ts:
                self.stamps.append(ts)


# ---------- дисплей ----------

class Display:
    def __init__(self):
        self.vsyncs, self.errors, self.frames = [], [], 0
        self.fences = FenceWatcher()
        self.fences.start()
        self._cbs = Listener(VSYNC_CB(self._on_vsync), HOTPLUG_CB(self._on_hotplug),
                             REFRESH_CB(lambda *a: None))
        self.dev = hwc.hwc2_compat_device_new(False)
        if not self.dev:
            raise RuntimeError("hwc2_compat_device_new: нет устройства (Lomiri не остановлен?)")
        hwc.hwc2_compat_device_register_callback(self.dev, C.byref(self._cbs), 0)
        self.disp = None
        for _ in range(50):
            self.disp = hwc.hwc2_compat_device_get_display_by_id(self.dev, 0)
            if self.disp:
                break
            time.sleep(0.1)
        if not self.disp:
            raise RuntimeError("дисплей 0 не появился")
        hwc.hwc2_compat_display_set_power_mode(self.disp, HWC2_POWER_MODE_ON)
        cfg = self.config()
        self.w, self.h = cfg["width"], cfg["height"]
        self.layer = hwc.hwc2_compat_display_create_layer(self.disp)
        hwc.hwc2_compat_layer_set_composition_type(self.layer, HWC2_COMPOSITION_CLIENT)
        hwc.hwc2_compat_layer_set_blend_mode(self.layer, HWC2_BLEND_MODE_NONE)
        hwc.hwc2_compat_layer_set_source_crop(self.layer, 0.0, 0.0, float(self.w), float(self.h))
        hwc.hwc2_compat_layer_set_display_frame(self.layer, 0, 0, self.w, self.h)
        hwc.hwc2_compat_layer_set_visible_region(self.layer, 0, 0, self.w, self.h)
        self._present_cb = PRESENT_CB(self._present)
        # внутренние функции HWC2::impl::Display (их нет в C API libhybris)
        h = hyb.android_dlopen(b"libhwc2_compat_layer.so", 2)
        self.sym = {k: hyb.android_dlsym(h, s) for k, s in
                    (("set", SYM_SET_CONFIG), ("period", SYM_VSYNC_PERIOD), ("switch", SYM_SWITCH_OK))}
        self.self_ptr = C.c_void_p.from_address(self.disp).value  # hwc2_compat_display { HWC2::Display *self; }

    def _on_hotplug(self, _l, _seq, display, connected, _primary):
        hwc.hwc2_compat_device_on_hotplug(self.dev, display, connected)

    def _on_vsync(self, _l, _seq, _display, ts):
        self.vsyncs.append(ts)

    def _present(self, _user, _window, buf):
        nt, nr = C.c_uint32(), C.c_uint32()
        err = hwc.hwc2_compat_display_validate(self.disp, C.byref(nt), C.byref(nr))
        if err not in (0, HWC2_ERROR_HAS_CHANGES):
            self.errors.append(f"validate {err}")
            return
        if nt.value or nr.value:
            hwc.hwc2_compat_display_accept_changes(self.disp)
        hwc.hwc2_compat_display_set_client_target(self.disp, 0, buf,
                                                  hwin.HWCNativeBufferGetFence(buf), 0)
        pf = C.c_int32(-1)
        err = hwc.hwc2_compat_display_present(self.disp, C.byref(pf))
        if err:
            self.errors.append(f"present {err}")
        fences = C.c_void_p()
        if hwc.hwc2_compat_display_get_release_fences(self.disp, C.byref(fences)) == 0:
            hwin.HWCNativeBufferSetFence(buf, hwc.hwc2_compat_out_fences_get_fence(fences, self.layer))
            hwc.hwc2_compat_out_fences_destroy(fences)
        if pf.value >= 0:
            self.fences.add(pf.value)
        self.frames += 1

    def config(self):
        p = hwc.hwc2_compat_display_get_active_config(self.disp)
        c = p.contents
        return {"id": c.id, "width": c.width, "height": c.height,
                "vsync_ns": c.vsyncPeriod, "hz": round(1e9 / c.vsyncPeriod, 2) if c.vsyncPeriod else None}

    def vsync_period_ns(self):
        if not self.sym["period"]:
            return None
        out = C.c_int64(0)
        err = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.POINTER(C.c_int64))(self.sym["period"])(
            self.self_ptr, C.byref(out))
        return out.value if err == 0 else f"ошибка {err}"

    def switch_supported(self):
        if not self.sym["switch"]:
            return None
        return bool(C.CFUNCTYPE(C.c_bool, C.c_void_p)(self.sym["switch"])(self.self_ptr))

    def set_mode(self, config_id):
        if not self.sym["set"]:
            return {"error": "нет символа setActiveConfigWithConstraints"}
        cons, tl = Constraints(0, False), Timeline()
        fn = C.CFUNCTYPE(C.c_int32, C.c_void_p, C.c_uint32, C.POINTER(Constraints), C.POINTER(Timeline))
        err = fn(self.sym["set"])(self.self_ptr, config_id, C.byref(cons), C.byref(tl))
        return {"error": err, "applied_at_ns": tl.newVsyncAppliedTimeNanos,
                "refresh_required": tl.refreshRequired}


# ---------- EGL / GLES ----------

EGL_DEFAULT_DISPLAY = None
EGL_NONE, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT = 0x3038, 0x3040, 0x0004
EGL_RED_SIZE, EGL_GREEN_SIZE, EGL_BLUE_SIZE, EGL_ALPHA_SIZE = 0x3024, 0x3023, 0x3022, 0x3021
EGL_CONTEXT_CLIENT_VERSION = 0x3098
GL_COLOR_BUFFER_BIT, GL_SCISSOR_TEST = 0x4000, 0x0C11
for f in ("eglGetDisplay", "eglCreateWindowSurface", "eglCreateContext"):
    getattr(egl, f).restype = C.c_void_p
egl.eglGetDisplay.argtypes = [C.c_void_p]
egl.eglInitialize.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p]
egl.eglChooseConfig.argtypes = [C.c_void_p, C.POINTER(C.c_int32), C.POINTER(C.c_void_p), C.c_int32,
                                C.POINTER(C.c_int32)]
egl.eglCreateWindowSurface.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p]
egl.eglCreateContext.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.POINTER(C.c_int32)]
egl.eglMakeCurrent.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p]
egl.eglSwapBuffers.argtypes = [C.c_void_p, C.c_void_p]
egl.eglSwapInterval.argtypes = [C.c_void_p, C.c_int32]
gl.glClearColor.argtypes = [C.c_float] * 4
gl.glScissor.argtypes = [C.c_int32] * 4
gl.glGetString.restype = C.c_char_p


def attribs(*pairs):
    flat = [v for p in pairs for v in p] + [EGL_NONE]
    return (C.c_int32 * len(flat))(*flat)


def setup_egl(d):
    dpy = egl.eglGetDisplay(EGL_DEFAULT_DISPLAY)
    if not dpy or not egl.eglInitialize(dpy, None, None):
        raise RuntimeError("eglInitialize не удался")
    cfg, n = C.c_void_p(), C.c_int32()
    egl.eglChooseConfig(dpy, attribs((EGL_RED_SIZE, 8), (EGL_GREEN_SIZE, 8), (EGL_BLUE_SIZE, 8),
                                     (EGL_ALPHA_SIZE, 8), (EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT)),
                        C.byref(cfg), 1, C.byref(n))
    win = hwin.HWCNativeWindowCreate(d.w, d.h, HAL_PIXEL_FORMAT_RGBA_8888, d._present_cb, None)
    surf = egl.eglCreateWindowSurface(dpy, cfg, win, None)
    ctx = egl.eglCreateContext(dpy, cfg, None, attribs((EGL_CONTEXT_CLIENT_VERSION, 2)))
    if not surf or not ctx or not egl.eglMakeCurrent(dpy, surf, surf, ctx):
        raise RuntimeError("EGL: окно или контекст не создались")
    egl.eglSwapInterval(dpy, 1)
    return dpy, surf


# цвета НаомиОС: тёплый уголь и янтарь
BG = (0.078, 0.067, 0.059)
AMBER = (1.0, 0.69, 0.22)


def run_frames(d, dpy, surf, seconds, label):
    """Рисует `seconds` с; возвращает статистику vsync и реального вывода кадров."""
    v0, f0, p0 = len(d.vsyncs), d.frames, len(d.fences.stamps)
    t0 = time.monotonic()
    swaps = []
    while time.monotonic() - t0 < seconds:
        t = time.monotonic() - t0
        gl.glDisable(GL_SCISSOR_TEST)
        gl.glClearColor(*BG, 1.0)
        gl.glClear(GL_COLOR_BUFFER_BIT)
        bar_h = 160
        y = int((d.h - bar_h) * (0.5 + 0.5 * __import__("math").sin(t * 2.4)))
        gl.glEnable(GL_SCISSOR_TEST)
        gl.glScissor(0, y, d.w, bar_h)
        gl.glClearColor(*AMBER, 1.0)
        gl.glClear(GL_COLOR_BUFFER_BIT)
        s = time.monotonic()
        egl.eglSwapBuffers(dpy, surf)
        swaps.append(time.monotonic() - s)
    time.sleep(0.15)  # дождаться последних fence
    return {"label": label, "frames": d.frames - f0, "seconds": round(time.monotonic() - t0, 2),
            "vsync": intervals(d.vsyncs[v0:]), "present": intervals(d.fences.stamps[p0:]),
            "swap_ms_median": round(sorted(swaps)[len(swaps) // 2] * 1000, 2) if swaps else None}


def intervals(stamps):
    st = sorted(stamps)
    iv = [(b - a) / 1e6 for a, b in zip(st, st[1:])]
    if len(iv) < 3:
        return {"count": len(st)}
    s = sorted(iv)
    med = s[len(s) // 2]
    return {"count": len(st), "median_ms": round(med, 3), "hz": round(1000 / med, 2),
            "p95_ms": round(s[int(len(s) * 0.95)], 3), "max_ms": round(s[-1], 3),
            "over_1_5x": sum(1 for x in iv if x > med * 1.5)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seconds", type=float, default=4)
    ap.add_argument("--mode", type=int, default=None, help="id режима панели для второго прогона")
    a = ap.parse_args()
    d = Display()
    hwc.hwc2_compat_display_set_vsync_enabled(d.disp, 1)
    dpy, surf = setup_egl(d)
    out = {"gl": (gl.glGetString(0x1F01) or b"").decode(), "size": [d.w, d.h],
           "config_before": d.config(), "vsync_period_ns_before": d.vsync_period_ns(),
           "vsync_switch_supported": d.switch_supported(), "runs": []}
    log(f"GPU {out['gl']}, {d.w}×{d.h}, режим {out['config_before']}")
    out["runs"].append(run_frames(d, dpy, surf, a.seconds, f"режим {out['config_before']['id']}"))
    log(f"прогон 1: {out['runs'][-1]}")
    if a.mode is not None:
        out["set_mode"] = d.set_mode(a.mode)
        log(f"смена режима → {a.mode}: {out['set_mode']}")
        run_frames(d, dpy, surf, 0.5, "переход")
        out["config_after"] = d.config()
        out["vsync_period_ns_after"] = d.vsync_period_ns()
        out["runs"].append(run_frames(d, dpy, surf, a.seconds, f"режим {a.mode}"))
        log(f"прогон 2: {out['runs'][-1]}")
    out["errors"] = d.errors[:20]
    json.dump(out, sys.stdout, ensure_ascii=False, indent=1)
    print()
    sys.stdout.flush()
    os._exit(0)  # не ждать потоков binder


if __name__ == "__main__":
    main()
