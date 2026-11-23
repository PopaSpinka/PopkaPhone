// «Избранное» под звездой полосы (webmenu_bookmarks_draw в bcdesk.c): модуль 232, шильдик «ИЗБРАННОЕ» с числом,
// строки 208 × 34 с шагом 40 — значок сайта (нет — точка), имя, справа корзина «удалить» и ручка «переименовать».
// Больше 14 — строка «…И ЕЩЁ N». Щелчок по строке — сайт в открытой вкладке (у закреплённой — в новой). Ручка — имя
// правят прямо в строке: Enter, щелчок мимо или закрыли список — сохранить; Esc — отменить (второй Esc — закрыть
// список). Корзина (своё, НаомиФона) — закладка уходит сразу; под мышью встаёт следующая, поэтому второй щелчок
// двойного (быстрее 400 мс) её не трогает.
import QtQuick
import ".."

FocusScope {
    id: list
    property var st                       // WebState
    property bool open: false
    signal dismissed()
    signal opened(string url)
    width: DeskStyle.webMenuW
    readonly property var items: st.bookmarks
    readonly property int shown: Math.min(items.length, DeskStyle.webBookmarksRows)
    readonly property real rowsBottom: items.length === 0 ? 40 + 36 - 6
                                     : 40 + shown * 40 - 6 + (items.length > shown ? 20 + 6 : 0)
    height: rowsBottom + 12
    visible: opacity > 0
    opacity: open ? 1 : 0
    transformOrigin: Item.Top
    Behavior on opacity { enabled: !list.open; NumberAnimation { duration: 150; easing.type: Easing.OutExpo } }
    // масштаб — своими анимациями: присваивание в show() рвало привязку, и меню после первого показа оставалось на 0,98
    onOpenChanged: {
        if (open) {
            shrink.stop()
            grow.restart()
        } else {
            grow.stop()
            shrink.restart()
        }
    }
    NumberAnimation { id: grow; target: list; property: "scale"; from: 0.98; to: 1; duration: 110; easing.type: Easing.OutCubic }
    NumberAnimation { id: shrink; target: list; property: "scale"; to: 0.99; duration: 150; easing.type: Easing.OutCubic }

    property int editing: -1              // id закладки, имя которой правят
    property double dropped: 0            // когда удалили последнюю (Date.now())
    function drop(b) {
        if (Date.now() - dropped < 400)
            return
        dropped = Date.now()
        commit()
        // url — проверка: id — место в списке, и если список уже сдвинулся, браузер другую закладку не тронет
        naomiWeb.send({ type: "bookmark-delete", id: b.id, url: b.url })
    }
    function show(px, py) {
        x = px
        y = py
        open = true
        forceActiveFocus()
    }
    function close() {
        if (!open)
            return
        commit()
        open = false
        dismissed()
    }
    function begin(id) {
        commit()
        editing = id
        st.renaming = id
    }
    function commit() {
        if (editing < 0)
            return
        const b = items.find(o => o.id === editing)
        const text = editor.text.trim()
        if (b && text && text !== b.title)
            naomiWeb.send({ type: "bookmark-rename", id: editing, title: editor.text })
        cancel()
    }
    function cancel() {
        editing = -1
        st.renaming = -1
        list.forceActiveFocus()
    }
    Keys.onEscapePressed: editing >= 0 ? cancel() : close()

    MouseArea { anchors.fill: parent; onPressed: list.commit() }   // щелчок по пустому месту — закончить правку
    DeskModule { anchors.fill: parent }
    Plate {
        x: 12; y: 12
        width: 208
        title: "ИЗБРАННОЕ"
        note: String(list.items.length)
    }

    component Caps11: Text {
        font.family: DeskStyle.mono
        font.pixelSize: 11
        font.letterSpacing: 11 * 0.12
        color: DeskStyle.dim
    }
    Column {   // пусто
        visible: list.items.length === 0
        x: 12; y: 40
        Caps11 { height: 18; verticalAlignment: Text.AlignVCenter; text: "ПОКА ПУСТО" }
        Caps11 { height: 18; verticalAlignment: Text.AlignVCenter; text: "★ В СТРОКЕ АДРЕСА — СЮДА" }
    }

    Repeater {
        model: list.shown
        delegate: Item {
            id: row
            required property int index
            readonly property var b: list.items[index] || { id: -1, title: "", url: "", icon: "" }
            readonly property bool edit: list.editing === b.id
            readonly property bool hovered: area.containsMouse
            readonly property real penX: 208 - 30
            readonly property real trashX: penX - 24          // корзина — левее ручки, через N_ITEM_GAP
            readonly property bool overPen: hovered && area.mouseX >= penX - 1
            readonly property bool overTrash: hovered && !overPen && area.mouseX >= trashX - 4
            x: 12; y: 40 + index * 40
            width: 208; height: DeskStyle.btnH

            Rectangle {
                anchors.fill: parent
                color: row.edit ? DeskStyle.surface : row.hovered ? DeskStyle.amberA(DeskStyle.itemHover) : "transparent"
                border.width: 1
                border.color: DeskStyle.amberA(row.edit ? 0.55 : DeskStyle.line)
            }
            Rectangle { width: 3; height: row.height; color: DeskStyle.amberA(row.edit ? 1 : 0.5) }
            Image {
                id: icon
                x: 21 - 8; y: 17 - 8
                width: 16; height: 16
                source: row.b.icon
                sourceSize: Qt.size(32, 32)
                smooth: true
                mipmap: true
                visible: row.b.icon !== "" && status !== Image.Error
            }
            Rectangle {
                visible: !icon.visible
                x: 21 - 3; y: 17 - 3
                width: 6; height: 6; radius: 3
                color: DeskStyle.dim
            }
            Text {
                visible: !row.edit
                x: 37
                width: row.trashX - 8 - x
                height: row.height
                verticalAlignment: Text.AlignVCenter
                text: row.b.title || row.b.url
                elide: Text.ElideRight
                font.family: DeskStyle.mono
                font.pixelSize: 13
                color: row.hovered ? DeskStyle.text : DeskStyle.muted
            }
            Item {   // корзина
                visible: !row.edit
                x: row.trashX; y: (row.height - 22) / 2
                width: 22; height: 22
                Rectangle {
                    anchors.fill: parent
                    radius: 2
                    color: DeskStyle.badA(0.14)
                    visible: row.overTrash
                }
                Glyph {
                    x: 11; y: 11
                    kind: "trash"
                    line: 1.3
                    color: row.overTrash ? DeskStyle.bad : row.hovered ? DeskStyle.muted : DeskStyle.dim
                }
            }
            Item {   // ручка
                visible: !row.edit
                x: row.penX; y: (row.height - 22) / 2
                width: 22; height: 22
                Rectangle {
                    anchors.fill: parent
                    radius: 2
                    color: DeskStyle.amberA(0.14)
                    visible: row.overPen
                }
                Glyph {
                    x: 11; y: 11
                    kind: "pen"
                    line: 1.3
                    color: row.overPen ? DeskStyle.amber : row.hovered ? DeskStyle.muted : DeskStyle.dim
                }
            }
            MouseArea {
                id: area
                anchors.fill: parent
                enabled: !row.edit
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onPressed: (m) => {
                    if (m.x >= row.trashX - 4 && m.x < row.penX - 1) {
                        list.drop(row.b)
                    } else if (m.x >= row.penX - 1) {
                        list.begin(row.b.id)
                        editor.y = row.y
                        editor.text = row.b.title
                        editor.selectAll()
                        editor.forceActiveFocus()
                    } else if (list.editing >= 0) {
                        list.commit()   // правили другое — только закончить правку
                    } else {
                        list.close()
                        list.opened(row.b.url)
                    }
                }
            }
        }
    }
    Caps11 {
        visible: list.items.length > list.shown
        x: 12; y: 40 + list.shown * 40
        height: 20
        verticalAlignment: Text.AlignVCenter
        text: "…И ЕЩЁ " + (list.items.length - list.shown)
    }

    // поле правки имени — то же, что строка адреса: моно 13, выделение янтарём, каретка янтарная
    TextInput {
        id: editor
        visible: list.editing >= 0
        x: 12 + 37
        width: 208 - 10 - 37
        height: DeskStyle.btnH
        verticalAlignment: TextInput.AlignVCenter
        font.family: DeskStyle.mono
        font.pixelSize: 13
        color: DeskStyle.text
        selectionColor: DeskStyle.amberA(0.35)
        selectedTextColor: DeskStyle.text
        selectByMouse: true
        clip: true
        cursorDelegate: Rectangle { width: 1.5; height: 16; y: (editor.height - 16) / 2; color: DeskStyle.amber }
        Text {
            visible: editor.text === ""
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            text: "Название"
            font: editor.font
            color: DeskStyle.dim
        }
        Keys.onReturnPressed: list.commit()
        Keys.onEnterPressed: list.commit()
        Keys.onEscapePressed: list.cancel()
    }
}
