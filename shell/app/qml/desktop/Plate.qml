// Шильдик — как draw_plate (draw_plate_value) в bcdesk: номер тускло, название янтарём моно-капсом с разрядкой,
// значение (value, цветом valueColor — «45%» у звука), линия до правого края, справа — подпись приглушённым (note;
// noteColor — другим цветом: «БЕЗ ПРОПУСКОВ» у монитора системы);
// reserve — место справа под кнопки шильдика.
import QtQuick

Item {
    id: p
    property string num
    property string title
    property string note
    property string value
    property color valueColor: DeskStyle.text
    property color noteColor: DeskStyle.muted
    property real reserve: 0
    height: DeskStyle.plateH

    component Caps: Text {
        font.family: DeskStyle.mono
        font.pixelSize: DeskStyle.caps
        height: DeskStyle.plateH
        verticalAlignment: Text.AlignVCenter
    }
    Caps { id: numText; text: p.num; color: DeskStyle.dim; font.letterSpacing: DeskStyle.caps * 0.1 }
    Caps {
        id: titleText
        x: p.num !== "" ? numText.implicitWidth + 9 : 0
        text: p.title
        color: DeskStyle.amber
        font.letterSpacing: DeskStyle.caps * 0.2
    }
    Caps {
        id: valueText
        visible: p.value !== ""
        x: titleText.x + titleText.implicitWidth + 9
        text: p.value
        color: p.valueColor
        font.letterSpacing: DeskStyle.caps * 0.1
    }
    readonly property real end: width - reserve - (reserve > 0 ? 9 : 0)
    Caps {
        id: noteText
        visible: p.note !== ""
        x: p.end - implicitWidth
        text: p.note
        color: p.noteColor
        font.letterSpacing: DeskStyle.caps * 0.2
    }
    Rectangle {   // линия
        readonly property real from: valueText.visible ? valueText.x + valueText.implicitWidth + 9
                                                       : titleText.x + titleText.implicitWidth + 9
        readonly property real to: noteText.visible ? noteText.x - 9 : p.end
        x: from
        y: Math.round(DeskStyle.plateH / 2)
        width: Math.max(0, to - from)
        height: 1
        color: DeskStyle.amberA(DeskStyle.line)
    }
}
