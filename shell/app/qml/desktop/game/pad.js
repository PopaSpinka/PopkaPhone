// Кнопки геймпада (docs/GAMES.md, «Кнопки геймпада»): что у геймпада есть, как подписывать кнопки и клавиши, рисунок
// 8BitDo Ultimate — общее для окна (PadKeysWindow), рисунка (PadArt) и клавиатуры (PadKeyboard). Имена кнопок и
// клавиш — те же, что в раскладке оболочки (pad_keys.h).
.pragma library

// кнопки: группа, имя в окне, метка на клавише (кто её занял), стрелка направления
var CTL = {
    a: { g: "face", n: "A", s: "A" }, b: { g: "face", n: "B", s: "B" }, x: { g: "face", n: "X", s: "X" }, y: { g: "face", n: "Y", s: "Y" },
    du: { g: "dp", n: "вверх", s: "D↑", d: "↑" }, dd: { g: "dp", n: "вниз", s: "D↓", d: "↓" },
    dl: { g: "dp", n: "влево", s: "D←", d: "←" }, dr: { g: "dp", n: "вправо", s: "D→", d: "→" },
    l1: { g: "sh", n: "L1", s: "L1" }, r1: { g: "sh", n: "R1", s: "R1" }, l2: { g: "sh", n: "L2", s: "L2" }, r2: { g: "sh", n: "R2", s: "R2" },
    ls_u: { g: "ls", n: "вверх", s: "L↑", d: "↑" }, ls_d: { g: "ls", n: "вниз", s: "L↓", d: "↓" },
    ls_l: { g: "ls", n: "влево", s: "L←", d: "←" }, ls_r: { g: "ls", n: "вправо", s: "L→", d: "→" }, l3: { g: "ls", n: "нажать", s: "L3", d: "L3" },
    rs_u: { g: "rs", n: "вверх", s: "R↑", d: "↑" }, rs_d: { g: "rs", n: "вниз", s: "R↓", d: "↓" },
    rs_l: { g: "rs", n: "влево", s: "R←", d: "←" }, rs_r: { g: "rs", n: "вправо", s: "R→", d: "→" }, r3: { g: "rs", n: "нажать", s: "R3", d: "R3" },
    minus: { g: "mid", n: "−", s: "−" }, plus: { g: "mid", n: "+", s: "+" }, star: { g: "mid", n: "★ снимок", s: "★" },
    home: { g: "mid", n: "Home", s: "H" },
    p1: { g: "back", n: "P1 · правый лепесток", s: "P1" }, p2: { g: "back", n: "P2 · левый лепесток", s: "P2" }
}
// крестовина и стики — по направлениям: щёлкнули по ним — первое, дальше по кругу
var GROUPS = { dp: ["du", "dd", "dl", "dr"], ls: ["ls_u", "ls_d", "ls_l", "ls_r", "l3"], rs: ["rs_u", "rs_d", "rs_l", "rs_r", "r3"] }
var GNAME = { dp: "крестовина", ls: "левый стик", rs: "правый стик" }

function group(id) { return CTL[id] ? CTL[id].g : id }
function fullName(id) {
    var c = CTL[id]
    return c ? (GNAME[c.g] ? GNAME[c.g] + " · " : "") + c.n : ""
}

// клавиши: как подписать на клавиатуре (KL) и коротко — на кнопке геймпада (KS)
var KL = { Esc: "ESC", Backspace: "⌫", Tab: "TAB", Caps: "CAPS", Enter: "ENTER", Shift: "SHIFT", RShift: "SHIFT",
           Ctrl: "CTRL", RCtrl: "CTRL", Alt: "ALT", RAlt: "ALT", Space: "SPACE", Up: "↑", Down: "↓", Left: "←", Right: "→" }
var KS = { Esc: "ESC", Backspace: "⌫", Tab: "TAB", Caps: "CAPS", Enter: "ENT", Shift: "SHFT", RShift: "ПР.SH",
           Ctrl: "CTRL", RCtrl: "ПР.CT", Alt: "ALT", RAlt: "ПР.AL", Space: "SPC", Up: "↑", Down: "↓", Left: "←", Right: "→" }
