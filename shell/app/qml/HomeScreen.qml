// Главный экран — только список программ внизу, по образцу пользователя (приложение Tesla, 11 ноября 2026):
// значок, название, подпись, «›». Больше на нём ничего нет — ни строки состояния (ShellView), ни заголовка:
// решение пользователя. Касание строки — подсветка и программа.
// Проект студии NaomiPhone загружен (screen — его главный экран, ShellView) — вместо списка его слои (Naomi.Screens):
// строка программы проекта открывает программу телефона с тем же именем (apps[row].id: chat, notes, settings),
// слой со ссылкой — экран проекта (openScreen → ShellView: ProjectPage).
// Обои проекта на главном рисует ShellView под ним (стоят на месте, пока над главным открывается программа) — тогда
// glass прозрачный.
import QtQuick
import Naomi.Ui
import Naomi.Screens

Item {
    id: root
    signal open(string app)
    signal openScreen(string id)        // ссылка слоя проекта на его экран
    property var screen: null            // главный экран проекта студии (null — свой список)
    property var projectApps: []         // программы проекта: [{id, title, sub, icon, to}]
    property date now: new Date()
    property color glass: Theme.canvas   // фон (прозрачный — под ним обои проекта)
    property real shiftX: 0              // сдвиг слоёв против выгорания (AOD — это главный, притушенный: AodScreen)
    property real shiftY: 0
    property real backdrop: 1            // панель «Background» (вторая ступень AOD — без неё)

    readonly property var apps: [
        { app: "settings", title: "Настройки", sub: "",            icon: "settings" }
    ]

    Rectangle { anchors.fill: parent; color: root.glass }

    ScreenView {
        visible: root.screen !== null
        scr: root.screen
        apps: root.projectApps
        device: ({ w: root.width, h: root.height, radius: 0, cutout: null })
        bezel: "transparent"
        glass: "transparent"
        shiftX: root.shiftX; shiftY: root.shiftY
        backdrop: root.backdrop
        now: root.now
        live: true
        onOpen: (to, row) => {
            const a = row >= 0 ? root.projectApps[row] : null
            if (a && a.id) root.open(a.id)
            else if (row < 0 && to) root.openScreen(to)
        }
    }

    Column {
        id: list
        visible: root.screen === null
        anchors.left: parent.left; anchors.right: parent.right
        anchors.bottom: parent.bottom; anchors.bottomMargin: 30
        transform: Translate { x: root.shiftX; y: root.shiftY }
        Repeater {
            model: root.apps
            Item {
                id: row
                required property var modelData
                width: list.width
                height: 68
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: 10; anchors.rightMargin: 10
                    radius: 18
                    color: Theme.surface
                    opacity: tap.pressed ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: 120 } }
                }
                Text {
                    x: 33 - width / 2
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.icon
                    font.family: "Material Symbols Rounded"; font.pixelSize: 26
                    font.variableAxes: ({ "FILL": 1, "wght": 400, "opsz": 24 })
                    renderType: Text.NativeRendering   // у залитых значков контуры накладываются — по полю расстояний дыры
                    color: Theme.muted
                }
                Column {
                    x: 64
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    Text {
                        text: row.modelData.title
                        font.family: Theme.sans; font.pixelSize: 21; font.weight: Font.Medium
                        color: Theme.text
                    }
                    Text {
                        visible: row.modelData.sub !== ""
                        text: row.modelData.sub
                        font.family: Theme.sans; font.pixelSize: 15
                        color: Theme.muted
                    }
                }
                Text {
                    anchors.right: parent.right; anchors.rightMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    text: "chevron_right"
                    font.family: "Material Symbols Rounded"; font.pixelSize: 24
                    color: Theme.dim
                }
                TapHandler { id: tap; onTapped: root.open(row.modelData.app) }
            }
        }
    }
}
