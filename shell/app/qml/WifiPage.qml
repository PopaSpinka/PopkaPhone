// «Настройки» → Wi-Fi: включить, сети рядом, подключиться (пароль — своей клавиатурой), отключиться,
// забыть. Данные и действия — служба naomid (naomiPhone.wifi, docs/NAOMID.md). Пока страница на экране,
// она раз в 30 с просит службу искать сети: список живой, только пока его смотрят.
// Касание: подключённая — действия; сохранённая или открытая — подключиться; закрытая — пароль.
// Долгое нажатие на сохранённую — действия (забыть). Пароль и действия — в листе снизу, над клавиатурой,
// а не в строке списка: список пересобирается после каждого скана и унёс бы поле с набранным паролем.
import QtQuick
import Naomi.Ui

Item {
    id: root
    property bool active                // страница на экране
    property bool inUse                 // раздел открыт или открывается / закрывается (строки списка держатся)
    property real keyboardHeight: 0     // сколько снизу закрывает клавиатура, точки
    signal backRequested()

    readonly property var wifi: naomiPhone.wifi
    property string sheetSsid: ""
    property string sheetMode: ""       // "" — листа нет; password; actions
    readonly property var sheetNet: wifi.networks.find(n => n.ssid === sheetSsid) ?? ({})
    property string note: ""            // последняя неудача словом
    property string noteSsid: ""

    function back() {
        if (sheetMode === "")
            return false
        closeSheet()
        return true
    }
    function openSheet(ssid, mode) {
        sheetSsid = ssid
        sheetMode = mode
        if (mode === "password") {
            password.text = ""
            show.on = false
            password.forceActiveFocus()
            Qt.inputMethod.show()
        }
    }
    function closeSheet() {
        sheetMode = ""
        Qt.inputMethod.hide()
        root.forceActiveFocus()
    }
    function connectTo(ssid, pass) {
        note = ""
        wifi.call("Connect", [ssid, pass])
        closeSheet()
    }
    function tap(n) {
        if (n.active)
            openSheet(n.ssid, "actions")
        else if (n.known || !n.secure)
            connectTo(n.ssid, "")
        else
            openSheet(n.ssid, "password")
    }
    function hold(n) {
        if (n.known || n.active)
            openSheet(n.ssid, "actions")
    }
    function failText(reason) {
        return ({ password: "пароль не подошёл", notfound: "сети нет рядом", unsupported: "такую защиту не умею",
                  timeout: "сеть не ответила", error: "не вышло" })[reason] ?? reason
    }

    Timer {
        interval: 30000
        repeat: true
        triggeredOnStart: true
        running: root.active && naomiPower.displayOn && root.wifi.enabled
        onTriggered: root.wifi.call("Scan")
    }
    Connections {
        target: root.wifi
        function onBusSignal(name, args) {
            if (name !== "Failed")
                return
            root.noteSsid = args[0]
            root.note = root.failText(args[1])
            if (args[1] === "password" && root.active)
                root.openSheet(args[0], "password")
        }
    }
    onActiveChanged: if (!active && sheetMode !== "") closeSheet()

    Rectangle { anchors.fill: parent; color: Theme.canvas }

    Flickable {
        anchors.fill: parent
        anchors.topMargin: Theme.statusBarHeight
        contentHeight: content.height + 80 + (sheet.visible ? sheet.height + root.keyboardHeight : 0)
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
            Caps { text: "wi-fi"; color: Theme.amber; font.pixelSize: 12 }
            NToggle {
                width: parent.width
                label: "wi-fi"
                value: Theme.wifiLabel(root.wifi)
                on: root.wifi.enabled
                onClicked: root.wifi.call("SetEnabled", [!root.wifi.enabled])
            }
            Caps {
                visible: root.note !== ""
                width: parent.width
                elide: Text.ElideRight
                text: root.noteSsid + " · " + root.note
                color: Theme.bad
                font.pixelSize: 10
            }
            Caps {
                visible: root.wifi.enabled
                text: root.wifi.networks.length > 0 ? "сети рядом" : "ищу сети…"
                font.pixelSize: 10
            }
            Column {
                width: parent.width
                visible: root.wifi.enabled
                Repeater {
                    // закрытый раздел списка не держит: строки не пересобираются впустую. Строятся в начале
                    // открытия (страница ещё прозрачна), разбираются после закрытия — не посреди перехода
                    model: root.inUse ? root.wifi.networks : []
                    delegate: Item {
                        id: row
                        required property var modelData
                        readonly property bool connecting: modelData.active && root.wifi.state === "connecting"
                        width: parent.width
                        height: 54
                        Bars {
                            anchors.verticalCenter: parent.verticalCenter
                            level: Theme.signalLevel(row.modelData.strength)
                            color: row.modelData.active ? Theme.amber : Theme.text
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            x: 28
                            width: parent.width - x - status.width - 12
                            elide: Text.ElideRight
                            text: row.modelData.ssid
                            font.family: Theme.sans
                            font.pixelSize: 16
                            color: row.modelData.active ? Theme.amber : Theme.text
                        }
                        Caps {
                            id: status
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            font.pixelSize: 9
                            text: row.modelData.active ? (row.connecting ? "подключаюсь" : "подключено")
                                : row.modelData.known ? "сохранена" : row.modelData.secure ? "пароль" : "открытая"
                            color: row.modelData.active ? Theme.amber : Theme.dim
                        }
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.surfaceHigh }
                        TapHandler {
                            onTapped: root.tap(row.modelData)
                            onLongPressed: root.hold(row.modelData)
                        }
                    }
                }
            }
            Caps { visible: !root.wifi.enabled; text: "wi-fi выключен"; font.pixelSize: 10 }
        }
    }

    // лист снизу: пароль к сети или действия с ней; стоит над клавиатурой. Нижние скруглённые углы
    // уходят на 20 точек под клавиатуру или за край экрана; без клавиатуры снизу — место жесту «домой».
    Rectangle {
        id: sheet
        readonly property real pad: root.keyboardHeight > 0 ? 18 : 40
        visible: root.sheetMode !== ""
        width: parent.width
        height: sheetBody.height + 20 + pad + 20
        y: parent.height - root.keyboardHeight - height + 20
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
                text: (root.sheetMode === "password" ? "пароль · " : "") + root.sheetSsid
                color: Theme.amber
                font.pixelSize: 10
            }
            Rectangle {
                visible: root.sheetMode === "password"
                width: parent.width
                height: 48
                radius: 3
                color: Theme.canvas
                border.width: 1
                border.color: password.activeFocus ? Theme.amberEdge : Theme.surfaceHigh
                TextInput {
                    id: password
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: show.width + 28
                    verticalAlignment: TextInput.AlignVCenter
                    clip: true
                    font.family: Theme.sans
                    font.pixelSize: 18
                    color: Theme.text
                    selectionColor: Theme.amberEdge
                    echoMode: show.on ? TextInput.Normal : TextInput.Password
                    passwordMaskDelay: 600
                    // клавиатура: латиница, без заглавных сами по себе, без подсказок (keyboard_context)
                    inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
                                      | Qt.ImhLatinOnly
                    onAccepted: if (text.length >= 8) root.connectTo(root.sheetSsid, text)
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: password.text === ""
                        text: "не меньше 8 знаков"
                        font.family: Theme.sans
                        font.pixelSize: 15
                        color: Theme.dim
                    }
                }
                Caps {
                    id: show
                    property bool on: false
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: on ? "скрыть" : "показать"
                    font.pixelSize: 9
                    TapHandler { margin: 12; onTapped: show.on = !show.on }
                }
            }
            Row {
                spacing: 8
                NButton { text: "отмена"; onClicked: root.closeSheet() }
                NButton {   // WPA: пароль от 8 знаков — короче NetworkManager не примет
                    visible: root.sheetMode === "password"
                    text: "подключить"
                    accent: true
                    enabled: password.text.length >= 8
                    onClicked: root.connectTo(root.sheetSsid, password.text)
                }
                NButton {
                    visible: root.sheetMode === "actions" && !root.sheetNet.active
                    text: "подключить"
                    accent: true
                    onClicked: root.connectTo(root.sheetSsid, "")
                }
                NButton {
                    visible: root.sheetMode === "actions" && root.sheetNet.active === true
                    text: "отключить"
                    onClicked: { root.wifi.call("Disconnect"); root.closeSheet() }
                }
                NButton {
                    visible: root.sheetMode === "actions" && root.sheetNet.known === true
                    text: "забыть"
                    onClicked: { root.wifi.call("Forget", [root.sheetSsid]); root.closeSheet() }
                }
            }
        }
    }
}
