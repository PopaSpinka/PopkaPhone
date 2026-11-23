// «Настройки» → «Экран» (макет, выбранный пользователем 17 ноября 2026): 01 размер — сколько точек на столе (крупнее
// или больше места), рядом монитор приставки и окно «Файлов» в настоящих долях; 02 резкость — сколько пикселей рисует
// телефон (доля ширины монитора), лупа и замеры 15 ноября; 03 монитор — частота из его режимов, разрешение — родное.
// Ведёт приставка (сообщение Screen): размер стола — новым DeskSize (масштаб сменился — браузер перезапустится),
// частоту — сменой режима монитора.
import QtQuick
import ".."

Item {
    id: page
    property var sw
    readonly property var s: sw.screen
    readonly property int deskW: s.deskWidth
    readonly property var frame: sw.frameSize(deskW, s.sharp)
    readonly property var meas: sw.measured[frame[0]]
    readonly property int hzOn: sw.rates.reduce((a, b) => Math.abs(b - s.hz) < Math.abs(a - s.hz) ? b : a, sw.rates[0])
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
    component Line: Rectangle { height: 1; color: DeskStyle.amberA(DeskStyle.line) }

    // ширина стола: резкость, при которой точка меньше пикселя кадра (или больше двух), — ближайшая годная
    function setWidth(w) {
        s.deskWidth = w
        if (!sw.sharpOk(w, s.sharp)) {
            let best = -1
            for (let i = 0; i < 4; ++i)
                if (sw.sharpOk(w, i) && (best < 0 || Math.abs(i - s.sharp) < Math.abs(best - s.sharp)))
                    best = i
            if (best >= 0)
                s.sharp = best
        }
    }

    Column {
        id: col
        width: page.width
        spacing: 20

        // ---------- 01 размер ----------
        Column {
            width: col.width
            spacing: 14
            Head {
                width: col.width; num: "01"; title: "РАЗМЕР"
                info: page.deskW + " × " + page.sw.deskH(page.deskW) + " · как " + page.sw.pct(page.deskW) + " %"
            }
            Row {
                spacing: 24
                MonitorArt { width: 300; sw: page.sw }
                Column {
                    id: sizeCol
                    width: col.width - 324
                    spacing: 12
                    Row {
                        spacing: 8
                        Text {
                            id: big
                            text: page.deskW + " × " + page.sw.deskH(page.deskW)
                            font.family: DeskStyle.sans
                            font.pixelSize: 40
                            font.weight: Font.ExtraLight
                            color: DeskStyle.text
                        }
                        Text {
                            y: big.baselineOffset - baselineOffset
                            text: "точек"
                            font.family: DeskStyle.sans
                            font.pixelSize: 12
                            color: DeskStyle.muted
                        }
                    }
                    Meta {
                        width: sizeCol.width
                        text: "точка = " + page.sw.comma(page.sw.monitor.w / page.deskW, 2) + " пикселя монитора, как "
                              + page.sw.pct(page.deskW) + " %\n«Файлы» 1000 × 600 занимают "
                              + Math.round(600000 / (page.deskW * page.sw.deskH(page.deskW)) * 100) + " % стола"
                    }
                    Item {   // крупнее ——— больше места
                        width: sizeCol.width
                        height: 16
                        Label { id: bigger; text: "КРУПНЕЕ"; height: parent.height; verticalAlignment: Text.AlignVCenter }
                        Label { id: more; anchors.right: parent.right; text: "БОЛЬШЕ МЕСТА"; height: parent.height; verticalAlignment: Text.AlignVCenter }
                        Line { x: bigger.width + 10; y: 8; width: more.x - 10 - x }
                    }
                    Item {   // засечки
                        id: notches
                        width: sizeCol.width
                        height: 62
                        readonly property real cell: width / page.sw.sizes.length
                        Line { x: notches.cell / 2; y: 22; width: notches.width - notches.cell }
                        Repeater {
                            model: page.sw.sizes
                            delegate: Item {
                                id: nt
                                required property int modelData
                                required property int index
                                readonly property bool on: page.deskW === modelData
                                readonly property bool hovered: ntArea.containsMouse
                                x: index * notches.cell
                                width: notches.cell
                                height: notches.height
                                Text {
                                    visible: nt.modelData === 1600
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "ОБЫЧНО"
                                    font.family: DeskStyle.mono
                                    font.pixelSize: 10
                                    font.letterSpacing: 10 * 0.12
                                    color: DeskStyle.dim
                                }
                                Rectangle {
                                    x: (nt.width - 10) / 2; y: 14
                                    width: 10; height: 16
                                    radius: 2
                                    color: nt.on ? DeskStyle.amberA(0.22) : "#1b1b19"
                                    border.width: 1
                                    border.color: nt.on ? DeskStyle.amber : nt.hovered ? DeskStyle.muted
                                                                                      : Qt.rgba(125 / 255, 120 / 255, 110 / 255, 0.55)
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    y: 36
                                    text: nt.modelData
                                    font.family: DeskStyle.mono
                                    font.pixelSize: 12
                                    color: nt.on ? DeskStyle.amber : nt.hovered ? DeskStyle.text : DeskStyle.muted
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    y: 51
                                    text: page.sw.pct(nt.modelData) + " %"
                                    font.family: DeskStyle.mono
                                    font.pixelSize: 11
                                    color: nt.on ? DeskStyle.amberA(0.75) : DeskStyle.dim
                                }
                                MouseArea {
                                    id: ntArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: page.setWidth(nt.modelData)
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---------- 02 резкость ----------
        Column {
            width: col.width
            spacing: 14
            Head {
                width: col.width; num: "02"; title: "РЕЗКОСТЬ"
                info: "кадр " + page.frame[0] + " × " + page.frame[1] + " · ×"
                      + page.sw.comma(page.sw.monitor.w / page.frame[0], 2) + " до монитора"
            }
            Row {
                spacing: 24
                Column {
                    width: 300
                    spacing: 10
                    Lupa {
                        width: 300; height: 150
                        frameK: page.frame[0] / page.deskW
                        monK: page.sw.monitor.w / page.deskW
                    }
                    Meta {
                        width: 300; color: DeskStyle.dim
                        text: "лупа ×3, примерно; цифры — замер 15 ноября: стол 1600 точек, 75 Гц"
                    }
                }
                Column {
                    id: sharpCol
                    width: col.width - 324
                    spacing: 12
                    Grid {
                        columns: 2
                        spacing: 7
                        Repeater {
                            model: 4
                            delegate: Item {
                                id: tile
                                required property int index
                                readonly property bool ok: page.sw.sharpOk(page.deskW, index)
                                readonly property bool on: page.s.sharp === index
                                readonly property real dy: tileArea.pressed && ok ? 1 : 0
                                width: (sharpCol.width - 7) / 2
                                height: 48
                                opacity: ok ? 1 : 0.4
                                Rectangle {
                                    y: tile.dy
                                    width: tile.width; height: tile.height
                                    color: tile.on ? DeskStyle.amberA(0.09) : tileArea.containsMouse && tile.ok ? DeskStyle.amberA(DeskStyle.itemHover) : "transparent"
                                    border.width: 1
                                    border.color: DeskStyle.amberA(DeskStyle.line)
                                }
                                Rectangle {
                                    y: tile.dy
                                    width: 3; height: tile.height
                                    color: tile.on ? DeskStyle.amber : Qt.rgba(125 / 255, 120 / 255, 110 / 255, 0.55)
                                }
                                Text {
                                    x: 14; y: 9 + tile.dy
                                    text: page.sw.sharpNames[tile.index].toUpperCase()
                                    font.family: DeskStyle.mono
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    font.letterSpacing: 12 * 0.14
                                    color: tile.on ? DeskStyle.amber : DeskStyle.muted
                                }
                                Text {
                                    x: 14; y: 27 + tile.dy
                                    text: tile.ok ? "кадр " + page.sw.frameOf(tile.index) + " · ×"
                                                    + page.sw.comma(page.sw.monitor.w / page.sw.frameOf(tile.index), 2)
                                                  : "не для этого размера"
                                    font.family: DeskStyle.mono
                                    font.pixelSize: 12
                                    color: DeskStyle.dim
                                }
                                MouseArea {
                                    id: tileArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: if (tile.ok) page.s.sharp = tile.index
                                }
                            }
                        }
                    }
                    Column {
                        width: sharpCol.width
                        Repeater {
                            model: [["Листание браузера", page.meas ? "новый кадр: " + page.meas.scroll + " %" : "не мерено"],
                                    ["Сжатие кадра", page.meas ? page.meas.enc + " мс" : "не мерено"],
                                    ["До экрана, медиана", page.meas ? page.meas.lat + " мс" : "не мерено"]]
                            delegate: Item {
                                required property var modelData
                                required property int index
                                width: sharpCol.width
                                height: 28
                                Line { visible: index > 0; width: parent.width }
                                Label { height: parent.height; verticalAlignment: Text.AlignVCenter; text: modelData[0].toUpperCase() }
                                Text {
                                    anchors.right: parent.right
                                    height: parent.height
                                    verticalAlignment: Text.AlignVCenter
                                    text: modelData[1]
                                    font.family: DeskStyle.mono
                                    font.pixelSize: 13
                                    color: DeskStyle.text
                                }
                            }
                        }
                    }
                }
            }
        }

        // ---------- 03 монитор ----------
        Column {
            width: col.width
            spacing: 6
            Head {
                width: col.width; num: "03"; title: "МОНИТОР"
                info: page.sw.monitorKnown ? (page.sw.monitor.name || "монитор") + " · " + page.sw.monitor.port
                                           : "приставка ещё не подключалась"
            }
            Item {
                width: col.width
                height: 48
                Label { height: parent.height; verticalAlignment: Text.AlignVCenter; text: "ЧАСТОТА" }
                Row {
                    x: 160
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    Repeater {
                        model: page.sw.rates
                        delegate: Seg {
                            required property int modelData
                            width: 104
                            text: modelData + " Гц"
                            tone: page.hzOn === modelData ? "on" : "off"
                            onClicked: page.s.hz = modelData
                        }
                    }
                }
            }
            Meta { x: 160; width: col.width - 160; text: page.sw.hzWord(page.hzOn) }
            Item { width: 1; height: 6 }
            Line { width: col.width }
            Item {
                width: col.width
                height: 40
                Label { height: parent.height; verticalAlignment: Text.AlignVCenter; text: "РАЗРЕШЕНИЕ" }
                Text {
                    x: 160
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    textFormat: Text.StyledText
                    text: "<font color=\"" + DeskStyle.text + "\">" + page.sw.monitor.w + " × " + page.sw.monitor.h
                          + "</font> — родное, всегда: кадр растягивает приставка"
                    font.family: DeskStyle.mono
                    font.pixelSize: 12
                    color: DeskStyle.muted
                }
            }
            Line { width: col.width }
            Item { width: 1; height: 4 }
            Meta {
                width: col.width
                color: DeskStyle.dim
                text: (page.sw.docked ? "меняется сразу" : "приставка не подключена — поменяется, когда подключится")
                      + "; размер и резкость перезапускают браузер стола, вкладки вернутся. Окно Mac — своим размером"
            }
        }
    }
}
