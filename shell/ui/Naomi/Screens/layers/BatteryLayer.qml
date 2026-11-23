// Layer "battery" — the battery's charge (naomiPhone.battery: percent, charging), in one of four looks (d.look):
//   line — capitals as the date's: "заряд · 87%" ("заряжается · 87%" while charging, in the accent);
//   ring — a ring round the percent (Google Sans Flex), the charge along it in the accent;
//   bar  — a thin pill filled to the charge, the percent beside it;
//   icon — Material Symbols' battery (its bars by the charge, a bolt while charging) and the percent.
// Props (the studio's state.js KINDS.battery): look, scale (1 — as drawn here), color (the accent: the charge),
// color2 (the rest: the text, the track); dx, dy. By default centered under the date (30 % of the height). Anchor — its
// middle.
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

    readonly property string look: d.look ?? "line"
    readonly property real k: d.scale ?? 1
    readonly property int percent: naomiPhone.battery.percent
    readonly property bool charging: naomiPhone.battery.charging
    readonly property color accent: d.color ?? Theme.amber
    readonly property color rest: d.color2 ?? Theme.muted
    readonly property real baseX: sw / 2
    readonly property real baseY: sh * 0.30
    width: body.item ? body.item.width : 0
    height: body.item ? body.item.height : 0
    x: baseX + (d.dx ?? 0) - width / 2
    y: baseY + (d.dy ?? 0) - height / 2

    Loader {
        id: body
        sourceComponent: ({ ring: ring, bar: bar, icon: icon })[root.look] ?? line
    }

    Component {
        id: line
        Caps {
            text: (root.charging ? "заряжается · " : "заряд · ") + root.percent + "%"
            font.pixelSize: Math.round(12 * root.k)
            font.letterSpacing: 1.2 * root.k
            color: root.charging ? root.accent : root.rest
        }
    }

    Component {
        id: ring
        Item {
            width: 46 * root.k; height: width
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath {   // the track
                    strokeWidth: 3.5 * root.k
                    strokeColor: Qt.rgba(root.rest.r, root.rest.g, root.rest.b, 0.28)
                    fillColor: "transparent"
                    PathAngleArc { centerX: 23 * root.k; centerY: 23 * root.k; radiusX: 21 * root.k; radiusY: 21 * root.k; startAngle: 0; sweepAngle: 360 }
                }
                ShapePath {   // the charge, from the top clockwise
                    strokeWidth: 3.5 * root.k
                    strokeColor: root.accent
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    PathAngleArc { centerX: 23 * root.k; centerY: 23 * root.k; radiusX: 21 * root.k; radiusY: 21 * root.k; startAngle: -90; sweepAngle: 3.6 * root.percent }
                }
            }
            Text {
                anchors.centerIn: parent
                text: root.percent
                font.family: Theme.clock
                font.pixelSize: Math.round(15 * root.k)
                font.variableAxes: ({ "wght": 500, "ROND": 100 })
                color: Theme.text
            }
            Text {   // the bolt while charging, at the ring's foot
                visible: root.charging
                anchors.horizontalCenter: parent.horizontalCenter
                y: parent.height - height * 0.55
                text: "bolt"
                font.family: "Material Symbols Rounded"
                font.pixelSize: Math.round(13 * root.k)
                font.variableAxes: ({ "FILL": 1, "wght": 500 })
                color: root.accent
                style: Text.Outline; styleColor: Theme.canvas
            }
        }
    }

    Component {
        id: bar
        Row {
            spacing: 10 * root.k
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 110 * root.k; height: 8 * root.k; radius: height / 2
                color: Qt.rgba(root.rest.r, root.rest.g, root.rest.b, 0.25)
                Rectangle {
                    width: Math.max(parent.height, parent.width * root.percent / 100); height: parent.height; radius: parent.radius
                    color: root.accent
                }
            }
            Caps {
                anchors.verticalCenter: parent.verticalCenter
                text: root.percent + "%"
                font.pixelSize: Math.round(12 * root.k)
                font.letterSpacing: 1.2 * root.k
                color: root.charging ? root.accent : root.rest
            }
        }
    }

    Component {
        id: icon
        Row {
            spacing: 4 * root.k
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.charging ? "battery_charging_full"
                    : root.percent >= 95 ? "battery_full" : "battery_" + Math.max(0, Math.min(6, Math.floor(root.percent / 14))) + "_bar"
                font.family: "Material Symbols Rounded"
                font.pixelSize: Math.round(22 * root.k)
                font.variableAxes: ({ "FILL": 1, "wght": 400 })
                rotation: 90
                color: root.charging ? root.accent : root.rest
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.percent + "%"
                font.family: Theme.sans
                font.pixelSize: Math.round(15 * root.k)
                font.weight: Font.Medium
                color: Theme.text
            }
        }
    }

    function box() { return [0, 0, width, height] }
    TapHandler { enabled: root.live && !!root.d.tap; onTapped: root.tapped(-1) }
}
