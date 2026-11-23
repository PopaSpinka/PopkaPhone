#!/bin/sh
# НаомиФон — штатная оболочка Lomiri: выключить / вернуть / состояние.
#   sudo sh lomiri.sh off      маскирует lightdm (он запускает Lomiri и его Mir-композитор), гасит его,
#                              и keep-display-on-during-boot (до 5 мин ждёт lightdm и держит загрузку);
#                              включает linger для phablet — службы пользователя (звук и т.п.)
#                              работают с загрузки без входа в сеанс
#   sudo sh lomiri.sh on       возвращает Lomiri
#   sh lomiri.sh status
# Пакеты Lomiri не удаляются — это делает сборка своего образа (этап 8).
set -eu
UNITS="lightdm.service keep-display-on-during-boot.service"

rw() {
    mount -o remount,rw /
    trap 'sync; mount -o remount,ro / || echo "внимание: корень остался на запись" >&2' EXIT
}

case "${1:-status}" in
    off)
        rw
        systemctl mask $UNITS >/dev/null
        systemctl daemon-reload
        loginctl enable-linger phablet
        systemctl stop $UNITS || true
        ;;
    on)
        rw
        systemctl unmask $UNITS >/dev/null
        systemctl daemon-reload
        systemctl start lightdm.service
        ;;
    status) ;;
    *) echo "использование: $0 off|on|status" >&2; exit 2 ;;
esac
echo "lightdm: $(systemctl is-enabled lightdm.service 2>/dev/null || true), $(systemctl is-active lightdm.service || true);" \
     "linger phablet: $(loginctl show-user phablet -p Linger --value 2>/dev/null || echo ?)"
