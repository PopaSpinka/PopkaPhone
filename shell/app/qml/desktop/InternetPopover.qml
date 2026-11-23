// Окно «Интернет» под значком интернета на панели — своё, НаомиФона (docs/DESKTOP.md, «Интернет»; вид выбран
// пользователем из макетов: окно Б9, значок кабеля А3): модуль 288, высота — по содержимому. Шильдик «03 ИНТЕРНЕТ»,
// справа в нём — выключатели квадратными кнопками-значками, как лупа у окна Bluetooth: кабель (три узла; только когда
// на кабеле приставка) и Wi-Fi (веер); горит — путь включён. Ниже — строка приставки: грань бирюзой и «интернет» —
// интернет идёт по кабелю, приглушённая «без интернета» — у приставки его нет, тусклая «выключен» — кабель выключен
// здесь (интернет по Wi-Fi, картинка стола — по-прежнему по кабелю). Выбор помнит стол (internet.conf) и ставит его
// службе (naomiPhone.cable, docs/NAOMID.md); до перезагрузки его помнит и она.
// Дальше — Wi-Fi, как было в окне Wi-Fi (по подобию окна Bluetooth): свои сети (подключённая первой, дальше
// сохранённые, что рядом, по имени; у подключённой, пока интернет по кабелю, — «запасной»), под ними «СЕТИ РЯДОМ» —
// свёрнуты: щелчок по подзаголовку (число сетей и стрелка) — раскрыть, ещё щелчок — свернуть; закрыли окно —
// свёрнуты снова. Рядом — по силе (числу дуг), при равной — по имени, не больше 8. Строка — имя; грань слева —
// состояние: бирюза — подключено, янтарь — подключается или под строкой открыто поле, красная — не вышло (справа —
// почему), приглушённая — сохранённая, серая — чужая. Справа — сила сигнала дугами, у закрытой несохранённой — замок.
// Щелчок: по сохранённой или открытой — подключиться, по закрытой — поле пароля под строкой (клавиатура приставки;
// Enter — подключить, Esc — убрать), по подключённой — ОТКЛЮЧИТЬ и ЗАБЫТЬ под строкой; правая кнопка по сохранённой —
// ПОДКЛЮЧИТЬ и ЗАБЫТЬ; ещё щелчок по той же строке — убрать. Поле и кнопки — не в строке, а между двумя частями списка:
// строки пересоздаются, когда сетей становится больше или меньше, и унесли бы набранный пароль. Пароль не подошёл —
// поле открывается снова. Пока окно открыто, список живой (Scan раз в 30 с, служба пересобирает его после каждого
// скана); закрыли — поле убрано, пароль стёрт, клавиатура — где была. Данные и действия — служба железа naomid
// (naomiPhone.wifi, naomiPhone.cable). Появляется и уходит как окно Bluetooth.
import QtQuick
import QtCore
import Naomi.Ui as Ui

