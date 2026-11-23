// «Настройки» → «Экран» → «Обои»: собрание обоев стола (naomiFiles.wallpapers — ~/.config/naomi/desktop/wallpapers),
// «Тёмные» — без картинки, «Из «Файлов»…» — открыть «Файлы» в «Изображениях»: картинка там правой кнопкой → «Сделать
// фоном рабочего стола» ляжет в собрание. Слева — монитор приставки с выбранными обоями; без дока у неё — те же.
import QtQuick
import ".."

Item {
    id: page
    property var sw
    implicitHeight: col.height

    readonly property string current: naomiFiles.wallpaper
    readonly property var tiles: {
        const out = []
        for (const path of naomiFiles.wallpapers) {
            const name = path.substring(path.lastIndexOf("/") + 1)
            out.push({ kind: "file", path: path, title: name.replace(/\.[^.]*$/, ""), sub: "" })
        }
        out.push({ kind: "dark", path: "", title: "Тёмные", sub: "без картинки" })
        out.push({ kind: "files", path: "", title: "Из «Файлов»…", sub: "картинка с телефона" })
        return out
    }
    readonly property string currentName: {
        if (current === "")
            return "тёмные"
        const t = tiles.find(o => o.kind === "file" && o.path === current)
        return t ? t.title : "своя картинка"
    }

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
                title: "ОБОИ"
                info: "сейчас: " + page.currentName
            }
        }
        Row {
            spacing: 24
            Column {
                width: 300
                spacing: 10
                MonitorArt { width: 300; sw: page.sw }
                Text {
                    width: 300
                    wrapMode: Text.Wrap
                    lineHeight: 1.3
                    text: "ложатся с обрезкой краёв, как в НаомиОС; без дока у приставки — те же обои"
                    font.family: DeskStyle.mono
                    font.pixelSize: 12
                    color: DeskStyle.dim
                }
            }
            Grid {
                id: grid
                columns: 2
                spacing: 9
                rowSpacing: 14
                readonly property real tileW: (col.width - 324 - spacing) / 2
                Repeater {
                    model: page.tiles
                    delegate: Item {
                        id: tile
                        required property var modelData
                        readonly property bool on: modelData.kind === "file" ? page.current === modelData.path
                                                                            : modelData.kind === "dark" && page.current === ""
                        readonly property bool hovered: tileArea.containsMouse
                        readonly property real dy: tileArea.pressed ? 1 : 0
                        width: grid.tileW
                        height: thumb.height + 8 + 14 + 6 + 14
                        Item {
                            id: thumb
                            y: tile.dy
                            width: tile.width
                            height: Math.round(width * 9 / 16)
                            Rectangle {
                                anchors.fill: parent
                                visible: tile.modelData.kind === "dark"
                                gradient: Gradient {
                                    GradientStop { position: 0; color: "#1b1424" }
                                    GradientStop { position: 1; color: "#08070b" }
                                }
                            }
                            Image {
                                anchors.fill: parent
                                visible: tile.modelData.kind === "file"
                                source: tile.modelData.kind === "file" ? "file://" + tile.modelData.path : ""
                                fillMode: Image.PreserveAspectCrop
                                sourceSize.width: 360
                                asynchronous: true
                            }
                            Glyph {
                                visible: tile.modelData.kind === "files"
                                kind: "plus"
                                x: thumb.width / 2; y: thumb.height / 2
                                color: tile.hovered ? DeskStyle.text : DeskStyle.dim
                            }
                            Rectangle {
                                anchors.fill: parent
                                color: "transparent"
                                border.width: 1
                                border.color: tile.on || tile.hovered ? DeskStyle.amberA(0.36) : DeskStyle.amberA(DeskStyle.line)
                            }
                            Rectangle {
                                width: 3; height: parent.height
                                color: tile.on ? DeskStyle.amber : tile.hovered ? DeskStyle.muted : Qt.rgba(125 / 255, 120 / 255, 110 / 255, 0.55)
                            }
                        }
                        Text {
                            y: thumb.height + 8 + tile.dy
                            width: tile.width
                            elide: Text.ElideRight
                            text: tile.modelData.title.toUpperCase()
                            font.family: DeskStyle.mono
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            font.letterSpacing: 12 * 0.14
                            color: tile.on ? DeskStyle.amber : DeskStyle.muted
                        }
                        Text {
                            y: thumb.height + 8 + 14 + 6 + tile.dy
                            width: tile.width
                            elide: Text.ElideMiddle
                            text: tile.on && tile.modelData.kind === "file" ? "сейчас на столе" : tile.modelData.sub
                            font.family: DeskStyle.mono
                            font.pixelSize: 12
                            color: DeskStyle.dim
                        }
                        MouseArea {
                            id: tileArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (tile.modelData.kind === "files")
                                    page.sw.desk.openFiles(naomiFiles.home + "/Pictures")
                                else if (!tile.on)
                                    naomiFiles.setWallpaper(tile.modelData.path)
                            }
                        }
                    }
                }
            }
        }
        Text {
            width: col.width
            wrapMode: Text.Wrap
            lineHeight: 1.3
            text: "своя картинка — в «Файлах»: правой кнопкой по ней → «Сделать фоном рабочего стола»; она появится здесь"
            font.family: DeskStyle.mono
            font.pixelSize: 12
            color: DeskStyle.dim
        }
    }
}