function kname(k) { return k ? (KL[k] || k) : "" }
function kshort(k) { return k ? (KS[k] || k) : "" }

// ряды клавиатуры: [клавиша, ширина в клавишах]; ARROWS — стрелки, как у Magic Keyboard (↑ над ↓ — вполвысоты)
var ROWS = [
    [["Esc", 1], ["1"], ["2"], ["3"], ["4"], ["5"], ["6"], ["7"], ["8"], ["9"], ["0"], ["-"], ["="], ["Backspace", 2]],
    [["Tab", 1.5], ["Q"], ["W"], ["E"], ["R"], ["T"], ["Y"], ["U"], ["I"], ["O"], ["P"], ["["], ["]"], ["\\", 1.5]],
    [["Caps", 1.75], ["A"], ["S"], ["D"], ["F"], ["G"], ["H"], ["J"], ["K"], ["L"], [";"], ["'"], ["Enter", 2.25]],
    [["Shift", 2.25], ["Z"], ["X"], ["C"], ["V"], ["B"], ["N"], ["M"], [","], ["."], ["/"], ["RShift", 2.75]],
    [["Ctrl", 1.5], ["Alt", 1.25], ["Space", 6.5], ["RAlt", 1.25], ["RCtrl", 1.5], ["ARROWS", 3]]
]
// клавиша Qt (настоящая клавиатура, пока окно впереди) → клавиша раскладки; нет — ""
function fromQt(key) {
    if (key >= 0x41 && key <= 0x5a) return String.fromCharCode(key)        // A…Z
    if (key >= 0x30 && key <= 0x39) return String.fromCharCode(key)        // 0…9
    var m = { 0x01000000: "Esc", 0x01000003: "Backspace", 0x01000001: "Tab", 0x01000024: "Caps", 0x01000004: "Enter",
              0x01000005: "Enter", 0x01000020: "Shift", 0x01000021: "Ctrl", 0x01000023: "Alt", 0x20: "Space",
              0x01000013: "Up", 0x01000015: "Down", 0x01000012: "Left", 0x01000014: "Right", 0x2d: "-", 0x3d: "=",
              0x5b: "[", 0x5d: "]", 0x5c: "\\", 0x3b: ";", 0x27: "'", 0x2c: ",", 0x2e: ".", 0x2f: "/" }
    return m[key] || ""
}

// «как есть»: что кнопка делает сама — у Денди (Mesen: Y и X — турбо, левый стик — крестовина) и PS1 (DualShock по
// месту, Home — ANALOG; naomi-game)
var NATIVE = {
    nes: { a: "A", b: "B", x: "ТУРБО A", y: "ТУРБО B", minus: "SELECT", plus: "START", du: "↑", dd: "↓", dl: "←", dr: "→",
           ls_u: "↑", ls_d: "↓", ls_l: "←", ls_r: "→" },
    ps1: { a: "○", b: "✕", x: "△", y: "□", l1: "L1", r1: "R1", l2: "L2", r2: "R2", minus: "SELECT", plus: "START",
           home: "ANALOG", l3: "L3", r3: "R3", du: "↑", dd: "↓", dl: "←", dr: "→",
           ls_u: "СТИК", ls_d: "СТИК", ls_l: "СТИК", ls_r: "СТИК", rs_u: "СТИК", rs_d: "СТИК", rs_l: "СТИК", rs_r: "СТИК" }
}
// подпись стика или крестовины целиком: «СТРЕЛКИ», «WASD», клавиши по порядку ↑ ↓ ← →, нет ни одной — «—»
function groupSum(g, keys, nat, native) {
    var ids = GROUPS[g].slice(0, 4)
    if (native) {
        var v = nat[ids[0]]
        return v === "↑" ? "КРЕСТ" : v ? v : "—"
    }
    var ks = ids.map(function (i) { return keys[i] || "" })
    var j = ks.join()
    if (j === "Up,Down,Left,Right") return "СТРЕЛКИ"
    if (j === "W,S,A,D") return "WASD"
    if (j === "I,K,J,L") return "IJKL"
    if (ks.every(function (k) { return !k })) return "—"
    return ks.map(function (k) { return kshort(k) || "·" }).join(" ")
}

