// HUD разработчика поверх всего: кадры по present-fence, температура, ток батареи (их HUD спрашивает у
// службы naomid раз в секунду, пока виден). Под строкой состояния. Включается в шторке или «Настройках».
// Покой (кадров < 20 в секунду) — экран не рисуется, потому что ничего не меняется: так и задумано.
import QtQuick
import Naomi.Ui

Rectangle {
    id: root
    readonly property bool idle: naomiDisplay.fps < 20
    property var sample: ({ currentMa: 0, cpuC: 0, skinC: 0 })
    height: line.height + 8
    radius: 4
    color: Qt.rgba(0, 0, 0, 0.62)
    border.color: Theme.amberFaint
    border.width: 1

    Text {
        id: line
        anchors.centerIn: parent
        font.family: Theme.mono
        font.pixelSize: 10
        color: root.idle ? Theme.dim : Theme.muted
        text: naomiDisplay.refreshHz.toFixed(0) + "Гц "
              + (root.idle ? "покой" : naomiDisplay.fps.toFixed(0) + "к/с p95 " + naomiDisplay.p95Ms.toFixed(1))
              + " проп " + naomiDisplay.drops
              + " · CPU " + root.sample.cpuC.toFixed(0) + "° кор " + root.sample.skinC.toFixed(0)
              + "° · " + root.sample.currentMa + "мА"
    }

    Timer {
        interval: 1000
        repeat: true
        triggeredOnStart: true
        running: root.visible && naomiPower.displayOn
        onTriggered: naomiPhone.battery.call("Sample", [], s => root.sample = s)
    }
}
