#!/bin/sh
# НаомиФон — Claude Desktop для Linux на столе (docs/CLAUDE.md): чужая программа как есть (Anthropic, бета для Linux,
# Electron), из её apt-репозитория — но не apt-ом: корень системы только для чтения и полон. Пакет распакован в
# ~/naomi/opt/claude-desktop, рядом — запускалка naomi-claude (её запускает стол: плитка CLAUDE в меню N). И xdg-open
# стола (naomi-xdg-open: ссылки — в браузер стола) — в /usr/local/bin: Claude ищет xdg-open только в системных папках.
#
#   sh claude.sh install   поставить или обновить до новейшей версии (пакет ≈ 184 МБ, распакован ≈ 660 МБ)
#   sh claude.sh status    что стоит и что новейшее в репозитории
#
# Проверка — как у apt: отпечаток ключа — из документации Anthropic (code.claude.com/docs/en/desktop-linux), им
# проверена подпись InRelease, по ней — SHA-256 списка пакетов, по списку — SHA-256 самого пакета.
set -eu
REPO=https://downloads.claude.ai/claude-desktop/apt/stable
KEY_URL=https://downloads.claude.ai/claude-desktop/key.asc
FPR=31DDDE24DDFAB679F42D7BD2BAA929FF1A7ECACE
ARCH=arm64
DEST=/home/phablet/naomi/opt/claude-desktop
DL=/home/phablet/Downloads/claude-desktop
HERE=$(cd "$(dirname "$0")" && pwd)

# новейший claude-desktop в подписанном списке: «версия путь sha256 размер»
newest() {
    mkdir -p "$DL"
    cd "$DL"
    export GNUPGHOME="$DL/gnupg"
    mkdir -p -m 700 "$GNUPGHOME"
    wget -q -O key.asc "$KEY_URL"
    fpr=$(gpg --batch --show-keys --with-colons key.asc 2>/dev/null | awk -F: '/^fpr:/ { print $10; exit }')
    [ "$fpr" = "$FPR" ] || { echo "ключ репозитория не тот: $fpr" >&2; exit 1; }
    gpg --batch --yes --dearmor -o key.gpg key.asc 2>/dev/null
    wget -q -O InRelease "$REPO/dists/stable/InRelease"
    gpgv --keyring "$DL/key.gpg" InRelease 2>/dev/null || { echo "подпись InRelease не сошлась" >&2; exit 1; }
    wget -q -O Packages "$REPO/dists/stable/main/binary-$ARCH/Packages"
    want=$(awk -v p="main/binary-$ARCH/Packages" '/^SHA256:/ { s = 1; next } /^[^ ]/ { s = 0 } s && $3 == p { print $1 }' InRelease)
    [ -n "$want" ] && [ "$want" = "$(sha256sum Packages | cut -d' ' -f1)" ] \
        || { echo "список пакетов не тот, что в подписанном InRelease" >&2; exit 1; }
    # последняя запись в файле — без пустой строки после неё
    { cat Packages; echo; } | awk '
        /^Package: / { p = ($2 == "claude-desktop") }
        p && /^Version:/ { v = $2 } p && /^Filename:/ { f = $2 } p && /^SHA256:/ { h = $2 } p && /^Size:/ { z = $2 }
        /^$/ { if (p && v) print v, f, h, z; p = 0; v = "" }' | sort -V | tail -1
}

running() { pgrep -f "^$DEST/usr/lib/claude-desktop/claude-desktop" >/dev/null; }

# Своё: запускалка рядом с пакетом и xdg-open стола. Claude сужает PATH до системных папок, принадлежащих root и
# закрытых на запись (защита от подмены программ), — xdg-open из папки пользователя он не видит. Поэтому —
# /usr/local/bin/xdg-open, как naomi-usb в /usr/local/sbin: корень на запись на миг (sudo); тот же — не трогать
ours() {
    install -m 755 "$HERE/naomi-claude" "$1/naomi-claude"
    cmp -s "$HERE/naomi-xdg-open" /usr/local/bin/xdg-open && return 0
    sudo -n sh -c 'mount -o remount,rw / || exit 1
        install -m 755 "$1" /usr/local/bin/xdg-open; r=$?
        sync; mount -o remount,ro / || echo "внимание: корень остался на запись" >&2
        exit $r' sh "$HERE/naomi-xdg-open" \
        && echo "xdg-open стола: /usr/local/bin/xdg-open (ссылки — в браузер стола)"
}

case "${1:-status}" in
install)
    set -- $(newest)
    ver=$1 file=$2 sum=$3 size=$4
    cd "$DL"   # newest — в подоболочке: её cd сюда не доходит
    if [ "$(cat "$DEST/.version" 2>/dev/null)" = "$ver" ]; then
        ours "$DEST"
        echo "стоит $ver — новейший"
        exit 0
    fi
    running && { echo "Claude запущен — сначала закрыть его (обновлять на ходу нельзя)" >&2; exit 1; }
    deb="claude-desktop_${ver}_$ARCH.deb"
    if ! echo "$sum  $deb" | sha256sum -c --status 2>/dev/null; then
        echo "качаю $ver ($((size / 1000000)) МБ) с downloads.claude.ai…"
        wget -q -O "$deb.part" "$REPO/$file"
        mv "$deb.part" "$deb"
        echo "$sum  $deb" | sha256sum -c --status || { rm -f "$deb"; echo "SHA-256 пакета не сошлась" >&2; exit 1; }
    fi
    rm -rf "$DEST.new"
    mkdir -p "$DEST.new"
    dpkg-deb -x "$deb" "$DEST.new"
    ours "$DEST.new"
    echo "$ver" > "$DEST.new/.version"
    rm -rf "$DEST.old"
    [ -d "$DEST" ] && mv "$DEST" "$DEST.old"
    mv "$DEST.new" "$DEST"
    rm -rf "$DEST.old"
    find "$DL" -maxdepth 1 -name 'claude-desktop_*.deb' ! -name "$deb" -delete
    echo "поставлен $ver: $DEST ($(du -sh "$DEST" | cut -f1))"
    ;;
status)
    echo "стоит:      $(cat "$DEST/.version" 2>/dev/null || echo нет)$(running && echo ' (запущен)' || true)"
    set -- $(newest)
    echo "новейший:   $1 ($(($4 / 1000000)) МБ)"
    ;;
*)
    echo "sh claude.sh install|status" >&2
    exit 1
    ;;
esac
