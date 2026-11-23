// «Настройки» → «Геймпад» (просьба пользователя 21 ноября 2026: станция геймпада включает его, когда берут, и
// выключает, когда ставят, — «появился: ничего или игра из списка… выключается: ничего или закрыть текущую игру»; тем
// же вечером: «чтобы список игр выпадал, и опцию — запускать последнюю игру, в которую я играл»). Слева — геймпад
// сейчас (naomiGames.gamepads: подключён или нет, имя); справа — что делать, когда он появился: выпадающий список
// («Ничего», «Последняя игра», игры — как в «03 GAMES»: имя и система), и когда пропал (ничего или закрыть игру).
// Значения — sw.pad (gamepad.conf стола), последняя игра — sw.desk.lastGame (game.conf); делает их стол (Desktop.qml,
// «геймпад и станция»). Список — как меню «Файлов» (popover.menu НаомиОС): поверхность, рамка янтарь .25, пункты sans 14.
import QtQuick
import ".."

Item {
    id: page
    property var sw
    readonly property var pad: sw.pad
    readonly property var pads: naomiGames.gamepads
    readonly property var games: naomiGames.games
    function gameOf(path) { return games.find(g => g.path === path) }
    readonly property var lastGame: gameOf(sw.desk ? sw.desk.lastGame : "")
    // выбор списка: «Ничего», «Последняя игра» (справа — какая), черта, игры
    readonly property var options: [{ key: "", name: "Ничего", tag: "" },
                                    { key: "last", name: "Последняя игра", tag: lastGame ? lastGame.name : "ещё не играли" },
                                    { sep: true }]
        .concat(games.map(g => ({ key: g.path, name: g.name, tag: g.label })))
    readonly property var chosen: options.find(o => !o.sep && o.key === pad.appearGame)
    readonly property bool chosenGone: !chosen   // выбранной игры больше нет в ~/Games
    implicitHeight: Math.max(illus.height, sets.height, drop.visible ? drop.y + drop.height + 12 : 0)
    onVisibleChanged: if (!visible) drop.visible = false

    component Label: Text {
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 12 * 0.12
        color: DeskStyle.muted
    }
    component Hint: Text {
        wrapMode: Text.Wrap
        lineHeight: 1.3
        font.family: DeskStyle.mono
        font.pixelSize: 12
        color: DeskStyle.dim
    }
    component Line: Rectangle { height: 1; color: DeskStyle.amberA(DeskStyle.line) }
    component Seg: NButton { height: 30 }

    Column {
        id: illus
        width: 190
        spacing: 12
        SubPlate { width: parent.width; text: "Сейчас" }
        Rectangle {   // геймпад: подключён — янтарём, нет — тускло
            width: parent.width
            height: 104
            radius: 10
            color: DeskStyle.amberA(page.pads.length ? 0.10 : 0.03)
            border.width: 1
            border.color: DeskStyle.amberA(page.pads.length ? 0.45 : DeskStyle.line)
            Behavior on color { ColorAnimation { duration: 180 } }
            Glyph {
                kind: "gamepad"
                unit: 3.6
                line: 2.2
                x: parent.width / 2
                y: parent.height / 2 + 2
                color: page.pads.length ? DeskStyle.amber : DeskStyle.dim
            }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: page.pads.length ? page.pads.join("\n") : "не подключён"
            font.family: DeskStyle.sans
            font.pixelSize: 14
            color: page.pads.length ? DeskStyle.text : DeskStyle.muted
        }
        Hint {
            width: parent.width
            text: "станция включает геймпад, когда его берут, и выключает, когда ставят"
        }
    }

    Column {
        id: sets
        x: 218
        width: page.width - x
        Label { id: appearLabel; height: 30; verticalAlignment: Text.AlignVCenter; text: "КОГДА ГЕЙМПАД ПОЯВИЛСЯ" }
        NButton {   // поле списка: что выбрано; справа — система или какая «последняя», уголок
            id: field
            width: sets.width
            tone: page.pad.appearGame === "" ? "off" : "on"
            text: page.chosen ? page.chosen.name : "Игры нет в списке"
            onClicked: drop.visible = !drop.visible
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 34
                y: field.dy
                height: field.height
                verticalAlignment: Text.AlignVCenter
                text: page.chosen ? page.chosen.tag : ""
                font.family: DeskStyle.mono
                font.pixelSize: DeskStyle.caps - 2
                font.letterSpacing: 1.0
                color: DeskStyle.dim
            }
            Glyph {
                kind: drop.visible ? "up" : "down"
                x: field.width - 18
                y: field.height / 2 + field.dy
                color: field.hovered || drop.visible ? DeskStyle.text : DeskStyle.muted
            }
        }
        Hint {
            width: sets.width
            topPadding: 9; bottomPadding: 12
            color: page.chosenGone ? DeskStyle.amber : DeskStyle.dim
            text: page.chosenGone ? "выбранной игры больше нет в ~/Games — геймпад появился, ничего не будет"
                : page.pad.appearGame === "" ? "взяли геймпад — ничего не происходит"
                : (page.pad.appearGame === "last" ? "взяли геймпад — запускается игра, в которую играли последней. "
                                                   : "взяли геймпад — игра запускается сама. ")
                  + "Идёт уже другая — она остаётся и показывается"
        }
        Line { width: sets.width }
        Label { height: 42; verticalAlignment: Text.AlignVCenter; text: "КОГДА ГЕЙМПАД ПРОПАЛ" }
        Row {
            spacing: 6
            Seg {
                width: (sets.width - 6) / 2; text: "Ничего"
                tone: page.pad.goneClose ? "off" : "on"; onClicked: page.pad.goneClose = false
            }
            Seg {
                width: (sets.width - 6) / 2; text: "Закрыть игру"
                tone: page.pad.goneClose ? "on" : "off"; onClicked: page.pad.goneClose = true
            }
        }
        Hint {
            width: sets.width
            topPadding: 9
            text: page.pad.goneClose
                  ? "геймпада нет 3 с — идущая игра закрывается. Сохранения игры (батарейка картриджа, карта памяти PS1) пишутся, прохождение без них — нет"
                  : "поставили геймпад — игра идёт дальше"
        }
    }

    MouseArea {   // список открыт: щелчок мимо — закрыть (сам щелчок съедается)
        anchors.fill: parent
        z: 9
        enabled: drop.visible
        acceptedButtons: Qt.AllButtons
        onPressed: drop.visible = false
    }
    Rectangle {   // выпадающий список: под полем, во всю его ширину; игр много — крутится колесом
        id: drop
        visible: false
        z: 10
        x: sets.x
        y: sets.y + appearLabel.height + field.height + 4
        width: field.width
        height: Math.min(list.contentHeight, 9 * 30) + 12
        color: DeskStyle.surface
        radius: 3
        Rectangle {   // рамка — поверх сплошного: у Rectangle с рамкой заливка внутри неё, и полупрозрачная рамка
            anchors.fill: parent   // показывала бы то, что под списком
            z: 1
            color: "transparent"
            border.width: 1
            border.color: DeskStyle.amberA(0.25)
            radius: 3
        }
        ListView {
            id: list
            x: 1; y: 6
            width: drop.width - 2
            height: drop.height - 12
            clip: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds
            model: page.options
            delegate: Item {
                id: row
                required property var modelData
                readonly property bool on: !modelData.sep && modelData.key === page.pad.appearGame
                width: list.width
                height: modelData.sep ? 9 : 30
                Rectangle {
                    visible: row.modelData.sep === true
                    x: 4; y: 4
                    width: parent.width - 8; height: 1
                    color: DeskStyle.amberA(0.12)
                }
                Rectangle {
                    anchors.fill: parent
                    visible: !row.modelData.sep && (area.containsMouse || row.on)
                    color: DeskStyle.amberA(area.containsMouse ? 0.12 : 0.07)
                }
                Rectangle {   // выбранный — грань янтарём
                    visible: row.on
                    width: 3; height: parent.height
                    color: DeskStyle.amber
                }
                Text {
                    visible: !row.modelData.sep
                    x: 16
                    width: tag.x - 10 - x
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    text: row.modelData.name || ""
                    font.family: DeskStyle.sans
                    font.pixelSize: 14
                    color: row.on ? DeskStyle.amber : DeskStyle.text
                }
                Text {
                    id: tag
                    visible: !row.modelData.sep
                    x: row.width - 16 - implicitWidth
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    text: row.modelData.tag || ""
                    font.family: DeskStyle.mono
                    font.pixelSize: DeskStyle.caps - 2
                    font.letterSpacing: 1.0
                    color: DeskStyle.dim
                }
                MouseArea {
                    id: area
                    anchors.fill: parent
                    enabled: !row.modelData.sep
                    hoverEnabled: true
                    onClicked: {
                        page.pad.appearGame = row.modelData.key
                        drop.visible = false
                    }
                }
            }
        }
    }
}
