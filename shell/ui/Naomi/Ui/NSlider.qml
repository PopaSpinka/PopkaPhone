// Ползунок НаомиОС: тонкая дорожка, янтарная заливка, круглый бегунок. value 0…1 — снаружи (привязка не
// рвётся: пока палец на ползунке, бегунок идёт за пальцем, потом снова показывает value — например,
// громкость, изменённую кнопками).
import QtQuick
import Naomi.Ui

Item {
    id: root
    property real value: 0.5
    signal moved(real value)
    implicitHeight: 36
    property real live: -1            // где палец; −1 — не тянут
    readonly property real shown: live >= 0 ? live : value

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width; height: 3; radius: 1.5
        color: Theme.surfaceHigh
        Rectangle { width: knob.x + knob.width / 2; height: parent.height; radius: 1.5; color: Theme.amber }
    }
    Rectangle {
        id: knob
        width: 22; height: 22; radius: 11
        anchors.verticalCenter: parent.verticalCenter
        x: root.shown * (root.width - width)
        color: Theme.text
        border.color: Theme.amber
        border.width: 1.5
    }
    DragHandler {
        target: null
        xAxis.enabled: true; yAxis.enabled: false
        onCentroidChanged: if (active) root.set(centroid.position.x)
        onActiveChanged: if (!active) root.live = -1
    }
    TapHandler { onTapped: (point) => { root.set(point.position.x); root.live = -1 } }

    function set(x) {
        const v = Math.max(0, Math.min(1, (x - knob.width / 2) / (width - knob.width)))
        root.live = v
        root.moved(v)
    }
}
