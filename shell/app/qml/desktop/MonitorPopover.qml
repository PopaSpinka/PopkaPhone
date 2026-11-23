// Монитор системы — окно под индикатором подключения на панели (docs/DESKTOP.md, «Монитор системы»). Вид выбран
// пользователем из десяти макетов — «Стопка графиков»: шесть величин друг под другом на одной шкале времени (минута,
// шаг — секунда); пропуск кадра — красная черта через все графики сразу: видно, что в этот миг было с процессором,
// видеочипом, диском и Android. Модуль 348 × 332, как окна звука и Bluetooth; висит поверх всех окон, пока по
// индикатору не щёлкнут ещё раз (щелчок мимо его не закрывает); его можно оттащить мышью куда угодно, двойной щелчок
// возвращает под индикатор. Данные — naomiMonitor (system_monitor.h): раз в секунду и только пока окно открыто.
// В шильдике — две кнопки: «Монитор» и «Экраны» (view). «Экраны» (docs/SCREENS.md) — на какие экраны выведен стол и
// какой главный; вид выбран пользователем из десяти макетов — С7 «Три положения» (21 ноября 2026): у каждого экрана
// строка с переключателем НЕТ / СТОЛ / ГЛАВНЫЙ, ниже — выключатель стола. Та же логика, что в «Настройках» телефона.
// Под строкой планшета — яркость его экрана: шкала и «АВТО» (просьба пользователя, 21 ноября 2026).
import QtQuick
import Naomi.Desk

