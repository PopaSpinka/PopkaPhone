// Край окна для размера (DeskWindow, WebWindow; docs/DESKTOP.md, «Курсор»): полоса вдоль всей стороны —
// DeskStyle.edgeOut точек снаружи окна и inner внутри; ближе DeskStyle.edgeCorner к концу стороны (вся дуга
// закругления угла) — угол, тянет обе стороны. Полосы соседних сторон у угла перекрываются — обе там «угол».
// Курсор — ↔ ↕ ⤡ ⤢ по тому, что схватишь; подсветки края нет (жёлтый «бегунок» убран по просьбе пользователя
// 17 ноября 2026). pull — что тянут: ставится при нажатии и до отпускания не меняется.
// Ставить прямым ребёнком окна; окно, у которого размер не тянут, прячет края (visible: false) — тогда курсор
// там того, что под ними.
import QtQuick

MouseArea {
    property int sx          // сторона: −1 — левая, 1 — правая
    property int sy          // −1 — верх, 1 — низ
    property real inner: DeskStyle.edgeIn
    readonly property real outer: DeskStyle.edgeOut
    property point pull: Qt.point(sx, sy)
    readonly property point aim: pressed ? pull : at(mouseX, mouseY)

    function at(mx, my) {   // что под мышью (в своих точках): сторона или угол — по месту вдоль стороны
        const k = DeskStyle.edgeCorner
        if (sx === 0) {
            const p = x + mx
            return Qt.point(p < k ? -1 : p > parent.width - k ? 1 : 0, sy)
        }
        const p = y + my
        return Qt.point(sx, p < k ? -1 : p > parent.height - k ? 1 : 0)
    }

    hoverEnabled: true
    x: sx > 0 ? parent.width - inner : -outer
    y: sy > 0 ? parent.height - inner : -outer
    width: sx === 0 ? parent.width + 2 * outer : outer + inner
    height: sy === 0 ? parent.height + 2 * outer : outer + inner
    cursorShape: aim.x * aim.y > 0 ? Qt.SizeFDiagCursor : aim.x * aim.y < 0 ? Qt.SizeBDiagCursor
               : aim.x !== 0 ? Qt.SizeHorCursor : Qt.SizeVerCursor
    onPressedChanged: if (pressed) pull = at(mouseX, mouseY)
}
