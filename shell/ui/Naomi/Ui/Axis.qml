// Переход Material shared axis между двумя экранами — «старым» (под ним, progress 0) и «новым» (progress 1).
// Перенос переходов Попка Студио для 7-дюймовой платы (esp32_amoled, lib/Halo/src/halo_nav.cpp, прямоугольный
// экран): без масштаба и пружины. Тот, от кого уходим, за первые 35 % пути уезжает на shift по ходу и гаснет;
// тот, к кому идём, во вторые 65 % приходит с той же стороны и проявляется — вместе их не видно никогда.
//   dir — куда едет всё, когда идём вперёд (к «новому»): +1 — вниз или вправо, −1 — вверх или влево. Назад —
//   в зеркале. Сдвиги (oldDelta, newDelta) — вдоль оси; на какую ось их ставить, решает тот, кто рисует.
//   fade — без направления (Material fade through, Попка Студио HALO_FX_FADE): никто не едет, «старый» гаснет
//   на месте, «новый» проявляется в середине, вырастая с 0,92 (newScale).
//   За пальцем: весь путь — travel dp хода (dragBegin, dragMove(d): d — dp в сторону роста progress, от места,
//   где жест подхватил палец). Отпустили (decide(v): v — dp/с в сторону роста): бросок быстрее fling — куда
//   бросили; иначе — куда шёл жест, если палец ушёл от начала хотя бы на 5 % пути, а вернул почти на место —
//   как было (в Попка Студио — треть пути; здесь решительнее, по просьбе пользователя: смахнул — сработало).
//   Дальше — по времени (goTo): кривая Material standard (0.2, 0, 0, 1), без хвоста и отскока.
import QtQuick

QtObject {
    id: ax
    property real progress: 0
    property bool forward: true        // к «новому» (true) или назад; решается только с места (0 или 1)
    property real dir: 1
    property real shift: 37            // dp
    property bool fade: false          // без направления: без сдвига, «новый» растёт с 0,92
    property real travel: 260          // dp хода пальца — весь путь
    property real fling: 650           // dp/с — бросок
    property bool dragging: false
    readonly property bool atRest: !dragging && !anim.running
    signal settled(real at)            // доехал (0 или 1) — после доводки по времени

    readonly property real e: forward ? progress : 1 - progress          // путь в сторону перехода
    readonly property real a: Math.min(1, Math.max(0, e / 0.35))         // уходящий: уезжает и гаснет
    readonly property real b: Math.min(1, Math.max(0, (e - 0.35) / 0.65))   // приходящий: приезжает, проявляется
    readonly property real oldDelta: fade ? 0 : forward ? dir * shift * a : dir * shift * (1 - b)
    readonly property real oldAlpha: forward ? 1 - a : b
    readonly property real newDelta: fade ? 0 : forward ? -dir * shift * (1 - b) : -dir * shift * a
    readonly property real newAlpha: forward ? b : 1 - a
    readonly property real newScale: fade ? 0.92 + 0.08 * newAlpha : 1

    // к target (0 или 1) за fullMs; byRemaining — доля по оставшемуся пути (не меньше 0,1)
    function goTo(target, fullMs, byRemaining) {
        anim.stop()
        dragging = false
        if (progress === 0 || progress === 1) forward = target > progress
        const d = Math.abs(target - progress)
        if (d === 0) { settled(progress); return }
        anim.to = target
        anim.duration = byRemaining ? fullMs * Math.max(0.1, d) : fullMs
        anim.start()
    }
    function jump(p) { anim.stop(); dragging = false; progress = p }      // сразу, без перехода
    function finish() { if (anim.running) { const t = anim.to; anim.stop(); progress = t; settled(t) } }

    property real _start: 0
    function dragBegin() {
        anim.stop()
        if (progress === 0 || progress === 1) forward = progress === 0
        _start = progress
        dragging = true
    }
    function dragMove(d) { progress = Math.max(0, Math.min(1, _start + d / travel)) }
    function decide(v) {
        dragging = false
        const moved = progress - _start
        const t = v > fling ? 1 : v < -fling ? 0 : Math.abs(moved) < 0.05 ? _start : moved > 0 ? 1 : 0
        return t >= 0.5 ? 1 : 0
    }

    property NumberAnimation anim: NumberAnimation {
        target: ax; property: "progress"
        easing.type: Easing.BezierSpline
        easing.bezierCurve: [0.2, 0, 0, 1, 1, 1]       // Material standard
        onFinished: ax.settled(ax.progress)
    }
}
