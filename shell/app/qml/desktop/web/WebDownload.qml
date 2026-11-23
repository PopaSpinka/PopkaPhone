// Загрузка на полосе браузера (webbar_download в bcdesk.c): рамка .16, грань слева 3 точки (качается — янтарь .5,
// готово — янтарь, не удалась — красный), значок 12 (стрелка, галочка, крестик), подпись моно 12: «45%  имя» или
// имя. Готово — щелчок открывает папку в «Файлах» и убирает с полосы; не удалась — убирает.
import QtQuick
import ".."

Item {
    id: d
    property var info                     // {id, name, received, total, state, path}
    signal activated()
    readonly property bool done: info.state === "completed"
    readonly property bool failed: info.state === "interrupted"
    readonly property string label: d.done || d.failed ? info.name
        : (info.total > 0 ? Math.floor(100 * info.received / info.total) : 0) + "%  " + info.name
    FontMetrics { id: fm; font.family: DeskStyle.mono; font.pixelSize: 12 }
    width: Math.min(DeskStyle.webDownloadW, Math.ceil(13 + 12 + 8 + fm.advanceWidth(label) + 10))
    height: DeskStyle.appH

    Rectangle {
        anchors.fill: parent
        color: area.containsMouse && (d.done || d.failed) ? DeskStyle.amberA(DeskStyle.itemHover) : "transparent"
        border.width: 1
        border.color: DeskStyle.amberA(DeskStyle.line)
    }
    Rectangle {
        width: 3; height: d.height
        color: d.failed ? DeskStyle.bad : DeskStyle.amberA(d.done ? 1 : 0.5)
    }
    Glyph {
        x: 13 + 6; y: d.height / 2
        kind: d.done ? "dl-done" : d.failed ? "dl-failed" : "dl-progress"
        line: 1.4
        color: d.failed ? DeskStyle.bad : DeskStyle.amber
    }
    Text {
        x: 33
        width: d.width - 10 - x
        height: d.height
        verticalAlignment: Text.AlignVCenter
        text: d.label
        elide: Text.ElideRight
        font: fm.font
        color: d.done ? DeskStyle.text : DeskStyle.muted
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: if (d.done || d.failed) d.activated()
    }
}
