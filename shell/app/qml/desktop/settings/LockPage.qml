// «Настройки» → «Экран» → «Без дока»: что монитор приставки показывает без телефона (docs/PI.md, «Без дока») — часы на
// обоях или одни обои, и где они: часы на рисунке монитора тащат мышью (просьба пользователя 17 ноября 2026: «схватить
// часы… и двигать куда я захочу»), «На место» — слева сверху, как было; через сколько без дела стол уходит сам (ни мыши,
// ни клавиш, ни звука; просьба 19 ноября 2026: «выход из режима рабочего стола… 10, 30, час, никогда»; по умолчанию —
// никогда; считает телефон, StreamService::checkIdleExit); что с монитором без дела — сон (сигнала нет) или
// приглушение до доли яркости (просьба пользователя 19 ноября 2026: «чтобы можно было выбрать сон как щас и
// приглушение экрана»; яркость — по DDC/CI) — и через сколько (без клавиш; по умолчанию 30 минут), «Тест» — то же сразу:
// стол уходит, через 1,5 с монитор спит или тускнеет (просьба того же дня: «протестить, не ожидая 10 минут»); будит ли
// его движение мыши (стол по-прежнему возвращают клавиша или щелчок). Ведёт приставка (Screen, IdleTest).
import QtQuick
import ".."

