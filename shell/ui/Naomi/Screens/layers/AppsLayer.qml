// Layer "apps" — the home screen's list of apps, as shell/app/qml/HomeScreen.qml draws it: a row each — the icon
// (Material Symbols Rounded, filled), the title (Google Sans Medium 21), the line under it (15, Theme.muted), "›"; at
// the bottom, 30 dp above the edge; a row lights up while pressed. The rows are the project's apps (state.js: project.apps
// — {title, sub, icon, to}); in Try a tap on a row opens its screen. Props: color (titles), color2 (the rest: lines,
// icons, "›" — painted all one color, they all follow);
// dx, dy. Anchor — the middle of its bottom edge (rows added grow it upwards, as on the phone).
import QtQuick
import Naomi.Ui

Item {
    id: root
    property var d: ({})
    property real sw: 393
    property real sh: 851
    property var now: new Date()
    property bool live: false
    property var apps: []
    signal tapped(int row)

    readonly property real baseX: sw / 2
    readonly property real baseY: sh - 30
    width: sw; height: list.height
    x: baseX + (d.dx ?? 0) - width / 2
    y: baseY + (d.dy ?? 0) - height

    Item {
        id: list
        width: root.width
        height: rep.count * 68
        Repeater {
            id: rep
            model: root.apps ? root.apps.length : 0
            Item {
                id: row
                readonly property var a: root.apps[index] ?? ({})
                width: list.width
                height: 68
                y: index * 68
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 10; anchors.rightMargin: 10
                    radius: 18
                    color: Theme.surface
                    opacity: tap.pressed ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }
                Text {
                    x: 33 - width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.a.icon ?? ""
                    font.family: "Material Symbols Rounded"; font.pixelSize: 26
                    font.variableAxes: ({ "FILL": 1, "wght": 400, "opsz": 24 })
                    color: root.d.color2 ?? Theme.muted
                }
                Column {
                    x: 64
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    Text {
                        text: row.a.title ?? ""
                        font.family: Theme.sans; font.pixelSize: 21; font.weight: Font.Medium
                        color: root.d.color ?? Theme.text
                    }
                    Text {
                        visible: (row.a.sub ?? "") !== ""
                        text: row.a.sub ?? ""
                        font.family: Theme.sans; font.pixelSize: 15
                        color: root.d.color2 ?? Theme.muted
                    }
                }
                Text {
                    anchors.right: parent.right; anchors.rightMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: "chevron_right"
                    font.family: "Material Symbols Rounded"; font.pixelSize: 24
                    color: root.d.color2 ?? Theme.dim
                }
                TapHandler { id: tap; enabled: root.live; onTapped: root.tapped(index) }
            }
        }
    }
    function box() { return [0, 0, width, Math.max(height, 1)] }
    // each row's box: where its link starts on the map
    function rows() {
        const out = []
        for (let i = 0; i < rep.count; i++) {
            const r = rep.itemAt(i)
            out.push(r ? [10, r.y, width - 10, r.y + r.height] : null)
        }
        return out
    }
}
