// Рисунок на «Мыши»: мышь с колесом и стрелкой «к себе», рядом страница; стрелка на странице — куда она поедет, если
// крутить колесо к себе: вниз — обычное направление, вверх — обратное (как пальцем по экрану); поворот — 200 мс без
// отскока. Координаты — в поле 200 × 150, линии 1,5.
import QtQuick
import QtQuick.Shapes
import ".."

Item {
    id: a
    property bool natural: false
    width: 190
    height: width * 0.75

    Item {
        width: 200; height: 150
        scale: a.width / 200
        transformOrigin: Item.TopLeft

        Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {   // корпус мыши и страница
                strokeColor: DeskStyle.muted; strokeWidth: 1.5; fillColor: "transparent"
                PathSvg { path: "M18 42 A28 28 0 0 1 74 42 V82 A28 28 0 0 1 18 82 Z" }
                PathSvg { path: "M122 14 H180 A4 4 0 0 1 184 18 V106 A4 4 0 0 1 180 110 H122 A4 4 0 0 1 118 106 V18 A4 4 0 0 1 122 14 Z" }
            }
            ShapePath {   // кнопки мыши, строки страницы
                strokeColor: DeskStyle.amberA(DeskStyle.line); strokeWidth: 1.5; fillColor: "transparent"
                PathSvg { path: "M46 14 V52 M18 52 H74 M128 30 h46 M128 42 h38 M128 54 h46 M128 66 h30 M128 78 h46 M128 90 h40" }
            }
            ShapePath {   // колесо
                strokeColor: DeskStyle.amber; strokeWidth: 1.5; fillColor: DeskStyle.amberA(0.22)
                PathSvg { path: "M42 30 A4 4 0 0 1 50 30 V40 A4 4 0 0 1 42 40 Z" }
            }
            ShapePath {   // «к себе»
                strokeColor: DeskStyle.amber; strokeWidth: 1.5; fillColor: "transparent"
                capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
                PathSvg { path: "M58 24 C63 28 63 42 58 46 M55 42 L58 46 L62 44" }
            }
            ShapePath {   // от мыши к странице — точками
                strokeColor: DeskStyle.dim; strokeWidth: 1.5; fillColor: "transparent"; capStyle: ShapePath.RoundCap
                PathSvg { path: "M85 62 h0.01 M91 62 h0.01 M97 62 h0.01 M103 62 h0.01 M109 62 h0.01" }
            }
        }
        Item {   // куда едет страница
            x: 144; y: 46
            width: 14; height: 34
            rotation: a.natural ? 180 : 0
            Behavior on rotation { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
            Shape {
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: DeskStyle.amber; strokeWidth: 1.5; fillColor: "transparent"
                    capStyle: ShapePath.RoundCap; joinStyle: ShapePath.RoundJoin
                    PathSvg { path: "M7 0 V34 M0 27 L7 34 L14 27" }
                }
            }
        }
        component Caption: Text {
            property real cx
            x: cx - implicitWidth / 2
            y: 124
            font.family: DeskStyle.mono
            font.pixelSize: 11
            font.letterSpacing: 11 * 0.08
            color: DeskStyle.dim
        }
        Caption { cx: 46; text: "К СЕБЕ" }
        Caption { cx: 151; text: "СТРАНИЦА" }
    }
}
