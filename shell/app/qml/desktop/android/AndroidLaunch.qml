// Плашка «приложение Android открывается» (docs/ANDROID.md): от щелчка в «03 ANDROID» до окна — Android включается
// 15–20 с, и всё это время на столе было бы пусто. Место и вид — как у плашки громкости (DeskVolumeOsd: модуль 288 × 66
// по середине, на 18 ниже панели): шильдик «ANDROID» и справа — «ВКЛЮЧАЕТСЯ» (Android ещё не готов) или «ОТКРЫВАЕТСЯ»,
// ниже — крутилка (Naomi.Ui Spinner — ожидание по умолчанию) и имя приложения (несколько — через запятую).
// Появляется и уходит как плашка громкости (110 и 150 мс).
import QtQuick
import Naomi.Ui
import ".."

Item {
    id: osd
    property var names: []          // имена открываемых приложений
    property bool starting: false   // Android ещё включается
    readonly property bool shown: names.length > 0
    width: 288
    height: 66

    property real t: shown ? 1 : 0
    Behavior on t { NumberAnimation { duration: osd.shown ? 110 : 150; easing.type: Easing.OutCubic } }
    visible: t > 0
    opacity: t
    scale: 0.98 + 0.02 * t
    transformOrigin: Item.Top

    DeskModule { anchors.fill: parent }
    Plate {
        x: 16; y: 14
        width: 256
        title: "ANDROID"
        note: osd.starting ? "ВКЛЮЧАЕТСЯ" : "ОТКРЫВАЕТСЯ"
    }
    Spinner {
        x: 16; y: 14 + 18 + 8
        size: 14
        color: DeskStyle.amber
    }
    Text {
        x: 16 + 14 + 10
        y: 14 + 18 + 8
        width: 256 - 24
        height: 14
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        text: osd.names.join(", ")
        font.family: DeskStyle.sans
        font.pixelSize: 14
        color: DeskStyle.text
    }
}
