// Кнопка действия НаомиОС: рамка 1 px, подпись капсом по центру. accent — главное действие (янтарь).
import QtQuick
import Naomi.Ui

Rectangle {
    id: root
    property string text
    property bool accent: false     // enabled: false — серая и не нажимается (свойство Item)
    signal clicked()

    implicitHeight: 44
    implicitWidth: caption.implicitWidth + 36
    radius: 3
    color: tap.pressed ? Theme.amberFaint : accent ? Qt.rgba(216 / 255, 178 / 255, 90 / 255, 0.06) : "transparent"
    border.color: accent ? Theme.amberEdge : Theme.surfaceHigh
    border.width: 1
    opacity: enabled ? 1 : 0.4

    Caps {
        id: caption
        anchors.centerIn: parent
        text: root.text
        color: root.accent ? Theme.amber : Theme.muted
        font.pixelSize: 11
    }
    TapHandler {
        id: tap
        onTapped: root.clicked()
    }
}
