// Naomi.Screens — one screen of a NaomiPhone studio project (studio/, studio/README.md), as the phone shows it: black
// glass with the passport's corners, its layers bottom to top (a QML file a kind: layers/<Kind>Layer.qml — Clock, Date,
// Charge, Battery, Weather, Text, Apps, App, Panel (Background); "Layer": a file Date.qml or Text.qml would hide JavaScript's Date and QtQuick's Text in that
// folder), the camera's hole over them. The same file draws in the studio (its engine: the map, Try) and on the phone
// (the shell: the lock screen and the home of a loaded project — ShellView.qml, docs/SHELL.md «Проект студии»).
// The quick settings panel ("shade") is no file of this module: the studio draws its own (shadeSource — studio/qml/
// layers/ShadeLayer.qml), the shell has the real one (QuickSettings.qml) and only reads that the project has it.
// What each layer file gets: d — the layer (the studio's state.js), sw, sh — the screen's size, now — the minute, live
// (taps work), apps — the project's apps (Apps), picked — selected in the studio (Shade); it gives back baseX, baseY
// (its anchor at dx = dy = 0), box() — what it draws, in its own coordinates (null — nothing), rows() (Apps), and
// tapped(row) — a tap on it (row: an app's row; -1 — the layer).
// wall — the project's wallpaper on this screen (Wallpaper.qml; null — none), under the layers, on the glass; glass —
// the glass's color (transparent: the shell draws the wallpaper under the lock screen and the home itself, it stays
// put while they move — ShellView.qml). backdrop — how much of the background layers (Panel) shows: AOD's second stage
// leaves the other layers on black (AodScreen.qml).
import QtQuick
import QtQuick.Shapes
import Naomi.Ui

