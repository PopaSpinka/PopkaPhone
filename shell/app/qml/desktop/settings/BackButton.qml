// «‹ Раздел» — назад из подраздела «Настроек» («Подробно» → «Мышь», «Обои» и «Без дока» → «Экран»): рамка линией,
// грань 3 точки тусклая, шеврон и подпись моно 12 капсом приглушённым (под мышью — светлым и заливка .07).
import QtQuick
import ".."

Item {
    id: back
    property string text
    signal clicked()
    width: label.x + label.implicitWidth + 11
    height: 30
    readonly property real dy: area.pressed ? 1 : 0

    Rectangle {
        y: back.dy
        width: back.width; height: back.height
        color: area.containsMouse ? DeskStyle.amberA(DeskStyle.itemHover) : "transparent"
        border.width: 1
        border.color: DeskStyle.amberA(DeskStyle.line)
    }
    Rectangle { y: back.dy; width: 3; height: back.height; color: DeskStyle.dim }
    Glyph {
        kind: "back"
        x: 3 + 10; y: back.height / 2 + back.dy
        color: area.containsMouse ? DeskStyle.text : DeskStyle.muted
    }
    Text {
        id: label
        x: 3 + 20; y: back.dy
        height: back.height
        verticalAlignment: Text.AlignVCenter
        text: back.text.toUpperCase()
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 12 * 0.12
        color: area.containsMouse ? DeskStyle.text : DeskStyle.muted
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: back.clicked()
    }
}
