// «Настройки» → Экраны (docs/SCREENS.md): на какие экраны выведен рабочий стол и какой из них главный. Вид выбран
// пользователем из десяти макетов — Т10 «Три положения» (21 ноября 2026): сверху выключатель стола (он же плитка
// «трансляция»), ниже у каждого экрана один переключатель — заставка, стол, главный. Главный — один: форма стола —
// его, остальные показывают ту же картинку с полями. Экраны — все, что телефон видел (naomiStream.screens): монитор
// приставки, окно Mac, планшет; не на связи — тускло, выбор для него можно поставить заранее, «убрать» — забыть.
import QtQuick
import Naomi.Ui

Item {
    id: root
    property bool active                // страница на экране
    property bool inUse                 // раздел открыт или открывается / закрывается (строки списка держатся)
    signal backRequested()

    readonly property var screens: naomiStream.screens
    readonly property int showing: screens.filter(s => s.state === "desk").length
    readonly property var mainNow: screens.find(s => s.mainNow && s.state !== "offline")
    readonly property string deskValue: !naomiStream.enabled ? "выкл"
                                        : showing === 1 ? "на 1 экране" : showing > 1 ? "на " + showing + " экранах"
                                        : screens.some(s => s.on) ? "ждёт экран" : "нет экранов"

    Rectangle { anchors.fill: parent; color: Theme.canvas }

    Flickable {
        anchors.fill: parent
        anchors.topMargin: Theme.statusBarHeight
        contentHeight: content.height + 80
        boundsBehavior: Flickable.DragOverBounds
        Column {
            id: content
            x: Theme.margin
            y: 34          // как у «Настроек»: ниже HUD разработчика
            width: root.width - 2 * Theme.margin
            spacing: 16
            Caps {
                text: "‹ настройки"
                font.pixelSize: 10
                TapHandler { margin: 12; onTapped: root.backRequested() }
            }
            Caps { text: "экраны"; color: Theme.amber; font.pixelSize: 12 }
            NToggle {
                width: parent.width
                label: "рабочий стол"
                value: root.deskValue
                on: naomiStream.enabled
                onClicked: naomiStream.enabled = !naomiStream.enabled
            }
            Repeater {
                model: root.inUse ? root.screens : []
                delegate: Column {
                    id: scr
                    required property var modelData
                    readonly property bool offline: modelData.state === "offline"
                    // не на связи, но включится сам, как только его поставят на «стол» или «главный» (телевизор)
                    readonly property bool standby: modelData.state === "standby"
                    width: content.width
                    spacing: 9
                    Item {
                        width: parent.width
                        height: 20
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - side.width - 12
                            elide: Text.ElideRight
                            text: scr.modelData.title
                            font.family: Theme.sans
                            font.pixelSize: 16
                            color: scr.modelData.state === "desk" ? Theme.amber : scr.offline ? Theme.dim : Theme.text
                        }
                        Caps {
                            id: side
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            font.pixelSize: 9
                            // «убрать» (забыть экран) — только у пропавшего; спящий телевизор — обычное его
                            // состояние: касание мимо переключателя стирало его из списка (22 ноября 2026)
                            text: scr.offline ? "нет связи · убрать"
                                              : scr.standby ? [scr.modelData.aspect, "спит"].filter(t => t !== "").join(" · ")
                                              : [scr.modelData.aspect, scr.modelData.via].filter(t => t !== "").join(" · ")
                            color: scr.offline ? Theme.bad : Theme.dim
                            TapHandler {
                                enabled: scr.offline
                                margin: 12
                                onTapped: naomiStream.forgetScreen(scr.modelData.id)
                            }
                        }
                    }
                    Rectangle {   // три положения: заставка — стол — главный
                        id: seg
                        width: parent.width
                        height: 42
                        radius: 3
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.surfaceHigh
                        opacity: scr.offline ? 0.45 : 1
                        Repeater {
                            model: ["заставка", "стол", "главный"]
                            delegate: Item {
                                id: cell
                                required property int index
                                required property string modelData
                                readonly property bool sel: scr.modelData.position === index
                                x: Math.round(index * seg.width / 3)
                                width: Math.round((index + 1) * seg.width / 3) - x
                                height: seg.height
                                Rectangle {
                                    visible: cell.sel
                                    x: 1; y: 1
                                    width: parent.width - 2; height: parent.height - 2
                                    color: Theme.amberFaint
                                }
                                Rectangle { visible: cell.sel; x: 1; y: parent.height - 3; width: parent.width - 2; height: 2; color: Theme.amber }
                                Rectangle { visible: cell.index > 0; width: 1; height: parent.height; color: Theme.surfaceHigh }
                                Caps {
                                    anchors.centerIn: parent
                                    text: cell.modelData
                                    font.pixelSize: 10
                                    color: cell.sel ? Theme.amber : Theme.dim
                                }
                                TapHandler { onTapped: naomiStream.setScreenPosition(scr.modelData.id, cell.index) }
                            }
                        }
                    }
                }
            }
            Caps {
                visible: root.screens.length === 0
                width: parent.width
                wrapMode: Text.WordWrap
                lineHeight: 1.5
                font.pixelSize: 10
                text: "экранов ещё не было: включите приставку, окно на mac или планшет — они появятся здесь"
            }
            Caps {
                visible: root.screens.length > 0
                width: parent.width
                elide: Text.ElideRight
                font.pixelSize: 10
                textFormat: Text.StyledText
                text: "форма стола · " + (root.mainNow ? '<font color="' + Theme.text + '">' + root.mainNow.aspect + "</font> · главный — "
                                                         + root.mainNow.title : "нет экрана")
            }
            Caps {
                visible: root.screens.length > 0
                width: parent.width
                wrapMode: Text.WordWrap
                lineHeight: 1.6
                font.pixelSize: 9
                text: "двойной тап на планшете, щелчок на mac или клавиша у малинки переносит стол на этот экран"
            }
        }
    }
}