Item {
    id: sv
    property var scr: null              // {id, name, layers}
    property var device: ({ w: 393, h: 851, radius: 28, cutout: { x: 29.5, y: 28, d: 18 } })
    property var apps: []
    property string picked: ""          // the selected layer's id
    property var now: new Date()
    property bool live: false
    property color bezel: "#1c1d20"     // round the corners (the map: the DOM tile's ring is there)
    signal open(string to, int row)
    property var wall: null             // the wallpaper (project.functions.wallpaper, when this screen shows it)
    property string assetBase: ""       // where the project's pictures are (Wallpaper.qml)
    property color glass: Theme.canvas
    property real backdrop: 1
    property real shiftX: 0             // the layers moved (AOD against burn-in) — not the background ones
    property real shiftY: 0
    signal reshaped()                   // a box changed by itself (the wallpaper's picture came)

    property url shadeSource: ""        // the studio's panel; none — a shade layer draws nothing here (the shell's own)
    property var appSources: ({})       // the studio's app screens (id → its QML); none — the app layer's placeholder
    readonly property int count: scr && scr.layers ? scr.layers.length : 0
    readonly property var kinds: ({ clock: "Clock", date: "Date", charge: "Charge", text: "Text", apps: "Apps", app: "App",
                                    battery: "Battery", weather: "Weather", panel: "Panel" })
    function sourceOf(type) {
        if (type === "shade") return shadeSource
        return kinds[type] ? Qt.resolvedUrl("layers/" + kinds[type] + "Layer.qml") : ""
    }
    width: device.w; height: device.h
    clip: true

    Rectangle { anchors.fill: parent; radius: sv.device.radius; color: sv.glass }
    Wallpaper {
        id: wp
        anchors.fill: parent
        visible: sv.wall !== null
        wall: sv.wall
        assetBase: sv.assetBase
        opacity: sv.shade ? sv.shade.contentAlpha : 1
        onReshaped: sv.reshaped()
    }

    // the shade (if the screen has one) moves the others from under it
    property Item shade: null
    Repeater {
        id: rep
        model: sv.count
        Loader {
            id: ld
            readonly property var d: sv.scr && sv.scr.layers ? sv.scr.layers[index] ?? null : null
            readonly property bool under: ld.d !== null && ld.d.type !== "shade" && sv.shade !== null
            source: d ? sv.sourceOf(d.type) : ""
            visible: !!d && !d.hidden
            opacity: (under ? sv.shade.contentAlpha : 1) * (ld.back ? sv.backdrop : 1)
            readonly property bool back: !!ld.d && ld.d.type === "panel"
            transform: Translate { x: ld.back ? 0 : sv.shiftX; y: (ld.under ? sv.shade.contentDy : 0) + (ld.back ? 0 : sv.shiftY) }
            onLoaded: {
                item.d = Qt.binding(() => ld.d ?? ({}))
                item.sw = Qt.binding(() => sv.width)
                item.sh = Qt.binding(() => sv.height)
                item.now = Qt.binding(() => sv.now)
                item.live = Qt.binding(() => sv.live)
                if (item.apps !== undefined) item.apps = Qt.binding(() => sv.apps)
                if (item.appSources !== undefined) item.appSources = Qt.binding(() => sv.appSources)
                if (item.picked !== undefined) item.picked = Qt.binding(() => !!ld.d && sv.picked === ld.d.id)
                if (d && d.type === "shade") sv.shade = item
            }
            onItemChanged: if (!item && sv.shade && !sv.hasShade()) sv.shade = null
            Connections {
                target: ld.item
                ignoreUnknownSignals: true
                function onTapped(row) {
                    if (row >= 0) { const a = sv.apps[row]; if (a) sv.open(a.to ?? "", row) }   // (the phone opens the app itself: apps[row].id)
                    else if (ld.d && ld.d.tap) sv.open(ld.d.tap.to, -1)
                }
            }
        }
    }
    function hasShade() {
        for (let i = 0; i < rep.count; i++) { const l = rep.itemAt(i); if (l && l.item && l.d && l.d.type === "shade") return true }
        return false
    }

    // the camera's hole (Pixel 5: top left)
    Rectangle {
        readonly property var c: sv.device.cutout
        visible: !!c
        x: c ? c.x - c.d / 2 : 0; y: c ? c.y - c.d / 2 : 0
        width: c ? c.d : 0; height: width; radius: width / 2
        color: "black"
    }
    // the corners: the bezel over what a layer draws past the glass's rounded edge
    Repeater {
        model: 4
        Shape {
            readonly property real r: sv.device.radius
            x: index % 2 ? sv.width - r : 0
            y: index > 1 ? sv.height - r : 0
            width: r; height: r
            visible: sv.bezel.a > 0
            rotation: [0, 90, 270, 180][index]
            ShapePath {
                strokeWidth: -1
                fillColor: sv.bezel
                startX: 0; startY: 0
                PathLine { x: r; y: 0 }
                PathArc { x: 0; y: r; radiusX: r; radiusY: r; direction: PathArc.Counterclockwise }
                PathLine { x: 0; y: 0 }
            }
        }
    }

    // What the studio's overlay draws by: each layer's box (screen coordinates; null — not drawn), its base, the apps'
    // rows and the wallpaper's box (w: past the screen's edges too)
    function boxes() {
        const b = [], a = [], rows = {}
        for (let i = 0; i < count; i++) {
            const ld = rep.itemAt(i), it = ld ? ld.item : null
            if (!it || !ld.visible || ld.opacity <= 0 || !it.visible) { b.push(null); a.push(it ? [it.baseX, it.baseY] : null); continue }
            const r = it.box()
            if (r) {
                const p = it.mapToItem(sv, r[0], r[1]), q = it.mapToItem(sv, r[2], r[3])
                b.push([round(p.x), round(p.y), round(q.x), round(q.y)])
            } else b.push(null)
            a.push([round(it.baseX), round(it.baseY)])
            if (it.rows) rows[i] = it.rows().map(r => {
                if (!r) return null
                const p = it.mapToItem(sv, r[0], r[1]), q = it.mapToItem(sv, r[2], r[3])
                return [round(p.x), round(p.y), round(q.x), round(q.y)]
            })
        }
        const w = sv.wall !== null ? wp.box() : null
        return { b: b, a: a, r: rows, w: w ? w.map(round) : null }
    }
    function round(v) { return Math.round(v * 10) / 10 }
    // a picture still coming (the studio's pictures of screens wait for it)
    function loading() { return sv.wall !== null && wp.loading }
    function reset() {
        for (let i = 0; i < rep.count; i++) { const ld = rep.itemAt(i); if (ld && ld.item && ld.item.reset) ld.item.reset() }
    }
}
