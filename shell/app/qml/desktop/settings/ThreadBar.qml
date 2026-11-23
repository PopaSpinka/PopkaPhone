// Полоса прокрутки страницы «Настроек» — «Нить», как у страниц браузера стола (shell/apps/web/scripts/scrollbar.js,
// docs/WEB.md, «Тёмные сайты и полоса „Нить“»): волосок 2 точки цвета muted .45 посередине полосы 12 без желоба,
// хват не короче 40; под мышью хват — 6 и светлее (text .7), тянут — янтарь. Щелчок мимо хвата — на окно вверх или
// вниз, как в браузере. Видна, только когда страница выше окна.
import QtQuick
import ".."

Item {
    id: t
    property Flickable flick
    readonly property real total: flick ? flick.contentHeight : 0
    readonly property real view: flick ? flick.height : 0
    readonly property real room: Math.max(0, total - view)
    visible: room > 0.5
    width: 12
    readonly property real thumbH: Math.min(height, Math.max(40, height * view / Math.max(1, total)))
    readonly property real thumbY: room > 0 ? (height - thumbH) * flick.contentY / room : 0
    readonly property bool overThumb: area.containsMouse && area.mouseY >= thumbY && area.mouseY <= thumbY + thumbH
    readonly property bool held: area.pressed && area.grab >= 0
    function scrollTo(y) { flick.contentY = Math.max(0, Math.min(room, y)) }

    Rectangle {
        readonly property bool wide: t.overThumb || t.held
        x: wide ? 3 : 5
        y: t.thumbY
        width: wide ? 6 : 2
        height: t.thumbH
        radius: width / 2
        color: t.held ? DeskStyle.amber : wide ? Qt.rgba(236 / 255, 233 / 255, 227 / 255, 0.7)
                                               : Qt.rgba(169 / 255, 164 / 255, 154 / 255, 0.45)
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        property real grab: -1     // где взяли хват (от его верха); −1 — щёлкнули мимо
        onPressed: (m) => {
            if (m.y >= t.thumbY && m.y <= t.thumbY + t.thumbH) {
                grab = m.y - t.thumbY
            } else {
                grab = -1
                t.scrollTo(t.flick.contentY + (m.y < t.thumbY ? -t.view : t.view))
            }
        }
        onPositionChanged: (m) => {
            if (pressed && grab >= 0)
                t.scrollTo((m.y - grab) / Math.max(1, t.height - t.thumbH) * t.room)
        }
        onReleased: grab = -1
    }
}
