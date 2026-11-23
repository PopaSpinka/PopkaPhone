// Шторка быстрых настроек — отдельный чёрный экран поверх всего. Переход к ней и назад — Naomi.Ui Axis
// (перенос перехода «циферблат ↔ галерея» Попка Студио для 7-дюймовой платы, docs/SHELL.md «Переходы»):
// вертикальная ось, «старый» — всё под шторкой (главный, программа, блокировка: ShellView берёт contentDy и
// contentAlpha), «новый» — шторка; открыть — палец вниз, всё уезжает вниз, шторка приходит сверху; закрыть — в
// зеркале. Путь — 260 dp пальца, после пальца — 0,38 с на весь путь (доля по оставшемуся).
// Числа Попка Студио — в пикселях платы 1024 × 600 на 7″ (≈ 170 точек на дюйм); здесь — те же сантиметры:
// 280 px ≈ 4,2 см ≈ 260 dp, 40 px ≈ 37 dp, 700 px/с ≈ 650 dp/с.
import QtQuick
import Naomi.Ui

Item {
    id: root
    property bool hudOn
    signal hudToggled()
    signal wifiRequested()      // долгое нажатие на Wi-Fi — открыть экран Wi-Fi
    signal bluetoothRequested() // то же для Bluetooth
    signal screensRequested()   // долгое нажатие на «трансляцию» — «Экраны»
    readonly property bool shown: progress > 0

    Axis { id: ax; dir: 1; shift: 37 }
    property alias progress: ax.progress
    readonly property real duration: 380      // мс — весь путь после пальца

    readonly property real sheetDy: ax.newDelta
    readonly property real sheetAlpha: ax.newAlpha
    readonly property real contentDy: ax.oldDelta
    readonly property real contentAlpha: ax.oldAlpha

    function open() { ax.goTo(1, duration, true) }
    function close() { ax.goTo(0, duration, true) }
    function hideNow() { ax.jump(0) }   // экран погас — сразу, без перехода

    // жест — и от строки состояния (ShellView), и по самой шторке
    property real dragTy0: 0
    function dragBegin(ty) { ax.dragBegin(); dragTy0 = ty }      // ty — где палец: путь — от места, где жест его подхватил
    function dragMove(ty) { ax.dragMove(ty - dragTy0) }
    function dragEnd(vy) { ax.goTo(ax.decide(vy), duration, true) }

    visible: shown
    // сама шторка: чёрный экран, органы сверху; уезжает и гаснет как целое
    Item {
        anchors.fill: parent
        opacity: root.sheetAlpha
        transform: Translate { y: root.sheetDy }

        Rectangle {
            anchors.fill: parent
            color: Theme.canvas
            // касание мимо органов (ниже них) — закрыть
            TapHandler { onTapped: (point) => { if (point.position.y > body.y + body.height + 8) root.close() } }
        }
        Column {
            id: body
            x: Theme.margin
            y: Theme.statusBarHeight + 12
            width: parent.width - 2 * Theme.margin
            spacing: 14
            Row {
                spacing: 10
                Caps { text: naomiPhone.wifi.state === "connected" ? "wi-fi · " + naomiPhone.wifi.ssid : "wi-fi · нет"; font.pixelSize: 10 }
                Caps { text: "· " + naomiPhone.battery.percent + "%"; font.pixelSize: 10 }
            }
            SettingsControls {
                width: parent.width
                hudOn: root.hudOn
                onHudToggled: root.hudToggled()
                onWifiRequested: root.wifiRequested()
                onBluetoothRequested: root.bluetoothRequested()
                onScreensRequested: root.screensRequested()
            }
        }
    }

    // вертикальный жест по всей шторке — закрыть (ползунки берут только горизонтальный)
    // (скорость — последняя на ходу: к onActiveChanged(false) Qt её уже обнуляет; путь — по положению
    // пальца, не translation: см. жест «домой» в ShellView)
    DragHandler {
        target: null
        xAxis.enabled: false
        property real vy: 0
        onActiveChanged: active ? root.dragBegin(centroid.scenePosition.y) : root.dragEnd(vy)
        onTranslationChanged: if (active) { vy = centroid.velocity.y; root.dragMove(centroid.scenePosition.y) }
    }
}
