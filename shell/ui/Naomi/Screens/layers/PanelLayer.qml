// Layer "panel" — Background: the screen black above (pure #000 — the AMOLED's pixels there are off), a panel from its
// top down to the screen's bottom edge: its color at its brightness (bright 0.1 … 1 — the color times it), its top
// corners rounded — radius (points) and smoothing (0 … 1): 0 — a quarter of a circle; more — the circle's arc shorter,
// a cubic Bézier on each side of it, the corner bends into the edges gradually (as iOS's corners, Figma's "corner
// smoothing"). The construction is the one Figma described (Daniel Furse, «Desperately seeking squircles», 2018): p —
// how far from the corner the curve leaves each edge, (1 + smoothing) · radius; the arc — 90° · (1 − smoothing), round
// the corner's diagonal; the Bézier from the edge to the arc runs along the edge (a, b) and then along the arc's tangent
// (c, d). Not enough room (the panel lower than its corners) — less smoothing, then a smaller radius.
// Its place: its top at the camera's row's bottom (Theme.statusBarHeight, 50) + dy; no dx (the screen's width). Props
// (state.js KINDS.panel): color, bright, radius, smooth; dy. Anchor — the middle of its top edge. The bottom of the
// screen's layers (the studio adds it under the others); AOD's second stage hides it (ScreenView backdrop).
import QtQuick
import QtQuick.Shapes
import Naomi.Ui

Item {
    id: root
    property var d: ({})
    property real sw: 393
    property real sh: 851
    property var now: new Date()
    property bool live: false
    signal tapped(int row)

    readonly property real baseX: sw / 2
    readonly property real baseY: Theme.statusBarHeight
    readonly property real panelTop: Math.max(0, Math.min(sh - 1, baseY + (d.dy ?? 0)))
    readonly property color fill: {
        const c = Qt.color(d.color ?? "#E2DED3"), k = Math.max(0, Math.min(1, d.bright ?? 1))
        return Qt.rgba(c.r * k, c.g * k, c.b * k, 1)
    }
    width: sw; height: sh

    Rectangle { anchors.fill: parent; color: "#000000" }
    Rectangle {   // (no rounding: a plain panel)
        visible: !panel.visible
        y: root.panelTop
        width: root.sw; height: root.sh - root.panelTop
        color: root.fill
    }
    Shape {
        id: panel
        y: root.panelTop
        width: root.sw; height: root.sh - root.panelTop
        visible: r >= 0.5
        preferredRendererType: Shape.CurveRenderer
        readonly property real room: Math.min(width, height) / 2
        readonly property real r: Math.max(0, Math.min(root.d.radius ?? 28, room))
        readonly property real s: r > 0 ? Math.max(0, Math.min(root.d.smooth ?? 0.6, room / r - 1)) : 0
        readonly property real p: Math.min((1 + s) * r, room)
        readonly property real arc: 90 * (1 - s)                                    // the arc, degrees
        readonly property real len: Math.sin(arc / 2 * Math.PI / 180) * r * Math.SQRT2  // the arc's run on each axis
        readonly property real tangent: r * Math.tan((90 - arc) / 4 * Math.PI / 180)   // the arc's start → the edge
        readonly property real c: tangent * Math.cos(45 * s * Math.PI / 180)
        readonly property real dd: c * Math.tan(45 * s * Math.PI / 180)
        readonly property real b: (p - len - c - dd) / 3
        readonly property real a: 2 * b
        ShapePath {
            strokeWidth: -1
            fillColor: root.fill
            startX: 0; startY: panel.height
            PathLine { x: 0; y: panel.p }
            // top left: up the left edge, the arc, along the top edge
            PathCubic {
                control1X: 0; control1Y: panel.p - panel.a
                control2X: 0; control2Y: panel.p - panel.a - panel.b
                x: panel.dd; y: panel.dd + panel.len
            }
            PathArc { x: panel.dd + panel.len; y: panel.dd; radiusX: panel.r; radiusY: panel.r }
            PathCubic {
                control1X: panel.p - panel.a - panel.b; control1Y: 0
                control2X: panel.p - panel.a; control2Y: 0
                x: panel.p; y: 0
            }
            PathLine { x: panel.width - panel.p; y: 0 }
            // top right: the same, mirrored
            PathCubic {
                control1X: panel.width - panel.p + panel.a; control1Y: 0
                control2X: panel.width - panel.p + panel.a + panel.b; control2Y: 0
                x: panel.width - panel.dd - panel.len; y: panel.dd
            }
            PathArc { x: panel.width - panel.dd; y: panel.dd + panel.len; radiusX: panel.r; radiusY: panel.r }
            PathCubic {
                control1X: panel.width; control1Y: panel.p - panel.a - panel.b
                control2X: panel.width; control2Y: panel.p - panel.a
                x: panel.width; y: panel.p
            }
            PathLine { x: panel.width; y: panel.height }
            PathLine { x: 0; y: panel.height }
        }
    }
    function box() { return [0, panelTop, sw, sh] }
}
