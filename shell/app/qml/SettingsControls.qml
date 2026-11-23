// Общие органы настроек — и в «Настройках», и в шторке: яркость и автояркость, громкость, Wi-Fi, Bluetooth,
// модем, звук, частота экрана, разрешение, гашение, AOD, HUD, трансляция (выключатель: приставка и Mac).
// При автояркости ползунок сдвигает её кривую под себя (naomiPower).
// С AOD гашение уводит в AOD — та же плитка называется «aod через»; это и автоблокировка: гашение блокирует телефон.
// Шаги (просьба пользователя, 12 ноября 2026): 10 с → 1 → 3 → 5 → 10 мин → никогда; значение не из них (было
// 15 или 30 с) — касание ведёт на следующий шаг выше. AOD по кругу: выкл → вкл (через 20 минут
// без движения — малое питание панели) → без экономии (панель всё время в обычном режиме).
// Wi-Fi, Bluetooth и трансляция: касание — включить/выключить, долгое нажатие — их экран (wifiRequested,
// bluetoothRequested, screensRequested — «Экраны», docs/SCREENS.md). Звук: касание — «без звука». Перезагрузка телефона — со второго касания (первое красит
// плитку красным, «нажми ещё раз», 4 с — как кнопки питания в меню N НаомиОС; docs/NAOMID.md, «Система»).
import QtQuick
import Naomi.Ui

