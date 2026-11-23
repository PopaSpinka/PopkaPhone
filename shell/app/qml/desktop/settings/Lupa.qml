// Лупа резкости («Экран»): буквы стола — как они лягут на пиксели монитора. Рисуются в кадре телефона (frameK пикселей
// кадра в точке — при этих размере и резкости), растягиваются плавно до пикселей монитора (monK в точке), как кадр
// растягивает слой дисплея приставки, и показываются ×3 без сглаживания: пиксель монитора — квадрат 3 × 3. Примерно:
// тянет видеочип телефона, а не приставки.
import QtQuick
import ".."

Item {
    id: l
    property real frameK: 1.5   // пикселей кадра в точке стола
    property real monK: 1.6     // пикселей монитора в точке стола
    readonly property int zoom: 3
    readonly property int mw: Math.ceil(width / zoom)    // пикселей монитора в лупе
    readonly property int mh: Math.ceil(height / zoom)
    clip: true

    Item {   // в пикселях кадра: точка стола — frameK
        id: src
        width: Math.max(1, Math.round(l.mw * l.frameK / l.monK))
        height: Math.max(1, Math.round(l.mh * l.frameK / l.monK))
        Rectangle { anchors.fill: parent; color: DeskStyle.surface }
        Text {
            x: 5 * l.frameK; y: 0
            text: "Файлы"
            font.family: DeskStyle.sans
            font.pixelSize: Math.max(1, Math.round(15 * l.frameK))
            color: DeskStyle.text
        }
        Text {
            x: 5 * l.frameK; y: 17 * l.frameK
            text: "WEB 14:32"
            font.family: DeskStyle.mono
            font.weight: Font.Medium
            font.pixelSize: Math.max(1, Math.round(11 * l.frameK))
            color: DeskStyle.amber
        }
    }
    ShaderEffectSource {   // кадр телефона: пиксель в пиксель, растянут до пикселей монитора плавно
        id: frame
        sourceItem: src
        hideSource: true
        textureSize: Qt.size(src.width, src.height)
        width: l.mw; height: l.mh
        smooth: true
    }
    ShaderEffectSource {   // пиксели монитора ×3
        sourceItem: frame
        hideSource: true
        textureSize: Qt.size(l.mw, l.mh)
        width: l.mw * l.zoom; height: l.mh * l.zoom
        smooth: false
    }
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: DeskStyle.amberA(DeskStyle.line)
    }
}
