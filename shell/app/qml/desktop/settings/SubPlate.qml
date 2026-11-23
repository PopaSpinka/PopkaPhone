// Подшильдик — строка раздела внутри окна (draw_subplate в bcdesk; веб Наоми .hm-fc-h): моно 12 капсом тускло с
// разрядкой .16, линия до правого края; справа на линии — своё (кнопки, значение): дети компонента.
import QtQuick
import ".."

Item {
    id: s
    property string text
    default property alias tools: slot.data
    height: 18

    Text {
        id: label
        height: s.height
        verticalAlignment: Text.AlignVCenter
        text: s.text.toUpperCase()
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 12 * 0.16
        color: DeskStyle.dim
    }
    Row {
        id: slot
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6
    }
    Rectangle {
        x: label.implicitWidth + 10
        y: Math.round(s.height / 2)
        width: Math.max(0, (slot.width > 0 ? slot.x - 10 : s.width) - x)
        height: 1
        color: DeskStyle.amberA(DeskStyle.line)
    }
}
