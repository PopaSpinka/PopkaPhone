// Дверь — кнопка-переход во всю ширину (веб Наоми .bk-room): рамка линией, грань 3 точки янтарём .36, заголовок моно
// 13 капсом с разрядкой .18, под ним подпись моно 12,5 приглушённым, справа шеврон тускло; под мышью — заливка .07.
import QtQuick
import ".."

Item {
    id: d
    property string title
    property string note
    signal clicked()
    height: 58
    readonly property real dy: area.pressed ? 1 : 0

    Rectangle {
        y: d.dy
        width: d.width; height: d.height
        color: area.containsMouse ? DeskStyle.amberA(DeskStyle.itemHover) : "transparent"
        border.width: 1
        border.color: DeskStyle.amberA(DeskStyle.line)
    }
    Rectangle { y: d.dy; width: 3; height: d.height; color: DeskStyle.amberA(0.36) }
    Text {
        x: 17; y: 14 + d.dy
        text: d.title.toUpperCase()
        font.family: DeskStyle.mono
        font.pixelSize: DeskStyle.caps
        font.weight: Font.Medium
        font.letterSpacing: DeskStyle.caps * 0.18
        color: DeskStyle.text
    }
    Text {
        x: 17; y: 33 + d.dy
        width: d.width - 17 - 40
        elide: Text.ElideRight
        text: d.note
        font.family: DeskStyle.mono
        font.pixelSize: 12
        color: DeskStyle.muted
    }
    Glyph {
        kind: "forward"
        x: d.width - 22; y: d.height / 2 + d.dy
        color: DeskStyle.dim
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: d.clicked()
    }
}