Item {
    id: pop
    property bool open: false
    readonly property var wifi: naomiPhone.wifi
    readonly property var cable: naomiPhone.cable
    readonly property bool available: naomiPhone.available
    readonly property bool on: available && wifi.enabled
    readonly property bool dock: available && cable.dock   // на кабеле приставка
    property bool nearOpen: false   // «Сети рядом» раскрыты (открыли окно — свёрнуты)

    // интернет по кабелю — выбор стола: помнит он, служба — до перезагрузки; расходятся (запуск оболочки, служба
    // перезапустилась) — служба получает выбор стола
    Settings {
        id: netSettings
        location: StandardPaths.writableLocation(StandardPaths.ConfigLocation) + "/naomi/desktop/internet.conf"
        category: "internet"
        property bool cable: true
    }
    function setCable(on) {
        netSettings.cable = on
        cable.call("SetEnabled", [on])
    }
    readonly property bool cableDiffers: available && cable.enabled !== netSettings.cable
    onCableDiffersChanged: if (cableDiffers) Qt.callLater(() => { if (pop.cableDiffers) pop.cable.call("SetEnabled", [netSettings.cable]) })
    property var errors: ({})   // ssid → почему не вышло (Failed)
    property string pending: "" // сеть, к которой только что начали подключаться отсюда (до ответа NetworkManager)
    property string sel: ""     // сеть, под строкой которой открыто поле пароля или кнопки
    property string mode: ""    // "" — ничего не открыто; password; actions
    property var keysBack: null // у кого была клавиатура до поля пароля

    readonly property var nets: on ? wifi.networks : []
    // порядок — спокойный: служба сортирует по силе, а она пляшет от скана к скану, и строка уезжала из-под мыши.
    // Свои — подключённая, дальше по имени; рядом — по числу дуг, внутри — по имени
    readonly property var mine: nets.filter(n => n.active || n.known)
        .sort((a, b) => a.active !== b.active ? (a.active ? -1 : 1) : a.ssid.localeCompare(b.ssid))
    readonly property var near: {
        const all = nets.filter(n => !n.active && !n.known).sort((a, b) =>
            DeskStyle.wifiArcs(b.strength) - DeskStyle.wifiArcs(a.strength) || a.ssid.localeCompare(b.ssid))
        const top = all.slice(0, 8)
        const s = all.find(n => n.ssid === pop.sel)
        if (s && !top.includes(s))   // открытая под полем — на виду, даже если ослабла
            top.push(s)
        return top
    }
    readonly property var selNet: mode !== "" ? nets.find(n => n.ssid === sel) ?? null : null
    readonly property bool selMine: selNet !== null && (selNet.active || selNet.known)
    // строки до поля (с той, под которой оно) и после
    function head(list) {
        const i = selNet !== null ? list.findIndex(n => n.ssid === sel) : -1
        return i < 0 ? list : list.slice(0, i + 1)
    }
    function tail(list) {
        const i = selNet !== null ? list.findIndex(n => n.ssid === sel) : -1
        return i < 0 ? [] : list.slice(i + 1)
    }
    readonly property var mineHead: head(mine)
    readonly property var mineTail: tail(mine)
    readonly property var nearHead: head(near)
    readonly property var nearTail: tail(near)

    function without(map, key) {
        const out = Object.assign({}, map)
        delete out[key]
        return out
    }
    function failText(reason) {
        return ({ password: "пароль не подошёл", notfound: "сети нет рядом", unsupported: "защита не та",
                  timeout: "сеть не ответила", error: "не вышло" })[reason] ?? "не вышло"
    }
    function expand(ssid, how) {
        collapse()
        if (!mine.some(n => n.ssid === ssid))   // сеть из «рядом» — показать их
            nearOpen = true
        sel = ssid
        mode = how
        if (how !== "password")
            return
        if (!keysBack)
            keysBack = pop.Window.activeFocusItem
        Qt.callLater(() => { if (pop.mode === "password") field.forceActiveFocus() })
    }
    function collapse() {
        const typing = field.activeFocus
        mode = ""
        sel = ""
        field.text = ""
        field.shown = false
        if (typing) {
            if (keysBack)
                keysBack.forceActiveFocus()
            else
                field.focus = false
        }
        keysBack = null
    }
    function connectTo(ssid, password) {
        errors = without(errors, ssid)
        pending = ssid
        pendingTimer.restart()
        wifi.call("Connect", [ssid, password])
        collapse()
    }
    function activate(n, right) {   // щелчок по строке: right — правой кнопкой
        if (sel === n.ssid && mode !== "") {
            collapse()
            return
        }
        if (n.active || (right && n.known)) {
            expand(n.ssid, "actions")
            return
        }
        if (right)
            return
        if (n.known || !n.secure)
            connectTo(n.ssid, "")
        else
            expand(n.ssid, "password")
    }

    Connections {
        target: pop.wifi
        function onBusSignal(name, args) {
            if (name !== "Failed")
                return
            const ssid = args[0], reason = args[1], ours = pop.pending === ssid
            if (ours)
                pop.pending = ""
            pop.errors = Object.assign({}, pop.errors, { [ssid]: reason })
            // пароль не подошёл к сети, начатой отсюда, — ввести снова
            if (ours && reason === "password" && pop.open && pop.mode === "")
                pop.expand(ssid, "password")
        }
    }
    // подключение, начатое отсюда, дошло — или не дошло ни до чего за 45 с (NetworkManager молчит)
    readonly property bool pendingDone: pending !== "" && wifi.state === "connected" && wifi.ssid === pending
    onPendingDoneChanged: if (pendingDone) pending = ""
    Timer { id: pendingTimer; interval: 45000; onTriggered: pop.pending = "" }
    // список живой, пока окно открыто: служба пересобирает его после каждого скана минуту после Scan
    Timer {
        interval: 30000
        repeat: true
        triggeredOnStart: true
        running: pop.open && pop.on
        onTriggered: pop.wifi.call("Scan")
    }
    // открытая под полем сеть пропала (ушла из списка, Wi-Fi выключили) — убрать поле; «рядом» свернули — тоже
    onSelNetChanged: if (mode !== "" && selNet === null) collapse()
    onNearOpenChanged: if (!nearOpen && mode !== "" && !selMine) collapse()

    width: 288
    height: 42 + content.height + 14

    // ---------- появление (как у окна Bluetooth): t — ход открытия или закрытия, 0…1 ----------
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
            collapse()
            errors = {}
            nearOpen = false
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

    // ---------- шильдик: выключатели кабеля и Wi-Fi — квадратные кнопки-значки (как лупа у окна Bluetooth,
    // bt_scan_button): горит — путь включён (заливка янтарём, значок янтарём), нет — пусто, значок приглушённый ----------
    readonly property int btnH: DeskStyle.plateH + 6
    component Switch: Item {
        id: sw
        property bool on: false
        property alias kind: glyph.kind
        property alias unit: glyph.unit
        property real dy: 0   // значок по высоте — на середину кнопки
        signal clicked()
        width: pop.btnH; height: pop.btnH
        Rectangle {   // draw_plate_button
            anchors.fill: parent
            color: sw.on ? DeskStyle.amberA(area.containsMouse ? 0.13 : 0.09)
                 : area.containsMouse ? DeskStyle.amberA(area.pressed ? DeskStyle.itemPress : DeskStyle.itemHover)
                 : "transparent"
            border.width: 1
            border.color: DeskStyle.amberA(DeskStyle.line)
        }
        Glyph {
            id: glyph
            x: parent.width / 2; y: parent.height / 2 + sw.dy
            color: sw.on ? DeskStyle.amber : area.containsMouse ? DeskStyle.text : DeskStyle.muted
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            onClicked: sw.clicked()
        }
    }
    Plate {
        x: 16; y: 14
        width: 256
        num: "03"
        title: "ИНТЕРНЕТ"
        reserve: pop.available ? pop.btnH + (pop.dock ? pop.btnH + 6 : 0) : 0
    }
    Switch {   // Wi-Fi
        visible: pop.available
        x: 16 + 256 - width; y: 14 + (DeskStyle.plateH - height) / 2
        on: pop.wifi.enabled
        kind: "wifi"
        unit: 0.9
        dy: -1.1 * 0.9   // веер со точкой: середина рисунка — выше его нуля
        onClicked: pop.wifi.call("SetEnabled", [!pop.wifi.enabled])
    }
    Switch {   // кабель: только когда на кабеле приставка
        visible: pop.dock
        x: 16 + 256 - pop.btnH - 6 - width; y: 14 + (DeskStyle.plateH - height) / 2
        on: pop.cable.enabled
        kind: "lan"
        unit: 0.85
        onClicked: pop.setCable(!pop.cable.enabled)
    }

    // ---------- подсказка — как у окна Bluetooth (bt_hint) ----------
    TextMetrics { id: capMetrics; font.family: DeskStyle.mono; font.pixelSize: 12; text: "Н" }
    FontMetrics { id: hintMetrics; font.family: DeskStyle.mono; font.pixelSize: 12 }
    component Hint: Item {
        id: hint
        property alias text: hintText.text
        property alias color: hintText.color
        property real above: 4
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

    // ---------- сила сигнала дугами: горящие — цветом, остальные — тускло ----------
    component Fan: Item {
        id: fan
        property int arcs: 3
        property color color: DeskStyle.muted
        property real unit: 1
        Glyph { kind: "wifi"; unit: fan.unit; arcFrom: fan.arcs; color: DeskStyle.unlit }
        Glyph { kind: "wifi"; unit: fan.unit; arcTo: fan.arcs; color: fan.color }
    }

    // ---------- строка сети: 40 точек, имя. Строки держатся, пока сетей в части столько же: модель Repeater — число,
    // сеть — list[index] (массив-модель пересоздавал все строки после каждого скана и сдвига силы сигнала) ----------
    component NetRow: Item {
        id: row
        required property int index
        property var list: []
        readonly property var net: list[index] ?? ({ ssid: "", strength: 0, secure: false, known: false, active: false })
        readonly property bool active: net.active
        readonly property bool known: net.known
        readonly property bool linked: active && pop.wifi.state === "connected"
        readonly property bool busy: (active && pop.wifi.state === "connecting")
                                     || (pop.pending === net.ssid && !linked)
        readonly property bool picked: pop.mode !== "" && pop.sel === net.ssid
        readonly property string error: linked || busy ? "" : pop.errors[net.ssid] ?? ""
        readonly property bool hover: area.containsMouse
        readonly property bool locked: net.secure && !known && !active
        readonly property bool spare: linked && pop.cable.active   // на связи, но интернет идёт по кабелю
        readonly property real sideW: error !== "" ? Math.ceil(errText.implicitWidth) + 12
                                    : busy ? 22 : (locked ? 6.4 + 6 : 0) + (spare ? Math.ceil(spareText.implicitWidth) + 6 : 0)
                                                  + 11.1 + 12
        width: 256
        height: 40

        Rectangle {   // под курсором — заливка, нажата — плотнее; открыто поле под строкой — как под курсором
            anchors.fill: parent
            visible: row.hover || row.picked
            color: DeskStyle.amberA(area.pressed ? DeskStyle.itemPress : DeskStyle.itemHover)
        }
        Rectangle {   // грань — состояние
            y: 9
            width: 3; height: 40 - 18
            color: row.busy || row.picked ? DeskStyle.amber : row.linked ? DeskStyle.teal
                 : row.error !== "" ? DeskStyle.bad : row.known ? DeskStyle.muted : DeskStyle.dim
        }
        Text {   // почему не вышло
            id: errText
            visible: row.error !== ""
            x: 256 - 8 - implicitWidth
            height: 40
            verticalAlignment: Text.AlignVCenter
            text: pop.failText(row.error)
            font.family: DeskStyle.mono
            font.pixelSize: 12
            font.letterSpacing: 12.5 * 0.04
            color: DeskStyle.bad
        }
        Ui.Spinner {   // идёт подключение
            visible: row.busy
            size: 12
            x: 256 - 8 - 6 - size / 2; y: 20 - size / 2
            color: DeskStyle.amber
        }
        Text {   // запасной путь: интернет сейчас по кабелю
            id: spareText
            visible: row.spare && row.error === "" && !row.busy
            x: 256 - 8 - 11.1 - 6 - implicitWidth
            height: 40
            verticalAlignment: Text.AlignVCenter
            text: "запасной"
            font.family: DeskStyle.mono
            font.pixelSize: 12
            font.letterSpacing: 12.5 * 0.04
            color: DeskStyle.dim
        }
        Glyph {   // замок закрытой сети, к которой пароля ещё нет
            visible: row.locked && row.error === "" && !row.busy
            kind: "lock"
            unit: 0.8
            x: 256 - 8 - 11.1 - 6 - 3.2; y: 20
            color: DeskStyle.dim
        }
        Fan {
            visible: row.error === "" && !row.busy
            unit: 0.8
            arcs: DeskStyle.wifiArcs(row.net.strength)
            color: row.linked ? DeskStyle.teal : DeskStyle.muted
            x: 256 - 8 - 11.1 / 2; y: 20
        }
        Text {
            x: 15
            width: 256 - 8 - row.sideW - x
            height: 40
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            text: row.net.ssid
            font.family: DeskStyle.sans
            font.pixelSize: 16
            color: row.active || !row.known ? DeskStyle.text : DeskStyle.muted
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
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (e) => pop.activate(row.net, e.button === Qt.RightButton)
        }
    }
    component Slot: Item {   // место под поле или кнопки в столбце: само поле — panel, поверх
        required property bool here
        width: 256
        height: here && panel.visible ? panel.height : 0
    }

    // ---------- содержимое под шильдиком ----------
    Column {
        id: content
        x: 16; y: 42
        width: 256
        Hint {
            visible: !pop.available
            text: "интернет недоступен — служба телефона не отвечает"
        }
        Item {   // приставка на кабеле: строка как у сети, справа — состояние и три узла
            visible: pop.dock
            width: 256
            height: 40
            readonly property color tone: !pop.cable.enabled ? DeskStyle.dim : pop.cable.active ? DeskStyle.teal : DeskStyle.muted
            Rectangle {   // грань — состояние
                y: 9
                width: 3; height: 40 - 18
                color: parent.tone
            }
            Text {
                x: 15
                width: 256 - 8 - 11.9 - 6 - cableState.implicitWidth - 12 - x
                height: 40
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                text: "Приставка Pi"
                font.family: DeskStyle.sans
                font.pixelSize: 16
                color: pop.cable.enabled ? DeskStyle.text : DeskStyle.muted
            }
            Text {
                id: cableState
                x: 256 - 8 - 11.9 - 6 - implicitWidth
                height: 40
                verticalAlignment: Text.AlignVCenter
                text: !pop.cable.enabled ? "выключен" : pop.cable.active ? "интернет" : "без интернета"
                font.family: DeskStyle.mono
                font.pixelSize: 12
                font.letterSpacing: 12.5 * 0.04
                color: pop.cable.active ? DeskStyle.teal : DeskStyle.dim
            }
            Glyph {
                kind: "lan"
                unit: 0.8
                x: 256 - 8 - 11.9 / 2; y: 20
                color: parent.tone
            }
            Rectangle {   // волосяной разделитель
                y: 40 - 1
                width: 256; height: 1
                color: DeskStyle.amberA(DeskStyle.line)
            }
        }
        Hint {
            visible: pop.available && !pop.wifi.enabled
            text: "wi-fi выключен — включите значком справа"
        }
        Repeater {   // свои — сразу под строкой приставки, без заголовка
            model: pop.mineHead.length
            delegate: NetRow { list: pop.mineHead }
        }
        Slot { id: slotMine; here: pop.selMine }
        Repeater {
            model: pop.mineTail.length
            delegate: NetRow { list: pop.mineTail }
        }
        Hint {
            visible: pop.on && pop.nets.length === 0
            text: "ищу сети рядом…"
        }
        Item {   // «СЕТИ РЯДОМ» — как «НАЙДЕНЫ РЯДОМ» у окна Bluetooth (draw_subplate), но кнопкой: число и стрелка справа,
                 // щелчок — раскрыть или свернуть
            visible: pop.on && pop.near.length > 0
            width: 256
            height: 14 + DeskStyle.plateH + 4
            readonly property color ink: nearArea.containsMouse ? DeskStyle.text : DeskStyle.dim
            Text {
                id: subText
                y: 14
                height: DeskStyle.plateH
                verticalAlignment: Text.AlignVCenter
                text: "СЕТИ РЯДОМ"
                font.family: DeskStyle.mono
                font.pixelSize: 12
                font.letterSpacing: 11.5 * 0.18
                color: parent.ink
            }
            Rectangle {
                x: subText.implicitWidth + 9
                y: 14 + Math.round(DeskStyle.plateH / 2)
                width: nearCount.x - 9 - x; height: 1
                color: DeskStyle.amberA(DeskStyle.line)
            }
            Text {
                id: nearCount
                x: 256 - 14 - implicitWidth
                y: 14
                height: DeskStyle.plateH
                verticalAlignment: Text.AlignVCenter
                text: pop.near.length
                font.family: DeskStyle.mono
                font.pixelSize: 12
                font.letterSpacing: 11.5 * 0.1
                color: parent.ink
            }
            Glyph {   // стрелка: свёрнуто — вправо, раскрыто — вниз
                kind: "forward"
                unit: 0.75
                x: 256 - 4; y: 14 + DeskStyle.plateH / 2
                rotation: pop.nearOpen ? 90 : 0
                color: parent.ink
            }
            MouseArea {
                id: nearArea
                y: 8
                width: 256; height: parent.height - 8
                hoverEnabled: true
                onClicked: pop.nearOpen = !pop.nearOpen
            }
        }
        Repeater {
            model: pop.nearOpen ? pop.nearHead.length : 0
            delegate: NetRow { list: pop.nearHead }
        }
        Slot { id: slotNear; here: !pop.selMine && pop.nearOpen }
        Repeater {
            model: pop.nearOpen ? pop.nearTail.length : 0
            delegate: NetRow { list: pop.nearTail }
        }
    }

    // ---------- поле пароля или кнопки — под выбранной строкой (место держит Slot) ----------
    Item {
        id: panel
        visible: pop.selNet !== null
        x: 16
        y: content.y + (pop.selMine ? slotMine.y : slotNear.y)
        width: 256
        height: panelCol.height + 10
        Column {
            id: panelCol
            y: 8
            width: 256
            spacing: 6
            Hint {   // стол идёт через эту же сеть — отключить её значит потерять стол
                visible: pop.mode === "actions" && pop.selNet !== null && pop.selNet.active
                         && naomiStream.route === "wifi"
                above: 0; below: 2
                text: "стол идёт через эту сеть — без неё он пропадёт"
            }
            Rectangle {   // поле пароля: поверхность, рамка янтарём, пока печатают, — ярче (как имя в «Файлах»)
                visible: pop.mode === "password"
                width: 256
                height: DeskStyle.btnH
                radius: 3
                color: DeskStyle.bg
                border.width: 1
                border.color: DeskStyle.amberA(field.activeFocus ? 0.55 : DeskStyle.line)
                TextInput {
                    id: field
                    property bool shown: false
                    x: 10
                    width: parent.width - x - showText.width - 18
                    height: parent.height
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    font.family: DeskStyle.sans
                    font.pixelSize: 15
                    color: DeskStyle.text
                    selectionColor: DeskStyle.amberA(0.35)
                    selectedTextColor: DeskStyle.text
                    selectByMouse: true
                    echoMode: shown ? TextInput.Normal : TextInput.Password
                    passwordCharacter: "•"
                    cursorDelegate: Rectangle { width: 1.5; color: DeskStyle.amber; visible: field.activeFocus }
                    Keys.onReturnPressed: if (text.length >= 8) pop.connectTo(pop.sel, text)
                    Keys.onEnterPressed: if (text.length >= 8) pop.connectTo(pop.sel, text)
                    Keys.onEscapePressed: pop.collapse()
                    Text {
                        visible: field.text === ""
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: "пароль, от 8 знаков"
                        font.family: DeskStyle.mono
                        font.pixelSize: 12
                        font.letterSpacing: 12.5 * 0.04
                        color: DeskStyle.dim
                    }
                }
                Text {   // показать или скрыть набранное
                    id: showText
                    x: parent.width - 10 - width
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: field.shown ? "СКРЫТЬ" : "ПОКАЗАТЬ"
                    font.family: DeskStyle.mono
                    font.pixelSize: 11
                    font.letterSpacing: 11 * 0.12
                    color: showArea.containsMouse ? DeskStyle.text : DeskStyle.muted
                    MouseArea {
                        id: showArea
                        anchors.fill: parent
                        anchors.margins: -6
                        hoverEnabled: true
                        onClicked: field.shown = !field.shown
                    }
                }
            }
            Row {
                spacing: 6
                NButton {   // подключить: с паролем — от 8 знаков (WPA короче не примет), иначе — по сохранённому
                    visible: pop.mode === "password" || (pop.selNet !== null && !pop.selNet.active)
                    width: 125
                    text: "подключить"
                    tone: pop.mode !== "password" || field.text.length >= 8 ? "on" : "off"
                    onClicked: {
                        if (pop.mode !== "password")
                            pop.connectTo(pop.sel, "")
                        else if (field.text.length >= 8)
                            pop.connectTo(pop.sel, field.text)
                    }
                }
                NButton {
                    visible: pop.mode === "actions" && pop.selNet !== null && pop.selNet.active
                    width: 125
                    text: "отключить"
                    onClicked: {
                        pop.wifi.call("Disconnect")
                        pop.collapse()
                    }
                }
                NButton {
                    visible: pop.mode === "actions" && pop.selNet !== null && pop.selNet.known
                    width: 125
                    text: "забыть"
                    onClicked: {
                        pop.wifi.call("Forget", [pop.sel])
                        pop.collapse()
                    }
                }
                NButton {
                    visible: pop.mode === "password"
                    width: 125
                    text: "отмена"
                    onClicked: pop.collapse()
                }
            }
        }
        Rectangle {   // волосяной разделитель снизу
            y: parent.height - 1
            width: 256; height: 1
            color: DeskStyle.amberA(DeskStyle.line)
        }
    }
}
