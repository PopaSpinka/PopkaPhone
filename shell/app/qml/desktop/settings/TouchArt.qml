// Рисунок трекпада с пальцами в «Настройках» → «Трекпад»: пример жеста (demo, момент t — touch.js) или настоящие
// пальцы трекпада приставки (live — naomiStream.padTouch.fingers: доли трекпада). Палец — янтарный кружок 14 мм, тот,
// что не считается (лежит, ладонь, печать), — пустой тусклый; круг касания расходится и тает; бирюзовый кружок —
// «пальцы сняты, кнопка ещё держится»; при правом щелчке в углу — сам угол.
import QtQuick
import ".."
import "touch.js" as Touch

Rectangle {
    id: art
    property string demo: "move"
    property real t: 0
    property var cfg: ({ swipeFingers: 4, palm: true, jitter: 1, right: 1, drag: 1 })
    property var live: null
    implicitHeight: Math.round(width * 115 / 162)
    height: implicitHeight
    radius: 10 * k
    color: DeskStyle.bg
    border.width: 1
    border.color: DeskStyle.amberA(0.36)
    readonly property real k: width / 162
    readonly property var pose: live ? ({ f: live.map(p => ({ x: p.x * 162, y: p.y * 115, off: p.off })) })
                                     : Touch.pose(demo, t, cfg)

    Rectangle {   // правый нижний угол
        visible: !!art.pose.zone
        x: art.width * 0.72; y: art.height * 0.70
        width: art.width - x - 2; height: art.height - y - 2
        radius: 5 * art.k
        color: DeskStyle.amberA(0.12)
    }
    Rectangle {   // круг касания
        readonly property var ring: art.pose.ring || null
        visible: !!ring
        width: ring ? ring.r * 2 * art.k : 0
        height: width
        radius: width / 2
        x: ring ? ring.x * art.k - width / 2 : 0
        y: ring ? ring.y * art.k - width / 2 : 0
        color: "transparent"
        border.width: 1
        border.color: DeskStyle.amber
        opacity: ring ? Math.max(0, ring.a) : 0
    }
    Rectangle {   // кнопка ещё держится
        readonly property var held: art.pose.held || null
        visible: !!held
        width: 30 * art.k
        height: width
        radius: width / 2
        x: held ? held.x * art.k - width / 2 : 0
        y: held ? held.y * art.k - width / 2 : 0
        color: "transparent"
        border.width: 1.5
        border.color: DeskStyle.teal
    }
    Repeater {
        model: 5
        delegate: Rectangle {
            required property int index
            readonly property var f: index < art.pose.f.length ? art.pose.f[index] : null
            visible: !!f
            width: 14 * art.k
            height: width
            radius: width / 2
            x: f ? f.x * art.k - width / 2 : 0
            y: f ? f.y * art.k - width / 2 : 0
            color: f && f.off ? "transparent" : DeskStyle.amber
            opacity: f && f.off ? 1 : 0.9
            border.width: f && f.off ? 1.2 : 0
            border.color: DeskStyle.dim
        }
    }
}
