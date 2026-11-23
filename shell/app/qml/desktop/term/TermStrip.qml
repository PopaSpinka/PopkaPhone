// Лента вкладок «Терминала» (docs/TERMINAL.md): вкладки и «+» за ними — на полосе окна и на панели в режиме дока.
// Раскладка — как у браузера (WebStrip): ширина вкладки — по значку и имени, не шире 200; не влезают — все ужимаются
// общим потолком шагом 4 до 56; между вкладками 2 и волосяная черта посередине; лишние уходят за правый край.
import QtQuick
import ".."

Item {
    id: strip
    property var tabs: []          // naomiTerm.tabs
    property int active: -1
    property int renaming: -1      // id вкладки, которую переименовывают
    signal tabPressed(int id, int button)
    signal tabDoubleClicked(int id)
    signal tabClosed(int id)
    signal tabRenamed(int id, string name)
    signal renameCancelled()
    signal plusClicked()
    height: DeskStyle.appH
    clip: true

    FontMetrics { id: fm; font.family: DeskStyle.mono; font.pixelSize: 13 }

    // значок 14 + 8, имя, точка программы; +1 — запас: подпись Text чуть шире замера (как у WebStrip)
    readonly property var naturals: tabs.map(t => Math.max(56, Math.min(DeskStyle.webTabW,
        Math.ceil(12 + 22 + fm.advanceWidth(t.label) + 1 + 12 + (t.running ? 6 : 0)))))
    readonly property var lay: {
        const n = tabs.length, nat = naturals
        const room = width - (DeskStyle.webGhostW + DeskStyle.itemGap) - DeskStyle.itemGap * Math.max(0, n - 1)
        let cap = DeskStyle.webTabW
        for (;;) {
            const used = nat.reduce((s, w) => s + Math.min(w, cap), 0)
            if (used <= room || cap <= DeskStyle.webTabMin)
                break
            cap = Math.max(DeskStyle.webTabMin, cap - 4)
        }
        const out = { xs: [], ws: [], plusX: 0, plusShown: false }
        let x = 0
        for (let i = 0; i < n; ++i) {
            const w = Math.floor(Math.min(nat[i], cap))
            out.xs.push(x)
            out.ws.push(w)
            x += w + DeskStyle.itemGap
        }
        out.plusX = x
        out.plusShown = x + DeskStyle.webGhostW <= width
        return out
    }

    Repeater {
        model: strip.tabs.length
        delegate: Item {
            id: slot
            required property int index
            readonly property var info: strip.tabs[index] || null
            x: strip.lay.xs[index] || 0
            width: strip.lay.ws[index] || 0
            height: DeskStyle.appH
            Rectangle {   // волосяная черта перед вкладкой (кроме первой) — посередине промежутка
                visible: slot.index > 0
                x: -DeskStyle.itemGap / 2
                y: 6; width: 1; height: DeskStyle.appH - 12
                color: DeskStyle.amberA(DeskStyle.line)
            }
            TermTab {
                visible: slot.info !== null
                width: slot.width
                info: slot.info || { id: -1, label: "", pinned: false, running: false, cwd: "" }
                isActive: slot.info !== null && slot.info.id === strip.active
                renaming: slot.info !== null && slot.info.id === strip.renaming
                onPressed: (button) => strip.tabPressed(slot.info.id, button)
                onDoubleClicked: strip.tabDoubleClicked(slot.info.id)
                onCloseClicked: strip.tabClosed(slot.info.id)
                onRenamed: (name) => strip.tabRenamed(slot.info.id, name)
                onRenameCancelled: strip.renameCancelled()
            }
        }
    }

    Rectangle {   // черта перед «+»
        visible: strip.tabs.length > 0 && strip.lay.plusShown
        x: strip.lay.plusX - DeskStyle.itemGap / 2
        y: 6; width: 1; height: DeskStyle.appH - 12
        color: DeskStyle.amberA(DeskStyle.line)
    }
    BarItem {   // «+» — новая вкладка
        id: plus
        visible: strip.lay.plusShown
        x: strip.lay.plusX
        width: DeskStyle.webGhostW
        onClicked: strip.plusClicked()
        Glyph {
            x: plus.width / 2; y: plus.height / 2 + plus.dy
            kind: "plus"
            unit: 1
            line: 1.5
            color: plus.hovered ? DeskStyle.text : DeskStyle.muted
        }
    }
}
