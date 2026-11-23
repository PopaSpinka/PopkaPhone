#!/bin/sh
# НаомиФон — игры Windows на столе (docs/WINDOWS.md): чужие части как есть, закреплённых версий, на самом телефоне.
#   Box64  — переводчик кода x86-64 в ARM64 (исходники, собираем сами);
#   Wine   — окружение Windows (готовая сборка x86-64 «wow64»: 32-битные программы — в 64-битном процессе, поэтому
#            хватает одного Box64). Версия 10.0: 11.22 на ядре телефона (адрес 39 бит) падает при запуске
#            (virtual.c: set_mem_area_state);
#   Mesa   — драйвер Vulkan видеочипа Adreno, Turnip, через устройство ядра Android /dev/kgsl-3d0 (исходники, сами);
#   DXVK   — DirectX 9–11 поверх Vulkan (готовые библиотеки Windows). Версия 2.4.1: 3.1.1 наш видеочип отвергает
#            («Device does not support required feature storageBuffer16BitAccess»);
#   d3dx9_30.dll — настоящая библиотека Microsoft из пакета DirectX (июнь 2010): встроенная в Wine не компилирует
#            шейдеры FlatOut 2 («Failed to create effect»). Кладётся рядом с игрой, не в систему префикса;
#   libvulkan1 — обычный загрузчик Vulkan из пакета Ubuntu (в системе стоит загрузчик libhybris — он драйверы Mesa
#            не грузит): только файл библиотеки, в ~/naomi/win/lib, систему не трогаем.
# Исходники и архивы — ~/naomi/third, готовое — ~/naomi/win. SHA-256 каждого архива сверяется с записанной здесь.
#
#   sh wine.sh fetch     скачать всё, чего ещё нет
#   sh wine.sh box64     собрать Box64 (в среде сборки)
#   sh wine.sh mesa      собрать Turnip (в среде сборки) и положить рядом загрузчик Vulkan
#   sh wine.sh d3dx9 ПАПКА   скачать пакет DirectX (96 МБ, download.microsoft.com), достать d3dx9_30.dll в папку игры
#   sh wine.sh status
set -eu
BASE=/home/phablet/naomi
SRC=$BASE/third
OUT=$BASE/win
DEVROOT=$BASE/bin/devroot.sh

BOX64_VER=0.4.4
BOX64_URL=https://codeload.github.com/ptitSeb/box64/tar.gz/refs/tags/v$BOX64_VER
BOX64_SUM=99c6de4f509e46ab1de15df740d0e0ea338a7790efa3f67510dfbb975cc24029
WINE_VER=10.0
WINE_URL=https://github.com/Kron4ek/Wine-Builds/releases/download/$WINE_VER/wine-$WINE_VER-amd64-wow64.tar.xz
WINE_SUM=aeebbbf239e548f0136f1cd72a2e109dd9a572a8f703da3c09fa40d74d5c255f
MESA_VER=26.3.1
MESA_URL=https://archive.mesa3d.org/mesa-$MESA_VER.tar.xz
MESA_SUM=4e4e438eac7b840a9383988cbb7c08dad8041da544b9319fc26d319fa7348961
DXVK_VER=2.4.1
DXVK_URL=https://github.com/doitsujin/dxvk/releases/download/v$DXVK_VER/dxvk-$DXVK_VER.tar.gz
DXVK_SUM=7b23db4e1386b5d9a3ec0d83daa8b06096b758639185c11a673373a5ae478d54

fetch() {   # $1 имя каталога, $2 адрес, $3 сумма, $4 разборщик tar (z, J)
    dir=$SRC/$1
    [ -e "$dir/.ready" ] && return 0
    mkdir -p "$SRC"
    file=$SRC/$1.tar
    echo "$1: скачиваю $2"
    wget -q --tries=3 -O "$file" "$2"
    sum=$(sha256sum "$file" | cut -d' ' -f1)
    echo "$1: архив $(du -h "$file" | cut -f1), SHA-256 $sum"
    if [ "$3" != "-" ] && [ "$3" != "$sum" ]; then
        echo "$1: SHA-256 не та, что записана ($3) — не распаковываю" >&2
        exit 1
    fi
    rm -rf "$dir"; mkdir -p "$dir"
    tar -x"$4"f "$file" -C "$dir" --strip-components=1
    rm -f "$file"
    echo "$sum" > "$dir/.ready"
}

fetch_all() {
    fetch "box64-$BOX64_VER" "$BOX64_URL" "$BOX64_SUM" z
    fetch "wine-$WINE_VER" "$WINE_URL" "$WINE_SUM" J
    fetch "dxvk-$DXVK_VER" "$DXVK_URL" "$DXVK_SUM" z
    fetch "mesa-$MESA_VER" "$MESA_URL" "$MESA_SUM" J
}