Item {
    id: pop
    property bool open: false
    property string route: ""          // путь трансляции: cable | wifi
    property real mbps: 0              // поток зрителю, Мбит/с
    property string androidState: ""   // off | starting | on | stopping
    property bool androidAsleep: false
    property bool androidFrozen: false
    property int androidWindows: 0

    property string view: "monitor"    // что в окне: monitor — графики, screens — экраны стола
    readonly property var screens: naomiStream.screens
    readonly property var mainNow: screens.find(s => s.mainNow && s.state !== "offline")

    width: 348
    height: view === "screens" ? screensView.y + screensView.height + 14 : 332

    // ---------- появление — как у окна звука (anim_curve): t — ход открытия или закрытия, 0…1 ----------
    property real openT: 1
    property real closeT: 1
    onOpenChanged: {
        if (open) {
            closeAnim.stop()
            closeT = 1
            openT = 0
            openAnim.restart()
        } else {
            openAnim.stop()
            closeT = 0
            closeAnim.restart()
        }
    }
    NumberAnimation { id: openAnim; target: pop; property: "openT"; from: 0; to: 1; duration: 110 }
    NumberAnimation { id: closeAnim; target: pop; property: "closeT"; from: 0; to: 1; duration: 150 }
    visible: open || closeAnim.running
    transformOrigin: Item.Top
    scale: open ? 0.98 + 0.02 * (1 - Math.pow(1 - openT, 3)) : 1 - 0.01 * (1 - Math.pow(1 - closeT, 3))
    opacity: open ? 1 : (Math.exp(-closeT * 150 / 38) - Math.exp(-150 / 38)) / (1 - Math.exp(-150 / 38))

    // Окно тащат за любое место (в нём нет кнопок): movedTo — куда встать левому верху (точки area, стол держит его в
    // своих границах); двойной щелчок — home: обратно под индикатор. Нажатия и колесо сквозь окно не проходят
    property Item area                 // стол: в его точках считается место окна
    signal movedTo(real x, real y)
    signal home()
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        cursorShape: pressed && pressedButtons & Qt.LeftButton ? Qt.ClosedHandCursor : Qt.ArrowCursor
        property point grab            // от левого верха окна до мыши при нажатии
        property bool dragging: false
        onPressed: (m) => {
            dragging = m.button === Qt.LeftButton && pop.area !== null
            if (!dragging)
                return
            const p = mapToItem(pop.area, m.x, m.y), o = pop.mapToItem(pop.area, 0, 0)
            grab = Qt.point(p.x - o.x, p.y - o.y)
        }
        onPositionChanged: (m) => {
            if (!dragging)
                return
            const p = mapToItem(pop.area, m.x, m.y)
            pop.movedTo(p.x - grab.x, p.y - grab.y)
        }
        onReleased: dragging = false
        onCanceled: dragging = false
        onDoubleClicked: (m) => { if (m.button === Qt.LeftButton) pop.home() }
        onWheel: (e) => {}
    }

    DeskModule { anchors.fill: parent }

    // ---------- числа ----------
    readonly property var now: naomiMonitor.now
    readonly property bool ready: now.fps !== undefined
    readonly property string dimC: "" + DeskStyle.dim
    function n0(v) { return v === undefined ? "–" : Math.round(v).toString() }
    function n1(v) { return v === undefined ? "–" : v.toFixed(1).replace(".", ",") }
    function u(s) { return ' <font color="' + dimC + '">' + s + '</font>' }    // единица — через пробел
    function deg(s) { return '<font color="' + dimC + '">°' + s + '</font>' }  // градусы — вплотную к числу
    function tint(s, c) { return '<font color="' + c + '">' + s + '</font>' }
    readonly property int missed: ready ? now.missed : 0
    readonly property string tempC: !ready ? "" + DeskStyle.text
                                    : now.hot ? "" + DeskStyle.bad : now.skin > 34 ? "" + DeskStyle.amber : "" + DeskStyle.text
    readonly property string androidText: {
        if (androidState !== "on")
            return tint(androidState === "starting" ? "включается" : "выключен", "" + DeskStyle.muted)
        if (androidFrozen)
            return tint("заморожен", "" + DeskStyle.muted)
        if (androidAsleep)
            return tint("спит", "" + DeskStyle.muted)
        const w = androidWindows, m = w % 10, h = w % 100
        const word = m === 1 && h !== 11 ? "окно" : m >= 2 && m <= 4 && (h < 12 || h > 14) ? "окна" : "окон"
        return w + u(word + " ·") + " " + n0(now.android) + u("% ·") + " " + n1(now.androidGb) + u("ГБ")
    }

    Plate {
        x: 16; y: 14
        width: 316
        num: "04"
        title: "СИСТЕМА"
        reserve: 142
    }
    NButton {   // переключатель вида — в шильдике
        x: 190; y: 11
        width: 72; height: 24
        fontSize: 12
        letterSpacing: 12 * 0.08
        centered: true
        text: "Монитор"
        tone: pop.view === "monitor" ? "on" : "off"
        onClicked: pop.view = "monitor"
    }
    NButton {
        x: 266; y: 11
        width: 66; height: 24
        fontSize: 12
        letterSpacing: 12 * 0.08
        centered: true
        text: "Экраны"
        tone: pop.view === "screens" ? "on" : "off"
        onClicked: pop.view = "screens"
    }

    // ---------- шесть дорожек ----------
    component Lane: Item {
        id: lane
        property string cap
        property string capNote      // вторая подпись за первой, своим цветом (пропуски кадров)
        property color capTone: DeskStyle.dim
        property string val
        property int kind       // SystemMonitor::Lane: 0 кадры, 1 процессор, 2 видеочип, 3 диск, 4 тепло, 5 Android
        property real hi: 100   // шкала графика: верх и низ
        property real lo: 0
        property real limit: -1
        property color tone: DeskStyle.teal
        property int marks: 1
        property bool first: false
        width: 316; height: 40
        Rectangle { visible: !lane.first; width: parent.width; height: 1; color: DeskStyle.amberA(DeskStyle.line) }
        Text {
            id: capText
            y: 6
            text: lane.cap
            color: DeskStyle.dim
            font.family: DeskStyle.mono
            font.pixelSize: 12
            font.letterSpacing: 1.2
            font.capitalization: Font.AllUppercase
        }
        Text {
            visible: lane.capNote !== ""
            x: capText.implicitWidth + 8
            y: 6
            text: lane.capNote
            color: lane.capTone
            font.family: DeskStyle.mono
            font.pixelSize: 12
            font.letterSpacing: 1.2
            font.capitalization: Font.AllUppercase
        }
        Text {
            anchors.right: parent.right
            y: 4
            textFormat: Text.StyledText
            text: lane.val
            color: DeskStyle.text
            font.family: DeskStyle.mono
            font.pixelSize: 13
        }
        MonitorGraph {
            y: 20
            width: parent.width; height: 20
            monitor: naomiMonitor
            lane: lane.kind
            max: lane.hi; min: lane.lo
            limit: lane.limit
            color: lane.tone
            marks: lane.marks
        }
    }
    Column {
        x: 16; y: 44
        visible: pop.view === "monitor"
        Lane {
            first: true
            cap: "Кадры"
            capNote: !pop.ready ? "" : pop.missed > 0 ? "· пропусков " + pop.missed : "· без пропусков"
            capTone: pop.missed > 0 ? DeskStyle.bad : DeskStyle.good
            val: pop.n0(pop.now.fps) + pop.u("к/с ·") + " " + pop.n1(pop.now.frameMs) + pop.u("мс")
            kind: 0
            hi: 30
            limit: pop.ready && pop.now.hz > 0 ? 1000 / pop.now.hz : -1   // кадр дольше — не успевает к обновлению монитора
            tone: DeskStyle.amber
            marks: 2
        }
        Lane {
            cap: "Процессор"
            val: pop.n0(pop.now.cpu) + pop.u("% · гл.") + " " + pop.n0(pop.now.prime)
            kind: 1
        }
        Lane {
            cap: "Видеочип"
            val: pop.n0(pop.now.gpu) + pop.u("% ·") + " " + pop.n0(pop.now.gpuMhz) + pop.u("МГц")
            kind: 2
        }
        Lane {
            cap: "Память · диск"
            val: pop.n1(pop.now.memGb) + pop.u("ГБ ·") + " " + pop.n0(pop.now.diskMb) + pop.u("МБ/с")
            kind: 3
            hi: 130
        }
        Lane {
            cap: "Тепло"
            val: pop.tint(pop.n1(pop.now.skin), pop.tempC) + pop.deg(" корпус ·") + " " + pop.n0(pop.now.chip) + pop.deg(" чип")
            kind: 4
            hi: 40; lo: 28
            tone: DeskStyle.amber
        }
        Lane {
            cap: "Android"
            val: pop.androidText
            kind: 5
            hi: 260
        }
    }

    // ---------- подвал: кадр и путь ----------
    Rectangle { visible: pop.view === "monitor"; x: 16; y: 294; width: 316; height: 1; color: DeskStyle.amberA(DeskStyle.line) }
    component Foot: Text {
        visible: pop.view === "monitor"
        y: 303
        color: DeskStyle.dim
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 0.7
    }
    Foot {
        x: 16
        text: pop.ready ? pop.now.width + "×" + pop.now.height + " · " + pop.now.hz + " Гц" : ""
    }
    Foot {
        x: 332 - implicitWidth
        text: (pop.route === "wifi" ? "Wi-Fi" : pop.route === "cable" ? "кабель" : "") + " · " + (pop.mbps < 10 ? pop.n1(pop.mbps) : pop.n0(pop.mbps)) + " Мбит/с"
    }

    // ---------- экраны (docs/SCREENS.md): строка на экран — грань по состоянию (бирюза — показывает стол, серая —
    // заставка, красная — нет связи), имя, три положения; ниже — выключатель стола ----------
    Column {
        id: screensView
        x: 16; y: 44
        width: 316
        visible: pop.view === "screens"
        // Строки не пересоздаются, когда список экранов меняется (модель — их число, строка читает свой экран по
        // номеру): иначе ползунок яркости пропадал бы из-под мыши на каждом шаге
        Repeater {
            model: pop.view === "screens" ? pop.screens.length : 0
            delegate: Item {
                id: row
                required property int index
                readonly property var scr: pop.screens[index] || ({})
                readonly property bool offline: scr.state === "offline"
                // не на связи, но включится сам, как только его поставят на «стол» или «главный» (телевизор)
                readonly property bool standby: scr.state === "standby"
                readonly property bool light: scr.light === true
                width: 316; height: 40 + (light ? 34 : 0)
                Rectangle {
                    y: 9
                    width: 3; height: 22
                    color: row.scr.state === "desk" ? DeskStyle.teal : row.offline ? DeskStyle.bad : DeskStyle.dim
                }
                Text {
                    x: 14
                    width: seg.x - 22
                    height: 40
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    textFormat: Text.StyledText
                    text: (row.scr.title || "") + (row.standby ? " <font color=\"" + DeskStyle.muted + "\">· спит</font>" : "")
                    color: row.offline ? DeskStyle.muted : DeskStyle.text
                    font.family: DeskStyle.sans
                    font.pixelSize: 15
                }
                Rectangle {
                    id: seg
                    x: parent.width - width
                    y: 7
                    width: 172; height: 26
                    color: "transparent"
                    border.width: 1
                    border.color: DeskStyle.amberA(DeskStyle.line)
                    opacity: row.offline ? 0.45 : 1
                    Repeater {
                        model: [{ t: "НЕТ", x: 0, w: 44 }, { t: "СТОЛ", x: 44, w: 52 }, { t: "ГЛАВНЫЙ", x: 96, w: 76 }]
                        delegate: Item {
                            id: cell
                            required property int index
                            required property var modelData
                            readonly property bool sel: row.scr.position === index
                            x: modelData.x
                            width: modelData.w; height: 26
                            Rectangle {
                                x: 1; y: 1
                                width: parent.width - 2; height: parent.height - 2
                                color: cell.sel ? DeskStyle.amberA(0.09) : hit.containsMouse ? DeskStyle.amberA(DeskStyle.itemHover) : "transparent"
                            }
                            Rectangle { visible: cell.sel; x: 1; y: 23; width: parent.width - 2; height: 2; color: DeskStyle.amber }
                            Rectangle { visible: cell.index > 0; width: 1; height: parent.height; color: DeskStyle.amberA(DeskStyle.line) }
                            Text {
                                anchors.centerIn: parent
                                text: cell.modelData.t
                                color: cell.sel ? DeskStyle.amber : hit.containsMouse ? DeskStyle.text : DeskStyle.dim
                                font.family: DeskStyle.mono
                                font.pixelSize: 12
                                font.letterSpacing: 0.96
                            }
                            MouseArea {
                                id: hit
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: naomiStream.setScreenPosition(row.scr.id, cell.index)
                            }
                        }
                    }
                }
                // Яркость экрана — у тех, где её можно ставить со стола (планшет): шкала, как громкость в окне звука
                // (нажать и тянуть), и «АВТО» — экран ведёт яркость сам по своему датчику света; тронули шкалу — «АВТО»
                // снимается. При «АВТО» шкала тусклая: показывает, что стояло
                Item {
                    visible: row.light
                    y: 40
                    width: 316; height: 30
                    DeskMeter {
                        x: 14; y: 12
                        width: 208
                        value: (row.scr.lightLevel || 0) / 100
                        fill: row.scr.lightAuto ? DeskStyle.dim : DeskStyle.amber
                        fillAlpha: row.scr.lightAuto ? 0.5 : 0.75
                        tick: row.scr.lightAuto ? DeskStyle.muted : DeskStyle.amber
                        tall: lightArea.containsMouse || lightArea.pressed
                    }
                    MouseArea {   // шире шкалы на 10 и выше на 12 — по ней легко попасть
                        id: lightArea
                        x: 14 - 10; y: 0
                        width: 208 + 20; height: 30
                        hoverEnabled: true
                        preventStealing: true
                        function apply(mx) {
                            const v = Math.max(1, Math.round(Math.max(0, Math.min(1, (mx + x - 14) / 208)) * 100))
                            if (v !== row.scr.lightLevel || row.scr.lightAuto)
                                naomiStream.setScreenLight(row.scr.id, false, v)
                        }
                        onPressed: (e) => apply(e.x)
                        onPositionChanged: (e) => { if (pressed) apply(e.x) }
                        onWheel: (e) => {   // колесо над шкалой — ±5 за щелчок
                            const step = (e.pixelDelta.y !== 0 ? e.pixelDelta.y : e.angleDelta.y / 120 * 100) > 0 ? 5 : -5
                            naomiStream.setScreenLight(row.scr.id, false, Math.max(1, Math.min(100, (row.scr.lightLevel || 60) + step)))
                        }
                    }
                    NButton {
                        x: 240; y: 3
                        width: 76; height: 24
                        fontSize: 12
                        letterSpacing: 12 * 0.08
                        centered: true
                        text: "Авто"
                        tone: row.scr.lightAuto ? "on" : "off"
                        onClicked: naomiStream.setScreenLight(row.scr.id, !row.scr.lightAuto, row.scr.lightLevel || 60)
                    }
                }
                Rectangle { y: parent.height - 1; width: parent.width; height: 1; color: DeskStyle.amberA(DeskStyle.line) }
            }
        }
        Item { width: 1; height: 10 }
        NButton {   // выключатель стола — как «закрыть сессию»: стол уходит со всех экранов, на них заставка
            width: 316
            text: "Рабочий стол"
            tone: naomiStream.enabled ? "on" : "off"
            onClicked: naomiStream.enabled = !naomiStream.enabled
            Text {
                x: parent.width - implicitWidth - 12
                y: parent.dy
                height: parent.height
                verticalAlignment: Text.AlignVCenter
                text: naomiStream.enabled ? "ВКЛЮЧЁН" : "ВЫКЛЮЧЕН"
                color: parent.ink
                font.family: DeskStyle.mono
                font.pixelSize: 12
                font.letterSpacing: 0.96
            }
        }
        Item { width: 1; height: 10 }
        Text {
            width: 316
            wrapMode: Text.WordWrap
            lineHeight: 1.35
            color: DeskStyle.muted
            font.family: DeskStyle.mono
            font.pixelSize: 12
            font.letterSpacing: 0.5
            text: pop.mainNow ? "Главный задаёт форму стола, сейчас " + pop.mainNow.aspect + ". Остальные показывают его с полями."
                              : "Стол не показывает ни один экран."
        }
    }
}
