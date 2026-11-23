// Шкала (громкость) — как draw_meter в bcdesk: дорожка 6 точек белым .06 со скруглением 2, заливка до значения, у её
// конца — риска 2 точки высотой 14 (под мышью и пока тянут — 18) с прорезью в 2 точки вокруг (в дорожке — цветом фона
// модуля: у bcdesk это CLEAR в группе дорожки, фон модуля на столе сплошной — выходит то же).
import QtQuick

Item {
    id: m
    property real value: 0
    property color fill: DeskStyle.amber
    property real fillAlpha: 0.75
    property color tick: DeskStyle.amber
    property bool tall: false
    property color cut: DeskStyle.surface
    height: 6

    readonly property real fw: Math.round(width * Math.max(0, Math.min(1, value)))
    readonly property real th: 6 + (tall ? 12 : 8)
    readonly property real tx: fw - 1

    Rectangle {
        width: parent.width; height: 6; radius: 2
        color: Qt.rgba(1, 1, 1, 0.06)
    }
    Rectangle {
        visible: m.fw > 0
        width: m.fw; height: 6; radius: Math.min(2, m.fw / 2)
        color: Qt.rgba(m.fill.r, m.fill.g, m.fill.b, m.fillAlpha)
    }
    Rectangle {   // прорезь вокруг риски — только в дорожке
        x: m.tx - 2; width: 2 + 4; height: 6
        color: m.cut
    }
    Rectangle {
        x: m.tx; y: (6 - m.th) / 2
        width: 2; height: m.th; radius: 1
        color: m.tick
    }
}
