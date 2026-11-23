// Поле проверки колеса в «Подробно»: 150 строк, каждая десятая — янтарём. Крутят колесо над ним — список едет ровно
// на столько, сколько прислал зритель: у приставки щелчок уже по настройкам (строк, направление, плавность, разгон),
// так их видно вживую. Справа — полоса прокрутки 2 точки.
import QtQuick
import ".."

Rectangle {
    id: w
    color: DeskStyle.surface
    border.width: 1
    border.color: DeskStyle.amberA(DeskStyle.line)
    radius: 2
    clip: true
    readonly property int rowH: 22
    readonly property int rows: 150
    property real pos: 0
    readonly property real maxPos: Math.max(0, rows * rowH + 2 - height)

    Column {
        x: 1
        y: 1 - w.pos
        width: w.width - 12
        Repeater {
            model: w.rows
            delegate: Item {
                required property int index
                readonly property bool tenth: (index + 1) % 10 === 0
                width: parent.width
                height: w.rowH
                Text {
                    x: 10
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: String(index + 1).padStart(3, "0")
                    font.family: DeskStyle.mono
                    font.pixelSize: 12
                    color: parent.tenth ? DeskStyle.amber : DeskStyle.dim
                }
                Text {
                    x: 46
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: parent.tenth ? "— " + (index + 1) + "-я —" : "строка списка"
                    font.family: DeskStyle.mono
                    font.pixelSize: 12
                    color: parent.tenth ? DeskStyle.amber : DeskStyle.muted
                }
            }
        }
    }
    Rectangle {   // полоса прокрутки
        x: w.width - 5; y: 3
        width: 2; height: w.height - 6
        radius: 1
        color: Qt.rgba(1, 1, 1, 0.06)
        Rectangle {
            width: 2; radius: 1
            height: Math.max(20, parent.height * w.height / (w.rows * w.rowH))
            y: (parent.height - height) * (w.maxPos > 0 ? w.pos / w.maxPos : 0)
            color: DeskStyle.amberA(0.35)
        }
    }
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        onWheel: (e) => {
            const d = e.pixelDelta.y !== 0 ? e.pixelDelta.y : e.angleDelta.y / 120 * 100
            w.pos = Math.max(0, Math.min(w.maxPos, w.pos - d))
        }
    }
}
