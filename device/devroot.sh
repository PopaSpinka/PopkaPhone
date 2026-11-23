#!/bin/sh
# НаомиФон — среда сборки на самом телефоне: копия корня Ubuntu Touch в /userdata/naomi/devroot,
# вход через chroot. Библиотеки в ней — те же версии, что в системе, поэтому собранное здесь
# запускается в системе как есть. Корень системы при этом не трогается.
#
#   sudo sh devroot.sh create        скопировать корень (≈ 3 ГБ, ничего не скачивает)
#   sudo sh devroot.sh packages      apt update + пакеты сборки (PACKAGES ниже; ≈ 175 МБ скачивания)
#   sudo sh devroot.sh mount         подключить /proc /sys /dev /run /android /home
#   sudo sh devroot.sh umount        отключить
#   sudo sh devroot.sh root КОМАНДА  выполнить в среде от root (apt и т. п.)
#   sudo sh devroot.sh user КОМАНДА  выполнить в среде от phablet (сборка)
#   sudo sh devroot.sh status
set -eu
DR=/userdata/naomi/devroot
BINDS="proc sys dev run android home"
# Пакеты сборки. Репозитории — те, что прописаны в системе: ports.ubuntu.com (noble) и
# repo.ubports.com (24.04-2.x, отсюда Qt 6.10 и libhybris-dev). android-headers-30 — его ждёт
# libhardware.pc (по зависимостям apt ставит только -29).
PACKAGES="build-essential cmake ninja-build pkg-config git
  qt6-base-dev qt6-base-private-dev qt6-declarative-dev qt6-declarative-private-dev
  qt6-wayland-dev qt6-wayland-private-dev qt6-shadertools-dev qml6-module-qtwayland-compositor qt6-wayland
  libhybris-dev android-headers-30 libegl-dev libgles-dev libwayland-dev libxkbcommon-dev
  libinput-dev libudev-dev libsystemd-dev libdbus-1-dev libts-dev libgstreamer1.0-dev libpulse-dev"
# libts-dev — без него CMake не находит Qt6InputSupportPrivate (а за ним EglFSDeviceIntegrationPrivate);
# libgstreamer1.0-dev — видео «Просмотра» стола (shell/preview; сам GStreamer 1.24 и декодеры — в системе);
# libpulse-dev — звук программы «Игр» (shell/apps/game)

mounted() { mountpoint -q "$DR/$1"; }

do_mount() {
    for d in $BINDS; do
        mkdir -p "$DR/$d"
        mounted "$d" || mount --rbind "/$d" "$DR/$d"
        mount --make-rslave "$DR/$d"
    done
}

do_umount() {
    for d in $BINDS; do
        mounted "$d" && umount -R "$DR/$d" || true
    done
}

in_env() {  # $1 — userspec, остальное — команда
    spec=$1; shift
    do_mount
    home=/root; [ "$spec" = root ] || home=/home/phablet
    exec chroot --userspec="$spec" "$DR" /usr/bin/env -i \
        PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
        HOME="$home" LANG=C.UTF-8 DEBIAN_FRONTEND=noninteractive TERM="${TERM:-dumb}" \
        /bin/sh -c "$*"
}

case "${1:-status}" in
    create)
        if [ -e "$DR/etc/os-release" ]; then echo "среда уже есть: $DR"; exit 0; fi
        mkdir -p "$DR"
        src=$(mktemp -d)
        mount --bind / "$src"            # корень без того, что подмонтировано поверх
        cp -a "$src/." "$DR/"
        umount "$src"; rmdir "$src"
        # службы при установке пакетов в среде не запускать
        printf '#!/bin/sh\nexit 101\n' > "$DR/usr/sbin/policy-rc.d"
        chmod 755 "$DR/usr/sbin/policy-rc.d"
        echo "создано: $DR ($(du -sh "$DR" | cut -f1))"
        ;;
    packages)
        in_env root "apt-get update && apt-get install --no-install-recommends -y $(echo $PACKAGES)"
        ;;
    mount) do_mount ;;
    umount) do_umount ;;
    root) shift; in_env root "$@" ;;
    user) shift; in_env phablet:phablet "$@" ;;
    status)
        [ -e "$DR/etc/os-release" ] && echo "среда: $DR ($(du -sh "$DR" 2>/dev/null | cut -f1))" || echo "среды нет"
        for d in $BINDS; do mounted "$d" && printf '%s ' "$d"; done; echo
        ;;
    *) echo "использование: $0 create|mount|umount|root|user|status" >&2; exit 2 ;;
esac
