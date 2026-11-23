// Значок файла «Файлов» — перенос NfIcon НаомиОС (files-grid.c): в коробке 96 × 74. Папка — янтарный контур с язычком
// и линией крышки; файл — лист с загнутым углом и типом моно-капсом (ZIP, TXT…); фото — миниатюрой, вписанной в
// коробку; в избранном — янтарная звёздочка с тёмной обводкой в правом верхнем углу. k — масштаб (в строке списка —
// 0,34: 33 × 25, без надписи и звёздочки); линии той же толщины при любом масштабе. dim — вполсилы (вырезано).
import QtQuick
import QtQuick.Shapes
import ".."

Item {
    id: icon
    property bool dir
    property string ext
    property bool starred
    property bool image
    property string path
    property real k: 1
    property bool dim: false
    width: 96 * k
    height: 74 * k
    readonly property real a: dim ? 0.5 : 1
    function pt(x, y) { return Qt.point(x * k, y * k) }

    // папка: x 14…82, y 14…66, язычок 30 × 7
    Shape {
        visible: icon.dir
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Qt.rgba(216 / 255, 178 / 255, 90 / 255, 0.85 * icon.a)
            fillColor: Qt.rgba(216 / 255, 178 / 255, 90 / 255, 0.07 * icon.a)
            strokeWidth: 1.4
            joinStyle: ShapePath.RoundJoin
            PathPolyline {
                path: [icon.pt(14, 16), icon.pt(16, 14), icon.pt(40, 14), icon.pt(46, 21), icon.pt(80, 21), icon.pt(82, 23),
                       icon.pt(82, 64), icon.pt(80, 66), icon.pt(16, 66), icon.pt(14, 64), icon.pt(14, 16)]
            }
        }
        ShapePath {
            strokeColor: Qt.rgba(216 / 255, 178 / 255, 90 / 255, 0.35 * icon.a)
            fillColor: "transparent"
            strokeWidth: 1
            PathPolyline { path: [icon.pt(15, 28), icon.pt(81, 28)] }
        }
    }
    // лист: x 28…68, y 6…70, угол 11
    Shape {
        visible: !icon.dir && !thumb.ready
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Qt.rgba(169 / 255, 164 / 255, 154 / 255, 0.55 * icon.a)
            fillColor: Qt.rgba(169 / 255, 164 / 255, 154 / 255, 0.04 * icon.a)
            strokeWidth: 1.2
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: [icon.pt(28, 6), icon.pt(57, 6), icon.pt(68, 17), icon.pt(68, 70), icon.pt(28, 70), icon.pt(28, 6)] }
        }
        ShapePath {
            strokeColor: Qt.rgba(169 / 255, 164 / 255, 154 / 255, 0.55 * icon.a)
            fillColor: "transparent"
            strokeWidth: 1.2
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: [icon.pt(57, 6), icon.pt(57, 17), icon.pt(68, 17)] }
        }
    }
    Text {   // тип файла на листе: моно-капс, 10 (длинный — 8,5), разрядка 1
        visible: !icon.dir && !thumb.ready && icon.k === 1 && icon.ext !== ""
        x: 48 - implicitWidth / 2
        y: 44 - implicitHeight / 2
        text: icon.ext
        font.family: DeskStyle.mono
        font.pixelSize: icon.ext.length > 4 ? 8.5 : 10
        font.letterSpacing: 1
        color: Qt.rgba(236 / 255, 233 / 255, 227 / 255, 0.82 * icon.a)
    }
    // фото — миниатюрой, вписанной в коробку
    Image {
        id: thumb
        readonly property bool ready: status === Image.Ready
        visible: ready
        anchors.centerIn: parent
        width: (96 - 4) * icon.k
        height: (74 - 2) * icon.k
        fillMode: Image.PreserveAspectFit
        source: icon.image ? "file://" + encodeURIComponent(icon.path).replace(/%2F/g, "/") : ""
        sourceSize: Qt.size(192, 144)
        asynchronous: true
        opacity: icon.a
    }
    // звёздочка «в избранном»: в углу папки, листа или фото
    Glyph {
        visible: icon.starred && icon.k === 1
        kind: "star"
        fill: true
        unit: 7.5 / 6
        line: 1.6
        color: DeskStyle.amber
        x: icon.dir ? 81 : thumb.ready ? thumb.x + (thumb.width + thumb.paintedWidth) / 2 - 1 : 67
        y: icon.dir ? 15 : thumb.ready ? thumb.y + (thumb.height - thumb.paintedHeight) / 2 + 1 : 7
    }
}
