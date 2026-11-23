#!/usr/bin/env python3
"""НаомиФон — задержка диска «со стороны стола» (docs/XRAY.md): раз в 20 мс читает 4 КБ мимо кэша (O_DIRECT) из
случайного места большого файла на /userdata и пишет, сколько ждал. От root: ioprobe.py СЕК ФАЙЛ → строки
«время_мкс задержка_мкс» (время — CLOCK_MONOTONIC, как у трассы стола)."""
import mmap
import os
import random
import sys
import time

PATH = "/userdata/system-data/var/lib/waydroid/images/system.img"   # 2,9 ГБ, только читается


def main():
    secs, out = float(sys.argv[1]), sys.argv[2]
    fd = os.open(PATH, os.O_RDONLY | os.O_DIRECT)
    blocks = os.fstat(fd).st_size // 4096
    buf = mmap.mmap(-1, 4096)   # O_DIRECT нужен буфер по границе страницы
    end = time.monotonic() + secs
    with open(out, "w", buffering=1) as f:   # построчно: измеритель останавливают сигналом
        while time.monotonic() < end:
            t0 = time.monotonic()
            os.preadv(fd, [buf], random.randrange(blocks) * 4096)
            f.write(f"{int(t0 * 1e6)} {int((time.monotonic() - t0) * 1e6)}\n")
            time.sleep(0.02)


if __name__ == "__main__":
    main()