Column {
    id: root
    property bool hudOn
    signal hudToggled()
    signal wifiRequested()
    signal bluetoothRequested()
    signal screensRequested()
    readonly property var btConnected: naomiPhone.bluetooth.devices.find(d => d.connected)
    spacing: 10

    readonly property var timeouts: [10, 60, 180, 300, 600, 0]   // с; 0 — не гасить
    property bool rebootArmed: false
    Timer { id: rebootTimer; interval: 4000; onTriggered: root.rebootArmed = false }
    function timeoutLabel(s) { return s === 0 ? "никогда" : s < 60 ? s + " с" : s / 60 + " мин" }

    Caps { text: "яркость"; font.pixelSize: 10 }
    NSlider {
        width: parent.width
        value: naomiPower.brightness
        onMoved: (v) => naomiPower.brightness = v
    }
    Caps { text: "громкость · " + Theme.outputLabel(naomiPhone.audio.output); font.pixelSize: 10 }
    NSlider {
        width: parent.width
        value: naomiPhone.audio.volume / 100
        onMoved: (v) => naomiPhone.audio.call("SetVolume", [v * 100])
    }
    Grid {
        width: parent.width
        columns: 2
        columnSpacing: 8; rowSpacing: 8
        NToggle {
            width: (parent.width - 8) / 2
            label: "wi-fi"
            value: naomiPhone.wifi.state === "connected" ? naomiPhone.wifi.ssid : Theme.wifiLabel(naomiPhone.wifi)
            on: naomiPhone.wifi.enabled
            onClicked: naomiPhone.wifi.call("SetEnabled", [!naomiPhone.wifi.enabled])
            onHeld: root.wifiRequested()
        }
        NToggle {
            width: (parent.width - 8) / 2
            label: "bluetooth"
            value: !naomiPhone.bluetooth.powered ? "выкл" : root.btConnected ? root.btConnected.name : "вкл"
            on: naomiPhone.bluetooth.powered
            onClicked: naomiPhone.bluetooth.call("SetPowered", [!naomiPhone.bluetooth.powered])
            onHeld: root.bluetoothRequested()
        }
        NToggle {   // модем сотовой связи: выключен — не ищет сеть и не мешает Wi-Fi; Wi-Fi и Bluetooth работают; помнится
            width: (parent.width - 8) / 2
            label: "модем"
            value: !naomiPhone.cell.enabled ? "выкл"
                   : naomiPhone.cell.state === "registered" || naomiPhone.cell.state === "roaming"
                     ? naomiPhone.cell.operator || "в сети" : Theme.cellLabel(naomiPhone.cell.state)
            on: naomiPhone.cell.enabled
            onClicked: naomiPhone.cell.call("SetEnabled", [!naomiPhone.cell.enabled])
        }
        NToggle {
            width: (parent.width - 8) / 2
            label: "звук"
            value: naomiPhone.audio.muted ? "без звука" : naomiPhone.audio.volume + "%"
            on: !naomiPhone.audio.muted
            onClicked: naomiPhone.audio.call("SetMuted", [!naomiPhone.audio.muted])
        }
        NToggle {
            width: (parent.width - 8) / 2
            label: "90 гц"
            value: naomiDisplay.refreshHz.toFixed(0) + " гц"
            on: naomiDisplay.refreshHz > 75
            onClicked: naomiDisplay.setRefresh(on ? 60 : 90)
        }
        NToggle {
            width: (parent.width - 8) / 2
            label: "aod"
            value: !naomiPower.aod ? "выкл" : naomiPower.aodDeep ? "вкл" : "без экономии"
            on: naomiPower.aod
            onClicked: {
                if (!naomiPower.aod) {
                    naomiPower.aodDeep = true
                    naomiPower.aod = true
                } else if (naomiPower.aodDeep) {
                    naomiPower.aodDeep = false
                } else {
                    naomiPower.aod = false
                }
            }
        }
        NToggle {
            width: (parent.width - 8) / 2
            label: naomiPower.aod ? "aod через" : "гасить экран"
            value: root.timeoutLabel(naomiPower.screenTimeout)
            on: naomiPower.screenTimeout > 0
            onClicked: {   // следующий шаг выше; после последнего — «никогда», после «никогда» — первый
                const t = naomiPower.screenTimeout, steps = root.timeouts.filter(s => s > 0)
                naomiPower.screenTimeout = t === 0 ? steps[0] : steps.find(s => s > t) ?? 0
            }
        }
        NToggle {
            width: (parent.width - 8) / 2
            label: "hud"
            value: root.hudOn ? "вкл" : "выкл"
            on: root.hudOn
            onClicked: root.hudToggled()
        }
        NToggle {   // ширина кадра: 720 — дисплей растягивает на панель; смена — перезапуск оболочки (≈ 2 с)
            width: (parent.width - 8) / 2
            label: "разрешение"
            value: naomiDisplay.renderSize.width + "p"
            on: naomiDisplay.renderSize.width >= naomiDisplay.panelSize.width
            onClicked: naomiDisplay.setRenderWidth(on ? 720 : 1080)
        }
        NToggle {   // выключатель рабочего стола (docs/STREAM.md, «Выключатель»); сколько экранов его показывают; помнится.
                    // Долгое нажатие — «Экраны»: на какие выведен и какой главный (docs/SCREENS.md)
            readonly property int showing: naomiStream.screens.filter(s => s.state === "desk").length
            width: (parent.width - 8) / 2
            label: "трансляция"
            value: !naomiStream.enabled ? "выкл" : showing > 1 ? showing + " экрана" : naomiStream.connected ? naomiStream.viewer || "есть" : "ждёт"
            on: naomiStream.enabled
            onClicked: naomiStream.enabled = !naomiStream.enabled
            onHeld: root.screensRequested()
        }
        NToggle {   // телефон целиком — службой железа; второе касание в течение 4 с перезагружает
            width: (parent.width - 8) / 2
            label: root.rebootArmed ? "нажми ещё раз" : "перезагрузка"   // подпись длинная — второго слова справа нет
            warn: root.rebootArmed
            onClicked: {
                if (root.rebootArmed) {
                    root.rebootArmed = false
                    naomiPhone.system.call("Reboot")
                } else {
                    root.rebootArmed = true
                    rebootTimer.restart()
                }
            }
        }
        NToggle {
            width: (parent.width - 8) / 2
            label: "автояркость"
            value: !naomiPower.autoBrightness ? "выкл" : naomiPower.ambientLux >= 0 ? naomiPower.ambientLux + " лк" : "вкл"
            on: naomiPower.autoBrightness
            onClicked: naomiPower.autoBrightness = !naomiPower.autoBrightness
        }
    }
}
