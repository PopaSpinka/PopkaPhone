#!/bin/sh
# НаомиФон — ядра эмуляторов для «Игр» стола (docs/GAMES.md): чужие программы как есть, из исходников закреплённой
# версии, собираются в среде сборки (devroot.sh) под процессор телефона. Готовое ядро — ~/naomi/cores/<имя>_libretro.so;
# его грузит своя программа naomi-game (shell/apps/game).
#
#   sh cores.sh [build]   скачать (если ещё нет) и собрать все ядра
#   sh cores.sh status    что собрано
#
# Скачивается с codeload.github.com архив одной версии (не вся история); SHA-256 архива сверяется с записанной здесь.
set -eu
BASE=/home/phablet/naomi
SRC=$BASE/third          # исходники ядер: по каталогу на версию
OUT=$BASE/cores
DEVROOT=$BASE/bin/devroot.sh

# имя | хранилище GitHub | версия (коммит) | SHA-256 архива | каталог сборки | цель make | файл ядра
CORES="
mesen|libretro/Mesen|0102910c39ad1a62bc3f784466f3f67ca9eae335|360f97e907ada9b8e28a95652aa1d07656340e1d84ff868312745a566b250e01|Libretro|-j6|Libretro/mesen_libretro.so
pcsx_rearmed|libretro/pcsx_rearmed|7b0e8f8fadfbce3d170d195b9d9eecaabfa08c8e|4f47bd98bac6cdf62dd4f7a56069b0b0d9999aa6800e636334d1788c5cf38ccf|.|-f Makefile.libretro -j6|pcsx_rearmed_libretro.so
"

fetch() {   # $1 имя, $2 хранилище, $3 версия, $4 сумма
    dir=$SRC/$1-$3
    [ -e "$dir/.ready" ] && return 0
    mkdir -p "$SRC"
    tar=$SRC/$1-$3.tar.gz
    echo "$1: скачиваю https://codeload.github.com/$2/tar.gz/$3"
    wget -q --tries=3 -O "$tar" "https://codeload.github.com/$2/tar.gz/$3"
    sum=$(sha256sum "$tar" | cut -d' ' -f1)
    echo "$1: архив $(du -h "$tar" | cut -f1), SHA-256 $sum"
    if [ "$4" != "-" ] && [ "$4" != "$sum" ]; then
        echo "$1: SHA-256 не та, что записана ($4) — не распаковываю" >&2
        exit 1
    fi
    rm -rf "$dir"; mkdir -p "$dir"
    tar -xzf "$tar" -C "$dir" --strip-components=1
    rm -f "$tar"
    echo "$sum" > "$dir/.ready"
}

build() {
    mkdir -p "$OUT"
    echo "$CORES" | while IFS='|' read -r name repo rev sum sub args lib; do
        [ -n "$name" ] || continue
        fetch "$name" "$repo" "$rev" "$sum"
        dir=$SRC/$name-$rev
        echo "$name: собираю ($dir/$sub)"
        sudo -n sh "$DEVROOT" user "make -C $dir/$sub $args >$dir/build.log 2>&1" || {
            tail -30 "$dir/build.log" >&2
            exit 1
        }
        cp "$dir/$lib" "$OUT/${name}_libretro.so"
        strip --strip-unneeded "$OUT/${name}_libretro.so" 2>/dev/null || true
        printf '%s %s архив %s\n' "$repo" "$rev" "$(cat "$dir/.ready")" > "$OUT/$name.version"
        echo "$name: готово — $(du -h "$OUT/${name}_libretro.so" | cut -f1)"
    done
}

case "${1:-build}" in
    build) build ;;
    status) ls -la "$OUT" 2>/dev/null || echo "ядер нет"; cat "$OUT"/*.version 2>/dev/null || true ;;
    *) echo "использование: $0 [build|status]" >&2; exit 2 ;;
esac
