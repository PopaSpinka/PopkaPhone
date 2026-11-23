// Окно «Терминала» на столе (docs/TERMINAL.md) — как окно браузера (WebWindow): полоса 40 с вкладками и «+»
// (TermStrip), справа «в док» (режим панели) и крестик (сворачивает; с Shift — закрыть терминал), под ней — текст
// открытой вкладки (TermView). Полоса и текст — одно целое: углы «непрерывные», рамка .16 поверх; фон текста — уголь
// окна, поэтому углы ничем не срезаются. Тянут за пустое место полосы, двойной щелчок — развернуть; развёрнутое тянут
// дальше 5 точек — прежний размер под курсором; размер — за невидимую рамку (EdgeGrip), не меньше 480 × 280.
// Режим дока — без полосы, текст во всю свободную область, вкладки — на панели. Появляется и уходит как окна НаомиОС:
// растёт от 0,96 и проявляется. Перетащили файлы из «Файлов» — их пути в строку, в кавычках.
import QtQuick
import QtQuick.Shapes
import Naomi.Desk
import ".."

Item {
    id: win
    property var st                       // TermState
    property Item area                    // стол без панели
    property bool active: false
    property bool minimized: false
    property bool maximized: false
    property bool panelMode: false
    property rect box                     // рамка окна с полосой (не развёрнутого)
    signal activated()
    signal closed()
    signal panelToggled()
    signal closeClicked(int modifiers)
    signal saveRequested()
    signal textMenu()                     // правая кнопка по тексту — «Копировать / Вставить»

    readonly property alias view: view
    readonly property bool barShown: !panelMode
    readonly property real barH: barShown ? DeskStyle.webBarH : 0
    readonly property rect frame: panelMode || maximized ? Qt.rect(0, 0, area.width, area.height) : box
    readonly property real e: DeskStyle.corner(width, height)
    readonly property size minSize: Qt.size(480, 280)
    x: frame.x; y: frame.y; width: frame.width; height: frame.height

    function activate() { activated() }
    function takeFocus() { view.forceActiveFocus() }
    property bool closing: false
    property bool shown: false
    Component.onCompleted: shown = true
    function close() {
        if (closing)
            return
        closing = true
        shown = false
    }
    // перетаскивание файлов (Desktop.qml): пути — в строку, каждый в кавычках, как их вставил бы терминал Linux
    function dragOver(p) {}
    function dropAt(p, paths) {
        if (!paths.length)
            return
        view.insert(paths.map(s => "'" + s.replace(/'/g, "'\\''") + "'").join(" ") + " ")
        activate()
    }

    visible: shown && !minimized || opacity > 0
    opacity: shown && !minimized ? 1 : 0
    scale: shown && !minimized ? 1 : 0.96
    Behavior on opacity { NumberAnimation { duration: win.shown && !win.minimized ? 180 : 140; easing.type: Easing.OutCubic } }
    Behavior on scale { NumberAnimation { duration: win.shown && !win.minimized ? 180 : 140; easing.type: Easing.OutCubic } }
    onOpacityChanged: if (closing && opacity === 0) closed()
    onActiveChanged: if (active && !minimized && !(st && st.renaming >= 0)) takeFocus()

    // ---------- фон ----------
    Shape {
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: DeskStyle.bg
            strokeColor: "transparent"
            PathPolyline { path: DeskStyle.outline(0, 0, win.width, win.height, win.e) }
        }
    }

    // ---------- текст ----------
    TermView {
        id: view
        y: win.barH
        width: win.width
        height: win.height - win.barH
        session: naomiTerm.activeSession
        fontFamily: DeskStyle.mono
        fontSize: 14
        pixel: naomiDesk.pixelScale
        // мигает, только пока окно в фокусе и стол кто-то смотрит
        blink: win.active && !win.minimized && naomiStream.connected && naomiStream.desktop && !naomiStream.deskAway
        onGridChanged: naomiTerm.setSize(cols, rows)
        Component.onCompleted: naomiTerm.setSize(cols, rows)
        onMenuRequested: win.textMenu()
        onShortcut: (name, arg) => {
            if (name === "new")
                win.st.newHere()
            else if (name === "close")
                win.st.close(naomiTerm.active)
            else if (name === "cycle")
                naomiTerm.cycle(arg)
            else if (name === "select")
                naomiTerm.select(arg)
        }
    }

    // ---------- полоса ----------
    Item {
        id: bar
        visible: win.barShown
        width: win.width
        height: DeskStyle.webBarH

        Rectangle {   // линия снизу
            y: DeskStyle.webBarH - 1
            width: parent.width; height: 1
            color: DeskStyle.amberA(DeskStyle.line)
        }
        // пустое место: тянут — окно едет (не выше панели); двойной щелчок — развернуть / вернуть
        MouseArea {
            anchors.fill: parent
            cursorShape: pressed && dragging ? Qt.ClosedHandCursor : Qt.ArrowCursor
            property point start
            property rect from
            property bool dragging: false
            onPressed: (m) => {
                win.activate()
                start = mapToItem(win.area, m.x, m.y)
                from = win.box
                dragging = !win.maximized
            }
            onPositionChanged: (m) => {
                const p = mapToItem(win.area, m.x, m.y)
                if (!dragging) {
                    if (Math.hypot(p.x - start.x, p.y - start.y) < 5)
                        return
                    const fx = start.x / win.area.width
                    win.box = Qt.rect(p.x - fx * from.width, p.y - start.y, from.width, from.height)
                    win.maximized = false
                    from = win.box
                    start = p
                    dragging = true
                }
                win.box = Qt.rect(from.x + p.x - start.x, Math.max(0, from.y + p.y - start.y), from.width, from.height)
            }
            onReleased: if (dragging) win.saveRequested()
            onDoubleClicked: {
                win.maximized = !win.maximized
                win.saveRequested()
            }
        }

        TermStrip {
            x: 8; y: 5
            width: Math.max(0, panelButton.x - DeskStyle.webGrabGap - x)
            tabs: naomiTerm.tabs
            active: naomiTerm.active
            renaming: win.st.renaming
            onTabPressed: (id, button) => win.st.pressed(id, button)
            onTabDoubleClicked: (id) => win.st.startRename(id)
            onTabClosed: (id) => win.st.close(id)
            onTabRenamed: (id, name) => win.st.rename(id, name)
            onRenameCancelled: win.st.cancelRename()
            onPlusClicked: win.st.plus()
        }

        WinButton {
            id: panelButton
            x: win.width - 6 - 2 * 30 - 2; y: 5
            glyph: "panel"
            unit: 0.9545
            line: 1.5
            onClicked: win.panelToggled()
        }
        WinButton {
            x: win.width - 6 - 30; y: 5
            glyph: "close"
            danger: true
            unit: 5.25 / 4.5
            line: 1.5
            onClicked: (mods) => win.closeClicked(mods)
        }
    }

    // ---------- рамка ----------
    Shape {
        z: 999
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "transparent"
            strokeColor: DeskStyle.amberA(DeskStyle.line)
            strokeWidth: 1
            PathPolyline { path: DeskStyle.outline(0.5, 0.5, win.width - 1, win.height - 1, Math.max(0, win.e - 0.5)) }
        }
    }

    // ---------- размер: невидимая рамка (окно с полосой, не развёрнутое) ----------
    component Edge: EdgeGrip {
        visible: win.barShown && !win.maximized
        property point start
        property rect from
        onPressed: (m) => {
            win.activate()
            start = mapToItem(win.area, m.x, m.y)
            from = win.box
        }
        onPositionChanged: (m) => {
            if (!pressed)
                return
            const p = mapToItem(win.area, m.x, m.y), dx = p.x - start.x, dy = p.y - start.y
            let x = from.x, y = from.y, w = from.width, h = from.height
            if (pull.x > 0) w = Math.max(win.minSize.width, from.width + dx)
            if (pull.x < 0) { w = Math.max(win.minSize.width, from.width - dx); x = from.x + from.width - w }
            if (pull.y > 0) h = Math.max(win.minSize.height, from.height + dy)
            if (pull.y < 0) {
                h = Math.max(win.minSize.height, from.height - dy)
                y = Math.max(0, from.y + from.height - h)
                h = from.y + from.height - y
            }
            win.box = Qt.rect(x, y, w, h)
        }
        onReleased: win.saveRequested()
    }
    Edge { sx: -1 }
    Edge { sx: 1 }
    Edge { sy: -1 }
    Edge { sy: 1 }

    // нажатие в окне — окно наверх (и дальше — тому, на что нажали)
    MouseArea {
        anchors.fill: parent
        z: 1000
        acceptedButtons: Qt.AllButtons
        Component.onCompleted: cursorShape = undefined   // без формы: иначе стрелка закрыла бы курсор текста
        onPressed: (m) => { win.activate(); m.accepted = false }
    }
}
