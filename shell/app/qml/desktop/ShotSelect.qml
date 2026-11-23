// Прицел скриншота области — перенос НаомиОС (bcdesk, «Скриншот: кадр монитора и выделение области», shot_select_*):
// Shift+Alt+4 — курсор-прицел, мышь — только ему: окнам и панели ни наведения, ни щелчков. Левая кнопка — начало
// рамки (можно и на панели — тогда в снимок попадёт и она), тянут — рамка в пределах стола: внутри серый 50 % на 30 %
// (как в macOS — виден и на белом, и на чёрном), по краю — линия цвета текста ровно в один пиксель кадра. Отпустили —
// рамка и прицел пропадают в том же кадре, и снимается то, что было в рамке (taken: точки стола, целые); меньше 4 × 4 —
// случайный щелчок, прицел остаётся. Правая кнопка и Esc (naomiStream.screenshotKey 0) — отмена.
import QtQuick

MouseArea {
    id: sel
    property bool selecting: false
    signal taken(rect area)
    function begin() {
        pulling = false
        selecting = true
        console.info("скриншот области: прицел")
    }
    function cancel() {
        if (!selecting)
            return
        pulling = false
        selecting = false
        console.info("скриншот области: отменён")
    }

    readonly property int minSize: 4   // SHOT_MIN
    visible: selecting
    hoverEnabled: true
    acceptedButtons: Qt.LeftButton | Qt.RightButton
    cursorShape: Qt.CrossCursor

    property bool pulling: false
    property point from
    property rect box   // выделенное: от нажатия до мыши, целые точки (наружу), в пределах стола
    function track(m) {
        const left = Math.max(0, Math.floor(Math.min(from.x, m.x))), right = Math.min(width, Math.ceil(Math.max(from.x, m.x)))
        const top = Math.max(0, Math.floor(Math.min(from.y, m.y))), bottom = Math.min(height, Math.ceil(Math.max(from.y, m.y)))
        box = Qt.rect(left, top, Math.max(0, right - left), Math.max(0, bottom - top))
    }
    onPressed: (m) => {
        if (m.button === Qt.RightButton) {
            cancel()
            return
        }
        pulling = true
        from = Qt.point(m.x, m.y)
        track(m)
    }
    onPositionChanged: (m) => {
        if (pulling)
            track(m)
    }
    onReleased: (m) => {
        if (m.button !== Qt.LeftButton || !pulling)
            return
        pulling = false
        track(m)
        if (box.width < minSize || box.height < minSize)
            return   // случайный щелчок: рамки нет, прицел остаётся
        const area = box
        selecting = false
        taken(area)
    }

    // Рамка — в пикселях кадра (то же округление, что у снимка: round(x·s)): каждая сторона — ровно один пиксель
    readonly property real s: naomiDesk.pixelScale   // как сцена ложится в кадр (DesktopScreen::pixelScale)
    readonly property int px0: Math.round(box.x * s)
    readonly property int py0: Math.round(box.y * s)
    readonly property int px1: Math.round((box.x + box.width) * s)
    readonly property int py1: Math.round((box.y + box.height) * s)
    Item {
        visible: sel.pulling && sel.px1 > sel.px0 && sel.py1 > sel.py0
        Rectangle {   // подсветка (SHOT_FILL: серый 50 % на 30 %)
            x: sel.px0 / sel.s
            y: sel.py0 / sel.s
            width: (sel.px1 - sel.px0) / sel.s
            height: (sel.py1 - sel.py0) / sel.s
            color: Qt.rgba(0.5, 0.5, 0.5, 0.3)
        }
        component Edge: Rectangle { color: "#ece9e3" }   // SHOT_EDGE — цвет текста НаомиОС
        Edge { x: sel.px0 / sel.s; y: sel.py0 / sel.s; width: (sel.px1 - sel.px0) / sel.s; height: 1 / sel.s }
        Edge { x: sel.px0 / sel.s; y: (sel.py1 - 1) / sel.s; width: (sel.px1 - sel.px0) / sel.s; height: 1 / sel.s }
        Edge { x: sel.px0 / sel.s; y: sel.py0 / sel.s; width: 1 / sel.s; height: (sel.py1 - sel.py0) / sel.s }
        Edge { x: (sel.px1 - 1) / sel.s; y: sel.py0 / sel.s; width: 1 / sel.s; height: (sel.py1 - sel.py0) / sel.s }
    }
}
