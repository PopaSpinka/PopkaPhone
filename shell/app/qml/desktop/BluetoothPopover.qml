// Окно Bluetooth под значком на панели — перенос POPUP_BLUETOOTH НаомиОС (desk-bt.c bt_popover_draw и
// bt_popover_activate в bcdesk.c; docs/DESKTOP.md «Bluetooth»): модуль 288, высота — по содержимому.
// Шильдик «02 BLUETOOTH», справа в нём — лупа (поиск на 30 с; пока ищет — янтарная крутилка, щелчок — остановить) и
// ВКЛЮЧЁН / ВЫКЛЮЧЕН. Ниже — свои устройства (подключённые первыми, дальше по имени), под ними «НАЙДЕНЫ РЯДОМ» — пока
// идёт поиск (самые слышные первыми, не больше 7). Строка — имя; грань слева — состояние: бирюза — подключено, красная
// — своё не подключено (у найденного — не вышло), янтарь — идёт дело (справа крутилка) или код спаривания, серая —
// найдено рядом. Справа — код, который набрать на устройстве, сила сигнала у найденных, заряд у своих (≤ 15 % —
// красным). Щелчок по своему — подключить или отключить, по найденному — спарить и подключить (поиск при этом
// останавливается: тот же радиоканал). Код «совпадает ли» при спаривании, начатом отсюда, подтверждается сам, как у
// агента НаомиОС: соглашаемся только на то, что начали сами. Закрыли окно — поиск остановлен.
// Данные и действия — служба железа naomid (naomiPhone.bluetooth, docs/NAOMID.md). Крутилка — Naomi.Ui Spinner
// (ожидание НаомиФона, у НаомиОС — дуга). Появляется и уходит как окно звука (SoundPopover).
import QtQuick
import Naomi.Ui as Ui

