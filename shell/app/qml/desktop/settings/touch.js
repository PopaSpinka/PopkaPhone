// Примеры жестов для рисунка трекпада (TouchArt.qml) в «Настройках» → «Трекпад»: где пальцы в момент t (секунды).
// Координаты — миллиметры трекпада 162 × 115. Ответ: { f: [{ x, y, off }], ring: { x, y, r, a }, held: { x, y }, zone }
// — пальцы (off — не считается), круг касания, кружок «кнопка ещё держится», правый нижний угол.
// cfg — настройки (TrackpadPage.qml): swipeFingers, palm, jitter, right, drag.
.pragma library

function ease(u) { return u * u * (3 - 2 * u) }
function tri(t, p) { const u = (t % p) / p; return u < 0.5 ? u * 2 : 2 - u * 2 }
function row(n, cx, cy, gap) {   // пальцы рядом, дугой
    const out = []
    for (let i = 0; i < n; ++i)
        out.push({ x: cx + (i - (n - 1) / 2) * gap, y: cy - Math.abs(i - (n - 1) / 2) * 3 + (i ? 0 : 4), off: false })
    return out
}
function tapAt(t, period, pts) {
    const u = t % period
    return { f: u < 0.22 ? pts : [], ring: u < 0.7 ? { x: pts[0].x + (pts.length > 1 ? 11 : 0), y: pts[0].y, r: (pts.length > 1 ? 16 : 7) + u * 24, a: 1 - u / 0.7 } : null }
}

const demos = {
    move: (t) => ({ f: [{ x: 81 + 40 * Math.sin(t * 1.3), y: 57 + 24 * Math.sin(t * 2.1 + 1) }] }),
    accel: (t) => {   // медленно — и рывок
        const u = (t % 3) / 3
        return { f: [{ x: u < 0.6 ? 40 + 30 * (u / 0.6) : 70 + 55 * ease((u - 0.6) / 0.25 > 1 ? 1 : (u - 0.6) / 0.25), y: 60 }] }
    },
    tap: (t) => tapAt(t, 1.6, [{ x: 81, y: 57 }]),
    pin: (t) => tapAt(t, 1.6, [{ x: 81, y: 57 }]),
    right: (t, c) => c.right === 2 ? Object.assign(tapAt(t, 1.8, [{ x: 138, y: 96 }]), { zone: true })
                                   : tapAt(t, 1.8, [{ x: 70, y: 60 }, { x: 93, y: 53 }]),
    scroll: (t) => { const y = 34 + 46 * ease(tri(t, 2.6)); return { f: [{ x: 69, y: y + 5 }, { x: 93, y: y }] } },
    inertia: (t) => {   // бросок — и пальцев нет
        const u = (t % 2.4) / 2.4
        return { f: u < 0.25 ? [{ x: 69, y: 85 - 50 * ease(u / 0.25) }, { x: 93, y: 80 - 50 * ease(u / 0.25) }] : [] }
    },
    pinch: (t) => { const d = 14 + 26 * ease(tri(t, 2.4)); return { f: [{ x: 81 - d, y: 57 + d * 0.55 }, { x: 81 + d, y: 57 - d * 0.55 }] } },
    drag: (t, c) => {
        if (c.drag === 2) {   // двойное касание — и ведут
            const u = (t % 3.6) / 3.6
            if (u < 0.06) return { f: [{ x: 50, y: 62 }] }
            if (u < 0.12 || u >= 0.7) return { f: [] }
            return { f: [{ x: 50 + 62 * ease((u - 0.12) / 0.58), y: 62 - 14 * ease((u - 0.12) / 0.58) }] }
        }
        if (c.drag === 0)     // нажали и ведут
            return { f: [{ x: 50 + 60 * ease(tri(t, 3)), y: 64 - 12 * ease(tri(t, 3)) }], ring: { x: 50 + 60 * ease(tri(t, 3)), y: 64 - 12 * ease(tri(t, 3)), r: 11, a: 0.8 } }
        return demos.hold(t, c)
    },
    hold: (t) => {            // три пальца: ведут, сняли — держит, вернули — дальше
        const u = (t % 4.6) / 4.6
        if (u < 0.34) { const k = ease(u / 0.34); return { f: row(3, 52 + 44 * k, 66 - 10 * k, 21) } }
        if (u < 0.52) return { f: [], held: { x: 96, y: 56 } }
        if (u < 0.86) { const k = ease((u - 0.52) / 0.34); return { f: row(3, 52 + 44 * k, 66 - 10 * k, 21) } }
        return { f: [] }
    },
    swipeSide: (t, c) => { const u = (t % 2.2) / 2.2; return { f: u < 0.3 ? row(c.swipeFingers, 44 + 74 * ease(u / 0.3), 60, 19) : [] } },
    swipeUp: (t, c) => { const u = (t % 2.2) / 2.2; return { f: u < 0.3 ? row(c.swipeFingers, 81, 88 - 52 * ease(u / 0.3), 19) : [] } },
    swipeDown: (t, c) => { const u = (t % 2.2) / 2.2; return { f: u < 0.3 ? row(c.swipeFingers, 81, 34 + 52 * ease(u / 0.3), 19) : [] } },
    rest: (t, c) => ({ f: [{ x: 86 + 34 * Math.sin(t * 1.4), y: 44 + 14 * Math.sin(t * 2.2) }, { x: 36, y: 100, off: c.palm }] }),
    jitter: (t, c) => { const a = [1.6, 0.9, 0.35][c.jitter]; return { f: [{ x: 81 + a * Math.sin(t * 47), y: 57 + a * Math.cos(t * 39) }] } },
    typing: (t) => { const u = t % 2; return { f: u > 0.4 && u < 1.3 ? [{ x: 30 + 20 * (u - 0.4), y: 30, off: true }] : [] } },
}
// застывший кадр — когда на карточку не навели: момент, где жест виден
const stills = { move: 0.9, accel: 2.2, tap: 0.1, pin: 0.1, right: 0.1, scroll: 0.5, inertia: 0.35, pinch: 0.9, drag: 0.9, hold: 0.9,
                 swipeSide: 0.4, swipeUp: 0.4, swipeDown: 0.4, rest: 0.6, jitter: 0, typing: 0.8 }

function pose(name, t, cfg) { return (demos[name] || demos.move)(t, cfg) }
function still(name) { return stills[name] !== undefined ? stills[name] : 0.5 }
