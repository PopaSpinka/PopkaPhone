// AOD («экран всегда включён») — главный экран, притушенный на месте. Экрана блокировки нет (решение пользователя,
// 12 ноября 2026: «главный экран просто потухает до той яркости, что была у блокировки, без деформации»): копии нет —
// это слой поверх всего, а под ним живой главный. Чёрный поверх сгущается до 0,4, обои под главным — ещё на 35 %
// темнее (ShellView), яркость панели падает до уровня AOD (naomiPower). Будят касание экрана, датчик отпечатка и
// кнопка питания — на главный (docs/SHELL.md «AOD»).
// amount — ход перехода (naomiPower.aodAmount), вид выбирается в начале входа:
// - с главного (homeUnder: на экране главный, без программы и шторки) — на месте: чёрный 0,4 × ход;
// - с программы, экрана проекта, шторки — через чёрный: всё гаснет в чёрный за первые 35 % пути — тут covered():
//   оболочка под чёрным уходит на главный (программа живёт дальше, как после «домой»), — а главный проявляется уже
//   притушенным за остальные 65 %. Выход из AOD — всегда с главного, на месте.
// Свой экран Always On у проекта (screen — готовый экран «Always On» студии) — через чёрный в обе стороны: он на
// чёрном, со сдвигом против выгорания, без притушения.
// Сдвиг против выгорания у главного — у его слоёв (не у обоев и панели «Background»): по кругу из 8 точек радиусом
// 4 dp, шаг — минута, от места, где они стояли в начале AOD (shiftX, shiftY — разница), × ход: на входе и выходе
// ничего не сдвигается. Кадр — раз в минуту. Вторая ступень (deep: панель в малом питании, naomiPower.deep) — обоев и
// панели «Background» нет, слои на чёрном (wallIn): гаснут сразу, в кадре ухода в малое питание (панель мигает при
// смене режима), возвращаются (сдвинули телефон) — за 0,5 с.
import QtQuick
import Naomi.Ui
import Naomi.Screens

Item {
    id: root
    property date now: new Date()
    property real amount: 1
    property bool homeUnder: true        // на экране главный: AOD — на месте
    property var screen: null            // свой экран Always On проекта (null — притушенный главный)
    property bool deep: false            // вторая ступень: панель в малом питании
    signal covered()                     // через чёрный: экран уже чёрный — оболочке пора на главный

    // обои и панель «Background» на второй ступени: 0 — нет; из неё — проявляются
    property real wallIn: 1
    onDeepChanged: {
        if (deep) { wallBack.stop(); wallIn = 0 }
        else wallBack.restart()
    }
    NumberAnimation { id: wallBack; target: root; property: "wallIn"; from: 0; to: 1; duration: 500; easing.type: Easing.OutCubic }

    // вид перехода — в его начале; в AOD выход — с главного на месте (свой экран — через чёрный)
    property bool viaBlack: false
    property real was: 0
    readonly property int step: (now.getHours() * 60 + now.getMinutes()) % 8
    property int step0: 0
    Component.onCompleted: step0 = step
    onAmountChanged: {
        if (was <= 0 && amount > 0) {
            viaBlack = screen !== null || !homeUnder
            step0 = step
        }
        if (viaBlack && was < 0.35 && amount >= 0.35) covered()
        if (amount >= 1 || amount <= 0) viaBlack = screen !== null && amount > 0
        was = amount
    }
    // сдвиг слоёв главного против выгорания: от места в начале AOD, × ход
    readonly property real shiftX: (Math.round(4 * Math.cos(step * Math.PI / 4)) - Math.round(4 * Math.cos(step0 * Math.PI / 4))) * amount
    readonly property real shiftY: (Math.round(4 * Math.sin(step * Math.PI / 4)) - Math.round(4 * Math.sin(step0 * Math.PI / 4))) * amount

    Rectangle {   // чёрный: притушить главный; через чёрный — сначала весь, потом до 0,4
        anchors.fill: parent
        color: Theme.canvas
        opacity: root.screen !== null ? Math.min(1, root.amount / 0.35)
               : root.viaBlack ? (root.amount < 0.35 ? root.amount / 0.35 : 1 - 0.6 * (root.amount - 0.35) / 0.65)
               : 0.4 * root.amount
    }
    Item {   // свой экран Always On проекта
        width: root.width; height: root.height
        visible: root.screen !== null && opacity > 0
        opacity: Math.max(0, (root.amount - 0.35) / 0.65)
        x: Math.round(4 * Math.cos(root.step * Math.PI / 4))
        y: Math.round(4 * Math.sin(root.step * Math.PI / 4))
        ScreenView {
            scr: root.screen
            device: ({ w: root.width, h: root.height, radius: 0, cutout: null })
            bezel: "transparent"
            now: root.now
        }
    }
}
