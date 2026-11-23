// Layer "app" — an app's screen. In the studio — the app itself: its own QML on made-up data (appSources: the studio's
// studio/qml/apps/<App>App.qml by the app's id — «Настройки», «Заметки», «Наоми»); interactive in Try (live). Else —
// a neutral placeholder: the app's icon (Material Symbols Rounded) and its name in the middle of a black screen. On the
// phone the layer is never shown: a link to an app's screen opens the app itself (ShellView.openScreen). Fixed (fills
// its screen). Props: app (its id: chat, notes, settings), icon, title (copied from the app when it was added: state.js
// addApp; projects from before "app" — the id by the icon).
import QtQuick
import Naomi.Ui

Item {
    id: root
    property var d: ({})
    property real sw: 393
    property real sh: 851
    property var now: new Date()
    property bool live: false
    property var appSources: ({})
    signal tapped(int row)

    readonly property string appId: d.app ?? ({ auto_awesome: "chat", edit_note: "notes", settings: "settings" })[d.icon] ?? ""
    readonly property url appSource: appSources[appId] ?? ""
    readonly property real baseX: sw / 2
    readonly property real baseY: sh / 2
    width: sw; height: sh

    Loader {
        id: app
        anchors.fill: parent
        active: root.appSource.toString() !== ""
        source: root.appSource
        enabled: root.live              // (on the map it only shows; in Try — a finger)
    }

    Column {
        id: col
        visible: app.status !== Loader.Ready
        anchors.centerIn: parent
        spacing: 14
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.d.icon ?? "apps"
            font.family: "Material Symbols Rounded"; font.pixelSize: 56
            font.variableAxes: ({ "FILL": 1, "wght": 400, "opsz": 48 })
            color: Theme.muted
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.d.title ?? ""
            font.family: Theme.sans; font.pixelSize: 21; font.weight: Font.Medium
            color: Theme.text
        }
    }
    function box() { return [0, 0, width, height] }
}