Item {
    id: page
    property var sw
    readonly property var s: sw.screen
    implicitHeight: col.height

    component Seg: NButton { height: 30 }
    component Label: Text {
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 12 * 0.12
        color: DeskStyle.muted
    }
    component Line: Rectangle { height: 1; color: DeskStyle.amberA(DeskStyle.line) }

    Column {
        id: col
        width: page.width
        spacing: 16

        Item {
            width: col.width
            height: 30
            BackButton { id: back; text: "Экран"; onClicked: page.sw.page = "screen" }
            Head {
                x: back.width + 14; y: (30 - height) / 2
                width: col.width - x
                title: "БЕЗ ДОКА"
                info: (page.s.clock ? "часы" : "одни обои")
                      + (page.s.exitAfter > 0 ? " · выход через " + page.sw.minutesWord(page.s.exitAfter) : "")
                      + " · " + page.sw.sleepWord(page.s.sleep, page.s.dim)
            }
        }
        Row {
            spacing: 24
            Column {
                width: 300
                spacing: 10
                MonitorArt { width: 300; sw: page.sw; lock: true; draggable: true }
                Item {
                    width: 300
                    height: Math.max(placeHint.height, home.height)
                    Text {
                        id: placeHint
                        width: home.visible ? 300 - home.width - 10 : 300
                        wrapMode: Text.Wrap
                        lineHeight: 1.3
                        text: page.s.clock ? "часы можно перетащить мышью — так они встанут на мониторе"
                                           : "телефон ушёл — панель тает; вернулся — возвращается, кадр в кадр"
                        font.family: DeskStyle.mono
                        font.pixelSize: 12
                        color: DeskStyle.dim
                    }
                    Seg {
                        id: home
                        anchors.right: parent.right
                        visible: page.s.clock && (page.s.clockX !== 275 || page.s.clockY !== 378)
                        width: 100
                        text: "На место"
                        onClicked: { page.s.clockX = 275; page.s.clockY = 378 }
                    }
                }
            }
            Column {
                id: sets
                width: col.width - 324
                Item {
                    width: sets.width
                    height: 44
                    Label { height: parent.height; verticalAlignment: Text.AlignVCenter; text: "ЧАСЫ НА ОБОЯХ" }
                    Row {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        Seg { width: 80; text: "Выкл"; tone: page.s.clock ? "off" : "on"; onClicked: page.s.clock = false }
                        Seg { width: 80; text: "Вкл"; tone: page.s.clock ? "on" : "off"; onClicked: page.s.clock = true }
                    }
                }
                Line { width: sets.width }
                Column {   // стол без дела уходит сам
                    topPadding: 12; bottomPadding: 12
                    spacing: 9
                    Label { text: "ВЫХОД ИЗ СТОЛА ЧЕРЕЗ" }
                    Row {
                        spacing: 6
                        Repeater {
                            model: [[600, "10 мин"], [1800, "30 мин"], [3600, "1 час"], [0, "Никогда"]]
                            delegate: Seg {
                                required property var modelData
                                width: (sets.width - 18) / 4
                                text: modelData[1]
                                tone: page.s.exitAfter === modelData[0] ? "on" : "off"
                                onClicked: page.s.exitAfter = modelData[0]
                            }
                        }
                    }
                    Text {
                        width: sets.width
                        wrapMode: Text.Wrap
                        lineHeight: 1.3
                        text: page.s.exitAfter > 0
                              ? "ни мыши, ни клавиш, ни звука столько — стол закрывается, как «закрыть сессию». Играет музыка или видео — не закрывается"
                              : "стол сам не закрывается"
                        font.family: DeskStyle.mono
                        font.pixelSize: 12
                        color: DeskStyle.dim
                    }
                }
                Line { width: sets.width }
                Column {
                    topPadding: 12
                    spacing: 9
                    Item {   // «Тест» — сразу то, что будет без дела: стол уходит, монитор через 1,5 с спит или тускнеет
                        width: sets.width
                        height: 30
                        Label { height: parent.height; verticalAlignment: Text.AlignVCenter; text: "МОНИТОР БЕЗ ДЕЛА" }
                        Seg {
                            anchors.right: parent.right
                            width: 80
                            text: "Тест"
                            enabled: page.sw.docked && page.s.sleep !== 0
                            opacity: enabled ? 1 : 0.45
                            onClicked: naomiStream.idleTest()
                        }
                    }
                    Row {
                        spacing: 6
                        Seg {
                            width: (sets.width - 6) / 2; text: "Сон"
                            tone: page.s.dim ? "off" : "on"; onClicked: page.s.dim = false
                        }
                        Seg {
                            width: (sets.width - 6) / 2; text: "Приглушение"
                            tone: page.s.dim ? "on" : "off"; onClicked: page.s.dim = true
                        }
                    }
                }
                Column {
                    topPadding: 16; bottomPadding: 12
                    spacing: 9
                    Label { text: page.s.dim ? "МОНИТОР ТУСКНЕЕТ ЧЕРЕЗ" : "МОНИТОР СПИТ ЧЕРЕЗ" }
                    Row {
                        spacing: 6
                        Repeater {
                            model: [[600, "10 мин"], [1800, "30 мин"], [3600, "1 час"], [0, "Никогда"]]
                            delegate: Seg {
                                required property var modelData
                                width: (sets.width - 18) / 4
                                text: modelData[1]
                                tone: page.s.sleep === modelData[0] ? "on" : "off"
                                onClicked: page.s.sleep = modelData[0]
                            }
                        }
                    }
                }
                Item {   // до какой яркости — только у приглушения
                    visible: page.s.dim
                    width: sets.width
                    height: 28
                    opacity: page.s.sleep !== 0 ? 1 : 0.45
                    enabled: page.s.sleep !== 0
                    Label { height: parent.height; verticalAlignment: Text.AlignVCenter; text: "ЯРКОСТЬ" }
                    DeskSlider {
                        x: 84
                        anchors.verticalCenter: parent.verticalCenter
                        width: sets.width - x - 56
                        from: 0; to: 100; stepSize: 5
                        value: page.s.dimLevel
                        onMoved: (v) => page.s.dimLevel = v
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.s.dimLevel + " %"
                        font.family: DeskStyle.mono
                        font.pixelSize: 13
                        color: DeskStyle.text
                    }
                }
                Text {
                    visible: page.s.dim
                    width: sets.width
                    topPadding: 6; bottomPadding: 12
                    wrapMode: Text.Wrap
                    lineHeight: 1.3
                    text: page.sw.monitor.ddc === 0
                          ? "монитор не отвечает по DDC/CI — включите DDC/CI в его меню; пока вместо приглушения — сон"
                          : "до стольких % яркости монитора (он и так тусклее — не трогает); разбудили — прежняя яркость сразу"
                    font.family: DeskStyle.mono
                    font.pixelSize: 12
                    color: page.sw.monitor.ddc === 0 ? DeskStyle.amber : DeskStyle.dim
                }
                Line { width: sets.width }
                Column {
                    topPadding: 12; bottomPadding: 12
                    spacing: 9
                    Label { text: "БУДИТЬ МОНИТОР" }
                    Row {
                        spacing: 6
                        Seg {
                            width: (sets.width - 6) / 2; text: "Клавиша, щелчок"
                            tone: page.s.wakeMove ? "off" : "on"; onClicked: page.s.wakeMove = false
                        }
                        Seg {
                            width: (sets.width - 6) / 2; text: "И движение мыши"
                            tone: page.s.wakeMove ? "on" : "off"; onClicked: page.s.wakeMove = true
                        }
                    }
                }
                Text {
                    width: sets.width
                    wrapMode: Text.Wrap
                    lineHeight: 1.3
                    text: (page.s.wakeMove ? "движение мыши будит монитор, стол возвращают клавиша или щелчок"
                                           : "движение мыши не будит — задели стол, монитор "
                                             + (page.s.dim ? "тусклый" : "спит") + " дальше; будят клавиша и щелчок")
                          + (page.s.dim ? "" : ". После сна стол — через 2,5 с: монитору надо поймать сигнал")
                    font.family: DeskStyle.mono
                    font.pixelSize: 12
                    color: DeskStyle.muted
                }
            }
        }
    }
}
