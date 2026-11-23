// Лента браузера (webbar_tabs в bcdesk.c): [звезда «Избранного»] вкладки — или строка адреса на месте открытой —
// и «+» за ними (на экране новой вкладки «+» нет — строка до края). Одна и та же на полосе окна и на панели (в режиме
// панели — без звезды). Ширина вкладки — по значку
// и имени, не шире 200; не влезают — все ужимаются общим потолком шагом 4 до 56; строка адреса — от 320 до всего
// свободного места, а у пустой вкладки — по слову «Поиск» или тому, что в ней печатают (WebField.hugW): правее —
// пустое место полосы, за него тянут окно. Между вкладками 2 и волосяная черта посередине; лишние уходят за правый край.
import QtQuick
import ".."

Item {
    id: strip
    property var st                       // WebState
    property bool withStar: true          // на полосе — звезда «Избранного», на панели её нет
    property bool bookmarksOpen: false
    signal starClicked()
    height: DeskStyle.appH
    clip: true

    FontMetrics { id: fm; font.family: DeskStyle.mono; font.pixelSize: 13 }

    function natural(t) {
        const icon = t.icon !== "" ? 24 : 0
        // +1 — запас: подпись Text чуть шире замера FontMetrics, и имя, которое влезает, обрезалось «…»
        return Math.max(30, Math.min(DeskStyle.webTabW, Math.ceil(12 + icon + fm.advanceWidth(st.tabLabel(t)) + 1 + 12)))
    }
    // ширины вкладок «как есть» — от вкладок, не от ширины ленты: при растягивании окна их не мерить заново (замер
    // шрифтом каждой подписи на каждом шаге — прогон стола 19 ноября 2026)
    readonly property var naturals: st.shownTabs.map(t => natural(t))
    // раскладка: x и ширина каждой вкладки (у той, где строка, — строки), «+». Открыт экран новой вкладки
    // (st.draftShown) — «+» нет, его место — строке
    readonly property var lay: {
        const tabs = st.shownTabs, field = st.fieldShown, naturals = strip.naturals
        const left = withStar ? DeskStyle.webGhostW + DeskStyle.itemGap : 0
        const right = width
        const n = tabs.length
        const plusRoom = st.draftShown ? 0 : DeskStyle.webGhostW + DeskStyle.itemGap
        const hugs = fieldBox.hugs, hugW = fieldBox.hugW
        const out = { xs: [], ws: [], fieldIndex: -1, fieldX: left, fieldW: 0, plusX: 0, plusShown: false }
        if (n === 0) {
            out.fieldW = Math.max(0, Math.min(hugW, right - left - plusRoom))
            out.plusX = left + out.fieldW + DeskStyle.itemGap
            out.plusShown = !st.draftShown && out.plusX + DeskStyle.webGhostW <= right
            return out
        }
        const nat = tabs.map((t, i) => t.id === st.shownActive && field ? 0 : naturals[i])
        const room = right - left - plusRoom - DeskStyle.itemGap * (n - 1)
        const need = !field ? 0 : hugs ? Math.min(hugW, DeskStyle.webActiveMin) : DeskStyle.webActiveMin
        let cap = DeskStyle.webTabW, used = 0
        for (;;) {
            used = nat.reduce((s, w) => s + (w > 0 ? Math.min(w, cap) : 0), 0)
            if (used + need <= room || cap <= DeskStyle.webTabMin)
                break
            cap = Math.max(DeskStyle.webTabMin, cap - 4)
        }
        const fieldW = Math.floor(hugs ? Math.min(hugW, Math.max(need, room - used)) : Math.max(need, room - used))
        let x = left
        for (let i = 0; i < n; ++i) {
            const isField = nat[i] === 0
            const w = isField ? fieldW : Math.floor(Math.min(nat[i], cap))
            out.xs.push(x)
            out.ws.push(w)
            if (isField) {
                out.fieldIndex = i
                out.fieldX = x
                out.fieldW = w
            }
            x += w + DeskStyle.itemGap
        }
        out.plusX = x
        out.plusShown = !st.draftShown && x + DeskStyle.webGhostW <= right
        return out
    }

    // звезда «Избранного»: открыт список — черта под ней и светлая
    BarItem {
        id: star
        visible: strip.withStar
        width: DeskStyle.webGhostW
        open: strip.bookmarksOpen
        onClicked: strip.starClicked()
        Glyph {
            x: star.width / 2; y: star.height / 2 + 0.8 + star.dy
            kind: "star"
            unit: 8.016 / 6
            line: 1.5
            color: star.hovered || strip.bookmarksOpen ? DeskStyle.text : DeskStyle.muted
        }
        Rectangle {
            visible: strip.bookmarksOpen
            x: DeskStyle.itemPad; y: DeskStyle.appH - 5 + star.dy
            width: star.width - 2 * DeskStyle.itemPad; height: 2
            color: DeskStyle.amber
        }
    }

    Repeater {
        model: strip.st.shownTabs.length
        delegate: Item {
            id: slot
            required property int index
            readonly property var info: strip.st.shownTabs[index] || null
            readonly property bool isField: index === strip.lay.fieldIndex
            x: strip.lay.xs[index] || 0
            width: strip.lay.ws[index] || 0
            height: DeskStyle.appH
            Rectangle {   // волосяная черта перед вкладкой (кроме первой) — посередине промежутка
                visible: slot.index > 0
                x: -DeskStyle.itemGap / 2
                y: 6; width: 1; height: DeskStyle.appH - 12
                color: DeskStyle.amberA(DeskStyle.line)
            }
            WebTab {
                visible: !slot.isField && slot.info !== null
                width: slot.width
                st: strip.st
                info: slot.info || { id: -1, url: "", title: "", pinned: false, loading: false, icon: "" }
                isActive: slot.info !== null && slot.info.id === strip.st.shownActive
            }
        }
    }

    WebField {
        id: fieldBox
        visible: strip.st.fieldShown && width > 0
        x: strip.lay.fieldX
        width: strip.lay.fieldW
        st: strip.st
    }

    BarItem {   // «+» — новая вкладка, как Ctrl+T
        id: plus
        visible: strip.lay.plusShown
        x: strip.lay.plusX
        width: DeskStyle.webGhostW
        onClicked: strip.st.newTab()
        Glyph {
            x: plus.width / 2; y: plus.height / 2 + plus.dy
            kind: "plus"
            unit: 1
            line: 1.5
            color: plus.hovered ? DeskStyle.text : DeskStyle.muted
        }
    }

    Connections {   // строка в фокус — у той ленты, что на виду
        target: strip.st
        function onFocusRequested(initial) { if (strip.visible) fieldBox.take(initial) }
        function onBlurRequested(refocus) { if (fieldBox.input.activeFocus) fieldBox.input.focus = false }
    }
}
