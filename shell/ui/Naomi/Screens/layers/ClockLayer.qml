// Layer "clock" — the big time, as the shell's lock screen drew it (ClockFace.qml — gone with the lock screen,
// 12 Nov 2026): hh:mm in Google Sans Flex,
// rounded (ROND 100), light (wght 300), 96 px, tabular figures, Theme.text; by default centered, its top at 17 % of
// the screen's height. Props (state.js KINDS.clock — the same defaults): size, weight, color; dx, dy — moved from there.
// The layer's anchor (what X and Y are in the studio) is the middle of its line; base — where it is at dx = dy = 0.
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

    readonly property real size: d.size ?? 96
    readonly property real baseX: sw / 2
    readonly property real baseY: sh * 0.17 + probe.height / 2
    width: t.width; height: t.height
    x: baseX + (d.dx ?? 0) - width / 2
    y: baseY + (d.dy ?? 0) - height / 2

    Text {
        id: t
        text: Qt.formatTime(root.now, "hh:mm")
        font.family: Theme.clock
        font.pixelSize: root.size
        font.variableAxes: ({ "wght": root.d.weight ?? 300, "ROND": 100, "opsz": Math.max(6, Math.min(144, root.size)) })
        font.features: { "tnum": 1 }
        color: root.d.color ?? Theme.text
    }
    // the default look's line: where the clock stands by default (ClockFace: its top at 17 %)
    Text {
        id: probe
        visible: false
        text: "0"
        font.family: Theme.clock
        font.pixelSize: 96
        font.variableAxes: ({ "wght": 300, "ROND": 100, "opsz": 96 })
    }
    TextMetrics { id: tm; font: t.font; text: t.text }
    // what it draws: the digits' ink
    function box() {
        const r = tm.tightBoundingRect
        return r.width > 0 ? [r.x, t.baselineOffset + r.y, r.x + r.width, t.baselineOffset + r.y + r.height] : [0, 0, width, height]
    }
    TapHandler { enabled: root.live && !!root.d.tap; onTapped: root.tapped(-1) }
}
