// Громкость на столе после клавиш — перенос плашки НаомиОС (bcdesk osd_redraw, docs/DESKTOP.md «Звук»): модуль
// 288 × 66 по середине, на 18 ниже панели; шильдик «ГРОМКОСТЬ» и справа «45%» (или «БЕЗ ЗВУКА»), под ним — шкала.
// Держится 1,5 с после последнего нажатия (каждое — заново); появляется и уходит как окно звука (110 и 150 мс).
// Повторные нажатия, пока видна, — только новые числа; таяла — проявляется снова. Окно звука открыто — не нужна.
// Она же — сообщение (osd_show_message): «СКРИНШОТ ——— НЕ ПОЛУЧИЛСЯ» и ниже, мелко, почему; держится 2 с.
// И она же — другая шкала (showLevel): «ЯРКОСТЬ 70%» после клавиш яркости — вид тот же.
import QtQuick

Item {
    id: osd
    property int volume: 0
    property bool muted: false
    property bool shown: false
    property bool message: false
    property string title
    property string value
    property string line
    property string levelTitle   // "" — громкость, иначе своя шкала: её шильдик
    property int level: 0
    function show() {
        message = false
        levelTitle = ""
        present(1500)
    }
    function showLevel(t, percent) {
        message = false
        levelTitle = t
        level = percent
        present(1500)
    }
    function showMessage(t, v, l) {
        title = t
        value = v
        line = l
        message = true
        present(2000)   // OSD_SHOW_MS + 500
    }
    function present(ms) {
        hideTimer.interval = ms
        hideTimer.restart()
        if (!shown) {
            shown = true
            closeAnim.stop()
            closeT = 1
            openT = 0
            openAnim.restart()
        }
    }
    function hideNow() {
        hideTimer.stop()
        shown = false
        openAnim.stop()
        closeAnim.stop()
        closeT = 1
    }
    Timer {
        id: hideTimer
        onTriggered: {
            osd.shown = false
            osd.closeT = 0
            closeAnim.restart()
        }
    }

    width: 288
    height: 66

    property real openT: 1
    property real closeT: 1
    NumberAnimation { id: openAnim; target: osd; property: "openT"; from: 0; to: 1; duration: 110 }
    NumberAnimation { id: closeAnim; target: osd; property: "closeT"; from: 0; to: 1; duration: 150 }
    visible: shown || closeAnim.running
    transformOrigin: Item.Top
    scale: shown ? 0.98 + 0.02 * (1 - Math.pow(1 - openT, 3)) : 1 - 0.01 * (1 - Math.pow(1 - closeT, 3))
    opacity: shown ? 1 : (Math.exp(-closeT * 150 / 38) - Math.exp(-150 / 38)) / (1 - Math.exp(-150 / 38))

    DeskModule { anchors.fill: parent }
    Plate {
        x: 16; y: 14
        width: 256
        title: osd.message ? osd.title : osd.levelTitle !== "" ? osd.levelTitle : "ГРОМКОСТЬ"
        note: osd.message ? osd.value : osd.levelTitle !== "" ? osd.level + "%" : osd.muted ? "БЕЗ ЗВУКА" : osd.volume + "%"
    }
    FontMetrics { id: metaFont; font: lineText.font }
    Text {   // почему: моно приглушённым (N_META − 0,5 = 12,5; размер в пикселях у Qt целый), на 12 ниже шильдика
        id: lineText
        visible: osd.message
        x: 16
        width: 256
        y: 14 + 18 + 12 + metaFont.tightBoundingRect("H").height - metaFont.ascent   // линия букв — на высоту заглавной ниже
        text: osd.line
        elide: Text.ElideRight
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 0.5
        color: DeskStyle.muted
    }
    DeskMeter {
        visible: !osd.message
        x: 16; y: 14 + 18 + 12
        width: 256
        readonly property bool off: osd.muted && osd.levelTitle === ""
        value: (osd.levelTitle !== "" ? osd.level : osd.volume) / 100
        fill: off ? DeskStyle.dim : DeskStyle.amber
        fillAlpha: off ? 0.5 : 0.75
        tick: off ? DeskStyle.muted : DeskStyle.amber
    }
}
