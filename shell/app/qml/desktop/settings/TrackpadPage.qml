// «Настройки» → «Трекпад» (выбор пользователя 22 ноября 2026 — вариант 04 «Карточки» из десяти макетов): каждая
// возможность трекпада — своя карточка: рисунок трекпада с жестом (под мышью жест движется, иначе стоит кадром),
// название и одно управление. Внизу «Проверка»: слева настоящие пальцы на трекпаде приставки и что распознано
// (naomiStream.padTouch — приставка шлёт, пока раздел открыт), справа поле — мишени, плашка, список.
// Значения — sw.touch (trackpad.conf стола); курсор, щелчки, прокрутку и жесты по ним делает приставка (сообщение
// Pad; docs/PI.md, «Трекпад как на Mac»). По умолчанию — как трекпад на Mac пользователя.
import QtQuick
import ".."
import "touch.js" as Touch

Item {
    id: page
    property var sw
    readonly property var cfg: sw.touch
    implicitHeight: col.height
    readonly property real cardW: Math.floor((width - 2 * 10) / 3)
    readonly property int swipeFingers: cfg.drag === 1 ? 4 : 3   // три пальца заняты перетаскиванием — смахивают четырьмя
    readonly property string swipeWord: swipeFingers === 4 ? "четырьмя пальцами" : "тремя пальцами"
    readonly property var artCfg: ({ swipeFingers: swipeFingers, palm: cfg.palm, jitter: cfg.jitter, right: cfg.right, drag: cfg.drag })
    // часы примеров: идут, пока мышь над карточкой
    property real clock: 0
    property int hovering: 0
    FrameAnimation {
        running: page.hovering > 0 && page.visible
        onTriggered: page.clock += frameTime
    }
    readonly property var onOff: [{ v: false, name: "Выкл" }, { v: true, name: "Вкл" }]
    function reset() {   // как трекпад на Mac пользователя
        cfg.speed = 50; cfg.accel = 1; cfg.tap = true; cfg.right = 1; cfg.drag = 1; cfg.hold = 700
        cfg.natural = true; cfg.sspeed = 50; cfg.inertia = 1; cfg.pinch = true; cfg.jitter = 1
        cfg.palm = true; cfg.typing = true; cfg.pin = true; cfg.swSide = 1; cfg.swUp = 1; cfg.swDown = 0
    }

    component Card: Rectangle {
        id: card
        property string title
        property string note
        property string demo
        default property alias control: slot.data
        width: page.cardW
        height: 12 + art.height + 10 + 36 + 8 + 30 + 12
        color: DeskStyle.surface
        border.width: 1
        border.color: DeskStyle.amberA(hov.containsMouse ? 0.36 : DeskStyle.line)
        MouseArea {
            id: hov
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            onContainsMouseChanged: page.hovering += containsMouse ? 1 : -1
            Component.onDestruction: if (containsMouse) --page.hovering
            TouchArt {
                id: art
                x: 12; y: 12
                width: card.width - 24
                demo: card.demo
                t: hov.containsMouse ? page.clock : Touch.still(card.demo)
                cfg: page.artCfg
            }
            Text {
                x: 12; y: art.y + art.height + 10
                width: card.width - 24
                elide: Text.ElideRight
                text: card.title
                font.family: DeskStyle.sans
                font.pixelSize: 15
                color: DeskStyle.text
            }
            Text {
                x: 12; y: art.y + art.height + 10 + 20
                width: card.width - 24
                elide: Text.ElideRight
                text: card.note
                font.family: DeskStyle.mono
                font.pixelSize: 12
                color: DeskStyle.dim
            }
            Item {
                id: slot
                x: 12; y: card.height - 12 - 30
                width: card.width - 24
                height: 30
            }
        }
    }
    component Pick: Choice { width: parent.width }
    component Flip: Row {   // «Выкл · Вкл»
        id: flip
        property bool on
        signal moved(bool on)
        spacing: 6
        NButton { width: (flip.parent.width - 6) / 2; height: 30; centered: true; text: "Выкл"; tone: flip.on ? "off" : "on"; onClicked: flip.moved(false) }
        NButton { width: (flip.parent.width - 6) / 2; height: 30; centered: true; text: "Вкл"; tone: flip.on ? "on" : "off"; onClicked: flip.moved(true) }
    }
    component Step: Stepper { anchors.horizontalCenter: parent.horizontalCenter }

    Column {
        id: col
        width: page.width
        spacing: 16

        Head { width: col.width; num: "01"; title: "ТРЕКПАД"; info: "наведите мышь на карточку — трекпад покажет жест" }
        Grid {
            columns: 3
            spacing: 10
            Card {
                title: "Указатель"; note: "скорость курсора"; demo: "move"
                Step { value: page.cfg.speed; step: 5; unit: "%"; onMoved: (v) => page.cfg.speed = v }
            }
            Card {
                title: "Ускорение"; note: "быстрый ход уносит дальше"; demo: "accel"
                Pick {
                    options: [{ v: 0, name: "Выкл" }, { v: 1, name: "Как на Mac" }, { v: 2, name: "Сильное" }]
                    value: page.cfg.accel; onMoved: (v) => page.cfg.accel = v
                }
            }
            Card {
                title: "Касание — щелчок"; note: "касание вместо нажатия"; demo: "tap"
                Flip { on: page.cfg.tap; onMoved: (v) => page.cfg.tap = v }
            }
            Card {
                title: "Правый щелчок"; note: "касанием или нажатием"; demo: "right"
                Pick {
                    options: [{ v: 1, name: "Два пальца" }, { v: 2, name: "Правый угол" }, { v: 0, name: "Выкл" }]
                    value: page.cfg.right; onMoved: (v) => page.cfg.right = v
                }
            }
            Card {
                title: "Перетаскивание"; note: "окна, файлы, выделение"; demo: "drag"
                Pick {
                    options: [{ v: 1, name: "Три пальца" }, { v: 2, name: "Два касания" }, { v: 0, name: "Нажатием" }]
                    value: page.cfg.drag; onMoved: (v) => page.cfg.drag = v
                }
            }
            Card {
                title: "Держать после снятия"; note: "три пальца: успеть вернуть"; demo: "hold"
                opacity: page.cfg.drag === 1 ? 1 : 0.45
                enabled: page.cfg.drag === 1
                Step { value: page.cfg.hold; from: 0; to: 1500; step: 100; unit: "мс"; onMoved: (v) => page.cfg.hold = v }
            }
            Card {
                title: "Прокрутка"; note: "куда едет страница"; demo: "scroll"
                Pick {
                    options: [{ v: true, name: "За пальцами" }, { v: false, name: "Как колесо" }]
                    value: page.cfg.natural; onMoved: (v) => page.cfg.natural = v
                }
            }
            Card {
                title: "Скорость прокрутки"; note: "двумя пальцами"; demo: "scroll"
                Step { value: page.cfg.sspeed; step: 5; unit: "%"; onMoved: (v) => page.cfg.sspeed = v }
            }
            Card {
                title: "Инерция"; note: "страница катится после броска"; demo: "inertia"
                Pick {
                    options: [{ v: 0, name: "Выкл" }, { v: 1, name: "Обычная" }, { v: 2, name: "Длинная" }]
                    value: page.cfg.inertia; onMoved: (v) => page.cfg.inertia = v
                }
            }
            Card {
                title: "Щипок — масштаб"; note: "страница в браузере"; demo: "pinch"
                Flip { on: page.cfg.pinch; onMoved: (v) => page.cfg.pinch = v }
            }
            Card {
                title: "Смахнуть вбок"; note: page.swipeWord; demo: "swipeSide"
                Pick {
                    options: [{ v: 1, name: "Назад / вперёд" }, { v: 2, name: "Вкладки" }, { v: 0, name: "Ничего" }]
                    value: page.cfg.swSide; onMoved: (v) => page.cfg.swSide = v
                }
            }
            Card {
                title: "Смахнуть вверх"; note: page.swipeWord; demo: "swipeUp"
                Pick {
                    options: [{ v: 1, name: "Меню N" }, { v: 0, name: "Ничего" }]
                    value: page.cfg.swUp; onMoved: (v) => page.cfg.swUp = v
                }
            }
            Card {
                title: "Смахнуть вниз"; note: page.swipeWord; demo: "swipeDown"
                Pick {
                    options: [{ v: 0, name: "Ничего" }, { v: 1, name: "Меню N" }]
                    value: page.cfg.swDown; onMoved: (v) => page.cfg.swDown = v
                }
            }
            Card {
                title: "Дрожь пальца"; note: "насколько гасить"; demo: "jitter"
                Pick {
                    options: [{ v: 0, name: "Слабо" }, { v: 1, name: "Средне" }, { v: 2, name: "Сильно" }]
                    value: page.cfg.jitter; onMoved: (v) => page.cfg.jitter = v
                }
            }
            Card {
                title: "Лежащий палец, ладонь"; note: "большой палец снизу"; demo: "rest"
                Pick {
                    options: [{ v: true, name: "Не считать" }, { v: false, name: "Считать" }]
                    value: page.cfg.palm; onMoved: (v) => page.cfg.palm = v
                }
            }
            Card {
                title: "Пауза при печати"; note: "касания под клавиши не идут"; demo: "typing"
                Flip { on: page.cfg.typing; onMoved: (v) => page.cfg.typing = v }
            }
            Card {
                title: "Замирать при щелчке"; note: "курсор не съезжает от нажатия"; demo: "pin"
                Flip { on: page.cfg.pin; onMoved: (v) => page.cfg.pin = v }
            }
            Card {
                title: "Как на Mac"; note: "вернуть всё, как было"; demo: "move"
                NButton { width: parent.width; height: 30; centered: true; text: "Сбросить"; onClicked: page.reset() }
            }
        }

        // ---------- проверка ----------
        Head { width: col.width; num: "02"; title: "ПРОВЕРКА"; info: live.what }
        Row {
            spacing: 22
            Column {
                id: live
                width: 200
                spacing: 8
                readonly property var touch: naomiStream.padTouch
                readonly property var fingers: touch && touch.fingers ? touch.fingers : []
                readonly property int counted: fingers.filter(f => !f.off).length
                readonly property var modes: ["", "курсор", "два пальца", "прокрутка", "щипок", "три пальца", "перетаскивание",
                                              "пальцы сняты, кнопка держится", "смахивание", "жест закончен"]
                readonly property string what: {
                    if (!touch || touch.mode === undefined)
                        return "положите пальцы на трекпад"
                    const m = modes[touch.mode] || ""
                    const b = touch.buttons === 1 ? "левая нажата" : touch.buttons === 2 ? "правая нажата" : ""
                    const n = fingers.length ? "пальцев " + counted + (fingers.length > counted ? " + лежит " + (fingers.length - counted) : "") : ""
                    return [n, m, b].filter(s => s !== "").join(" · ") || "положите пальцы на трекпад"
                }
                TouchArt { width: parent.width; live: live.fingers }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    lineHeight: 1.3
                    text: "<font color=\"" + DeskStyle.text + "\">ваши пальцы</font><br>пустой кружок — палец не считается"
                    font.family: DeskStyle.mono
                    font.pixelSize: 12
                    color: DeskStyle.muted
                }
            }
            Rectangle {   // поле: мишени, плашка, список
                id: field
                width: col.width - 200 - 22
                height: 190
                color: DeskStyle.surface
                border.width: 1
                border.color: DeskStyle.amberA(DeskStyle.line)
                radius: 2
                clip: true
                property int hits: 0
                property string said: "щёлкните по кружку, перетащите плашку, прокрутите список"
                readonly property real areaW: width - list.width
                MouseArea {   // мимо мишеней: правый щелчок виден и здесь
                    width: field.areaW; height: field.height - 26
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    onClicked: (m) => field.said = m.button === Qt.RightButton ? "правый щелчок" : "левый щелчок — мимо мишени"
                    onDoubleClicked: field.said = "двойной щелчок — мимо мишени"
                }
                Repeater {
                    model: [[24, 22], [118, 40], [208, 16]]
                    delegate: Rectangle {
                        id: target
                        required property var modelData
                        x: modelData[0]; y: modelData[1]
                        width: 34; height: 34; radius: 17
                        color: flash.running ? DeskStyle.amberA(0.36) : "transparent"
                        border.width: 1.5
                        border.color: DeskStyle.amberA(tArea.containsMouse ? 1 : 0.36)
                        Rectangle { anchors.centerIn: parent; width: 6; height: 6; radius: 3; color: DeskStyle.amber }
                        Timer { id: flash; interval: 180 }
                        MouseArea {
                            id: tArea
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: (m) => {
                                flash.restart()
                                if (m.button === Qt.RightButton) {
                                    field.said = "правый щелчок — в мишень"
                                } else {
                                    ++field.hits
                                    field.said = "левый щелчок · попаданий " + field.hits
                                }
                            }
                            onDoubleClicked: field.said = "двойной щелчок — в мишень"
                        }
                    }
                }
                Rectangle {   // плашка
                    id: tile
                    x: 24; y: 96
                    width: 112; height: 40
                    color: tileArea.pressed ? DeskStyle.amberA(0.12) : DeskStyle.bg
                    border.width: 1
                    border.color: DeskStyle.amberA(0.36)
                    Rectangle { width: 3; height: parent.height; color: DeskStyle.amber }
                    Text {
                        anchors.centerIn: parent
                        text: "ПЕРЕТАЩИТЕ"
                        font.family: DeskStyle.mono
                        font.pixelSize: 11
                        font.letterSpacing: 11 * 0.1
                        color: DeskStyle.amber
                    }
                    MouseArea {
                        id: tileArea
                        anchors.fill: parent
                        drag.target: tile
                        drag.threshold: 0
                        drag.minimumX: 0; drag.maximumX: field.areaW - tile.width
                        drag.minimumY: 0; drag.maximumY: field.height - 26 - tile.height
                        onPressed: field.said = "плашка взята"
                        onPositionChanged: if (pressed) field.said = "перетаскивание: " + Math.round(tile.x) + ", " + Math.round(tile.y)
                        onReleased: field.said = "отпущено: " + Math.round(tile.x) + ", " + Math.round(tile.y)
                    }
                }
                WheelTest {
                    id: list
                    x: field.width - width
                    width: 170
                    height: field.height - 26
                    onPosChanged: field.said = "прокрутка: строка " + (Math.round(pos / rowH) + 1)
                }
                Rectangle {   // что произошло
                    y: field.height - 26
                    width: field.width; height: 26
                    color: DeskStyle.bg
                    Rectangle { width: parent.width; height: 1; color: DeskStyle.amberA(DeskStyle.line) }
                    Text {
                        x: 10
                        width: parent.width - 20
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                        text: field.said
                        font.family: DeskStyle.mono
                        font.pixelSize: 12
                        color: DeskStyle.muted
                    }
                }
            }
        }
        Text {
            width: col.width
            wrapMode: Text.Wrap
            lineHeight: 1.3
            text: "трекпад приставки; в окне Mac — как в macOS"
            font.family: DeskStyle.mono
            font.pixelSize: 12
            color: DeskStyle.dim
        }
    }
}
