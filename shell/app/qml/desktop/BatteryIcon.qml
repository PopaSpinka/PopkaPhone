// Значок батареи на панели (docs/DESKTOP.md, «Батарея»; выбран пользователем из десяти макетов — «Часы работы»):
// корпус лёжа с заливкой по заряду и пупочкой справа, слева — сколько ещё проработает от батареи («6:40», прогноз
// DeskBattery), на зарядке — процент. Зарядка подключена — заливка бирюзой и молния поверх (с прорезью цвета панели),
// мало (20 % и меньше) — заливка и надпись красным. Ширина — по самому широкому («10:00»): смена надписи панель не
// двигает. Правый край — правый край элемента; середина по высоте — y.
import QtQuick
import QtQuick.Shapes

Item {
    id: icon
    property var battery                // DeskBattery
    property bool lit: false            // под мышью или окно открыто — светлее, как у соседних значков
    property font font                  // шрифт надписей панели

    readonly property color ink: lit ? DeskStyle.text : DeskStyle.muted
    readonly property color tone: battery.low ? DeskStyle.bad : battery.plugged ? DeskStyle.teal : ink
    readonly property string label: !battery.plugged && battery.clockText !== "" ? battery.clockText : battery.percent + "%"

    // корпус: 18 × 10 по середине линии (1,5), скругление 2,4; пупочка 1,5 × 4,5; заливка — внутри с зазором 1,5
    readonly property real bodyW: 19.5
    readonly property real gap: 5
    TextMetrics { id: widest; font: icon.font; text: "10:00" }
    TextMetrics { id: shown; font: icon.font; text: icon.label }
    width: Math.ceil(widest.tightBoundingRect.width) + gap + bodyW + 1.5
    height: 14

    Text {   // по высоте — как надписи панели (BarText): строкой во всю панель, по середине
        x: icon.width - 1.5 - icon.bodyW - icon.gap - shown.tightBoundingRect.width - shown.tightBoundingRect.x
        y: icon.height / 2 - DeskStyle.panelH / 2
        height: DeskStyle.panelH
        verticalAlignment: Text.AlignVCenter
        font: icon.font
        text: icon.label
        color: icon.battery.low ? DeskStyle.bad : icon.ink
    }
    Rectangle {   // корпус
        id: body
        x: icon.width - 1.5 - icon.bodyW
        anchors.verticalCenter: parent.verticalCenter
        width: icon.bodyW
        height: 11.5
        radius: 3.15
        color: "transparent"
        border.width: 1.5
        border.color: icon.ink
    }
    Rectangle {   // пупочка
        x: icon.width - 1.5
        anchors.verticalCenter: parent.verticalCenter
        width: 1.5
        height: 4.5
        radius: 0.6
        color: icon.ink
    }
    Rectangle {   // заливка по заряду: внутри 13,5 × 5,5
        x: body.x + 3
        anchors.verticalCenter: parent.verticalCenter
        width: icon.battery.percent > 0 ? Math.max(1.2, 13.5 * Math.min(100, icon.battery.percent) / 100) : 0
        height: 5.5
        radius: 1
        color: icon.tone
    }
    Shape {   // молния поверх корпуса: сначала прорезь цвета панели, потом сама молния
        id: bolt
        visible: icon.battery.plugged
        x: body.x + body.width / 2
        y: icon.height / 2
        preferredRendererType: Shape.CurveRenderer
        readonly property var pts: [[0.22, -1], [-0.62, 0.14], [-0.04, 0.14], [-0.22, 1], [0.62, -0.14], [0.04, -0.14], [0.22, -1]]
            .map(p => Qt.point(p[0] * 4.3, p[1] * 4.3))
        ShapePath {
            fillColor: DeskStyle.bg
            strokeColor: DeskStyle.bg
            strokeWidth: 1.6
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: bolt.pts }
        }
        ShapePath {
            fillColor: DeskStyle.text
            strokeColor: "transparent"
            PathPolyline { path: bolt.pts }
        }
    }
}
