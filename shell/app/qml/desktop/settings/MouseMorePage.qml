// «Настройки» → «Мышь» → «Подробно» (из варианта 9 макетов, в виде 10-го): 01 наборы «Точно, Обычно, Быстро» (тронули
// что-то — «Своё»); 02 прокрутка — кривая одного щелчка (путь страницы во времени: к «мягкости» — 95 %, e^−3, как
// считает приставка) и поле проверки колеса рядом, плавность, мягкость, строки, разгон; 03 указатель — скорость
// курсора от скорости руки (ускорение — та же кривая, что у приставки, pi/dock/input.cpp; пунктир — 1:1), скорость
// и ускорение. «‹ Мышь» — назад.
import QtQuick
import ".."

Item {
    id: page
    property var sw
    readonly property var cfg: sw.cfg
    implicitHeight: col.height

    component Seg: NButton { height: 30 }
    component Label: Text {
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 12 * 0.12
        color: DeskStyle.muted
    }
    component Meta: Text {
        wrapMode: Text.Wrap
        lineHeight: 1.3
        font.family: DeskStyle.mono
        font.pixelSize: 12
        color: DeskStyle.muted
    }
    component Value: Text {
        horizontalAlignment: Text.AlignRight
        font.family: DeskStyle.mono
        font.pixelSize: 13
        color: DeskStyle.text
    }
    // строка «подпись — управление — значение» с линией сверху
    component CRow: Item {
        id: crow
        property string label
        property bool first: false
        default property alias control: slot.data
        height: 40
        Rectangle { visible: !crow.first; width: parent.width; height: 1; color: DeskStyle.amberA(DeskStyle.line) }
        Label { height: crow.height; verticalAlignment: Text.AlignVCenter; text: crow.label.toUpperCase() }
        Item {
            id: slot
            x: 126
            width: crow.width - x
            height: crow.height
        }
    }

    Column {
        id: col
        width: page.width
        spacing: 16

        // ---------- назад и шильдик ----------
        Item {
            width: col.width
            height: 30
            BackButton { id: back; text: "Мышь"; onClicked: page.sw.page = "mouse" }
            Plate {
                x: back.width + 14
                y: (30 - height) / 2
                width: col.width - x
                title: "ПОДРОБНО"
            }
        }

        // ---------- 01 наборы ----------
        Column {
            width: col.width
            spacing: 14
            Head { width: col.width; num: "01"; title: "НАБОРЫ" }
            Row {
                id: presets
                spacing: 7
                readonly property real tileW: (col.width - 3 * spacing) / 4
                Repeater {
                    model: page.sw.presets.concat([{ id: "svoe", name: "Своё", lines3: ["как настроите", "ползунками", "ниже"] }])
                    delegate: Item {
                        id: tile
                        required property var modelData
                        readonly property bool on: page.sw.preset === modelData.id
                        readonly property real dy: tileArea.pressed ? 1 : 0
                        width: presets.tileW
                        height: 86
                        Rectangle {
                            y: tile.dy
                            width: tile.width; height: tile.height
                            color: tile.on ? DeskStyle.amberA(0.09) : tileArea.containsMouse ? DeskStyle.amberA(DeskStyle.itemHover) : "transparent"
                            border.width: 1
                            border.color: DeskStyle.amberA(DeskStyle.line)
                        }
                        Rectangle {
                            y: tile.dy
                            width: 3; height: tile.height
                            color: tile.on ? DeskStyle.amber : Qt.rgba(125 / 255, 120 / 255, 110 / 255, 0.55)
                        }
                        Text {
                            x: 15; y: 12 + tile.dy
                            text: tile.modelData.name.toUpperCase()
                            font.family: DeskStyle.mono
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            font.letterSpacing: 12 * 0.14
                            color: tile.on ? DeskStyle.amber : DeskStyle.muted
                        }
                        Column {
                            x: 15; y: 30 + tile.dy
                            Repeater {
                                model: tile.modelData.lines3 || page.sw.presetLines(tile.modelData)
                                delegate: Text {
                                    required property string modelData
                                    text: modelData
                                    font.family: DeskStyle.mono
                                    font.pixelSize: 12
                                    color: DeskStyle.dim
                                }
                            }
                        }
                        MouseArea {
                            id: tileArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: page.sw.applyPreset(tile.modelData.id)
                        }
                    }
                }
            }
        }

        // ---------- 02 прокрутка ----------
        Column {
            width: col.width
            spacing: 14
            Head {
                width: col.width
                num: "02"; title: "ПРОКРУТКА"
                info: "щелчок → " + page.cfg.lines + " " + page.sw.linesWord(page.cfg.lines)
                       + (page.cfg.smooth ? ", " + page.sw.softWord(page.cfg.soft) + " " + page.cfg.soft + " мс" : ", сразу")
            }
            Row {
                spacing: 22
                Column {
                    width: col.width - 232 - 22
                    spacing: 8
                    Chart {
                        width: parent.width
                        height: 116
                        readonly property real soft: page.cfg.soft
                        line: {
                            if (!page.cfg.smooth)
                                return [[0, 0], [0, 1], [1, 1]]
                            const out = []
                            for (let t = 0; t <= 400; t += 4)
                                out.push([t / 400, 1 - Math.exp(-3 * t / soft)])
                            return out
                        }
                        mark: page.cfg.smooth ? { u: Math.min(soft, 400) / 400, v: 1 - Math.exp(-3), text: soft + " мс" } : null
                        corner: page.cfg.smooth ? "" : "сразу"
                        labels: [{ u: 0, text: "0" }, { u: 0.25, text: "100" }, { u: 0.5, text: "200" }, { u: 0.75, text: "300" },
                                 { u: 1, text: "400 мс" }]
                    }
                    Meta {
                        width: parent.width
                        text: "<font color=\"" + DeskStyle.text + "\">" + (page.cfg.smooth
                              ? page.sw.softWord(page.cfg.soft) + ": доезжает за " + page.cfg.soft + " мс"
                              : "выкл: страница прыгает сразу") + "</font><br>по высоте — путь страницы, по ширине — время"
                    }
                }
                Column {
                    width: 232
                    spacing: 8
                    WheelTest { width: parent.width; height: 150 }
                    Meta {
                        width: parent.width
                        text: "<font color=\"" + DeskStyle.text + "\">проверка</font><br>крутите колесо над списком"
                    }
                }
            }
            Grid {
                columns: 2
                columnSpacing: 26
                readonly property real cellW: (col.width - columnSpacing) / 2
                CRow {
                    width: parent.cellW
                    label: "Плавно"
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        Seg { width: (parent.parent.width - 6) / 2; text: "Выкл"; tone: page.cfg.smooth ? "off" : "on"; onClicked: page.cfg.smooth = false }
                        Seg { width: (parent.parent.width - 6) / 2; text: "Вкл"; tone: page.cfg.smooth ? "on" : "off"; onClicked: page.cfg.smooth = true }
                    }
                }
                CRow {
                    width: parent.cellW
                    label: "Мягкость"
                    opacity: page.cfg.smooth ? 1 : 0.45
                    enabled: page.cfg.smooth
                    Behavior on opacity { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    DeskSlider {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 74
                        from: 60; to: 400; stepSize: 10
                        value: page.cfg.soft
                        onMoved: (v) => page.cfg.soft = v
                    }
                    Value { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: page.cfg.soft + " мс" }
                }
                CRow {
                    width: parent.cellW
                    label: "За щелчок"
                    DeskSlider {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 74
                        from: 1; to: 10; stepSize: 1
                        value: page.cfg.lines
                        onMoved: (v) => page.cfg.lines = v
                    }
                    Value { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: page.cfg.lines + " стр" }
                }
                CRow {
                    width: parent.cellW
                    label: "Разгон колеса"
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6
                        Seg { width: (parent.parent.width - 6) / 2; text: "Выкл"; tone: page.cfg.wheelAccel ? "off" : "on"; onClicked: page.cfg.wheelAccel = false }
                        Seg { width: (parent.parent.width - 6) / 2; text: "Вкл"; tone: page.cfg.wheelAccel ? "on" : "off"; onClicked: page.cfg.wheelAccel = true }
                    }
                }
            }
        }

        // ---------- 03 указатель ----------
        Column {
            width: col.width
            spacing: 14
            Head {
                width: col.width
                num: "03"; title: "УКАЗАТЕЛЬ"
                info: page.cfg.speed + " %, " + page.sw.accelName(page.cfg.accel)
            }
            Row {
                spacing: 22
                Column {
                    width: col.width - 290 - 22
                    spacing: 8
                    Chart {
                        width: parent.width
                        height: 116
                        // рука — 0…4 отсчёта мыши в мс (ускорение растёт с 0,3 до 3), курсор — рука × ускорение; в поле —
                        // до 16 (сильное на краю). Скорость указателя множит обе линии — форму не меняет, она — в числе
                        line: {
                            const out = []
                            for (let i = 0; i <= 80; ++i) {
                                const v = 4 * i / 80
                                out.push([i / 80, page.sw.gain(v, page.cfg.accel) * v / 16])
                            }
                            return out
                        }
                        ref: [[0, 0], [1, 4 / 16]]
                        readonly property real end: page.sw.gain(4, page.cfg.accel)
                        mark: ({ u: 1, v: end * 4 / 16,
                                 text: "×" + (end * page.sw.speedFactor(page.cfg.speed)).toFixed(1).replace(".", ",") })
                        labels: [{ u: 0, text: "медленно" }, { u: 0.5, text: "рука" }, { u: 1, text: "быстро" }]
                    }
                    Meta {
                        width: parent.width
                        text: "<font color=\"" + DeskStyle.text + "\">" + page.sw.accelWord(page.cfg.accel)
                              + "</font><br>по ширине — рука, по высоте — курсор; пунктир — ровно за рукой"
                    }
                }
                Column {
                    width: 290
                    spacing: 12
                    CRow {
                        first: true
                        width: parent.width
                        label: "Скорость"
                        DeskSlider {
                            x: -26
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 48 + 26
                            from: 0; to: 100; stepSize: 1
                            value: page.cfg.speed
                            onMoved: (v) => page.cfg.speed = v
                        }
                        Value { anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter; text: page.cfg.speed + " %" }
                    }
                    SubPlate { width: parent.width; text: "Ускорение" }
                    Row {
                        spacing: 6
                        Seg { width: (290 - 12) / 3; text: "Выкл"; tone: page.cfg.accel === 0 ? "on" : "off"; onClicked: page.cfg.accel = 0 }
                        Seg { width: (290 - 12) / 3; text: "Мягкое"; tone: page.cfg.accel === 1 ? "on" : "off"; onClicked: page.cfg.accel = 1 }
                        Seg { width: (290 - 12) / 3; text: "Сильное"; tone: page.cfg.accel === 2 ? "on" : "off"; onClicked: page.cfg.accel = 2 }
                    }
                }
            }
        }
    }
}
