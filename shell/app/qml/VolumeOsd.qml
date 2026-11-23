// Громкость поверх всего — когда нажали кнопку громкости: куда идёт звук, полоска, проценты (или «без
// звука»). Держится 1,5 с после последнего нажатия, потом гаснет. Данные — служба naomid (naomiPhone.audio).
import QtQuick
import Naomi.Ui

Rectangle {
    id: root
    function show() {
        hide.restart()
        opacity = 1
    }

    width: 236
    height: 44
    radius: 22
    color: Theme.canvas
    border.width: 1
    border.color: Theme.amberEdge
    opacity: 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: Theme.closeMs; easing.type: Theme.easing } }
    Timer { id: hide; interval: 1500; onTriggered: root.opacity = 0 }

    readonly property bool muted: naomiPhone.audio.muted
    readonly property real level: muted ? 0 : naomiPhone.audio.volume / 100

    Caps {
        id: label
        anchors.left: parent.left; anchors.leftMargin: 18
        anchors.verticalCenter: parent.verticalCenter
        width: 74
        elide: Text.ElideRight
        text: root.muted ? "без звука" : Theme.outputLabel(naomiPhone.audio.output)
        color: root.muted ? Theme.dim : Theme.muted
        font.pixelSize: 9
    }
    Rectangle {   // полоска
        anchors.left: label.right; anchors.leftMargin: 8
        anchors.right: percent.left; anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        height: 4; radius: 2
        color: Theme.surfaceHigh
        Rectangle { width: parent.width * root.level; height: parent.height; radius: 2; color: Theme.amber }
    }
    Caps {
        id: percent
        anchors.right: parent.right; anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 32
        horizontalAlignment: Text.AlignRight
        text: root.muted ? "—" : naomiPhone.audio.volume
        color: Theme.amber
        font.pixelSize: 10
    }
}
