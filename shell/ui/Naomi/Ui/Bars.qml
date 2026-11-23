// Четыре столбика уровня сигнала (Wi-Fi, сотовая сеть). level 0…4.
import QtQuick
import Naomi.Ui

Row {
    property int level: 0
    property color color: Theme.text
    spacing: 1.5
    Repeater {
        model: 4
        Rectangle {
            required property int index
            width: 2.5
            height: 3.5 + index * 2.2
            anchors.bottom: parent.bottom
            radius: 0.5
            color: parent.color
            opacity: index < parent.level ? 1 : 0.25
        }
    }
}
