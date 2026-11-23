#!/bin/sh
# НаомиФон — Android-приложения (Waydroid, docs/ANDROID.md): настройка на телефоне, от root (tools/naomi android).
#
#   sudo sh android-setup.sh init     образы Android и настройка Waydroid: LineageOS 20 (Android 13) с Google Play —
#                                     система ~1,33 ГБ и драйверы ~41 МБ с SourceForge (каталог ota.waydro.id); уже
#                                     настроен — не качает; свойства Android — окно на приложение, русский язык, курсор без копии; звук
#                                     Android — через «приёмную» оболочки (asound.conf в слое Waydroid)
#   sudo sh android-setup.sh tune     настройки внутри Android — нужен включённый Android (открыть любое приложение со
#                                     стола): плотность стола, без экранной клавиатуры при настоящей, без NFC, русские
#                                     буквы на правом Alt
#   sudo sh android-setup.sh status   что сейчас
#
# Свойства — в [properties] /var/lib/waydroid/waydroid.cfg: Waydroid кладёт их в waydroid_base.prop при каждом
# запуске контейнера (waydroid upgrade -o). Настройки tune Android хранит у себя (/data) — переживают перезапуски.
# Слой Waydroid (/var/lib/waydroid/overlay — поверх системы Android, mount_overlays) подключается при включении
# Android: новое в нём — со следующего включения.
set -eu
CFG=/var/lib/waydroid/waydroid.cfg
OVERLAY=/var/lib/waydroid/overlay

prop() {   # ключ = значение в [properties] (нет — добавить)
    grep -q "^$1 = " "$CFG" && sed -i "s|^$1 = .*|$1 = $2|" "$CFG" \
        || sed -i "s|^\[properties\]\$|[properties]\n$1 = $2|" "$CFG"
}

case "${1:-status}" in
init)
    if [ -f "$CFG" ]; then
        echo "Waydroid уже настроен ($CFG) — образы не качаю"
    else
        waydroid init -s GAPPS
    fi
    # окно Waydroid на каждую задачу Android, а не весь Android одним окном
    prop persist.waydroid.multi_windows true
    prop persist.sys.locale ru-RU
    # Курсор Android — буфером без копии. По умолчанию служба вывода Waydroid на каждом кадре считывает картинку
    # курсора через видеочип (glReadPixels в общую память) и ждёт этого: 11–22 % видеочипа и 16 % ядра, пока курсор
    # над окном Android (docs/ANDROID-AUDIT.md, находка 4). Имя свойства в коде Waydroid перевёрнуто: true включает
    # буфер видеочипа, а не общую память. Стол курсоры Android не показывает вовсе
    prop persist.waydroid.cursor_force_shm true
    # Звук: служба звука Waydroid пишет в ALSA-устройство «pulse» (модуль pulse → PulseAudio телефона) с буфером 21 мс.
    # Поток, пересозданный модулем во время игры на динамике телефона (задержка 23 мс), крутит ожидание — ядро на 90 %
    # (docs/ANDROID.md, «Звук»). Поэтому поток рождается в «приёмной» naomi.android — пустом выходе оболочки
    # (shell/app/android_sound.h), оттуда оболочка ведёт его на выход стола. handle_underrun false: при недогрузке
    # (телефон занят) модуль по умолчанию бросает поток и создаёт новый — под нагрузкой 1–2 раза в секунду
    mkdir -p "$OVERLAY/system/etc"
    cat > "$OVERLAY/system/etc/asound.conf" <<'EOF'
# НаомиФон (device/android-setup.sh, docs/ANDROID.md «Звук»): поток Android рождается в «приёмной» оболочки;
# недогрузку переживает PulseAudio тишиной — модуль поток не пересоздаёт
pcm.!pulse {
    type pulse
    device "naomi.android"
    handle_underrun false
}
EOF
    waydroid upgrade -o   # пересобирает waydroid_base.prop; включённый Android при этом перезапускается
    grep -A5 '^\[properties\]' "$CFG"
    ;;
