// Крутилка ожидания НаомиФона — одна на всё (просьба пользователя 12 ноября 2026: «везде, где нужна, по
// умолчанию»): заставка запуска программы, ожидание подключения, загрузка. Это «Морфинг» — крутилка автора из
// его другого проекта, без её круглой тёмной подложки: белый контур
// перетекает круг → скруглённый квадрат (и чуть меньше) → лист → круг и за это время делает полный оборот,
// 1,8 с на круг; на каждом из трёх отрезков — плавно (полуволна косинуса). Размеры — как там при size 20:
// контур 2, скругление 10 (круг) и 3; при другом size всё растёт в той же доле.
import QtQuick

Item {
    id: root
    property real size: 20
    property color color: "white"
    width: size
    height: size

    // ключевые кадры: скругления углов (верх-лево, верх-право, низ-право, низ-лево) при size 20, поворот, масштаб
    readonly property var keys: [
        { r: [10, 10, 10, 10], turn: 0, scale: 1 },
        { r: [3, 3, 3, 3], turn: 90, scale: 0.85 },
        { r: [3, 10, 3, 10], turn: 180, scale: 1 },
        { r: [10, 10, 10, 10], turn: 360, scale: 1 }
    ]
    property real phase: 0
    readonly property int part: Math.min(2, Math.floor(phase * 3))
    readonly property real ease: (1 - Math.cos(Math.PI * (phase * 3 - part))) / 2
    readonly property real k: size / 20
    function at(from, to) { return from + (to - from) * ease }

    NumberAnimation on phase {   // крутится, только пока её видно
        from: 0
        to: 1
        duration: 1800
        loops: Animation.Infinite
        running: root.visible && root.opacity > 0
    }

    Rectangle {
        readonly property var a: root.keys[root.part]
        readonly property var b: root.keys[root.part + 1]
        anchors.fill: parent
        color: "transparent"
        border.color: root.color
        border.width: 2 * root.k
        antialiasing: true
        topLeftRadius: root.at(a.r[0], b.r[0]) * root.k
        topRightRadius: root.at(a.r[1], b.r[1]) * root.k
        bottomRightRadius: root.at(a.r[2], b.r[2]) * root.k
        bottomLeftRadius: root.at(a.r[3], b.r[3]) * root.k
        rotation: root.at(a.turn, b.turn)
        scale: root.at(a.scale, b.scale)
    }
}
