// Layer "text" — any text, in the phone's text font (Theme.sans: Google Sans, as the home screen's titles), Theme.text.
// By default in the middle of the screen. Props (state.js KINDS.text): text, size, weight, color; dx, dy. Anchor — the
// middle of its line.
import QtQuick
import Naomi.Ui

Item {
    id: root
    property var d: ({})
    property real sw: 393
    property real sh: 851
    property var now: new Date()
    property bool live: false
    signal tapped(int row)

    readonly property real size: d.size ?? 21
    readonly property real baseX: sw / 2
    readonly property real baseY: sh / 2
    width: t.width; height: t.height
    x: baseX + (d.dx ?? 0) - width / 2
    y: baseY + (d.dy ?? 0) - height / 2

    Text {
        id: t
        text: root.d.text ?? "Text"
        font.family: Theme.sans
        font.pixelSize: root.size
        font.variableAxes: ({ "wght": root.d.weight ?? 500 })
        color: root.d.color ?? Theme.text
    }
    TextMetrics { id: tm; font: t.font; text: t.text }
    function box() {
        const r = tm.tightBoundingRect
        return r.width > 0 ? [r.x, t.baselineOffset + r.y, r.x + r.width, t.baselineOffset + r.y + r.height] : [0, 0, Math.max(width, 8), height]
    }
    TapHandler { enabled: root.live && !!root.d.tap; onTapped: root.tapped(-1) }
}
