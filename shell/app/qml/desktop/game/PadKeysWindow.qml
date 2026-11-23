// Окно «Кнопки геймпада» (docs/GAMES.md, «Кнопки геймпада»): шестерёнка у игры в «03 GAMES» (просьба пользователя
// 21 ноября 2026; вид — вариант 01 «Выноски» из макетов). Сверху — игра (идёт — «ИДЁТ» бирюзой) и «Как есть /
// Клавишами / Сбросить»; ниже рисунок геймпада с выносками (PadArt). «Клавишами»: выбрал кнопку (на рисунке, на её
// плашке или нажав её на самом геймпаде — пока окно впереди, геймпад не играет, а выбирает: naomiPadKeys.listening) —
// снизу выезжает клавиатура (PadKeyboard); щелчок по клавише или нажатие на настоящей клавиатуре — кнопка жмёт эту
// клавишу в игре («Нет клавиши» — не жмёт ничего). Стик и крестовина — по направлениям (↑ ↓ ← → и «нажать»): назначил одно — выбрано следующее; у
// стиков — «Стрелки» и «WASD» разом. «Как есть»: рисунок показывает, что кнопка делает сама (Денди, PS1), нажатая на
// геймпаде — горит. Раскладку хранит и переводит оболочка (naomiPadKeys, pad_keys.h): правка — сразу и в идущей игре.
import QtQuick
import ".."
import "pad.js" as Pad

