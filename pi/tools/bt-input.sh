#!/bin/sh
# НаомиФон, приставка на Pi — свои клавиатуры, мыши и трекпады Bluetooth (docs/PI.md, «Трекпад»). От root.
#   bt-input.sh connect       после запуска Bluetooth (служба naomi-dock-bt): спаренные устройства ввода, что не на
#                             связи, — позвать через 3, 15 и 45 с. Сами они зовут приставку по нажатию, но после
#                             перезапуска Bluetooth или перезагрузки приставка сказала им «выключаюсь» (HCI 0x15) — и
#                             они не зовут, пока не тронут, хотя ещё не спят и вызов принимают (Magic Trackpad — 40 с
#                             сам не вернулся, а позвали — на связи)
#   bt-input.sh pair [адрес]  спарить: ждать устройство ввода в поиске до 5 минут (его выключили и включили — видно) и
#                             сразу спарить с сохранением ключа, сделать доверенным, подключить. С адресом — это
#                             устройство (прежнее спаривание — прочь), без — первое новое клавиатура / мышь / трекпад.
#                             Приставка вне этого спаривание не принимает (pairable off — так у Raspberry Pi OS): с
#                             ним выключенным устройство спаривается без сохранения ключа, и BlueZ его не пускает
#                             («Rejected connection from !bonded device»)
set -u
HID=00001124-0000-1000-8000-00805f9b34fb

inputs() {   # спаренные устройства ввода (профиль HID)
    for d in $(bluetoothctl devices Paired | cut -d' ' -f2); do
        bluetoothctl info "$d" | grep -q "$HID" && echo "$d"
    done
}

case "${1:-connect}" in
connect)
    for delay in 3 12 30; do
        sleep $delay
        for d in $(inputs); do
            bluetoothctl info "$d" | grep -q "Connected: yes" && continue
            if timeout 15 bluetoothctl connect "$d" > /dev/null 2>&1; then
                echo "$d: на связи"
            else
                echo "$d: не ответил (спит или выключен — позовёт сам по нажатию)"
            fi
        done
    done
    ;;
pair)
    want=${2:-}
    [ -n "$want" ] && bluetoothctl remove "$want" > /dev/null 2>&1
    bluetoothctl pairable on > /dev/null
    timeout 300 bluetoothctl --timeout 295 scan on > /dev/null 2>&1 &
    scan=$!
    echo "жду устройство в поиске (выключите и включите его)…"
    found=
    for i in $(seq 600); do
        for d in ${want:-$(bluetoothctl devices | cut -d' ' -f2)}; do
            info=$(bluetoothctl info "$d" 2> /dev/null) || continue
            if [ -z "$want" ]; then
                echo "$info" | grep -q "Paired: yes" && continue
                echo "$info" | grep -q "Icon: input-" || continue
            fi
            found=$d
            break 2
        done
        sleep 0.5
    done
    kill $scan 2> /dev/null
    bluetoothctl scan off > /dev/null 2>&1
    if [ -z "$found" ]; then
        bluetoothctl pairable off > /dev/null
        echo "за 5 минут не появилось"
        exit 1
    fi
    echo "вижу: $(bluetoothctl info "$found" | sed -n 's/^\tName: //p') ($found) — спариваю"
    for t in 1 2 3; do
        timeout 30 bluetoothctl --agent NoInputNoOutput --timeout 25 pair "$found" 2>&1 | grep -aE "successful|Failed" | tail -1
        bluetoothctl info "$found" | grep -q "Bonded: yes" && break
        sleep 1
    done
    bluetoothctl trust "$found" > /dev/null
    timeout 20 bluetoothctl connect "$found" > /dev/null 2>&1
    bluetoothctl pairable off > /dev/null
    bluetoothctl info "$found" | grep -E "Bonded|Trusted|Connected" | tr -d '\t' | tr '\n' ' '
    echo
    bluetoothctl info "$found" | grep -q "Bonded: yes"
    ;;
*)
    echo "bt-input.sh connect | pair [адрес]" >&2
    exit 2
    ;;
esac
