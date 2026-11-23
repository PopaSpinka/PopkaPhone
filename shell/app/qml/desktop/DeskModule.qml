// Модуль («прибор») — как draw_module в bcdesk: поверхность сплошная (у НаомиОС — .97; на столе прозрачности нет),
// рамка 1 точка волосяным янтарём,
// углы «непрерывные» (четверть суперэллипса |x|^5 + |y|^5 = 1, не больше 32 точек и 35 % меньшей стороны), угловые
// скобки — тот же контур янтарём .35 у левого верхнего и правого нижнего углов.
import QtQuick
import QtQuick.Shapes

Item {
    id: m
    readonly property real e: DeskStyle.corner(width, height)
    readonly property real len: Math.max(DeskStyle.bracket, e + 2)
    readonly property var path: DeskStyle.outline(0.5, 0.5, width - 1, height - 1, e)

    Shape {
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            fillColor: DeskStyle.surface
            strokeColor: DeskStyle.amberA(DeskStyle.line)
            strokeWidth: 1
            PathPolyline { path: m.path }
        }
    }
    component Bracket: Item {
        id: br
        property real ox
        property real oy
        clip: true
        width: m.len + 1; height: m.len + 1
        Shape {
            x: -br.ox; y: -br.oy
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "transparent"
                strokeColor: DeskStyle.amberA(0.35)
                strokeWidth: 1
                PathPolyline { path: m.path }
            }
        }
    }
    Bracket { x: -1; y: -1; ox: -1; oy: -1 }
    Bracket { x: m.width - m.len; y: m.height - m.len; ox: x; oy: y }
}
