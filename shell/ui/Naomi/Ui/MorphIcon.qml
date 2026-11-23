// Значок-трансформер: плюс перетекает в крутилку «Морфинг» (Naomi.Ui/Spinner) и обратно одной фигурой —
// шейдер shaders/morph.frag рисует её полем расстояний на видеочипе (один прямоугольник, без геометрии).
//   busy — плюс (false) или крутилка (true); size — сторона в точках (фигура — 21 и 20 при size 30).
// Ход — одно сквозное движение без швов (просьба пользователя: «без рывков, зациклить саму в себя»):
//   занят — плюс за enterMs «втягивается»: лучи стягиваются в точку, точка без остановки раздувается в кольцо (из
//   четырёх вариантов на телефоне пользователь выбрал этот, 12 ноября 2026); кольцо, не останавливаясь на
//   передачу, крутится «Морфингом» (круг крутилки начинается и кончается тем же кольцом); свободен — крутилка
//   докручивает свой круг до кольца (не обрывается посреди оборота), и только тогда кольцо за leaveMs стягивается
//   обратно в плюс.
// Всё — цепочки аниматоров потока отрисовки (UniformAnimator в SequentialAnimation): передачи между частями
// идут там же, поток интерфейса их не задерживает. Перебили на полпути — новая цепочка начинается с того места,
// где остановилась прежняя (остановленный аниматор возвращает своё положение в свойство).
import QtQuick
import QtQuick.Window

Item {
    id: root
    property bool busy: false
    property real size: 30
    property color color: "white"
    property int enterMs: 520
    property int leaveMs: 460
    readonly property int turnMs: 1800       // круг «Морфинга»
    width: size
    height: size

    ShaderEffect {
        id: fx
        anchors.fill: parent
        property real morph: 0
        property real phase: -1                                 // −1 — крутилки нет
        property real span: 30                                  // фигура рассчитана на поле 30 × 30
        property real px: root.width * Screen.devicePixelRatio
        property color tint: root.color
        fragmentShader: Qt.resolvedUrl("shaders/morph.frag.qsb")
    }

    // занят: (плюс → кольцо) → (докрутить начатый круг, если перебили уход) → круги без конца
    SequentialAnimation {
        id: enter
        UniformAnimator { id: inMorph; target: fx; uniform: "morph"; to: 1; easing.type: Easing.InOutCubic }
        UniformAnimator { id: inTail; target: fx; uniform: "phase"; to: 1 }
        UniformAnimator { target: fx; uniform: "phase"; from: 0; to: 1; duration: root.turnMs; loops: Animation.Infinite }
    }
    // свободен: докрутить круг до кольца → кольцо → плюс
    SequentialAnimation {
        id: leave
        UniformAnimator { id: outTail; target: fx; uniform: "phase"; to: 1 }
        UniformAnimator { id: outMorph; target: fx; uniform: "morph"; to: 0; easing.type: Easing.InOutCubic }
    }

    onBusyChanged: {
        enter.stop()
        leave.stop()
        const m = fx.morph, p = fx.phase
        const spinning = p >= 0 && p < 1
        if (busy) {
            inMorph.from = m
            inMorph.duration = spinning ? 0 : root.enterMs * Math.max(0.15, 1 - m)
            inTail.from = spinning ? p : 1
            inTail.duration = spinning ? (1 - p) * root.turnMs : 0
            if (!spinning)
                fx.phase = -1
            enter.start()
        } else {
            outTail.from = p
            outTail.duration = spinning ? (1 - p) * root.turnMs : 0
            outMorph.from = m
            outMorph.duration = root.leaveMs * Math.max(0.15, m)
            leave.start()
        }
    }
}
