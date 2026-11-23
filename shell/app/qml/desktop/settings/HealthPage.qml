// «Настройки» → «Батарея» → «Здоровье» (к варианту 08, выбор пользователя 21 ноября 2026): кольцо — сколько ёмкости
// осталось от паспортной, рядом ёмкость, циклы, сопротивление, напряжение покоя и счётчики тепла самой батареи (сколько
// она провела тёплой и почти полной — путь к вздутию; хранятся в её памяти и только растут), когда последний раз была
// 100 % и когда калибровка; ниже — неделя заряда и мощности (тот же график, что в окне батареи на панели). Данные —
// naomid: Charge.Health() и Battery.History(), при входе в раздел и раз в минуту, пока он виден.
import QtQuick
import QtQuick.Shapes
import ".."

Item {
    id: page
    property var sw
    property var health: ({})
    property var history: null
    readonly property bool ready: health.fullMah !== undefined
    readonly property int capPct: ready && health.designMah > 0 ? Math.round(health.fullMah / health.designMah * 100) : 0
    // последний 100 %: что видела служба заряда, а нет — по истории UPower (служба могла начать позже)
    readonly property real lastFull: health.lastFull > 0 ? health.lastFull : history && history.lastFull > 0 ? history.lastFull : 0
    implicitHeight: col.height

    function load() {
        naomiPhone.charge.call("Health", [], h => page.health = h)
        naomiPhone.battery.call("History", [7 * 86400, 168], h => page.history = h)
    }
    Timer {
        interval: 60000
        running: page.visible
        repeat: true
        triggeredOnStart: true
        onTriggered: page.load()
    }

    readonly property var months: ["января", "февраля", "марта", "апреля", "мая", "июня", "июля", "августа",
                                   "сентября", "октября", "ноября", "декабря"]
    function day(t) { const d = new Date(t * 1000); return d.getDate() + " " + months[d.getMonth()] }
    function grp(v) { return String(Math.round(v)).replace(/\B(?=(\d{3})+(?!\d))/g, " ") }
    // секунды → «93 сут», «6 ч», «40 мин»
    function span(s) {
        return s >= 2 * 86400 ? Math.round(s / 86400) + " сут" : s >= 3600 ? Math.round(s / 3600) + " ч"
             : s > 0 ? Math.round(s / 60) + " мин" : "0"
    }
    function swell(t) {
        const r = (health.swelling || []).find(x => x.tempC === t)
        return r ? span(r.chargeS + r.dischargeS) : "нет данных"
    }

    component Label: Text {
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 12 * 0.12
        color: DeskStyle.muted
    }
    component Meta: Text {
        wrapMode: Text.Wrap
        lineHeight: 1.3
        font.family: DeskStyle.mono
        font.pixelSize: 12
        color: DeskStyle.dim
    }
    component Line: Rectangle { height: 1; color: DeskStyle.amberA(DeskStyle.line) }

    Column {
        id: col
        width: page.width
        spacing: 20

        Item {
            width: col.width
            height: 30
            BackButton { id: back; text: "Батарея"; onClicked: page.sw.page = "battery" }
            Head {
                x: back.width + 14; y: (30 - height) / 2
                width: col.width - x
                title: "ЗДОРОВЬЕ"
                info: page.ready ? page.capPct + " % ёмкости · " + page.health.cycles + " циклов" : ""
            }
        }
        Row {
            spacing: 26
            Column {
                width: 180
                spacing: 12
                Item {   // кольцо ёмкости
                    width: 150; height: 150
                    anchors.horizontalCenter: parent.horizontalCenter
                    Shape {
                        anchors.fill: parent
                        preferredRendererType: Shape.CurveRenderer
                        ShapePath {
                            strokeColor: Qt.rgba(1, 1, 1, 0.06)
                            strokeWidth: 9
                            fillColor: "transparent"
                            PathAngleArc { centerX: 75; centerY: 75; radiusX: 64; radiusY: 64; startAngle: 0; sweepAngle: 360 }
                        }
                        ShapePath {
                            strokeColor: DeskStyle.amber
                            strokeWidth: 9
                            fillColor: "transparent"
                            capStyle: ShapePath.FlatCap
                            PathAngleArc { centerX: 75; centerY: 75; radiusX: 64; radiusY: 64; startAngle: -90; sweepAngle: 3.6 * page.capPct }
                        }
                    }
                    Column {
                        anchors.centerIn: parent
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: page.ready ? page.capPct + "%" : "…"
                            font.family: DeskStyle.sans
                            font.pixelSize: 38
                            font.weight: Font.ExtraLight
                            color: DeskStyle.text
                        }
                        Label { anchors.horizontalCenter: parent.horizontalCenter; text: "ЁМКОСТИ"; font.pixelSize: 11 }
                    }
                }
                Meta {
                    width: 180
                    horizontalAlignment: Text.AlignHCenter
                    text: page.ready ? page.grp(page.health.fullMah) + " из " + page.grp(page.health.designMah) + " мА·ч по паспорту" : ""
                }
            }
            Column {
                id: kv
                width: col.width - 206
                Repeater {
                    model: page.ready ? [
                        ["Ёмкость", page.grp(page.health.fullMah) + " мА·ч"],
                        ["Циклы", String(page.health.cycles)],
                        ["Сопротивление", page.health.resistanceMohm + " мОм"],
                        ["Напряжение покоя", (page.health.ocvMv / 1000).toFixed(2).replace(".", ",") + " В"],
                        ["Теплее 30 °C, 90 %+", page.swell(30)],
                        ["Теплее 35 °C, 90 %+", page.swell(35)],
                        ["Теплее 40 °C, 95 %+", page.swell(40)],
                        ["Последний 100 %", page.lastFull > 0 ? page.day(page.lastFull) : "не видела"],
                        ["Калибровка", page.health.nextCalibration > 0 ? page.day(page.health.nextCalibration) : "не нужна"]] : []
                    delegate: Item {
                        required property var modelData
                        required property int index
                        width: kv.width
                        height: 28
                        Line { visible: index > 0; width: parent.width }
                        Label { height: parent.height; verticalAlignment: Text.AlignVCenter; text: modelData[0].toUpperCase() }
                        Text {
                            anchors.right: parent.right
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            text: modelData[1]
                            font.family: DeskStyle.mono
                            font.pixelSize: 13
                            color: DeskStyle.text
                        }
                    }
                }
            }
        }
        Column {
            width: col.width
            spacing: 12
            SubPlate { width: col.width; text: "Неделя" }
            Row {
                spacing: 26
                BatteryChart { history: page.history }
                Meta {
                    width: col.width - 316 - 26
                    text: "заряд и мощность за неделю — как в окне батареи на панели. «Теплее 35 °C, 90 %+» — счётчик самой батареи "
                          + "в её памяти: сколько она провела тёплой и почти полной, на зарядке и без; это и есть путь к вздутию. Только растёт"
                          + (page.health.since > 0 ? ". Эту батарею служба видит с " + page.day(page.health.since) : "")
                }
            }
        }
    }
}
