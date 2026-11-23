// «Настройки»: органы настроек, переход к сетям и «о телефоне» — живые данные naomiPhone (служба naomid)
// и naomiDisplay. Ток и температуры служба не опрашивает сама — страница спрашивает их раз в 3 с, пока
// открыта главная. Разделы (section: "wifi", "bluetooth", "screens") открываются вбок — Naomi.Ui Axis, как экран из галереи
// Попка Студио (0,30 с, сдвиг 28 dp); «назад» (back()) — сначала лист раздела, потом сам раздел (0,26 с по
// оставшемуся пути), потом выход из «Настроек» (это решает оболочка). Жест «назад» оболочки ведёт закрытие
// раздела пальцем: backKind() / backBegin / backMove / backEnd.
// Страница живёт всё время, как программа-процесс (ShellView, settingsLoader): закрыли — спрятана, открыли
// снова — там же, где была (раздел, прокрутка). Пока закрыта (open — false), ничего не опрашивает.
// Опрос (ток, тепло, нагрузка, адреса) — только когда всё стоит: переход оболочки и переход раздела закончились.
// Опрос в начале перехода стоил пропущенных кадров: 13 на 12 открытий.
import QtQuick
import Naomi.Ui

Item {
    id: root
    property bool open                  // «Настройки» открыты в оболочке (и пока уходят)
    property bool still                 // открыты, и переход оболочки закончился
    property bool hudOn
    signal hudToggled()
    property string section: ""         // "" — главная, "wifi", "bluetooth", "screens"
    property string shown: ""           // раздел на экране: остаётся, пока он уходит обратно
    property bool instant: false        // смена раздела без перехода (страница спрятана)
    onSectionChanged: {
        if (section !== "") shown = section
        if (instant) { secAx.jump(section !== "" ? 1 : 0); if (section === "") shown = "" }
        else if (section !== "") secAx.goTo(1, 300, false)
        else secAx.goTo(0, 260, true)
    }
    // открыть раздел (из шторки): на экране — переходом, спрятанной — сразу (её саму откроет переход оболочки)
    function openSection(s, animated) {
        instant = !animated
        section = s
        instant = false
    }
    property real keyboardHeight: 0     // сколько снизу закрывает клавиатура оболочки, точки
    property var sample: ({ currentMa: 0, cpuC: 0, skinC: 0 })

    // 0 — главная, 1 — раздел
    Axis { id: secAx; dir: -1; shift: 28; onSettled: (at) => { if (at === 0) root.shown = "" } }

    // для жеста «назад» оболочки: что закроет «назад» — лист раздела, сам раздел или ничего (тогда — программу)
    function backKind() {
        return wifiPage.sheetMode !== "" || btPage.sheetMode !== "" ? "sheet" : section !== "" ? "page" : ""
    }
    function backBegin() { secAx.dragBegin() }
    function backMove(dx) { secAx.dragMove(-dx) }
    function backEnd(vx) {
        if (secAx.decide(-vx) === 0) section = ""
        else secAx.goTo(1, 160, false)
    }

    function back() {
        if (wifiPage.back() || btPage.back())
            return true
        if (section === "")
            return false
        section = ""
        return true
    }

    Timer {
        interval: 3000
        repeat: true
        triggeredOnStart: true
        running: root.still && secAx.atRest && root.section === "" && root.visible && naomiPower.displayOn
        onTriggered: {
            naomiPhone.battery.call("Sample", [], s => root.sample = s)
            root.sys = naomiSystem.sample()
        }
    }
    property var sys: ({ memUsed: 0, memTotal: 0, cpuLoad: -1, cores: 0, freqs: [], nets: [], rxRate: -1, txRate: -1 })
    // числа по-русски: десятичная запятая; объёмы — в двоичных гигабайтах, как считает система
    function num(v, digits) { return v.toFixed(digits).replace(".", ",") }
    function rate(b) {
        if (b < 0) return "…"
        if (b >= 1048576) return num(b / 1048576, 1) + " мб/с"
        return (b < 10240 ? num(b / 1024, 1) : Math.round(b / 1024)) + " кб/с"
    }
    function netLine(kind) {
        const n = sys.nets.find(x => x.kind === kind)
        return n ? n.ip : ""
    }

    Rectangle { anchors.fill: parent; color: Theme.canvas }

    // строка-переход к разделу: название, состояние, «›»
    component NavRow: Item {
        id: nav
        property string key
        property string value
        property bool lit: false
        signal opened()
        width: parent ? parent.width : 0
        height: 46
        Caps {
            id: navKey
            anchors.verticalCenter: parent.verticalCenter
            text: nav.key
            color: Theme.muted
            font.pixelSize: 11
        }
        Caps {
            anchors.left: navKey.right; anchors.leftMargin: 16
            anchors.right: chevron.left; anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            text: nav.value
            color: nav.lit ? Theme.amber : Theme.dim
            font.pixelSize: 10
        }
        Text {
            id: chevron
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "›"
            font.family: Theme.sans
            font.pixelSize: 20
            color: Theme.muted
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.surfaceHigh }
        TapHandler { onTapped: nav.opened() }
    }

    component Line: Row {
        property string key
        property string value
        spacing: 12
        Caps { width: 92; text: key; font.pixelSize: 10 }
        Caps { text: value; color: Theme.text; font.pixelSize: 10; font.letterSpacing: 0.8 }
    }

    Flickable {
        width: parent.width
        height: parent.height - Theme.statusBarHeight
        y: Theme.statusBarHeight
        x: secAx.oldDelta
        opacity: secAx.oldAlpha
        visible: secAx.oldAlpha > 0
        contentHeight: content.height + 80
        boundsBehavior: Flickable.DragOverBounds
        Column {
            id: content
            x: Theme.margin
            y: 34
            width: root.width - 2 * Theme.margin
            spacing: 22
            Caps { text: "настройки"; color: Theme.amber; font.pixelSize: 12 }
            SettingsControls {
                width: parent.width
                hudOn: root.hudOn
                onHudToggled: root.hudToggled()
                onWifiRequested: root.section = "wifi"
                onBluetoothRequested: root.section = "bluetooth"
                onScreensRequested: root.section = "screens"
            }
            Rectangle { width: parent.width; height: 1; color: Theme.surfaceHigh }
            Column {   // рабочий стол: на какие экраны выведен (docs/SCREENS.md)
                width: parent.width
                spacing: 4
                Caps { text: "рабочий стол"; color: Theme.amber; font.pixelSize: 10 }
                NavRow {
                    readonly property var live: naomiStream.screens.filter(s => s.state === "desk")
                    key: "экраны"
                    value: !naomiStream.enabled ? "стол выключен" : live.length ? live.map(s => s.title).join(" · ") : "ждёт экран"
                    lit: naomiStream.enabled && live.length > 0
                    onOpened: root.section = "screens"
                }
            }
            Column {
                width: parent.width
                spacing: 4
                Caps { text: "сети"; color: Theme.amber; font.pixelSize: 10 }
                NavRow {
                    key: "wi-fi"
                    value: naomiPhone.wifi.state === "connected" ? naomiPhone.wifi.ssid : Theme.wifiLabel(naomiPhone.wifi)
                    lit: naomiPhone.wifi.state === "connected"
                    onOpened: root.section = "wifi"
                }
                NavRow {
                    readonly property var dev: naomiPhone.bluetooth.devices.find(d => d.connected)
                    key: "bluetooth"
                    value: !naomiPhone.bluetooth.powered ? "выкл" : dev ? dev.name : "вкл"
                    lit: dev !== undefined
                    onOpened: root.section = "bluetooth"
                }
            }
            Column {
                width: parent.width
                spacing: 9
                Caps { text: "о телефоне"; color: Theme.amber; font.pixelSize: 10 }
                Line {
                    key: "батарея"
                    value: naomiPhone.battery.percent + "% · " + (naomiPhone.battery.charging ? "заряжается" : "разряд")
                           + " · " + root.sample.currentMa + " ма"
                }
                Line {
                    key: "тепло"
                    value: "cpu " + root.sample.cpuC.toFixed(1) + "° · корпус " + root.sample.skinC.toFixed(1) + "°"
                }
                Line {
                    key: "wi-fi"
                    value: naomiPhone.wifi.state === "connected" ? naomiPhone.wifi.ssid + " · " + naomiPhone.wifi.strength + "%"
                           : naomiPhone.wifi.state === "off" ? "выкл" : "нет"
                }
                Line {
                    key: "сеть"
                    value: naomiPhone.cell.state === "registered" || naomiPhone.cell.state === "roaming"
                           ? naomiPhone.cell.operator + " · " + naomiPhone.cell.strength + "%"
                           : Theme.cellLabel(naomiPhone.cell.state)
                }
                Line {   // кадр меньше панели — его растягивает дисплей
                    readonly property size r: naomiDisplay.renderSize
                    readonly property size p: naomiDisplay.panelSize
                    key: "экран"
                    value: (r.width < p.width ? r.width + " × " + r.height + " → " : "") + p.width + " × " + p.height
                           + " · " + naomiDisplay.refreshHz.toFixed(0) + " гц"
                }
                Line { key: "система"; value: "ubuntu touch 24.04 · halium 13" }
                Line { key: "оболочка"; value: "наомифон 0.3" }
            }
            Column {   // раз в 3 с, пока «Настройки» открыты (naomiSystem, system_stats.h)
                width: parent.width
                spacing: 9
                Caps { text: "нагрузка"; color: Theme.amber; font.pixelSize: 10 }
                Line {
                    key: "память"
                    value: root.sys.memTotal > 0
                           ? root.num(root.sys.memUsed / 1073741824, 1) + " из " + root.num(root.sys.memTotal / 1073741824, 1)
                             + " гб · " + Math.round(100 * root.sys.memUsed / root.sys.memTotal) + "%"
                           : "…"
                }
                Line {
                    key: "процессор"
                    value: (root.sys.cpuLoad < 0 ? "…" : Math.round(root.sys.cpuLoad) + "%") + " · " + root.sys.cores + " ядер"
                }
                Line {   // кластеры: малые ядра · большое · главное
                    key: "частота"
                    value: root.sys.freqs.length ? root.sys.freqs.map(f => root.num(f, 2)).join(" · ") + " ггц" : "…"
                }
            }
            Column {
                width: parent.width
                spacing: 9
                Caps { text: "адреса и трафик"; color: Theme.amber; font.pixelSize: 10 }
                Line { key: "wi-fi"; value: root.netLine("wifi") || "нет адреса" }
                Line { key: "мобильная"; value: root.netLine("mobile") || "нет адреса" }
                Line { key: "кабель"; visible: root.netLine("cable") !== ""; value: root.netLine("cable") }
                Line { key: "трафик"; value: "↓ " + root.rate(root.sys.rxRate) + " · ↑ " + root.rate(root.sys.txRate) }
            }
        }
    }

    WifiPage {
        id: wifiPage
        width: parent.width
        height: parent.height
        x: secAx.newDelta
        opacity: secAx.newAlpha
        visible: secAx.newAlpha > 0 && root.shown === "wifi"
        active: root.open && root.section === "wifi"
        inUse: root.shown === "wifi"
        keyboardHeight: root.keyboardHeight
        onBackRequested: root.back()
    }
    BluetoothPage {
        id: btPage
        width: parent.width
        height: parent.height
        x: secAx.newDelta
        opacity: secAx.newAlpha
        visible: secAx.newAlpha > 0 && root.shown === "bluetooth"
        active: root.open && root.section === "bluetooth"
        inUse: root.shown === "bluetooth"
        onBackRequested: root.back()
    }
    ScreensPage {
        width: parent.width
        height: parent.height
        x: secAx.newDelta
        opacity: secAx.newAlpha
        visible: secAx.newAlpha > 0 && root.shown === "screens"
        active: root.open && root.section === "screens"
        inUse: root.shown === "screens"
        onBackRequested: root.back()
    }
}
