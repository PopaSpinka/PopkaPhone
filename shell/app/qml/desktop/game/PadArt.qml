// Рисунок окна «Кнопки геймпада» (docs/GAMES.md, «Кнопки геймпада»): 8BitDo Ultimate спереди — все кнопки на своих
// местах, как на фото и в инструкции 8BitDo; сверху бамперы L1 R1 и курки L2 R2, лепестки P1 P2 — пунктиром (они
// сзади). От кнопок — выноски к плашкам с клавишей, как в инструкции (вариант 01, выбор пользователя 21 ноября 2026);
// середина (− ★ Home +) — плашками со значками под корпусом. Кнопку выбирают щелчком по ней или по её плашке
// (picked); стик и крестовина — целиком (окно идёт по их направлениям). Плоская кнопка под ★ — профиль самого
// геймпада, телефону не видна: тусклая, не выбирается. Поле — 940 × 394 точки, корпус — с 170 (pad.js).
//   Цвет кнопки: выбрана — заливка и контур янтарём; под мышью — светлый контур; назначена — контур янтарём
//   вполсилы; нет — приглушённый.
import QtQuick
import QtQuick.Shapes
import ".."
import "pad.js" as Pad

Item {
    id: art
    property string sel: ""          // выбранная кнопка
    property var keys: ({})          // «клавишами»: кнопка → клавиша
    property var nat: ({})           // «как есть»: кнопка → что делает сама
    property bool asIs: false
    property string flash: ""        // «как есть»: кнопку нажали на геймпаде — горит
    signal picked(string id)
    width: 940
    height: 394

    readonly property int tx: 170
    readonly property string selGroup: sel ? Pad.group(sel) : ""
    readonly property color keyFill: "#141512"
    function lit(id) {
        const grp = Pad.GROUPS[id]
        return (!asIs && sel !== "" && (sel === id || selGroup === id))
            || (flash !== "" && (flash === id || (!!grp && grp.indexOf(flash) >= 0)))
    }
    function isSet(id) {
        const grp = Pad.GROUPS[id]
        if (grp)
            return grp.some(i => isSet(i))
        return asIs ? !!nat[id] : !!keys[id]
    }
    function val(id) { return asIs ? (nat[id] || "") : Pad.kshort(keys[id]) }
    function chipVal(id) { return Pad.GROUPS[id] ? Pad.groupSum(id, keys, nat, asIs) : (val(id) || "—") }
    function stroke(id, hover) {
        return lit(id) ? DeskStyle.amber : hover ? DeskStyle.text : isSet(id) ? DeskStyle.amberA(0.55) : DeskStyle.muted
    }
    function fill(id, hover) { return lit(id) ? DeskStyle.amberA(0.22) : hover ? DeskStyle.amberA(0.07) : keyFill }
    function ink(id) { return lit(id) ? DeskStyle.amber : DeskStyle.muted }

    component Caps: Text {
        font.family: DeskStyle.mono
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    // кнопка-круг (A B X Y, середина): cx, cy, r — в поле корпуса
    component Round: Rectangle {
        id: rb
        property string cid
        property real cx
        property real cy
        property real r
        property string label
        readonly property bool hovered: rbArea.containsMouse
        x: art.tx + cx - r
        y: cy - r
        width: 2 * r
        height: 2 * r
        radius: r
        antialiasing: true
        color: art.fill(cid, hovered)
        border.width: 1.5
        border.color: art.stroke(cid, hovered)
        Caps {
            anchors.centerIn: parent
            text: rb.label
            font.pixelSize: 13
            font.weight: Font.Medium
            color: art.ink(rb.cid)
        }
        MouseArea { id: rbArea; anchors.fill: parent; hoverEnabled: true; onClicked: art.picked(rb.cid) }
    }
    // бампер, курок: скруглённый прямоугольник, повёрнут вдоль плеча
    component Shoulder: Rectangle {
        id: sh
        property string cid
        property real cx
        property real cy
        readonly property bool hovered: shArea.containsMouse
        x: art.tx + cx - width / 2
        y: cy - height / 2
        antialiasing: true
        color: art.fill(cid, hovered)
        border.width: 1.5
        border.color: art.stroke(cid, hovered)
        Caps {
            anchors.centerIn: parent
            text: Pad.CTL[sh.cid].n
            font.pixelSize: 13
            font.weight: Font.Medium
            color: art.ink(sh.cid)
        }
        MouseArea { id: shArea; anchors.fill: parent; hoverEnabled: true; onClicked: art.picked(sh.cid) }
    }
    // стик: колодец, шляпка, ободок шляпки
    component Stick: Item {
        id: st
        property string cid
        property real cx
        property real cy
        readonly property bool hovered: stArea.containsMouse
        x: art.tx + cx - 36
        y: cy - 36
        width: 72
        height: 72
        Rectangle {
            anchors.fill: parent
            radius: 36
            antialiasing: true
            color: "#121310"
            border.width: 1.5
            border.color: art.stroke(st.cid, st.hovered)
        }
        Rectangle {
            x: 12; y: 12; width: 48; height: 48
            radius: 24
            antialiasing: true
            color: art.fill(st.cid, st.hovered)
            border.width: 1.5
            border.color: art.stroke(st.cid, st.hovered)
        }
        Rectangle {
            x: 22; y: 22; width: 28; height: 28
            radius: 14
            antialiasing: true
            color: "transparent"
            border.width: 1.5
            border.color: art.lit(st.cid) ? DeskStyle.amber : DeskStyle.muted
            opacity: 0.35
        }
        MouseArea { id: stArea; anchors.fill: parent; hoverEnabled: true; onClicked: art.picked(st.cid) }
    }
    // фигура по пути SVG (в поле корпуса, если at не задан)
    component Path: Shape {
        property alias d: svg.path
        property alias fillColor: sp.fillColor
        property alias strokeColor: sp.strokeColor
        property alias strokeWidth: sp.strokeWidth
        property alias strokeStyle: sp.strokeStyle
        property alias dashPattern: sp.dashPattern
        x: art.tx
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            id: sp
            fillColor: "transparent"
            strokeWidth: 1.5
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { id: svg }
        }
    }

    // ---------- корпус ----------
    Path {
        d: Pad.body()
        fillColor: "#121310"
        strokeColor: Qt.rgba(DeskStyle.muted.r, DeskStyle.muted.g, DeskStyle.muted.b, 0.7)
        strokeWidth: 1.6
    }

    // ---------- выноски: пунктир к кнопке, у выбранной — сплошная янтарём ----------
    readonly property var leads: Pad.LEFT.map(it => ({ id: it.id, pts: [Qt.point(80, it.y), Qt.point(Math.max(84, tx + it.to[0] - 26), it.y),
                                                                        Qt.point(tx + it.to[0], it.to[1])] }))
        .concat(Pad.RIGHT.map(it => ({ id: it.id, pts: [Qt.point(860, it.y), Qt.point(tx + (it.ex || it.to[0] + 26), it.y),
                                                        Qt.point(tx + it.to[0], it.to[1])] })))
    Shape {
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Qt.rgba(DeskStyle.muted.r, DeskStyle.muted.g, DeskStyle.muted.b, 0.42)
            strokeWidth: 1
            fillColor: "transparent"
            strokeStyle: ShapePath.DashLine
            dashPattern: [3, 3]
            PathMultiline { paths: art.leads.filter(l => !art.lit(l.id)).map(l => l.pts) }
        }
        ShapePath {
            strokeColor: DeskStyle.amber
            strokeWidth: 1
            fillColor: "transparent"
            PathMultiline { paths: art.leads.filter(l => art.lit(l.id)).map(l => l.pts) }
        }
    }

    // ---------- плечи: курки L2 R2 (дальше) и бамперы L1 R1 ----------
    Shoulder { cid: "l2"; cx: 159; cy: 34; width: 74; height: 24; radius: 9; rotation: -9 }
    Shoulder { cid: "r2"; cx: 441; cy: 34; width: 74; height: 24; radius: 9; rotation: 9 }
    Shoulder { cid: "l1"; cx: 161; cy: 64; width: 128; height: 18; radius: 8; rotation: -9 }
    Shoulder { cid: "r1"; cx: 439; cy: 64; width: 128; height: 18; radius: 8; rotation: 9 }

    // ---------- лепестки сзади: пунктиром ----------
    Repeater {
        model: [{ id: "p2", d: Pad.PADDLE, cx: 106 }, { id: "p1", d: Pad.mirror(Pad.PADDLE), cx: 494 }]
        delegate: Item {
            id: pd
            required property var modelData
            readonly property bool hovered: pdArea.containsMouse
            Path {
                d: pd.modelData.d
                fillColor: art.fill(pd.modelData.id, pd.hovered)
                strokeColor: art.stroke(pd.modelData.id, pd.hovered)
                strokeStyle: ShapePath.DashLine
                dashPattern: [2.6, 2]
            }
            Caps {
                x: art.tx + pd.modelData.cx - 20; y: 310; width: 40; height: 16
                text: Pad.CTL[pd.modelData.id].s
                font.pixelSize: 10
                color: art.lit(pd.modelData.id) ? DeskStyle.amber : DeskStyle.dim
            }
            MouseArea {
                id: pdArea
                x: art.tx + pd.modelData.cx - 18; y: 288; width: 36; height: 60
                hoverEnabled: true
                onClicked: art.picked(pd.modelData.id)
            }
        }
    }

    // ---------- стики ----------
    Stick { cid: "ls"; cx: 182; cy: 170 }
    Stick { cid: "rs"; cx: 362; cy: 236 }

    // ---------- крестовина: контур, четыре плеча — каждое своя кнопка ----------
    Path {
        d: "M228 206 h20 v20 h20 v20 h-20 v20 h-20 v-20 h-20 v-20 h20 Z"
        fillColor: art.keyFill
        strokeColor: art.lit("dp") ? DeskStyle.amberA(0.55) : DeskStyle.muted
    }
    Repeater {
        model: [{ id: "du", x: 228, y: 206, w: 20, h: 21, tri: "M238 212 l-4 6 h8 Z" },
                { id: "dd", x: 228, y: 245, w: 20, h: 21, tri: "M238 260 l-4 -6 h8 Z" },
                { id: "dl", x: 208, y: 226, w: 21, h: 20, tri: "M214 236 l6 -4 v8 Z" },
                { id: "dr", x: 247, y: 226, w: 21, h: 20, tri: "M262 236 l-6 -4 v8 Z" }]
        delegate: Item {
            id: arm
            required property var modelData
            readonly property bool hovered: armArea.containsMouse
            readonly property bool on: art.lit(modelData.id)
            Rectangle {
                visible: arm.on || arm.hovered
                x: art.tx + arm.modelData.x; y: arm.modelData.y
                width: arm.modelData.w; height: arm.modelData.h
                radius: 2
                color: arm.on ? DeskStyle.amberA(0.22) : DeskStyle.amberA(0.07)
                border.width: arm.on ? 1.5 : 0
                border.color: DeskStyle.amber
            }
            Path {
                d: arm.modelData.tri
                strokeColor: "transparent"
                fillColor: arm.on ? DeskStyle.amber : arm.hovered ? DeskStyle.text : DeskStyle.dim
            }
            MouseArea {
                id: armArea
                x: art.tx + arm.modelData.x; y: arm.modelData.y
                width: arm.modelData.w; height: arm.modelData.h
                hoverEnabled: true
                onClicked: art.picked(arm.modelData.id)
            }
        }
    }

    // ---------- A B X Y — как у Nintendo: X сверху, A справа, B снизу, Y слева ----------
    Round { cid: "x"; cx: 420; cy: 136; r: 16; label: "X" }
    Round { cid: "a"; cx: 454; cy: 170; r: 16; label: "A" }
    Round { cid: "b"; cx: 420; cy: 204; r: 16; label: "B" }
    Round { cid: "y"; cx: 386; cy: 170; r: 16; label: "Y" }

    // ---------- середина: Home с кольцом, − ★ +, кнопка профиля, огоньки профиля ----------
    Rectangle {
        x: art.tx + 282; y: 94; width: 36; height: 36
        radius: 18
        antialiasing: true
        color: "transparent"
        border.width: 1.2
        border.color: Qt.rgba(DeskStyle.dim.r, DeskStyle.dim.g, DeskStyle.dim.b, 0.55)
    }
    Round { cid: "home"; cx: 300; cy: 112; r: 13 }
    Round { cid: "minus"; cx: 262; cy: 148; r: 10 }
    Round { cid: "star"; cx: 300; cy: 148; r: 10 }
    Round { cid: "plus"; cx: 338; cy: 148; r: 10 }
    Repeater {   // значки кнопок середины
        model: [{ id: "home", d: "M294 112 l6 -6 l6 6 M296 110 v7 h8 v-7", w: 1.3 }, { id: "minus", d: "M257 148 h10", w: 1.5 },
                { id: "star", d: Pad.star(300, 148.5, 6), w: 1.2 }, { id: "plus", d: "M333 148 h10 M338 143 v10", w: 1.5 }]
        delegate: Path {
            required property var modelData
            d: modelData.d
            strokeWidth: modelData.w
            strokeColor: art.lit(modelData.id) ? DeskStyle.amber : DeskStyle.muted
        }
    }
    Rectangle {   // профиль — не наш
        x: art.tx + 287; y: 176; width: 26; height: 13
        radius: 6.5
        antialiasing: true
        color: "#121310"
        border.width: 1.5
        border.color: Qt.rgba(125 / 255, 120 / 255, 110 / 255, 0.55)
    }
    Repeater {
        model: [204, 213, 222]
        delegate: Rectangle {
            required property int modelData
            x: art.tx + 298; y: modelData - 2; width: 4; height: 4
            radius: 2
            color: DeskStyle.dim
        }
    }

    // ---------- плашки выносок: имя кнопки, под ним — клавиша ----------
    component Chip: Item {
        id: ch
        property string cid
        property bool atRight: false
        readonly property bool on: art.lit(cid)
        readonly property string value: art.chipVal(cid)
        Text {
            x: ch.atRight ? 76 - implicitWidth : 0
            y: -26
            height: 12
            verticalAlignment: Text.AlignVCenter
            text: Pad.LNAME[ch.cid]
            font.family: DeskStyle.mono
            font.pixelSize: 10
            font.letterSpacing: 10 * 0.14
            color: ch.on ? DeskStyle.amber : DeskStyle.dim
        }
        Rectangle {
            y: -12
            width: 76
            height: 24
            radius: 2
            color: ch.on ? DeskStyle.amberA(0.12) : chArea.containsMouse ? DeskStyle.amberA(0.07) : DeskStyle.surface
            border.width: 1
            border.color: ch.on ? DeskStyle.amber : chArea.containsMouse ? DeskStyle.amberA(0.36) : DeskStyle.amberA(DeskStyle.line)
            Caps {
                anchors.fill: parent
                text: ch.value
                font.pixelSize: ch.value.length > 7 ? 10 : 12
                font.weight: ch.value === "—" ? Font.Normal : Font.DemiBold
                color: ch.on ? DeskStyle.amber : ch.value === "—" ? DeskStyle.dim : DeskStyle.text
            }
            MouseArea { id: chArea; anchors.fill: parent; hoverEnabled: true; onClicked: art.picked(ch.cid) }
        }
    }
    Repeater {
        model: Pad.LEFT
        delegate: Chip { required property var modelData; cid: modelData.id; x: 0; y: modelData.y }
    }
    Repeater {
        model: Pad.RIGHT
        delegate: Chip { required property var modelData; cid: modelData.id; atRight: true; x: 864; y: modelData.y }
    }
    Repeater {   // середина: значок кнопки и клавиша
        model: [{ id: "minus", d: "M-5 0 h10", w: 1.4 }, { id: "star", d: Pad.star(0, 0.5, 5.2), w: 1.2 },
                { id: "home", d: "M-5 1 l5 -5 l5 5 M-3.4 -0.6 v6 h6.8 v-6", w: 1.3 }, { id: "plus", d: "M-4.5 0 h9 M0 -4.5 v9", w: 1.4 }]
        delegate: Rectangle {
            id: mc
            required property var modelData
            required property int index
            readonly property bool on: art.lit(modelData.id)
            readonly property string value: art.val(modelData.id) || "—"
            x: art.tx + 196 + index * 53
            y: 353
            width: 48
            height: 26
            radius: 2
            color: on ? DeskStyle.amberA(0.12) : mcArea.containsMouse ? DeskStyle.amberA(0.07) : DeskStyle.surface
            border.width: 1
            border.color: on ? DeskStyle.amber : mcArea.containsMouse ? DeskStyle.amberA(0.36) : DeskStyle.amberA(DeskStyle.line)
            Shape {
                x: 12; y: 13
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: mc.on ? DeskStyle.amber : DeskStyle.muted
                    strokeWidth: mc.modelData.w
                    fillColor: "transparent"
                    capStyle: ShapePath.RoundCap
                    joinStyle: ShapePath.RoundJoin
                    PathSvg { path: mc.modelData.d }
                }
            }
            Caps {
                x: 22; width: 24; height: parent.height
                text: mc.value
                font.pixelSize: mc.value.length > 3 ? 9 : 11
                font.weight: mc.value === "—" ? Font.Normal : Font.DemiBold
                color: mc.on ? DeskStyle.amber : mc.value === "—" ? DeskStyle.dim : DeskStyle.text
            }
            MouseArea { id: mcArea; anchors.fill: parent; hoverEnabled: true; onClicked: art.picked(mc.modelData.id) }
        }
    }
}
