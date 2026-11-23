// График в «Подробно»: сетка по ширине и основание линией .16, кривая янтарём 1,6 с заливкой .12 под ней (веб Наоми
// .en-curve), пунктир тусклым — для сравнения, отметка — пунктир от основания до точки, точка и значение над ней;
// подписи — по низу (у краёв прижаты к краю). Пунктир — отрезками: у CurveRenderer своего пунктира нет.
import QtQuick
import QtQuick.Shapes
import ".."

Item {
    id: c
    property var line: []        // [[u, v], …]: u и v — 0…1, v — вверх
    property var ref: []         // пунктиром, так же
    property var gridX: [0, 0.25, 0.5, 0.75, 1]
    property var labels: []      // [{ u, text }]
    property var mark: null      // { u, v, text }
    property string corner: ""   // надпись в левом верхнем углу поля
    height: 128

    readonly property real l: 10
    readonly property real r: width - 14
    readonly property real t: 18
    readonly property real b: height - 24
    function px(u) { return l + u * (r - l) }
    function py(v) { return b - Math.max(0, Math.min(1, v)) * (b - t) }
    function pts(a) { return a.map(p => Qt.point(px(p[0]), py(p[1]))) }
    // пунктир по ломаной (в точках поля): штрих on, пропуск off
    function dashes(a, on, off) {
        const out = []
        let draw = true, left = on
        for (let i = 1; i < a.length; ++i) {
            let x0 = a[i - 1].x, y0 = a[i - 1].y
            const x1 = a[i].x, y1 = a[i].y
            let len = Math.hypot(x1 - x0, y1 - y0)
            while (len > 0) {
                const s = Math.min(left, len), k = s / len
                const xm = x0 + (x1 - x0) * k, ym = y0 + (y1 - y0) * k
                if (draw)
                    out.push([Qt.point(x0, y0), Qt.point(xm, ym)])
                x0 = xm; y0 = ym; len -= s; left -= s
                if (left <= 0) {
                    draw = !draw
                    left = draw ? on : off
                }
            }
        }
        return out
    }

    Repeater {
        model: c.gridX
        delegate: Rectangle {
            required property real modelData
            x: Math.round(c.px(modelData)); y: c.t - 6
            width: 1; height: c.b - y
            color: DeskStyle.amberA(DeskStyle.line)
        }
    }
    Rectangle { x: c.l; y: Math.round(c.b); width: c.r - c.l; height: 1; color: DeskStyle.amberA(DeskStyle.line) }
    Shape {
        preferredRendererType: Shape.CurveRenderer
        ShapePath {   // заливка под кривой
            strokeColor: "transparent"
            fillColor: DeskStyle.amberA(0.12)
            PathPolyline {
                path: c.line.length ? c.pts(c.line.concat([[c.line[c.line.length - 1][0], 0], [c.line[0][0], 0], c.line[0]])) : []
            }
        }
        ShapePath {
            strokeColor: DeskStyle.dim
            strokeWidth: 1
            fillColor: "transparent"
            PathMultiline { paths: c.ref.length ? c.dashes(c.pts(c.ref), 3, 4) : [] }
        }
        ShapePath {
            strokeColor: DeskStyle.amber
            strokeWidth: 1.6
            fillColor: "transparent"
            joinStyle: ShapePath.RoundJoin
            capStyle: ShapePath.RoundCap
            PathPolyline { path: c.pts(c.line) }
        }
        ShapePath {
            strokeColor: DeskStyle.amberA(0.75)
            strokeWidth: 1
            fillColor: "transparent"
            PathMultiline {
                paths: c.mark ? c.dashes([Qt.point(c.px(c.mark.u), c.b), Qt.point(c.px(c.mark.u), c.py(c.mark.v))], 3, 3) : []
            }
        }
    }
    Rectangle {
        visible: c.mark !== null
        x: c.mark ? c.px(c.mark.u) - 2.5 : 0
        y: c.mark ? c.py(c.mark.v) - 2.5 : 0
        width: 5; height: 5; radius: 2.5
        color: DeskStyle.amber
    }
    Text {
        visible: c.mark !== null && c.mark.text !== ""
        x: c.mark ? Math.max(c.l, Math.min(c.r - implicitWidth, c.px(c.mark.u) - implicitWidth / 2)) : 0
        y: c.mark ? Math.max(0, c.py(c.mark.v) - 6 - implicitHeight) : 0
        text: c.mark ? c.mark.text : ""
        font.family: DeskStyle.mono
        font.pixelSize: 12
        color: DeskStyle.text
    }
    Text {
        visible: c.corner !== ""
        x: c.l + 8; y: c.t + 4
        text: c.corner
        font.family: DeskStyle.mono
        font.pixelSize: 12
        color: DeskStyle.text
    }
    Repeater {
        model: c.labels
        delegate: Text {
            required property var modelData
            x: modelData.u <= 0 ? c.px(0) : modelData.u >= 1 ? c.px(1) - implicitWidth : c.px(modelData.u) - implicitWidth / 2
            y: c.b + 6
            text: modelData.text
            font.family: DeskStyle.mono
            font.pixelSize: 11
            color: DeskStyle.dim
        }
    }
}
