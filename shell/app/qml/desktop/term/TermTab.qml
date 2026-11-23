// Вкладка «Терминала» (docs/TERMINAL.md; вид — выбор пользователя Б7 «Папка и программа», 20 ноября 2026): как вкладка
// браузера (WebTab) — без рамки, моно 13, открытая — светлая и янтарная черта снизу, под мышью — подложка .07 и у
// незакреплённой шириной от 90 — крестик. Перед именем значок: папка — оболочка ждёт команды, уголок бирюзой — в ней
// работает программа (имя вкладки тогда — имя программы, если вкладку не переименовали), и бирюзовая точка справа
// сверху — видно и у неоткрытой. Точка не мигает: мигание — кадр стола за кадром, пока программа работает.
//   Левая кнопка — открыть (на нажатии, как в браузере), двойной щелчок — переименовать прямо во вкладке (Enter — да,
//   Esc — нет, пустое — снова по папке), средняя — закрыть, правая — меню вкладки.
import QtQuick
import ".."

Item {
    id: tab
    property var info: ({ id: -1, label: "", pinned: false, running: false, cwd: "" })
    property bool isActive: false
    property bool renaming: false
    signal pressed(int button)
    signal doubleClicked()
    signal closeClicked()
    signal renamed(string name)
    signal renameCancelled()
    height: DeskStyle.appH

    readonly property bool hovered: area.containsMouse
    readonly property bool closable: !info.pinned && width >= 90 && !renaming
    readonly property real closeX: width - 26
    readonly property bool overClose: closable && hovered && area.mouseX >= closeX && area.mouseX < closeX + 20
                                      && area.mouseY >= 5 && area.mouseY < 25

    Rectangle {
        anchors.fill: parent
        color: DeskStyle.amberA(DeskStyle.itemHover)
        visible: tab.hovered && !tab.renaming
    }
    Glyph {
        x: 12 + 7; y: tab.height / 2
        kind: tab.info.running ? "proc" : "folder"
        unit: tab.info.running ? 1 : 0.86
        line: 1.3
        color: tab.info.running ? DeskStyle.teal : tab.isActive ? DeskStyle.amber : DeskStyle.dim
    }
    Text {
        visible: !tab.renaming && width > 8
        x: 34
        width: (tab.hovered && tab.closable ? tab.closeX - 4 : tab.width - 12) - x
        height: tab.height
        verticalAlignment: Text.AlignVCenter
        text: tab.info.label
        elide: Text.ElideRight
        font.family: DeskStyle.mono
        font.pixelSize: 13
        color: tab.isActive || tab.hovered ? DeskStyle.text : DeskStyle.muted
    }
    Rectangle {   // работает программа — бирюзовая точка (под мышью её место у крестика)
        visible: tab.info.running && !tab.hovered && !tab.renaming
        x: tab.width - 11; y: 6
        width: 5; height: 5
        radius: 2.5
        color: DeskStyle.teal
    }
    Rectangle {   // открытая — черта снизу
        visible: tab.isActive && !tab.renaming
        x: 12; y: tab.height - 5
        width: tab.width - 24; height: 2
        color: DeskStyle.amber
    }

    // имя правят прямо во вкладке
    Rectangle {
        visible: tab.renaming
        x: 30; y: 4
        width: tab.width - 34; height: tab.height - 8
        color: DeskStyle.amberA(0.09)
        border.width: 1
        border.color: DeskStyle.amberA(0.45)
        TextInput {
            id: field
            x: 6
            width: parent.width - 12
            height: parent.height
            verticalAlignment: TextInput.AlignVCenter
            clip: true
            font.family: DeskStyle.mono
            font.pixelSize: 13
            color: DeskStyle.text
            selectionColor: DeskStyle.amberA(0.35)
            selectedTextColor: DeskStyle.text
            cursorDelegate: Rectangle { width: 1.5; color: DeskStyle.amber }
            property bool done: false
            Keys.onReturnPressed: { done = true; tab.renamed(text) }
            Keys.onEnterPressed: { done = true; tab.renamed(text) }
            Keys.onEscapePressed: { done = true; tab.renameCancelled() }
            onActiveFocusChanged: if (!activeFocus && tab.renaming && !done) tab.renamed(text)
        }
    }
    onRenamingChanged: {
        if (!renaming)
            return
        field.done = false
        field.text = info.label
        field.selectAll()
        field.forceActiveFocus()
    }

    // крестик: только под мышью; над самим крестиком — красный и подложка
    Item {
        visible: tab.closable && tab.hovered
        x: tab.closeX; y: 5
        width: 20; height: 20
        Rectangle {
            anchors.fill: parent
            radius: 2
            color: DeskStyle.badA(0.14)
            visible: tab.overClose
        }
        Glyph {
            x: 10; y: 10
            kind: "close"
            unit: 5 / 4.5
            line: 1.6
            color: tab.overClose ? DeskStyle.bad : DeskStyle.dim
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        enabled: !tab.renaming
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onPressed: (m) => {
            // крестик — только видимый (под мышью): у браузера невидимый тоже ловит щелчок, а здесь закрыть вкладку —
            // убить её программы
            if (m.button === Qt.LeftButton && tab.overClose)
                tab.closeClicked()
            else
                tab.pressed(m.button)
        }
        onDoubleClicked: (m) => { if (m.button === Qt.LeftButton) tab.doubleClicked() }
    }
}
