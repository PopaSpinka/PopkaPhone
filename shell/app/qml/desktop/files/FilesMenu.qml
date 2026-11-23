// Меню «Файлов» — как popover.menu НаомиОС: поверхность, рамка янтарь .25, скругление 3, поля 6 сверху и снизу;
// пункты — sans 14, поля 6 × 16, под мышью — янтарь .12; черты — янтарь .12 с отступом 4. Открывается у мыши,
// в пределах окна; щелчок мимо или Esc — закрыть.
//   items: [{ label, run }, { sep: true }, …]
import QtQuick
import ".."

Item {
    id: menu
    anchors.fill: parent
    visible: shown
    property bool shown: false
    property var items: []
    signal done()

    readonly property int rowH: 30
    TextMetrics { id: tm; font.family: DeskStyle.sans; font.pixelSize: 14 }
    function popup(x, y, list) {
        let w = 180, h = 12
        for (const it of list) {
            h += it.sep ? 9 : rowH
            if (!it.sep) {
                tm.text = it.label
                w = Math.max(w, Math.ceil(tm.advanceWidth) + 32)
            }
        }
        items = list
        box.width = w
        box.height = h
        shown = true
        box.x = Math.max(4, Math.min(x, width - box.width - 4))
        box.y = Math.max(4, Math.min(y, height - box.height - 4))
    }
    function close() {
        if (!shown)
            return
        shown = false
        done()
    }

    MouseArea {   // мимо меню — закрыть
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onPressed: menu.close()
    }
    Rectangle {
        id: box
        color: DeskStyle.surface
        border.width: 1
        border.color: DeskStyle.amberA(0.25)
        radius: 3
        Column {
            id: col
            y: 6
            width: box.width
            Repeater {
                model: menu.items
                delegate: Item {
                    id: row
                    required property var modelData
                    width: col.width
                    height: modelData.sep ? 9 : menu.rowH
                    Rectangle {
                        visible: row.modelData.sep === true
                        y: 4
                        width: parent.width; height: 1
                        color: DeskStyle.amberA(0.12)
                    }
                    Rectangle {
                        anchors.fill: parent
                        visible: !row.modelData.sep && area.containsMouse
                        color: DeskStyle.amberA(0.12)
                    }
                    Text {
                        id: label
                        visible: !row.modelData.sep
                        x: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.modelData.label || ""
                        font.family: DeskStyle.sans
                        font.pixelSize: 14
                        color: DeskStyle.text
                    }
                    MouseArea {
                        id: area
                        anchors.fill: parent
                        enabled: !row.modelData.sep
                        hoverEnabled: true
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        onClicked: {
                            const run = row.modelData.run
                            menu.close()
                            if (run)
                                run()
                        }
                    }
                }
            }
        }
    }
}
