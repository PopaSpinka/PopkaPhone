#!/bin/sh
# НаомиФон — запись батареи без кабеля (docs/POWER.md). Раз в $1 с (по умолчанию
# 300) строка в CSV: время, заряд по счётчику (мкА·ч — точнее процентов), ток и напряжение, температура,
# загрузка процессора за промежуток, засыпания системы и глубокий сон SoC (AOSD/CXSD), Wi-Fi, экран: подсветка
# (0 — погашен, 96 — AOD оболочки) и режим панели («On: 1080x2340@90», «LP» — малое питание, «Off»).
# Запускается от root (нужны /sys/power/system_sleep/stats и rpmh_stats) отдельной службой, чтобы жить без
# SSH и кабеля:  sudo systemd-run --unit=naomi-battlog --collect sh battlog.sh [сек]
# Остановить:    sudo systemctl stop naomi-battlog
# Пишет в /home/phablet/naomi/battery/battery-<дата>.csv; рядом — снимок процессов в начале.
INTERVAL=${1:-300}
B=/sys/class/power_supply/battery
DIR=/home/phablet/naomi/battery
mkdir -p "$DIR"
OUT="$DIR/battery-$(date +%Y%m%d-%H%M).csv"
ps -eo pid,comm,time --sort=-time | head -25 > "${OUT%.csv}-ps-start.txt"
echo "time,uptime_s,status,capacity,charge_uah,current_ua,voltage_uv,temp_dC,cpu_busy_pct,suspends,aosd,cxsd,apss_sleep_s,wifi,backlight,panel" > "$OUT"
chown -R phablet:phablet "$DIR"

BL=/sys/class/backlight/panel0-backlight
cpu() { awk '/^cpu /{print $2+$3+$4+$7+$8+$9, $5+$6}' /proc/stat; }   # занято, простой (jiffies)
mode_count() { awk -v m="$1" '$0 ~ "RPM Mode:"m {getline; sub(/.*count:/, ""); print; exit}' /sys/power/system_sleep/stats; }
apss_sleep() {   # накопленный сон процессорного кластера, тики 19,2 МГц → секунды
    h=$(awk '/^APSS/{f=1} f && /Accumulated Duration/{sub(/.*:/, ""); print; exit}' /sys/power/rpmh_stats/master_stats)
    [ -n "$h" ] && echo $((h / 19200000))
}
set -- $(cpu)
busy0=$1 idle0=$2
while true; do
    set -- $(cpu)
    busy=$1 idle=$2
    db=$((busy - busy0)) di=$((idle - idle0))
    pct=$([ $((db + di)) -gt 0 ] && echo $((100 * db / (db + di))) || echo 0)
    busy0=$busy idle0=$idle
    wifi=$(nmcli -t -f WIFI general 2>/dev/null)
    echo "$(date +%s),$(cut -d' ' -f1 /proc/uptime),$(cat $B/status),$(cat $B/capacity),$(cat $B/charge_counter),$(cat $B/current_now),$(cat $B/voltage_now),$(cat $B/temp),$pct,$(cat /sys/power/suspend_stats/success),$(mode_count aosd),$(mode_count cxsd),$(apss_sleep),$wifi,$(cat $BL/brightness),$(cat $BL/state)" >> "$OUT"
    sleep "$INTERVAL"
done