tune)
    waydroid shell -- getprop sys.boot_completed | grep -q 1 \
        || { echo "Android не включён: открой приложение Android со стола и повтори"; exit 1; }
    # 240 точек на дюйм = 1,5 пикселя на dp, как масштаб стола: dp Android — точка стола. Своя плотность экрана Android
    # хранит в настройках; ro.sf.lcd_density в waydroid.cfg не годится — сессия дописывает плотность телефона (440)
    # последней, и берётся она
    waydroid shell -- wm density 240
    # курсор без копии (см. init): свойство, уже записанное в данных Android, сильнее значения из waydroid.cfg;
    # служба вывода читает его при включении Android — со следующего включения
    waydroid shell -- setprop persist.waydroid.cursor_force_shm true
    # клавиатура Android на экране — только без настоящей (Waydroid по умолчанию показывает всегда)
    waydroid shell -- settings put secure show_ime_with_hard_keyboard 0
    # NFC Android: падает каждые ~5 с (нет NFC HAL в контейнере; NFC телефона Waydroid на это время останавливает сам —
    # nfcd), «pm disable» его не держит (постоянная служба) — убрать у пользователя 0
    waydroid shell -- pm uninstall -k --user 0 com.android.nfc >/dev/null 2>&1 || true
    # Русские буквы: своя раскладка клавиатуры Waydroid — Generic.kcm и ЙЦУКЕН на правом Alt (Shift — заглавные), её
    # Android берёт по имени устройства из /data/system/devices/keychars. Оболочка шлёт «ф» как правый Alt + A
    # (StreamInput::keyToAndroid) — без переключения раскладок в Android, которое расходилось бы с раскладкой стола
    waydroid shell -- cat /system/usr/keychars/Generic.kcm | awk -v q="'" '
        BEGIN {
            split("A:0444:0424 B:0438:0418 C:0441:0421 D:0432:0412 E:0443:0423 F:0430:0410 G:043f:041f H:0440:0420 " \
                  "I:0448:0428 J:043e:041e K:043b:041b L:0434:0414 M:044c:042c N:0442:0422 O:0449:0429 P:0437:0417 " \
                  "Q:0439:0419 R:043a:041a S:044b:042b T:0435:0415 U:0433:0413 V:043c:041c W:0446:0426 X:0447:0427 " \
                  "Y:043d:041d Z:044f:042f LEFT_BRACKET:0445:0425 RIGHT_BRACKET:044a:042a SEMICOLON:0436:0416 " \
                  "APOSTROPHE:044d:042d COMMA:0431:0411 PERIOD:044e:042e GRAVE:0451:0401", m, " ")
            for (i in m) { split(m[i], f, ":"); lo[f[1]] = f[2]; up[f[1]] = f[3] }
        }
        /^key / { k = $2 }
        /^}/ && (k in lo) {
            printf "    ralt:                               %s\\u%s%s\n", q, lo[k], q
            printf "    shift+ralt, capslock+ralt:          %s\\u%s%s\n", q, up[k], q
        }
        /^}/ && k == "3" { print "    shift+ralt:                         " q "\\u2116" q }
        /^}/ { k = "" }
        { print }' | waydroid shell -- sh -c 'mkdir -p /data/system/devices/keychars &&
            cat > /data/system/devices/keychars/wayland_keyboard.kcm &&
            chmod 644 /data/system/devices/keychars/wayland_keyboard.kcm'
    echo "готово: плотность $(waydroid shell -- wm density | tail -1); раскладка — русские на правом Alt" \
         "(подхватится со следующего включения Android)"
    ;;
status)
    waydroid status || true
    [ -f "$CFG" ] && grep -A5 '^\[properties\]' "$CFG" || true
    [ -f "$OVERLAY/system/etc/asound.conf" ] && echo "звук: через «приёмную» naomi.android" || echo "звук: asound.conf нет"
    ;;
*)
    echo "android-setup.sh init | tune | status" >&2
    exit 2
    ;;
esac
