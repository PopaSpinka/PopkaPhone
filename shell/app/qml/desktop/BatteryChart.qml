// График окна батареи (docs/DESKTOP.md, «Батарея»; вид выбран пользователем — «Заряд и мощность»): две дорожки на одной
// шкале времени, как в мониторе системы. Сверху — заряд, линия с заливкой (0–50–100 %; телефон был выключен — разрыв),
// снизу — мощность батареи: в неё — бирюзой вверх, из неё — янтарём вниз (±3 Вт, больше — упирается в край). Под ними —
// время: сутки — по три часа (12A 3 6 9 12P …), неделя — по дням. Данные — naomid History (DeskBattery.history).
import QtQuick
import QtQuick.Shapes

Item {
    id: chart
    property var history: null

    readonly property real cw: 284           // сами дорожки; справа — подписи шкал
    readonly property real t1: 14            // заряд: верх и высота
    readonly property real h1: 46
    readonly property real t2: t1 + h1 + 20  // мощность
    readonly property real h2: 36
    readonly property real zero: t2 + h2 / 2
    readonly property real maxW: 3
    width: 316
    height: t2 + h2 + 18

    readonly property int n: history && history.level ? history.level.length : 0
    readonly property real bw: n > 0 ? cw / n : 1
    readonly property color gridColor: Qt.rgba(236 / 255, 233 / 255, 227 / 255, 0.1)

    // куски линии заряда без разрывов: line — точки по серединам корзин, area — они же с низом дорожки
    readonly property var segments: {
        const out = []
        let cur = null
        for (let i = 0; i < n; ++i) {
            const L = history.level[i]
            if (L < 0) {
                cur = null
                continue
            }
            if (!cur) {
                cur = []
                out.push(cur)
            }
            cur.push(Qt.point((i + 0.5) * bw, t1 + (1 - L / 100) * h1))
        }
        return out.map(line => ({
            line: line,
            area: [Qt.point(line[0].x, t1 + h1)].concat(line, [Qt.point(line[line.length - 1].x, t1 + h1), Qt.point(line[0].x, t1 + h1)])
        }))
    }
    readonly property var bars: {
        const out = []
        for (let i = 0; i < n; ++i) {
            const w = history.watt[i]
            if (w === undefined || w === null || isNaN(w) || history.level[i] < 0)
                continue
            const v = Math.max(-maxW, Math.min(maxW, w)), h = Math.abs(v) / maxW * h2 / 2
            if (h < 0.4)
                continue
            out.push({ x: i * bw + 0.3, y: v > 0 ? zero - h : zero, w: Math.max(0.6, bw - 0.6), h: h, up: v > 0 })
        }
        return out
    }
    readonly property var ticks: {
        const out = []
        if (!n)
            return out
        const DAYS = ["SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"]
        for (let i = 0; i < n; ++i) {
            const d = new Date((history.start + i * history.step) * 1000), h = d.getHours()
            if (history.step < 3600) {
                if (d.getMinutes() === 0 && h % 3 === 0)
                    out.push({ x: i * bw, label: h === 0 ? "12A" : h === 12 ? "12P" : String(h % 12) })
            } else if (h === 0) {
                out.push({ x: i * bw, label: DAYS[d.getDay()] })
            }
        }
        return out
    }

    component Small: Text {
        font.family: DeskStyle.mono
        font.pixelSize: 10
        font.letterSpacing: 0.6
        color: DeskStyle.dim
    }
    component Scale: Small {   // подпись шкалы справа (x — у места), серединой на уровне линии
        property real at
        y: at - height / 2
    }

    Shape {   // сетка: заряд 100 / 50 / 0 и отметки времени — пунктиром; ноль мощности — сплошной
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: chart.gridColor
            strokeWidth: 1
            fillColor: "transparent"
            strokeStyle: ShapePath.DashLine
            dashPattern: [2, 3]
            PathMultiline {
                paths: [0, 0.5, 1].map(k => [Qt.point(0, chart.t1 + k * chart.h1), Qt.point(chart.cw, chart.t1 + k * chart.h1)])
                    .concat(chart.ticks.map(t => [Qt.point(t.x, chart.t1), Qt.point(t.x, chart.t2 + chart.h2 + 4)]))
            }
        }
        ShapePath {
            strokeColor: chart.gridColor
            strokeWidth: 1
            fillColor: "transparent"
            PathPolyline { path: [Qt.point(0, chart.zero), Qt.point(chart.cw, chart.zero)] }
        }
    }

    Repeater {
        model: chart.segments
        delegate: Shape {
            id: seg
            required property var modelData
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: Qt.rgba(99 / 255, 182 / 255, 176 / 255, 0.16)
                strokeColor: "transparent"
                PathPolyline { path: seg.modelData.area }
            }
            ShapePath {
                fillColor: "transparent"
                strokeColor: DeskStyle.teal
                strokeWidth: 1.4
                joinStyle: ShapePath.RoundJoin
                capStyle: ShapePath.RoundCap
                PathPolyline { path: seg.modelData.line }
            }
        }
    }
    Repeater {
        model: chart.bars
        delegate: Rectangle {
            id: bar
            required property var modelData
            x: bar.modelData.x
            y: bar.modelData.y
            width: bar.modelData.w
            height: bar.modelData.h
            color: bar.modelData.up ? DeskStyle.teal : DeskStyle.amber
            opacity: 0.85
        }
    }

    Small { y: 0; text: "ЗАРЯД" }
    Small { y: chart.t2 - 15; text: "МОЩНОСТЬ" }
    Scale { x: chart.cw + 5; at: chart.t1; text: "100%" }
    Scale { x: chart.cw + 5; at: chart.t1 + chart.h1 / 2; text: "50%" }
    Scale { x: chart.cw + 5; at: chart.t1 + chart.h1; text: "0%" }
    Scale { x: chart.cw + 5; at: chart.t2; text: "+" + chart.maxW + " Вт" }
    Scale { x: chart.cw + 5; at: chart.t2 + chart.h2; text: "−" + chart.maxW }
    Repeater {
        model: chart.ticks
        delegate: Small {
            id: tick
            required property var modelData
            x: tick.modelData.x + 3
            y: chart.t2 + chart.h2 + 6
            text: tick.modelData.label
        }
    }
}
