// «Настройки» → «Мышь» (выбор пользователя 17 ноября 2026 — вариант 10 из макетов): слева рисунок — куда поедет
// страница, если крутить колесо к себе; справа уставки «− +»: скорость указателя, направление колеса, строк за
// щелчок, плавность и мягкость; внизу — дверь «Подробно» (наборы, графики, ускорение). Значения — в sw.cfg.
import QtQuick
import ".."

Item {
    id: page
    property var sw                          // окно «Настроек»: cfg, наборы, подписи
    readonly property var cfg: sw.cfg
    implicitHeight: door.y + door.height

    component SetRow: Item {
        id: row
        property string label
        property bool first: false
        default property alias control: slot.data
        width: sets.width
        height: 56
        Rectangle {
            visible: !row.first
            width: parent.width; height: 1
            color: DeskStyle.amberA(DeskStyle.line)
        }
        Text {
            height: row.height
            verticalAlignment: Text.AlignVCenter
            text: row.label.toUpperCase()
            font.family: DeskStyle.mono
            font.pixelSize: 12
            font.letterSpacing: 12 * 0.12
            color: DeskStyle.muted
        }
        Item {
            id: slot
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: childrenRect.width
            height: childrenRect.height
        }
    }
    component Seg: NButton {
        width: 104
        height: 30
    }

    Column {
        id: illus
        width: 190
        spacing: 12
        SubPlate { width: parent.width; text: "Колесо" }
        MouseArt { natural: page.cfg.natural }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            lineHeight: 1.3
            text: page.sw.hint()
            font.family: DeskStyle.mono
            font.pixelSize: 12
            color: DeskStyle.muted
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            lineHeight: 1.3
            text: "мышь приставки; в окне Mac — как в macOS"
            font.family: DeskStyle.mono
            font.pixelSize: 12
            color: DeskStyle.dim
        }
    }

    Column {
        id: sets
        x: 218
        width: page.width - x
        SetRow {
            first: true
            label: "Скорость указателя"
            Stepper { value: page.cfg.speed; step: 5; unit: "%"; onMoved: (v) => page.cfg.speed = v }
        }
        SetRow {
            label: "Направление"
            Row {
                spacing: 6
                Seg { text: "Обычное"; tone: page.cfg.natural ? "off" : "on"; onClicked: page.cfg.natural = false }
                Seg { text: "Обратное"; tone: page.cfg.natural ? "on" : "off"; onClicked: page.cfg.natural = true }
            }
        }
        SetRow {
            label: "Строк за щелчок"
            Stepper {
                value: page.cfg.lines; from: 1; to: 10; unit: page.sw.linesWord(page.cfg.lines)
                onMoved: (v) => page.cfg.lines = v
            }
        }
        SetRow {
            label: "Плавная прокрутка"
            Row {
                spacing: 6
                Seg { text: "Выкл"; tone: page.cfg.smooth ? "off" : "on"; onClicked: page.cfg.smooth = false }
                Seg { text: "Вкл"; tone: page.cfg.smooth ? "on" : "off"; onClicked: page.cfg.smooth = true }
            }
        }
        SetRow {
            label: "Мягкость"
            opacity: page.cfg.smooth ? 1 : 0.45
            enabled: page.cfg.smooth
            Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
            Stepper { value: page.cfg.soft; from: 60; to: 400; step: 10; unit: "мс"; onMoved: (v) => page.cfg.soft = v }
        }
    }

    Door {
        id: door
        y: Math.max(illus.height, sets.height) + 20
        width: page.width
        title: "Подробно"
        note: "набор «" + page.sw.presetName + "» · " + page.sw.accelName(page.cfg.accel) + " · разгон колеса "
              + (page.cfg.wheelAccel ? "вкл" : "выкл")
        onClicked: page.sw.page = "more"
    }
}