// ---------- рисунок: 8BitDo Ultimate спереди (точки — по фото и инструкции, поле 600 × 400) ----------
var RH = [[365, 78, 420, 80, 458, 88], [500, 96, 526, 116, 536, 150], [550, 200, 566, 280, 570, 318],
          [574, 356, 548, 382, 520, 378], [496, 375, 480, 356, 466, 334], [446, 304, 424, 288, 394, 284],
          [360, 280, 330, 278, 300, 278]]
// корпус: правая половина кривыми, левая — она же зеркалом
function body() {
    var pts = [[300, 78]], d = "M300 78", i, s
    for (i = 0; i < RH.length; i++) {
        s = RH[i]
        pts.push([s[4], s[5]])
        d += " C" + s[0] + " " + s[1] + " " + s[2] + " " + s[3] + " " + s[4] + " " + s[5]
    }
    for (i = RH.length - 1; i >= 0; i--) {
        s = RH[i]
        var S = pts[i]
        d += " C" + (600 - s[2]) + " " + s[3] + " " + (600 - s[0]) + " " + s[1] + " " + (600 - S[0]) + " " + S[1]
    }
    return d + " Z"
}
// лепесток сзади, под левой рукой (P2); P1 — он же зеркалом
var PADDLE = "M96 292 C112 288 124 300 122 322 C120 340 108 348 98 342 C88 336 86 300 96 292 Z"
function mirror(d) { return d.replace(/(\d+(?:\.\d+)?) (\d+(?:\.\d+)?)/g, function (m, x, y) { return (600 - Number(x)) + " " + y }) }
function star(cx, cy, r) {
    var p = ""
    for (var i = 0; i < 10; i++) {
        var a = -Math.PI / 2 + i * Math.PI / 5, rr = i % 2 ? r * 0.45 : r
        p += (i ? "L" : "M") + (cx + rr * Math.cos(a)).toFixed(1) + " " + (cy + rr * Math.sin(a)).toFixed(1)
    }
    return p + "Z"
}

// выноски (вариант 01 из макетов): плашка с клавишей слева или справа, пунктир — к кнопке (to, в поле рисунка;
// ex — где пунктир поворачивает); середина (− ★ Home +) — плашками со значками под корпусом
var LEFT = [{ id: "l2", y: 26, to: [124, 38] }, { id: "l1", y: 74, to: [100, 74] }, { id: "ls", y: 170, to: [146, 170] },
            { id: "dp", y: 236, to: [208, 236] }, { id: "p2", y: 330, to: [90, 324] }]
var RIGHT = [{ id: "r2", y: 26, to: [476, 38] }, { id: "r1", y: 74, to: [500, 74] }, { id: "x", y: 122, to: [434, 128], ex: 470 },
             { id: "a", y: 166, to: [470, 168] }, { id: "y", y: 212, to: [400, 165], ex: 474 },
             { id: "b", y: 258, to: [430, 216], ex: 452 }, { id: "rs", y: 304, to: [388, 262], ex: 420 },
             { id: "p1", y: 350, to: [506, 332], ex: 526 }]
var LNAME = { l2: "L2", l1: "L1", ls: "ЛЕВЫЙ СТИК", dp: "КРЕСТОВИНА", p2: "P2 · СЗАДИ", r2: "R2", r1: "R1", x: "X", a: "A",
              y: "Y", b: "B", rs: "ПРАВЫЙ СТИК", p1: "P1 · СЗАДИ" }
var MID = ["minus", "star", "home", "plus"]
