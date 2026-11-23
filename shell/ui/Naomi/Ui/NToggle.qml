// Кнопка-переключатель НаомиОС: рамка 1 px, левая грань 3 px — серая выкл., янтарь вкл.; подпись капсом.
// Долгое нажатие — held() (например, открыть подробные настройки того, что переключается). warn — красным: кнопка
// ждёт подтверждения (перезагрузка телефона — со второго касания).
import QtQuick
import Naomi.Ui

Rectangle {
    id: root
    property string label
    property string value          // мелкая строка справа (состояние словом)
    property bool on: false
    property bool warn: false
    readonly property color tint: warn ? Theme.bad : Theme.amber
    signal clicked()
    signal held()

    implicitHeight: 46
    radius: 3
    color: warn ? Qt.rgba(Theme.bad.r, Theme.bad.g, Theme.bad.b, 0.12) : on ? Theme.amberFaint : "transparent"
    border.color: warn ? Qt.rgba(Theme.bad.r, Theme.bad.g, Theme.bad.b, 0.6) : on ? Theme.amberEdge : Theme.surfaceHigh
    border.width: 1

    Rectangle { width: 3; height: parent.height - 2; x: 1; y: 1; color: root.warn || root.on ? root.tint : Theme.dim }
    Caps {
        id: label
        anchors.left: parent.left; anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: root.warn || root.on ? root.tint : Theme.muted
        font.pixelSize: 11
    }
    Caps {   // длинное (имя сети) — с многоточием, не наезжая на подпись
        anchors.left: label.right; anchors.leftMargin: 12
        anchors.right: parent.right; anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        text: root.value
        color: root.warn || root.on ? root.tint : Theme.dim
        font.pixelSize: 10
    }
    TapHandler {
        onTapped: root.clicked()
        onLongPressed: root.held()
    }
}
