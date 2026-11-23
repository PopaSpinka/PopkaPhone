#!/usr/bin/env python3
"""НаомиФон — экран безопасного режима (служба naomi-safe): оболочка упала несколько раз подряд.

Без Qt и без оболочки — тем же путём, что device/hwcspike.py: тёмный экран с красной полосой
вверху и янтарной внизу, чтобы было видно, что телефон жив и ждёт разработчика. SSH и ADB при
этом работают как обычно. Держит кадр, пока службу не остановят.
"""
import sys
import time

sys.path.insert(0, "/usr/local/lib/naomi")
import hwcspike as hw  # noqa: E402

BAD = (0.85, 0.44, 0.37)   # «плохо» из палитры НаомиОС


def main():
    d = hw.Display()
    hw.hwc.hwc2_compat_display_set_power_mode(d.disp, hw.HWC2_POWER_MODE_ON)
    dpy, surf = hw.setup_egl(d)
    for _ in range(3):  # пара кадров, чтобы composer точно вывел картинку
        hw.gl.glDisable(hw.GL_SCISSOR_TEST)
        hw.gl.glClearColor(*hw.BG, 1.0)
        hw.gl.glClear(hw.GL_COLOR_BUFFER_BIT)
        hw.gl.glEnable(hw.GL_SCISSOR_TEST)
        hw.gl.glScissor(0, d.h - 140, d.w, 40)
        hw.gl.glClearColor(*BAD, 1.0)
        hw.gl.glClear(hw.GL_COLOR_BUFFER_BIT)
        hw.gl.glScissor(0, 120, d.w, 12)
        hw.gl.glClearColor(*hw.AMBER, 1.0)
        hw.gl.glClear(hw.GL_COLOR_BUFFER_BIT)
        hw.egl.eglSwapBuffers(dpy, surf)
    print("naomi-safe: безопасный режим — оболочка не запускается, жду разработчика", flush=True)
    while True:
        time.sleep(3600)


if __name__ == "__main__":
    main()
