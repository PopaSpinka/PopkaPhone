// Уставка «− значение +» (веб Наоми .bk-set-b, кондиционер): кнопки 30 × 30 с гранью 3 тусклой, моно 16; между ними
// значение sans 22 тонко и единица мельче приглушённым. У края диапазона кнопка гаснет (.45). Шаг — по сетке step.
import QtQuick
import ".."

Row {
    id: s
    property int value
    property int from: 0
    property int to: 100
    property int step: 1
    property string unit
    signal moved(int value)
    spacing: 8

    function go(dir) {
        const v = Math.max(from, Math.min(to, (dir > 0 ? Math.floor(value / step + 1e-6) + 1 : Math.ceil(value / step - 1e-6) - 1) * step))
        if (v !== value)
            moved(v)
    }

    component Btn: Item {
        id: b
        property string label
        property bool enabled_: true
        signal clicked()
        width: 30; height: 30
        opacity: enabled_ ? 1 : 0.45
        readonly property real dy: area.pressed && enabled_ ? 1 : 0
        Rectangle {
            y: b.dy
            width: b.width; height: b.height
            color: area.containsMouse && b.enabled_ ? DeskStyle.amberA(DeskStyle.itemHover) : "transparent"
            border.width: 1
            border.color: DeskStyle.amberA(DeskStyle.line)
        }
        Rectangle { y: b.dy; width: 3; height: b.height; color: DeskStyle.dim }
        Text {
            x: 3; y: b.dy
            width: b.width - 3; height: b.height
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: b.label
            font.family: DeskStyle.mono
            font.pixelSize: 16
            color: DeskStyle.text
        }
        MouseArea {
            id: area
            anchors.fill: parent
            hoverEnabled: true
            onClicked: if (b.enabled_) b.clicked()
        }
    }

    Btn { label: "−"; enabled_: s.value > s.from; onClicked: s.go(-1) }
    Item {
        width: 96; height: 30
        Row {
            anchors.centerIn: parent
            spacing: 4
            Text {
                id: num
                text: s.value
                font.family: DeskStyle.sans
                font.pixelSize: 22
                font.weight: Font.Light
                color: DeskStyle.text
            }
            Text {
                y: num.baselineOffset - baselineOffset
                text: s.unit
                font.family: DeskStyle.sans
                font.pixelSize: 12
                color: DeskStyle.muted
            }
        }
    }
    Btn { label: "+"; enabled_: s.value < s.to; onClicked: s.go(1) }
}
