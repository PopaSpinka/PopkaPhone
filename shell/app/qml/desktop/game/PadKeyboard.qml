// Клавиатура окна «Кнопки геймпада» (docs/GAMES.md, «Кнопки геймпада»): маленькая, латиницей, как английская
// раскладка Magic Keyboard — цифры, буквы, знаки, Esc, Tab, Shift, Ctrl, Alt, пробел, стрелки (↑ над ↓ вполвысоты).
// Щелчок по клавише — назначить её выбранной кнопке (chosen). Клавиша выбранной кнопки горит янтарём; занятые другими —
// светлее, в углу метка кнопки (A, L1, D↑…). u — ширина клавиши в точках.
import QtQuick
import ".."
import "pad.js" as Pad

Item {
    id: kb
    property real u: 30
    property string current: ""          // клавиша выбранной кнопки
    property var badges: ({})            // клавиша → метки кнопок, что её заняли
    signal chosen(string key)
    readonly property real gap: 4
    readonly property real kh: Math.round(u * 0.88)
    implicitWidth: 15 * u + 14 * gap
    implicitHeight: 5 * kh + 4 * gap

    component Key: Rectangle {
        id: key
        property string k
        property real w: 1
        property real h: kb.kh
        readonly property var marks: kb.badges[k] || []
        readonly property bool on: k !== "" && k === kb.current
        readonly property bool wide: (Pad.KL[k] || "").length > 2
        width: w * kb.u + (w - 1) * kb.gap
        height: h
        radius: 3
        antialiasing: true
        color: on ? DeskStyle.amberA(0.22) : keyArea.containsMouse ? DeskStyle.amberA(0.07) : "#141512"
        border.width: 1
        border.color: on ? DeskStyle.amber : keyArea.containsMouse || marks.length ? DeskStyle.amberA(0.36)
                                                                                  : DeskStyle.amberA(DeskStyle.line)
        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: keyArea.pressed ? 1 : 0
            text: Pad.kname(key.k)
            font.family: DeskStyle.mono
            font.pixelSize: Math.round(kb.u * (key.wide ? 0.27 : key.h < kb.kh ? 0.3 : 0.36))
            font.weight: Font.Medium
            font.letterSpacing: key.wide ? kb.u * 0.27 * 0.12 : 0
            color: key.on ? DeskStyle.amber : keyArea.containsMouse || key.marks.length ? DeskStyle.text : DeskStyle.muted
        }
        Text {   // кто занял: метки кнопок
            visible: key.marks.length > 0
            x: 3; y: 2
            text: key.marks.length > 2 ? key.marks.slice(0, 2).join(" ") + "…" : key.marks.join(" ")
            font.family: DeskStyle.mono
            font.pixelSize: Math.max(7, Math.round(kb.u * (key.h < kb.kh ? 0.2 : 0.23)))
            font.weight: Font.DemiBold
            color: DeskStyle.amber
            opacity: key.on ? 1 : 0.85
        }
        MouseArea { id: keyArea; anchors.fill: parent; hoverEnabled: true; onClicked: kb.chosen(key.k) }
    }
    Component {   // стрелки: ← [↑ над ↓] →
        id: arrows
        Row {
            spacing: kb.gap
            Key { k: "Left" }
            Column {
                spacing: 2
                Key { k: "Up"; h: (kb.kh - 2) / 2 }
                Key { k: "Down"; h: (kb.kh - 2) / 2 }
            }
            Key { k: "Right" }
        }
    }

    Column {
        spacing: kb.gap
        Repeater {
            model: Pad.ROWS
            delegate: Row {
                id: row
                required property var modelData
                spacing: kb.gap
                Repeater {
                    model: row.modelData
                    delegate: Loader {
                        id: ld
                        required property var modelData
                        sourceComponent: ld.modelData[0] === "ARROWS" ? arrows : plain
                        Component {
                            id: plain
                            Key { k: ld.modelData[0]; w: ld.modelData.length > 1 ? ld.modelData[1] : 1 }
                        }
                    }
                }
            }
        }
    }
}
