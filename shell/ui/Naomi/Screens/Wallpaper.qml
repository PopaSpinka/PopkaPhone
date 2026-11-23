// Naomi.Screens Wallpaper — the project's wallpaper (the studio's System → Wallpaper: project.functions.wallpaper —
// {on, file, scale, dx, dy, dim, lock, home}). The picture covers the screen as a wallpaper does (both sides covered,
// its middle on the screen's middle), times scale, moved by dx, dy — past the screen's edges too: whoever shows it
// cuts it (ScreenView, the shell's screens); the studio shows the rest dimmed while it is edited (studio/qml/Engine.qml).
// dim — black over it (0 … 0.8), so the layers read on a bright picture. file — a picture of the project's: the
// studio's studio/projects/assets/<file> (studio.py /assets/), the phone's ~/.config/naomi/assets/<file> (Load and
// Live send it); assetBase — where they are. The picture's own turn (a photo's EXIF orientation) — applied (the studio
// turns the pixels when it takes a picture in; autoTransform — for one that came another way).
import QtQuick

Item {
    id: root
    property var wall: null
    property string assetBase: ""
    readonly property bool loading: img.status === Image.Loading
    readonly property bool ready: img.status === Image.Ready
    signal reshaped()                    // its box changed: the picture came (the studio's frame follows it)

    Image {
        id: img
        source: root.wall && root.wall.file && root.assetBase ? root.assetBase + root.wall.file : ""
        autoTransform: true
        asynchronous: true
        smooth: true
        mipmap: true
        // cover: the smaller of the two scales that fill the screen's width and its height would leave a side bare
        readonly property real k: implicitWidth > 0 && implicitHeight > 0
                                  ? Math.max(root.width / implicitWidth, root.height / implicitHeight) * (root.wall ? root.wall.scale ?? 1 : 1) : 0
        width: implicitWidth * k
        height: implicitHeight * k
        x: root.width / 2 + (root.wall ? root.wall.dx ?? 0 : 0) - width / 2
        y: root.height / 2 + (root.wall ? root.wall.dy ?? 0 : 0) - height / 2
        onStatusChanged: root.reshaped()
    }
    Rectangle {
        anchors.fill: parent
        color: "black"
        opacity: root.wall ? Math.max(0, Math.min(0.8, root.wall.dim ?? 0)) : 0
        visible: opacity > 0 && root.ready
    }
    // where the picture is, in its own coordinates (null — not there yet)
    function box() { return ready ? [img.x, img.y, img.x + img.width, img.y + img.height] : null }
}
