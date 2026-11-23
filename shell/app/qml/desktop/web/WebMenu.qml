// Меню вкладки и меню страницы браузера (webmenu_draw в bcdesk.c): модуль шириной 232, пункты — кнопки НаомиОС
// 208 × 34 через 6, поля 12. Левым верхним углом — у курсора (+2, +2); не влезает снизу — над курсором; по краям —
// не ближе 8 справа и 6 слева. Появляется: масштаб 0,98 → 1 за 110 мс; уходит: → 0,99 за 150 мс и тает.
// Пока открыто — клавиши у меню: Esc закрывает.
import QtQuick
import ".."

FocusScope {
    id: menu
    property var items: []                // [{text, tone, action}]
    property bool open: false
    property Item bounds                  // стол: в каких пределах
    signal picked(string action)
    signal dismissed()
    width: DeskStyle.webMenuW
    height: 12 + items.length * DeskStyle.btnH + Math.max(0, items.length - 1) * 6 + 12
    visible: opacity > 0
    opacity: open ? 1 : 0
    transformOrigin: Item.Top
    Behavior on opacity {
        enabled: !menu.open
        NumberAnimation { duration: 150; easing.type: Easing.OutExpo }
    }
    // масштаб — своими анимациями: присваивание в show() рвало привязку, и меню после первого показа оставалось на 0,98
    onOpenChanged: {
        if (open) {
            shrink.stop()
            grow.restart()
        } else {
            grow.stop()
            shrink.restart()
        }
    }
    NumberAnimation { id: grow; target: menu; property: "scale"; from: 0.98; to: 1; duration: 110; easing.type: Easing.OutCubic }
    NumberAnimation { id: shrink; target: menu; property: "scale"; to: 0.99; duration: 150; easing.type: Easing.OutCubic }

    function show(px, py, list) {
        items = list
        let x = px + 2, y = py + 2
        if (y + height > bounds.height - 8)
            y = py - 4 - height - 2
        x = Math.max(DeskStyle.popGap, Math.min(x, bounds.width - 8 - width))
        menu.x = x
        menu.y = Math.max(DeskStyle.popGap, y)
        open = true
        forceActiveFocus()
    }
    function close() {
        if (!open)
            return
        open = false
        dismissed()
    }
    Keys.onEscapePressed: close()

    MouseArea { anchors.fill: parent }   // щелчок по самому меню — не мимо него
    DeskModule { anchors.fill: parent }
    Column {
        x: 12; y: 12
        spacing: 6
        Repeater {
            model: menu.items
            delegate: NButton {
                required property var modelData
                text: modelData.text
                tone: modelData.tone
                onClicked: {
                    menu.close()
                    menu.picked(modelData.action)
                }
            }
        }
    }
}
