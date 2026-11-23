// «Настройки» → Bluetooth: включить, мои устройства, устройства рядом, спарить и подключить, отключить,
// забыть. Данные и действия — служба naomid (naomiPhone.bluetooth, docs/NAOMID.md). Пока страница на
// экране, она раз в 25 с просит службу искать устройства (поиск идёт 30 с после последней просьбы).
// Касание: подключённое — действия; своё — подключить; чужое — спарить и подключить. Долгое нажатие на
// своё — действия. Код спаривания (телефон, компьютер) — в листе снизу: совпадает ли он с кодом на
// устройстве. Наушники и колонки спариваются без кода.
import QtQuick
import Naomi.Ui

Item {
    id: root
    property bool active                // страница на экране
    property bool inUse                 // раздел открыт или открывается / закрывается (строки списка держатся)
    signal backRequested()

    readonly property var bt: naomiPhone.bluetooth
    readonly property var mine: bt.devices.filter(d => d.paired)
    readonly property var nearby: bt.devices.filter(d => !d.paired)
    property string sheetAddress: ""
    property string sheetName: ""
    property string sheetMode: ""       // "" — листа нет; actions; confirm (сверить код); display (ввести код)
    property string sheetCode: ""
    readonly property var sheetDev: bt.devices.find(d => d.address === sheetAddress) ?? ({})
    property string note: ""
    property string noteName: ""

    function back() {
        if (sheetMode === "")
            return false
        if (sheetMode === "confirm")
            bt.call("ConfirmPairing", [false])
        sheetMode = ""
        return true
    }
    function openSheet(d, mode) {
        sheetAddress = d.address
        sheetName = d.name
        sheetMode = mode
    }
    function tap(d) {
        if (d.busy)
            return
        if (d.connected) {
            openSheet(d, "actions")
        } else {
            note = ""
            bt.call("Connect", [d.address])
        }
    }
    function hold(d) {
        if (d.paired)
            openSheet(d, "actions")
    }
    function failText(reason) {
        return ({ pairing: "не спарилось", connect: "не подключилось", notfound: "нет рядом",
                  error: "не вышло" })[reason] ?? reason
    }

    Timer {
        interval: 25000
        repeat: true
        triggeredOnStart: true
        running: root.active && naomiPower.displayOn && root.bt.powered
        onTriggered: root.bt.call("Scan")
    }
    Connections {
        target: root.bt
        function onBusSignal(name, args) {
            if (name === "Failed") {
                const d = root.bt.devices.find(x => x.address === args[0])
                root.noteName = d ? d.name : args[0]
                root.note = root.failText(args[1])
                if (root.sheetMode === "confirm" || root.sheetMode === "display")
                    root.sheetMode = ""
            } else if (name === "Pairing") {
                if (args[3] === "cancel") {
                    if (root.sheetMode === "confirm" || root.sheetMode === "display")
                        root.sheetMode = ""
                    return
                }
                root.sheetAddress = args[0]
                root.sheetName = args[1]
                root.sheetCode = args[2]
                root.sheetMode = args[3]
            }
        }
    }
    onActiveChanged: if (!active && sheetMode !== "") back()

    Rectangle { anchors.fill: parent; color: Theme.canvas }

    // строка устройства (Component, а не inline component: строке нужен id root этой страницы)
    Component {
        id: deviceRow
        Item {
            id: row
            required property var modelData
            width: parent ? parent.width : 0
            height: 54
            readonly property string kind: Theme.deviceKind(modelData.icon)
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - status.width - 12
                elide: Text.ElideRight
                text: row.modelData.name
                font.family: Theme.sans
                font.pixelSize: 16
                color: row.modelData.connected ? Theme.amber : Theme.text
            }
            Caps {
                id: status
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                font.pixelSize: 9
                text: row.modelData.busy ? (row.modelData.connected ? "отключаю" : row.modelData.paired ? "подключаю" : "спариваю")
                    : row.modelData.connected ? "подключено" : row.kind
                color: row.modelData.connected || row.modelData.busy ? Theme.amber : Theme.dim
            }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.surfaceHigh }
            TapHandler {
                onTapped: root.tap(row.modelData)
                onLongPressed: root.hold(row.modelData)
            }
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.topMargin: Theme.statusBarHeight
        contentHeight: content.height + 80 + (sheet.visible ? sheet.height : 0)
        boundsBehavior: Flickable.DragOverBounds
        Column {
            id: content
            x: Theme.margin
            y: 34          // как у «Настроек»: ниже HUD разработчика
            width: root.width - 2 * Theme.margin
            spacing: 16
            Caps {
                text: "‹ настройки"
                font.pixelSize: 10
                TapHandler { margin: 12; onTapped: root.backRequested() }
            }
            Caps { text: "bluetooth"; color: Theme.amber; font.pixelSize: 12 }
            NToggle {
                width: parent.width
                label: "bluetooth"
                value: root.bt.powered ? "вкл" : "выкл"
                on: root.bt.powered
                onClicked: root.bt.call("SetPowered", [!root.bt.powered])
            }
            Caps {
                visible: root.note !== ""
                width: parent.width
                elide: Text.ElideRight
                text: root.noteName + " · " + root.note
                color: Theme.bad
                font.pixelSize: 10
            }
            Column {
                width: parent.width
                visible: root.bt.powered && root.mine.length > 0
                spacing: 4
                Caps { text: "мои устройства"; font.pixelSize: 10 }
                Repeater { model: root.inUse ? root.mine : []; delegate: deviceRow }   // как в WifiPage
            }
            Column {
                width: parent.width
                visible: root.bt.powered
                spacing: 4
                Caps {
                    text: root.nearby.length > 0 ? "рядом" : root.bt.discovering ? "ищу устройства…" : "рядом никого"
                    font.pixelSize: 10
                }
                Repeater { model: root.inUse ? root.nearby : []; delegate: deviceRow }
            }
            Caps { visible: !root.bt.powered; text: "bluetooth выключен"; font.pixelSize: 10 }
        }
    }

    // лист снизу: действия с устройством или код спаривания
    Rectangle {
        id: sheet
        visible: root.sheetMode !== ""
        width: parent.width
        height: sheetBody.height + 20 + 40 + 20
        y: parent.height - height + 20
        color: Theme.canvas
        radius: 18
        border.width: 1
        border.color: Theme.amberEdge
        MouseArea { anchors.fill: parent }   // касания листа не уходят в список под ним

        Column {
            id: sheetBody
            x: Theme.margin
            y: 20
            width: parent.width - 2 * Theme.margin
            spacing: 14
            Caps {
                width: parent.width
                elide: Text.ElideRight
                text: (root.sheetMode === "confirm" || root.sheetMode === "display" ? "спаривание · " : "")
                      + root.sheetName
                color: Theme.amber
                font.pixelSize: 10
            }
            Text {   // код спаривания крупно: 123 456
                visible: root.sheetMode === "confirm" || root.sheetMode === "display"
                text: root.sheetCode.length === 6 ? root.sheetCode.slice(0, 3) + " " + root.sheetCode.slice(3)
                                                  : root.sheetCode
                font.family: Theme.mono
                font.pixelSize: 34
                font.letterSpacing: 4
                color: Theme.text
            }
            Caps {
                visible: root.sheetMode === "confirm" || root.sheetMode === "display"
                width: parent.width
                wrapMode: Text.WordWrap
                text: root.sheetMode === "confirm" ? "такой же код на устройстве?" : "введите этот код на устройстве"
                font.pixelSize: 10
            }
            Row {
                spacing: 8
                // код
                NButton {
                    visible: root.sheetMode === "confirm"
                    text: "нет"
                    onClicked: { root.bt.call("ConfirmPairing", [false]); root.sheetMode = "" }
                }
                NButton {
                    visible: root.sheetMode === "confirm"
                    text: "да, совпадает"
                    accent: true
                    onClicked: { root.bt.call("ConfirmPairing", [true]); root.sheetMode = "" }
                }
                NButton {
                    visible: root.sheetMode === "display"
                    text: "скрыть"
                    onClicked: root.sheetMode = ""
                }
                // действия
                NButton {
                    visible: root.sheetMode === "actions"
                    text: "отмена"
                    onClicked: root.sheetMode = ""
                }
                NButton {
                    visible: root.sheetMode === "actions" && !root.sheetDev.connected
                    text: "подключить"
                    accent: true
                    onClicked: { root.bt.call("Connect", [root.sheetAddress]); root.sheetMode = "" }
                }
                NButton {
                    visible: root.sheetMode === "actions" && root.sheetDev.connected === true
                    text: "отключить"
                    onClicked: { root.bt.call("Disconnect", [root.sheetAddress]); root.sheetMode = "" }
                }
                NButton {
                    visible: root.sheetMode === "actions" && root.sheetDev.paired === true
                    text: "забыть"
                    onClicked: { root.bt.call("Forget", [root.sheetAddress]); root.sheetMode = "" }
                }
            }
        }
    }
}