Item {
    id: pop
    property bool open: false
    readonly property var bt: naomiPhone.bluetooth
    readonly property bool available: naomiPhone.available
    readonly property bool on: available && bt.powered
    property var codes: ({})    // адрес → код спаривания, который набрать на устройстве (Pairing display)
    property var errors: ({})   // адрес → почему не вышло (Failed)
    // адреса, спаривание которых начато отсюда: щелчок по чужому (спарить) и по своему (забыло спаривание — служба
    // спарит заново, docs/NAOMID.md «Клавиатура») — код подтверждаем сами
    property var ours: ({})

    readonly property var mine: bt.devices.filter(d => d.paired).sort((a, b) =>
        a.connected !== b.connected ? (a.connected ? -1 : 1) : a.name.localeCompare(b.name, undefined, { sensitivity: "base" }))
    readonly property var found: bt.devices.filter(d => !d.paired && (d.busy || (bt.discovering && d.rssi > -999)))
        .sort((a, b) => a.busy !== b.busy ? (a.busy ? -1 : 1) : b.rssi - a.rssi).slice(0, 7)

    function without(map, key) {
        const out = Object.assign({}, map)
        delete out[key]
        return out
    }
    function activate(d) {   // HIT_ROW: своё — подключить или отключить, найденное — спарить и подключить
        if (d.busy)
            return
        errors = without(errors, d.address)
        codes = without(codes, d.address)
        if (d.connected) {
            bt.call("Disconnect", [d.address])
            return
        }
        if (bt.discovering)
            bt.call("StopScan")
        ours = Object.assign({}, ours, { [d.address]: true })
        bt.call("Connect", [d.address])
    }
    Connections {
        target: pop.bt
        function onBusSignal(name, args) {
            const address = args[0]
            if (name === "Failed") {
                pop.errors = Object.assign({}, pop.errors, { [address]: args[1] })
                pop.codes = pop.without(pop.codes, address)
                pop.ours = pop.without(pop.ours, address)
            } else if (name === "Pairing") {
                const mode = args[3]
                if (mode === "display") {
                    pop.codes = Object.assign({}, pop.codes, { [address]: args[2] })
                } else if (mode === "confirm") {
                    if (pop.ours[address])
                        pop.bt.call("ConfirmPairing", [true])
                } else if (address === "") {   // cancel: вопрос ушёл у всех
                    pop.codes = {}
                } else {
                    pop.codes = pop.without(pop.codes, address)
                    pop.ours = pop.without(pop.ours, address)
                }
            }
        }
    }
    width: 288
    height: 42 + content.height + 14

    // ---------- появление (anim_curve): t — ход открытия или закрытия, 0…1 ----------
    property real openT: 1
    property real closeT: 1
    onOpenChanged: {
        if (open) {
            closeAnim.stop()
            closeT = 1
            openT = 0
            openAnim.restart()
        } else {
            openAnim.stop()
            closeT = 0
            closeAnim.restart()
            if (bt.discovering)   // panel_close: закрыли окно — поиск остановлен
                bt.call("StopScan")
        }
    }
    NumberAnimation { id: openAnim; target: pop; property: "openT"; from: 0; to: 1; duration: 110 }
    NumberAnimation { id: closeAnim; target: pop; property: "closeT"; from: 0; to: 1; duration: 150 }
    visible: open || closeAnim.running
    transformOrigin: Item.Top
    scale: open ? 0.98 + 0.02 * (1 - Math.pow(1 - openT, 3)) : 1 - 0.01 * (1 - Math.pow(1 - closeT, 3))
    opacity: open ? 1 : (Math.exp(-closeT * 150 / 38) - Math.exp(-150 / 38)) / (1 - Math.exp(-150 / 38))

    MouseArea { anchors.fill: parent }   // щелчок по пустому месту окна — ничего, и не мимо него

    DeskModule { anchors.fill: parent }

    // ---------- шильдик: лупа и ВКЛЮЧЁН / ВЫКЛЮЧЕН ----------
    Text {   // мерка подписи кнопки (plate_button_w): моно 11,5 (у Qt — 12), разрядка .08
        id: powerText
        visible: false
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 11.5 * 0.08
        text: pop.bt.powered ? "ВКЛЮЧЁН" : "ВЫКЛЮЧЕН"
    }
    readonly property real powerW: pop.available ? Math.ceil(powerText.implicitWidth) + 3 + 2 * 10 + 2 : 0
    readonly property int btnH: DeskStyle.plateH + 6
    Plate {
        x: 16; y: 14
        width: 256
        num: "02"
        title: "BLUETOOTH"
        reserve: pop.powerW + (pop.on ? pop.btnH + 6 : 0)
    }
    NButton {
        visible: pop.available
        x: 16 + 256 - width; y: 14 + (DeskStyle.plateH - height) / 2
        width: pop.powerW; height: pop.btnH
        text: powerText.text
        tone: pop.bt.powered ? "on" : "off"
        fontSize: powerText.font.pixelSize
        letterSpacing: powerText.font.letterSpacing
        onClicked: pop.bt.call("SetPowered", [!pop.bt.powered])
    }
    Item {   // поиск (bt_scan_button): квадратная кнопка в шильдике; пока идёт — залита, в ней крутилка
        visible: pop.on
        x: 16 + 256 - pop.powerW - 6 - width; y: 14 + (DeskStyle.plateH - height) / 2
        width: pop.btnH; height: pop.btnH
        Rectangle {   // draw_plate_button
            anchors.fill: parent
            color: pop.bt.discovering ? DeskStyle.amberA(scanArea.containsMouse ? 0.13 : 0.09)
                 : scanArea.containsMouse ? DeskStyle.amberA(scanArea.pressed ? DeskStyle.itemPress : DeskStyle.itemHover)
                 : "transparent"
            border.width: 1
            border.color: DeskStyle.amberA(DeskStyle.line)
        }
        Ui.Spinner {
            visible: pop.bt.discovering
            anchors.centerIn: parent
            size: DeskStyle.icon
            color: DeskStyle.amber
        }
        Glyph {
            visible: !pop.bt.discovering
            kind: "search"
            unit: (DeskStyle.icon - DeskStyle.iconLine) / 11.5
            x: parent.width / 2; y: parent.height / 2
            color: scanArea.containsMouse ? DeskStyle.text : DeskStyle.muted
        }
        MouseArea {
            id: scanArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: pop.bt.call(pop.bt.discovering ? "StopScan" : "Scan")
        }
    }

    // ---------- подсказка (bt_hint): моно 12,5 (у Qt — 12) приглушённым, по словам на строки через высоту заглавной + 8
    TextMetrics { id: capMetrics; font.family: DeskStyle.mono; font.pixelSize: 12; text: "Н" }
    FontMetrics { id: hintMetrics; font.family: DeskStyle.mono; font.pixelSize: 12 }
    component Hint: Item {
        id: hint
        property alias text: hintText.text
        property real above: 4   // отступ сверху и снизу — у каждой подсказки свои
        property real below: 6
        readonly property real cap: capMetrics.tightBoundingRect.height
        width: 256
        height: above + (hintText.lineCount - 1) * (cap + 8) + cap + below
        Text {
            id: hintText
            y: hint.above + hint.cap - hintMetrics.ascent
            width: parent.width
            wrapMode: Text.WordWrap
            lineHeightMode: Text.FixedHeight
            lineHeight: hint.cap + 8
            font.family: DeskStyle.mono
            font.pixelSize: 12
            font.letterSpacing: 12.5 * 0.04
            color: DeskStyle.muted
        }
    }

    // ---------- строка устройства (bt_row): 40 точек, имя ----------
    component DeviceRow: Item {
        id: row
        required property var modelData
        readonly property bool mine: modelData.paired
        readonly property bool busy: modelData.busy
        readonly property string code: modelData.connected ? "" : pop.codes[modelData.address] ?? ""
        readonly property bool failed: !modelData.connected && pop.errors[modelData.address] !== undefined
        readonly property bool hover: !busy && area.containsMouse
        // справа: код спаривания, у найденных — сигнал, у своих — заряд (с 1.43 НаомиОС)
        readonly property string side: code !== "" ? "код " + code
            : !busy && !mine ? (modelData.rssi <= -999 ? "сигнал пропал" : modelData.rssi >= -60 ? "сигнал хороший"
                                : modelData.rssi >= -75 ? "сигнал средний" : "сигнал слабый")
            : !busy && modelData.battery >= 0 ? modelData.battery + "%" : ""
        readonly property real nameRight: 256 - 8 - (side !== "" ? Math.ceil(sideText.implicitWidth) + 12 : busy ? 22 : 0)
        width: 256
        height: 40

        Rectangle {   // под курсором — заливка, нажата — плотнее
            anchors.fill: parent
            visible: row.hover
            color: DeskStyle.amberA(area.pressed ? DeskStyle.itemPress : DeskStyle.itemHover)
        }
        Rectangle {   // грань — состояние
            y: 9
            width: 3; height: 40 - 18
            color: row.busy || row.code !== "" ? DeskStyle.amber : row.modelData.connected ? DeskStyle.teal
                 : row.mine || row.failed ? DeskStyle.bad : DeskStyle.dim
        }
        Text {
            id: sideText
            visible: row.side !== ""
            x: 256 - 8 - implicitWidth
            height: 40
            verticalAlignment: Text.AlignVCenter
            text: row.side
            font.family: DeskStyle.mono
            font.pixelSize: 12
            font.letterSpacing: 12.5 * 0.04
            color: row.code !== "" ? DeskStyle.amber : !row.mine ? DeskStyle.dim
                 : row.modelData.battery <= 15 ? DeskStyle.bad : DeskStyle.muted
        }
        Ui.Spinner {   // row_spinner: идёт дело
            visible: row.busy && row.side === ""
            size: 12
            x: 256 - 8 - 6 - size / 2; y: 20 - size / 2
            color: DeskStyle.amber
        }
        Text {   // row_name
            x: 15
            width: row.nameRight - x
            height: 40
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            text: row.modelData.name
            font.family: DeskStyle.sans
            font.pixelSize: 16
            color: row.modelData.connected || !row.mine ? DeskStyle.text : DeskStyle.muted
        }
        Rectangle {   // волосяной разделитель
            y: 40 - 1
            width: 256; height: 1
            color: DeskStyle.amberA(DeskStyle.line)
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            onClicked: pop.activate(row.modelData)
        }
    }

    // ---------- содержимое под шильдиком ----------
    Column {
        id: content
        x: 16; y: 42
        width: 256
        Hint {
            visible: !pop.on
            text: pop.available ? "bluetooth выключен — включите кнопкой справа" : "bluetooth недоступен — служба телефона не отвечает"
        }
        Repeater {   // свои — сразу под шильдиком, без заголовка
            model: pop.on ? pop.mine : []
            delegate: DeviceRow {}
        }
        Hint {
            visible: pop.on && pop.mine.length === 0 && !pop.bt.discovering && pop.found.length === 0
            text: "устройств нет — найдите их значком поиска"
        }
        Item {   // «НАЙДЕНЫ РЯДОМ» (draw_subplate): моно 11,5 (у Qt — 12) тускло, разрядка .18, линия до края
            visible: pop.on && (pop.bt.discovering || pop.found.length > 0)
            width: 256
            height: 14 + DeskStyle.plateH + 4
            Text {
                id: subText
                y: 14
                height: DeskStyle.plateH
                verticalAlignment: Text.AlignVCenter
                text: "НАЙДЕНЫ РЯДОМ"
                font.family: DeskStyle.mono
                font.pixelSize: 12
                font.letterSpacing: 11.5 * 0.18
                color: DeskStyle.dim
            }
            Rectangle {
                x: subText.implicitWidth + 9
                y: 14 + Math.round(DeskStyle.plateH / 2)
                width: 256 - x; height: 1
                color: DeskStyle.amberA(DeskStyle.line)
            }
        }
        Hint {
            visible: pop.on && pop.bt.discovering && pop.found.length === 0
            above: 6; below: 8
            text: "включите на устройстве режим сопряжения — оно появится здесь"
        }
        Repeater {
            model: pop.on ? pop.found : []
            delegate: DeviceRow {}
        }
    }
}
