// Layer "date" — the date under the clock, as the shell's lock screen had it (ClockFace.qml — gone with the lock screen, 12 Nov 2026): Naomi.Ui Caps (Google Sans Medium,
// capitals, letter spacing 1.2), 12 px, Theme.muted, "dddd · d MMMM" in the Russian locale; by default centered 2 px
// under the default clock. Props (state.js KINDS.date): size, color; dx, dy. Anchor — the middle of its line.
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

    readonly property real size: d.size ?? 12
    readonly property real baseX: sw / 2
    readonly property real baseY: sh * 0.17 + clockProbe.height + 2 + probe.height / 2
    width: t.width; height: t.height
    x: baseX + (d.dx ?? 0) - width / 2
    y: baseY + (d.dy ?? 0) - height / 2

    Caps {
        id: t
        text: root.now.toLocaleDateString(Qt.locale("ru_RU"), "dddd · d MMMM")
        font.pixelSize: root.size
        font.letterSpacing: 1.2 * root.size / 12
        color: root.d.color ?? Theme.muted
    }
    Caps { id: probe; visible: false; text: "0"; font.pixelSize: 12 }
    Text {
        id: clockProbe
        visible: false
        text: "0"
        font.family: Theme.clock
        font.pixelSize: 96
        font.variableAxes: ({ "wght": 300, "ROND": 100, "opsz": 96 })
    }
    TextMetrics { id: tm; font: t.font; text: t.text.toUpperCase() }
    function box() {
        const r = tm.tightBoundingRect
        return r.width > 0 ? [r.x, t.baselineOffset + r.y, Math.max(r.x + r.width, t.width - root.size * 0.1), t.baselineOffset + r.y + r.height]
                           : [0, 0, width, height]
    }
    TapHandler { enabled: root.live && !!root.d.tap; onTapped: root.tapped(-1) }
}
