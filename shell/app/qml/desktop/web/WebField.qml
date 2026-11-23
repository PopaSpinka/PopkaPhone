// Строка адреса браузера (web_field_* в bcdesk.c): открытая вкладка, нарисованная полем, — когда в ней печатают или
// она пустая новая. Только текст, моно 13, без значка и черты; пусто — серое «Поиск». Выделение — янтарь .35,
// каретка 1,5 × 16 янтарём, мигает по 530 мс. Печатают в вкладке с сайтом — справа звёздочка «в Избранное»
// (щелчок по ней строку не отпускает). Enter — адрес или поиск (WebState.submit), Esc — отпустить.
import QtQuick
import ".."

Item {
    id: f
    property var st                       // WebState
    height: DeskStyle.appH

    readonly property var tab: st.shownActiveTab
    readonly property bool starShown: st.typing && tab !== null && tab.url !== "" && width >= DeskStyle.webFieldMinW
    readonly property bool starred: starShown && st.bookmarked(tab.url)
    readonly property bool sentShown: !st.focused && st.sent !== ""
    readonly property alias input: input
    // Пустая вкладка: строка — только по слову «Поиск» (или тому, что в ней печатают, что ушло Enter-ом), справа от
    // слова тот же отступ, что слева от него до звезды; дальше — пустое место полосы, за него тянут окно (WebStrip).
    // Вкладка с сайтом — строка как была, во всё свободное место. Просьба пользователя 21 ноября 2026
    readonly property bool hugs: !tab || tab.url === ""
    readonly property real hugW: 12 + Math.ceil(Math.max(label.implicitWidth, st.focused ? input.contentWidth + 2 : 0))
                                 + DeskStyle.webHugPad

    function take(initial) {
        input.text = initial !== null ? initial : (f.tab ? f.tab.url : "")
        input.selectAll()
        input.forceActiveFocus()
        caret.restart()
    }

    Rectangle {   // наведение — только когда не печатают
        anchors.fill: parent
        color: DeskStyle.amberA(DeskStyle.itemHover)
        visible: hover.hovered && !f.st.focused
    }
    HoverHandler { id: hover; cursorShape: Qt.IBeamCursor }

    Item {
        x: 10; y: 1
        width: f.width - 24 - (f.starShown ? 26 : 0) + 4
        height: f.height - 2
        clip: true
        Text {   // «Поиск» или то, что только что ушло Enter-ом
            id: label
            x: 2
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            visible: f.sentShown || input.text === "" || !f.st.focused
            text: f.sentShown ? f.st.sent : f.st.focused || !f.tab || f.tab.url === "" ? "Поиск" : ""
            font: input.font
            color: f.sentShown ? DeskStyle.text : DeskStyle.dim
        }
        TextInput {
            id: input
            x: 2
            width: parent.width - 4
            height: parent.height
            verticalAlignment: TextInput.AlignVCenter
            visible: f.st.focused
            font.family: DeskStyle.mono
            font.pixelSize: 13
            color: DeskStyle.text
            selectionColor: DeskStyle.amberA(0.35)
            selectedTextColor: DeskStyle.text
            selectByMouse: true
            persistentSelection: true
            clip: true
            onActiveFocusChanged: if (!activeFocus && f.st.focused && f.st.renaming < 0 && !f.st.newTabPending) f.st.blurField(false)
            onCursorPositionChanged: caret.restart()
            onTextEdited: caret.restart()
            cursorDelegate: Rectangle {
                width: 1.5
                height: 16
                y: (input.height - height) / 2
                color: DeskStyle.amber
                visible: input.activeFocus && caret.on
            }
            Keys.onPressed: (e) => f.key(e)
        }
    }
    // мигание каретки: горит 530 мс, гаснет 530; нажатие и щелчок — снова горит
    QtObject {
        id: caret
        property bool on: true
        function restart() { on = true; blink.restart() }
        property Timer blink: Timer {
            interval: DeskStyle.caretBlinkMs
            repeat: true
            running: input.activeFocus
            onTriggered: caret.on = !caret.on
        }
    }

    // не в фокусе: нажатие — фокус, адрес выделен; в фокусе второй щелчок — выделить всё
    MouseArea {
        anchors.fill: parent
        anchors.topMargin: -2
        anchors.bottomMargin: -2
        enabled: !f.st.focused
        cursorShape: Qt.IBeamCursor   // поверх поля: своя стрелка закрыла бы текстовый курсор
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        onPressed: (m) => {
            if (m.button === Qt.LeftButton)
                f.st.focusField(null)
            else if (f.tab && f.tab.id !== -2)
                f.st.tabPressed(f.tab, m.button)
        }
    }
    TapHandler {
        enabled: f.st.focused
        onDoubleTapped: input.selectAll()
    }

    // звёздочка: страница в «Избранном» — залита янтарём, нет — контур, под мышью светлый на подложке
    Item {
        visible: f.starShown
        x: f.width - 36
        width: 36; height: f.height
        Rectangle {
            x: 4; y: 5
            width: 24; height: 20
            radius: 2
            color: DeskStyle.amberA(0.12)
            visible: starArea.containsMouse && !f.starred
        }
        Glyph {
            x: 16; y: 15 + 0.8
            kind: "star"
            unit: 8.016 / 6
            line: 1.5
            fill: f.starred
            color: f.starred ? DeskStyle.amber : starArea.containsMouse ? DeskStyle.text : DeskStyle.dim
        }
        MouseArea {
            id: starArea
            anchors.fill: parent
            hoverEnabled: true
            onPressed: naomiWeb.send({ type: "bookmark", url: f.tab.url, title: f.tab.title, on: !f.starred })
        }
    }

    // клавиши строки (web_field_key): правка текста — у TextInput; здесь — Enter, Esc и сочетания браузера
    function key(e) {
        const ctrl = e.modifiers & Qt.ControlModifier, shift = e.modifiers & Qt.ShiftModifier
        if (e.key === Qt.Key_Return || e.key === Qt.Key_Enter) {
            f.st.submit(input.text)
        } else if (e.key === Qt.Key_Escape) {
            f.st.blurField(true)
        } else if (ctrl && (e.key === Qt.Key_W || e.key === Qt.Key_F4)) {
            f.st.close(f.st.active)
        } else if (ctrl && shift && e.key === Qt.Key_T) {
            naomiWeb.send({ type: "restore-closed" })
        } else if (ctrl && (e.key === Qt.Key_T || e.key === Qt.Key_N)) {
            f.st.newTab()
        } else if (ctrl && e.key === Qt.Key_L) {
            input.selectAll()
        } else if (ctrl && (e.key === Qt.Key_Tab || e.key === Qt.Key_Backtab || e.key === Qt.Key_PageDown
                            || e.key === Qt.Key_PageUp)) {
            f.st.blurField(false)
            naomiWeb.send({ type: "cycle", step: shift || e.key === Qt.Key_Backtab || e.key === Qt.Key_PageUp ? -1 : 1 })
        } else if (ctrl && e.key >= Qt.Key_1 && e.key <= Qt.Key_9) {
            f.st.blurField(false)
            naomiWeb.send({ type: "select", index: e.key === Qt.Key_9 ? -1 : e.key - Qt.Key_1 })
        } else if ((ctrl && ![Qt.Key_A, Qt.Key_C, Qt.Key_X, Qt.Key_V, Qt.Key_Z, Qt.Key_Left, Qt.Key_Right,
                              Qt.Key_Backspace, Qt.Key_Delete, Qt.Key_Insert].includes(e.key))
                   || (e.modifiers & (Qt.AltModifier | Qt.MetaModifier))) {
            // прочие с Ctrl, Alt или ⌘ — мимо
        } else {
            caret.restart()
            return
        }
        e.accepted = true
    }
}