build_box64() {
    fetch "box64-$BOX64_VER" "$BOX64_URL" "$BOX64_SUM" z
    dir=$SRC/box64-$BOX64_VER
    mkdir -p "$OUT/box64"
    # Наши правки чужих исходников (две строки): адрес загрузки самого Box64 — выше 4 ГБ (в пределах 39 бит адреса
    # ядра телефона). По умолчанию он садится на 0x34800000, а его куча — где-то до 0x60000000: 32-битной программе
    # Wine в её 2 ГБ остаётся самый большой свободный кусок ~630 МБ, и установщик FlatOut 2 (распаковщику нужно
    # 640 МБ подряд) вставал на 0 % с «out of memory» (21 ноября 2026)
    sed -i 's|^set(BOX64_ELF_ADDRESS "0x34800000").*|set(BOX64_ELF_ADDRESS "0x3000000000")  # НаомиФон: выше 4 ГБ|' "$dir/CMakeLists.txt"
    # и его служебные блоки (find47bitBlock) — тоже выше 4 ГБ: с 0x60000000 они делили нижние 2 ГБ пополам, самый
    # большой свободный кусок был 764 МБ, а распаковщик просит больше
    sed -i 's|^#define HIGH   (void\*)0x60000000|#define HIGH   (void*)0x3100000000LL|' "$dir/src/custommem.c"
    echo "box64: собираю"
    sudo -n sh "$DEVROOT" user "cmake -S $dir -B $dir/build -G Ninja -DARM_DYNAREC=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DNOGIT=ON -Wno-dev >$dir/build.log 2>&1 && ninja -C $dir/build >>$dir/build.log 2>&1" || {
        tail -30 "$dir/build.log" >&2
        exit 1
    }
    cp "$dir/build/box64" "$OUT/box64/box64"
    rm -rf "$OUT/box64/x64lib"
    cp -r "$dir/x64lib" "$OUT/box64/x64lib"     # libstdc++ и libgcc для программ x86-64 — из исходников Box64
    echo "box64: готово — $("$OUT/box64/box64" --version 2>&1 | head -1)"
}

build_mesa() {
    fetch "mesa-$MESA_VER" "$MESA_URL" "$MESA_SUM" J
    dir=$SRC/mesa-$MESA_VER
    # meson — не из пакетов среды (там 1.3, Mesa просит 1.5): pip3 install --user "meson>=1.5,<1.9" (поставлен 1.8.5)
    echo "mesa: собираю Turnip (KGSL)"
    sudo -n sh "$DEVROOT" user "cd $dir && /home/phablet/.local/bin/meson setup build --prefix=$OUT/mesa -Dbuildtype=release \
        -Dvulkan-drivers=freedreno -Dgallium-drivers= -Dfreedreno-kmds=kgsl -Dplatforms=wayland,x11 \
        -Dglx=disabled -Degl=disabled -Dgbm=disabled -Dopengl=false -Dgles1=disabled -Dgles2=disabled \
        -Dllvm=disabled -Dzstd=disabled -Dxmlconfig=disabled >$dir/build.log 2>&1 \
        && ninja -C build >>$dir/build.log 2>&1 && ninja -C build install >>$dir/build.log 2>&1" || {
        tail -40 "$dir/build.log" >&2
        exit 1
    }
    # загрузчик Vulkan: пакет libvulkan1 (150 КБ) — скачать и распаковать, не ставя
    mkdir -p "$SRC/libvulkan1" "$OUT/lib"
    sudo -n sh "$DEVROOT" user "cd $SRC/libvulkan1 && { ls libvulkan1_*.deb >/dev/null 2>&1 || apt-get download libvulkan1 >/dev/null; } \
        && rm -rf x && dpkg-deb -x libvulkan1_*.deb x && cp -P x/usr/lib/aarch64-linux-gnu/libvulkan.so.1* $OUT/lib/"
    echo "mesa: готово — $(ls "$OUT"/mesa/lib/*/libvulkan_freedreno.so 2>/dev/null); загрузчик — $(ls "$OUT"/lib/libvulkan.so.1.*)"
}

DIRECTX_URL=https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe
DIRECTX_SUM=053f76dcbb28802e23341b6a787e3b0791c0fa5c8d4d011b1044172dbf89c73b
D3DX9_SUM=5edeed79f2359527a55b8189cfa8b9b121cd608d44eead905a0f3436938ad532

d3dx9() {   # $1 — папка игры; нужен cabextract в среде сборки (apt-get install cabextract)
    [ -d "$1" ] || { echo "нет папки игры: $1" >&2; exit 2; }
    dir=$SRC/directx-jun2010
    mkdir -p "$dir"
    if [ ! -e "$dir/d3dx9_30.dll" ]; then
        wget -q --tries=3 -O "$dir/dx.exe" "$DIRECTX_URL"
        [ "$(sha256sum "$dir/dx.exe" | cut -d' ' -f1)" = "$DIRECTX_SUM" ] || { echo "directx: SHA-256 не та" >&2; exit 1; }
        sudo -n sh "$DEVROOT" user "cd $dir && cabextract -q -F '*d3dx9_30*x86*' dx.exe && cabextract -q -F d3dx9_30.dll Apr2006_d3dx9_30_x86.cab"
        rm -f "$dir/dx.exe"
    fi
    [ "$(sha256sum "$dir/d3dx9_30.dll" | cut -d' ' -f1)" = "$D3DX9_SUM" ] || { echo "d3dx9_30.dll: SHA-256 не та" >&2; exit 1; }
    cp "$dir/d3dx9_30.dll" "$1/"
    echo "d3dx9_30.dll → $1"
}

case "${1:-status}" in
    fetch) fetch_all ;;
    box64) build_box64 ;;
    mesa) build_mesa ;;
    d3dx9) d3dx9 "${2:-}" ;;
    status)
        for d in "box64-$BOX64_VER" "wine-$WINE_VER" "dxvk-$DXVK_VER" "mesa-$MESA_VER"; do
            [ -e "$SRC/$d/.ready" ] && echo "$d: есть (архив $(cat "$SRC/$d/.ready"))" || echo "$d: нет"
        done
        ls "$OUT" 2>/dev/null || true
        ;;
    *) echo "использование: $0 fetch|box64|mesa|d3dx9 ПАПКА|status" >&2; exit 2 ;;
esac
