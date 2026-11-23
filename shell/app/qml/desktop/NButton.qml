// Кнопка меню НаомиОС (draw_nbutton в bcdesk.c): рамка 1 точка янтарём .16, слева грань 3 точки цветом тона, подпись
// моно 13 капсом с разрядкой 0,12 (другие — fontSize, letterSpacing: у кнопки в шильдике 11,5 и 0,08), с отступом 12,
// по середине по заглавным; нажата — всё на 1 ниже. centered — подпись по середине кнопки (кнопки в шильдике
// «FORCE QUIT»); disabled — всё вполсилы (.45), без наведения и нажатия.
//   tone: "on" — заливка янтарь .09 (под мышью .13), грань и подпись янтарём;
//         "off" — без заливки (под мышью .07), грань тусклая, подпись приглушённая (под мышью светлая);
//         "bad" — заливка красным .10 (.16), грань и подпись красным;
//         "go" — действие бирюзой («до 100 % один раз»): грань бирюзой, подпись светлая; "goOn" — оно идёт: заливка
//         бирюзой .14 (.18), грань и подпись бирюзой.
import QtQuick

Item {
    id: b
    property string text
    property string tone: "off"
    property int fontSize: DeskStyle.caps
    property real letterSpacing: DeskStyle.caps * 0.12
    property bool centered: false
    property bool disabled: false
    readonly property bool hovered: area.containsMouse && !disabled
    readonly property real dy: area.pressed && !disabled ? 1 : 0
    opacity: disabled ? 0.45 : 1
    signal clicked()
    width: 208
    height: DeskStyle.btnH

    readonly property color ink: tone === "on" ? DeskStyle.amber : tone === "bad" ? DeskStyle.bad
                               : tone === "goOn" ? DeskStyle.teal : hovered || tone === "go" ? DeskStyle.text : DeskStyle.muted
    function tealA(a) { return Qt.rgba(99 / 255, 182 / 255, 176 / 255, a) }
    Rectangle {
        y: b.dy
        width: b.width; height: b.height
        color: b.tone === "on" ? DeskStyle.amberA(b.hovered ? 0.13 : 0.09)
             : b.tone === "bad" ? DeskStyle.badA(b.hovered ? 0.16 : 0.10)
             : b.tone === "goOn" ? b.tealA(b.hovered ? 0.18 : 0.14)
             : b.hovered ? DeskStyle.amberA(DeskStyle.itemHover) : "transparent"
        border.width: 1
        border.color: DeskStyle.amberA(DeskStyle.line)
    }
    Rectangle {   // грань
        y: b.dy
        width: 3; height: b.height
        color: b.tone === "on" ? DeskStyle.amber : b.tone === "bad" ? DeskStyle.bad
             : b.tone === "go" || b.tone === "goOn" ? DeskStyle.teal : DeskStyle.dim
    }
    Text {
        x: b.centered ? 3 + Math.round((b.width - 3 - implicitWidth) / 2) : 12
        y: b.dy
        height: b.height
        verticalAlignment: Text.AlignVCenter
        text: b.text.toUpperCase()
        font.family: DeskStyle.mono
        font.pixelSize: b.fontSize
        font.letterSpacing: b.letterSpacing
        color: b.ink
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: if (!b.disabled) b.clicked()
    }
}
