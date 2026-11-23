// Миниатюра скриншота — перенос НаомиОС (bcdesk, «Миниатюра снимка», shot_thumb_*): снимок, вписанный в 320 × 200
// точек (мелкий — как есть; размер и картинку готовит naomiShot), в правом верхнем углу под панелью с отступом 14.
// Углы и волосяная рамка — как у окон (DeskStyle.outline, янтарь .16). Выезжает справа на 28 точек и проявляется за
// 180 мс, висит 6 с (мышь над ней — ещё по секунде), уходит тем же путём за 260 мс — появление, проигранное назад:
// вправо и тает. Над ней — рука, окнам под ней мышь не уходит; нажали и отпустили на ней — opened (снимок в окне
// «Просмотра»). Перед новым снимком прошлая убирается сразу (hideNow) — на него она не попадёт.
import QtQuick
import QtQuick.Shapes

Item {
    id: th
    property real restX       // где стоит, когда выехала (пиксель в пиксель — ставит стол)
    property string path
    property string image
    signal opened(string path)

    readonly property int showMs: 6000   // THUMB_SHOW_MS
    readonly property int inMs: 180      // THUMB_IN_MS
    readonly property int outMs: 260     // THUMB_OUT_MS
    readonly property int slide: 28      // THUMB_SLIDE

    function show(file, picture, size) {
        path = file
        th.image = picture
        width = size.width
        height = size.height
        shown = true
        dir = 1
        anim.duration = inMs
        anim.restart()
        hideTimer.interval = showMs
        hideTimer.restart()
    }
    function hideNow() {
        hideTimer.stop()
        anim.stop()
        shown = false
        dir = 0
        image = ""
    }

    property bool shown: false
    property int dir: 0       // +1 — выезжает, −1 — уходит
    property real t: 1        // ход анимации, 0…1
    // ease_out_cubic; уход — то же появление назад
    readonly property real k: { const u = dir < 0 ? 1 - t : t; return 1 - Math.pow(1 - u, 3) }
    NumberAnimation {
        id: anim
        target: th
        property: "t"
        from: 0
        to: 1
        onFinished: if (th.dir < 0) th.hideNow()
    }
    Timer {   // срок вышел: мышь над ней — подождать ещё секунду, нет — уходит
        id: hideTimer
        onTriggered: {
            if (mouse.containsMouse) {
                interval = 1000
                restart()
                return
            }
            th.dir = -1
            anim.duration = th.outMs
            anim.restart()
        }
    }

    visible: shown
    opacity: k
    x: restX + Math.round((1 - k) * slide)

    readonly property real e: DeskStyle.corner(width, height)
    Image {   // снимок — заливка контура (ShapePath.fillItem): углы как у окон, без маски и лишнего слоя
        id: pic
        visible: false
        width: th.width
        height: th.height
        source: th.image
        cache: false
        smooth: true
    }
    Shape {
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillItem: pic
            strokeColor: "transparent"
            PathPolyline { path: DeskStyle.outline(0, 0, th.width, th.height, th.e) }
        }
    }
    Shape {   // волосяная рамка, как у окон
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: "transparent"
            strokeColor: DeskStyle.amberA(DeskStyle.line)
            strokeWidth: 1
            PathPolyline { path: DeskStyle.outline(0.5, 0.5, th.width - 1, th.height - 1, th.e) }
        }
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        cursorShape: Qt.PointingHandCursor
        onClicked: (m) => {
            if (m.button !== Qt.LeftButton)
                return
            const p = th.path
            th.hideNow()
            th.opened(p)
        }
    }
}