DeskWindow {
    id: pw
    tool: true
    property var game: null                  // {path, name, system, label} — строка naomiGames.games
    property string runningPath: ""          // какая игра идёт (Desktop.qml)
    readonly property bool running: !!game && runningPath === game.path
    property string sel: ""                  // выбранная кнопка
    property string flash: ""                // «как есть»: нажали на геймпаде
    readonly property var lay: {
        naomiPadKeys.revision
        return game ? naomiPadKeys.layout(game.path, game.system) : { mode: "native", keys: {} }
    }
    readonly property bool keysMode: lay.mode === "keys"
    readonly property var keys: lay.keys || ({})
    readonly property var nat: game ? (Pad.NATIVE[game.system] || ({})) : ({})
    readonly property string selGroup: sel ? Pad.group(sel) : ""
    readonly property var badges: {   // клавиша → метки кнопок, что её заняли
        const by = {}
        if (keysMode)
            for (const id in keys)
                if (keys[id] && Pad.CTL[id])
                    (by[keys[id]] = by[keys[id]] || []).push(Pad.CTL[id].s)
        return by
    }
    onGameChanged: sel = ""
    width: 880
    height: Math.min(barH + col.implicitHeight + 44, area ? area.height - 36 : 900)
    Behavior on height { enabled: pw.shown; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

    barContent: Text {
        x: 22
        height: pw.barH
        verticalAlignment: Text.AlignVCenter
        text: "КНОПКИ ГЕЙМПАДА"
        font.family: DeskStyle.mono
        font.pixelSize: DeskStyle.caps
        font.weight: Font.Medium
        font.letterSpacing: DeskStyle.caps * 0.2
        color: DeskStyle.amber
    }

    function save(mode, keys) {
        if (game)
            naomiPadKeys.setLayout(game.path, { mode: mode, keys: keys })
    }
    function pick(id) {
        if (!keysMode)
            return
        const grp = Pad.GROUPS[id]
        if (grp)
            sel = selGroup === id ? sel : grp[0]
        else
            sel = sel === id ? "" : id   // второй щелчок — снять выбор
    }
    function assign(k) {
        if (!sel || !keysMode)
            return
        const m = Object.assign({}, keys), done = sel
        if (k)
            m[done] = k
        else
            delete m[done]
        save("keys", m)
        const grp = Pad.GROUPS[Pad.group(done)]
        if (grp && k && grp.indexOf(done) < grp.length - 1)
            sel = grp[grp.indexOf(done) + 1]
    }
    function preset(g, wasd) {
        const m = Object.assign({}, keys), ks = wasd ? ["W", "S", "A", "D"] : ["Up", "Down", "Left", "Right"]
        Pad.GROUPS[g].slice(0, 4).forEach((id, i) => m[id] = ks[i])
        save("keys", m)
    }

    // окно впереди — геймпад наш: нажатая кнопка выбирается (или горит)
    Binding { target: naomiPadKeys; property: "listening"; value: pw.active && pw.shown && !pw.minimized && !pw.closing }
    Connections {
        target: naomiPadKeys
        function onPressed(control) {
            if (!pw.active)
                return
            if (pw.keysMode) {
                pw.sel = control
            } else {
                pw.flash = control
                flashTimer.restart()
            }
        }
    }
    Timer { id: flashTimer; interval: 450; onTriggered: pw.flash = "" }

    // настоящая клавиатура: кнопка выбрана — нажатая клавиша её
    Item {
        id: keyCatch
        focus: true
        Keys.onPressed: (e) => {
            if (!pw.keysMode || !pw.sel)
                return
            const k = Pad.fromQt(e.key)
            if (k) {
                pw.assign(k)
                e.accepted = true
            }
        }
    }
    onActiveChanged: if (active) keyCatch.forceActiveFocus()

    component Label: Text {
        font.family: DeskStyle.mono
        font.pixelSize: 12
        color: DeskStyle.dim
    }

    MouseArea {   // колесо — когда окно ниже содержимого (стол маленький)
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        Component.onCompleted: cursorShape = undefined
        onWheel: (e) => {
            const d = e.pixelDelta.y !== 0 ? e.pixelDelta.y : e.angleDelta.y / 120 * 100
            scroller.contentY = Math.max(0, Math.min(scroller.contentHeight - scroller.height, scroller.contentY - d))
        }
    }
    Flickable {
        id: scroller
        anchors.fill: parent
        clip: contentHeight > height
        interactive: false
        contentHeight: col.implicitHeight + 44
        Column {
            id: col
            x: 26; y: 20
            width: scroller.width - 52
            spacing: 14

            Item {   // игра и режим
                width: col.width
                height: 30
                Plate {
                    y: (30 - DeskStyle.plateH) / 2
                    width: parent.width - modes.width - 14
                    title: pw.game ? pw.game.name.toUpperCase() : ""
                    value: pw.game ? pw.game.label : ""
                    valueColor: DeskStyle.dim
                    note: pw.running ? "ИДЁТ" : ""
                    noteColor: DeskStyle.teal
                }
                Row {
                    id: modes
                    anchors.right: parent.right
                    spacing: 6
                    NButton { width: 116; height: 30; text: "Как есть"; tone: pw.keysMode ? "off" : "on"; onClicked: pw.save("native", pw.keys) }
                    NButton { width: 132; height: 30; text: "Клавишами"; tone: pw.keysMode ? "on" : "off"; onClicked: pw.save("keys", pw.keys) }
                    NButton {
                        width: 116; height: 30; text: "Сбросить"
                        disabled: !pw.keysMode || !Object.keys(pw.keys).length
                        onClicked: pw.save("keys", {})
                    }
                }
            }
            Label {
                visible: pw.keysMode
                width: col.width
                textFormat: Text.StyledText
                text: "выбери кнопку — на рисунке или нажми её на геймпаде, потом клавишу — здесь или на клавиатуре"
                      + (pw.running ? " · <font color=\"" + DeskStyle.teal + "\">игра идёт: правка работает сразу</font>" : "")
            }
            Item {   // геймпад с выносками — во всю ширину окна
                width: col.width
                height: art.height * art.scale
                PadArt {
                    id: art
                    scale: col.width / width
                    transformOrigin: Item.TopLeft
                    sel: pw.sel
                    keys: pw.keys
                    nat: pw.nat
                    asIs: !pw.keysMode
                    flash: pw.flash
                    onPicked: (id) => pw.pick(id)
                }
            }

            Rectangle {   // «как есть»: что это значит
                visible: !pw.keysMode
                width: col.width
                height: nativeText.y + nativeText.implicitHeight + 14
                color: "transparent"
                border.width: 1
                border.color: DeskStyle.amberA(DeskStyle.line)
                Rectangle { width: 3; height: parent.height; color: DeskStyle.teal }
                Text {
                    x: 16; y: 14
                    text: "КАК ЕСТЬ"
                    font.family: DeskStyle.mono
                    font.pixelSize: 12
                    font.letterSpacing: 12 * 0.12
                    color: DeskStyle.teal
                }
                Label {
                    id: nativeText
                    x: 16; y: 36
                    width: parent.width - 32
                    wrapMode: Text.Wrap
                    lineHeight: 1.3
                    color: DeskStyle.muted
                    text: !pw.game ? ""
                        : pw.game.system === "win" ? "игра Windows видит геймпад сама, только если умеет (XInput). Не умеет — «Клавишами»: кнопка геймпада жмёт клавишу"
                        : "геймпад у " + (pw.game.system === "nes" ? "Денди" : "PS1") + " свой и уже работает: на рисунке — что делает каждая кнопка, нажатая на геймпаде загорается. «Клавишами» — назначить свои клавиши"
                }
            }

            Column {   // «клавишами», кнопка выбрана: что выбрано, направления, клавиатура
                visible: pw.keysMode && pw.sel !== ""
                width: col.width
                spacing: 12
                Rectangle { width: parent.width; height: 1; color: DeskStyle.amberA(DeskStyle.line) }
                Item {
                    width: parent.width
                    height: 30
                    Row {
                        id: selRow
                        height: 30
                        spacing: 12
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: Pad.fullName(pw.sel).toUpperCase()
                            font.family: DeskStyle.mono
                            font.pixelSize: DeskStyle.caps
                            font.weight: Font.Medium
                            font.letterSpacing: DeskStyle.caps * 0.16
                            color: DeskStyle.amber
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "→"
                            font.family: DeskStyle.mono
                            font.pixelSize: DeskStyle.caps
                            color: DeskStyle.dim
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: pw.keys[pw.sel] ? Pad.kname(pw.keys[pw.sel]) : "нет клавиши"
                            font.family: DeskStyle.mono
                            font.pixelSize: DeskStyle.caps
                            font.weight: pw.keys[pw.sel] ? Font.DemiBold : Font.Normal
                            color: pw.keys[pw.sel] ? DeskStyle.text : DeskStyle.dim
                        }
                    }
                    Rectangle {
                        x: selRow.width + 12
                        y: 15
                        width: Math.max(0, none.x - 12 - x)
                        height: 1
                        color: DeskStyle.amberA(DeskStyle.line)
                    }
                    NButton {
                        id: none
                        anchors.right: done.left
                        anchors.rightMargin: 6
                        width: 150; height: 30; text: "Нет клавиши"
                        disabled: !pw.keys[pw.sel]
                        onClicked: pw.assign("")
                    }
                    NButton { id: done; anchors.right: parent.right; width: 110; height: 30; text: "Готово"; onClicked: pw.sel = "" }
                }
                Item {   // направления стика или крестовины; у стиков — наборы
                    visible: !!Pad.GROUPS[pw.selGroup]
                    width: parent.width
                    height: 30
                    Row {
                        spacing: 6
                        Repeater {
                            model: Pad.GROUPS[pw.selGroup] || []
                            delegate: NButton {
                                required property string modelData
                                width: 112
                                height: 30
                                fontSize: 12
                                tone: pw.sel === modelData ? "on" : "off"
                                text: Pad.CTL[modelData].n + "  " + (Pad.kshort(pw.keys[modelData]) || "·")
                                onClicked: pw.sel = modelData
                            }
                        }
                    }
                    Row {
                        visible: pw.selGroup !== "dp"
                        anchors.right: parent.right
                        spacing: 6
                        NButton { width: 104; height: 30; fontSize: 12; text: "Стрелки"; onClicked: pw.preset(pw.selGroup, false) }
                        NButton { width: 80; height: 30; fontSize: 12; text: "WASD"; onClicked: pw.preset(pw.selGroup, true) }
                    }
                }
                PadKeyboard {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: implicitWidth
                    height: implicitHeight
                    u: 33
                    current: pw.keys[pw.sel] || ""
                    badges: pw.badges
                    onChosen: (key) => pw.assign(key)
                }
            }
        }
    }
}
