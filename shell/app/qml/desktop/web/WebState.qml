// Браузер на столе — общее у полосы окна и панели (docs/WEB.md): вкладки от браузера (naomiWeb), строка адреса,
// бег черты «грузится», щелчки по вкладкам и команды браузеру. Как у bcdesk: строка адреса и вкладки одни —
// рисуются там, где сейчас полоса (над окном или на панели, в режиме панели).
import QtQuick
import ".."

QtObject {
    id: st

    // ---------- от браузера ----------
    readonly property var tabs: naomiWeb.tabs
    readonly property int active: naomiWeb.active
    readonly property var activeTab: tabs.find(t => t.id === active) || null
    readonly property var bookmarks: naomiWeb.bookmarks
    readonly property var downloads: naomiWeb.downloads

    // ---------- строка адреса ----------
    property bool focused: false          // в строке печатают (web_focused)
    property bool newTabPending: false    // «+» нажали, браузер ещё не прислал новую вкладку (web_new_tab_pending)
    property int pendingFrom: -1          // какая была открыта, когда нажали «+»
    property string sent: ""              // ушло Enter-ом из пустой вкладки (web_sent) — пока браузер не скажет адрес
    readonly property Timer sentTimer: Timer {
        interval: DeskStyle.webSentMs
        onTriggered: st.sent = ""
    }
    property int renaming: -1             // id закладки, которую переименовывают в списке «Избранного»
    readonly property bool typing: focused && renaming < 0
    // что рисует полоса: пока ждём вкладку после «+», она уже стоит в конце пустой и открытой — браузер добавит её
    // туда же, и полоса не дёрнется (у bcdesk полоса в это время просто не менялась)
    readonly property var shownTabs: newTabPending ? tabs.concat([{ id: -2, url: "", title: "", pinned: false,
                                                                    loading: false, icon: "" }]) : tabs
    readonly property int shownActive: newTabPending ? -2 : active
    readonly property var shownActiveTab: shownTabs.find(t => t.id === shownActive) || null
    // строка вместо открытой вкладки: в ней печатают или вкладка пустая новая (webbar_tabs: field)
    readonly property bool fieldShown: !shownActiveTab || typing || shownActiveTab.url === ""
    // открыт экран новой вкладки (черновик или ждём его после «+») — «+» не нужен: полоса его прячет (своё, НаомиФона)
    readonly property bool draftShown: newTabPending || (activeTab !== null && !!activeTab.draft)

    // стол: дать строке фокус (initial — текст, null — адрес открытой вкладки) / отпустить (refocus — клавиатуру
    // окну браузера)
    signal focusRequested(var initial)
    signal blurRequested(bool refocus)
    signal raiseRequested()               // окно браузера — вперёд (и из свёрнутых)

    function focusField(initial) {
        focused = true
        focusRequested(initial === undefined ? null : initial)
    }
    function blurField(refocus) {
        focused = false
        blurRequested(refocus)
    }
    // Enter в строке (web_field_submit)
    function submit(text) {
        blurField(true)
        if (!String(text).trim())
            return
        if (activeTab && activeTab.url === "") {
            sent = text
            sentTimer.restart()
        }
        naomiWeb.send({ type: "navigate", id: active, text: text })
        raiseRequested()
    }
    // «+» и Ctrl+T (webframe_new_tab): полоса не меняется, пока браузер не пришлёт вкладку, а клавиатура — уже строке.
    // Браузер пришлёт черновик (docs/WEB.md, «„+“ — экран, а не вкладка»); он уже открыт — только строка
    function newTab() {
        if (activeTab && activeTab.draft) {
            focusField("")
            return
        }
        newTabPending = true
        pendingFrom = active
        pendingTimer.restart()
        naomiWeb.send({ type: "new-tab" })
        focusField("")
    }
    readonly property Timer pendingTimer: Timer {
        interval: 1000
        onTriggered: st.newTabPending = false
    }

    // ---------- вкладки ----------
    property int tabClickId: -1
    property double tabClickAt: 0
    // имя вкладки: у закреплённой — из «Избранного» (та же страница, иначе тот же сайт), иначе заголовок, иначе адрес
    function site(url) {
        const m = String(url).match(/^[a-z][a-z0-9+.-]*:\/\/([^/?#:]+)/i)
        return m ? m[1].replace(/^www\./, "") : ""
    }
    function tabLabel(t) {
        if (t.pinned && t.url) {
            const b = bookmarks.find(b => b.url === t.url) || bookmarks.find(b => site(b.url) && site(b.url) === site(t.url))
            if (b && b.title)
                return b.title
        }
        return t.title || t.url
    }
    function bookmarked(url) { return bookmarks.some(b => b.url === url) }

    // нажатие по вкладке (webbar_tab_click): левая — открыть; по открытой обычной второй раз за 400 мс — строка
    // адреса; по открытой закреплённой — ничего; средняя — закрыть; правая — меню вкладки (menu(id) у полосы; у
    // черновика меню нет — закреплять и дублировать нечего). Вкладку выбрали, пока ждём черновик после «+», — туда,
    // даже если это та, с которой нажали «+»: черновик браузер сам бросит
    signal tabMenu(int id)
    function tabPressed(t, button) {
        if (button === Qt.MiddleButton) {
            close(t.id)
            return
        }
        if (button === Qt.RightButton) {
            if (t.draft)
                return
            naomiWeb.send({ type: "focus" })
            tabMenu(t.id)
            return
        }
        if (focused)
            blurField(false)
        const now = Date.now()
        if (t.id !== active || newTabPending) {
            newTabPending = false
            pendingTimer.stop()
            naomiWeb.send({ type: "activate", id: t.id })
        } else if (!t.pinned && tabClickId === t.id && now - tabClickAt < DeskStyle.doubleClickMs) {
            tabClickId = -1
            focusField(null)
            return
        }
        tabClickId = t.id
        tabClickAt = now
        raiseRequested()
    }
    function close(id) {
        if (id === active && focused)
            blurField(true)
        naomiWeb.send({ type: "close", id: id })
    }

    // ---------- бег «грузится» (web_tab_line) ----------
    // догрузилась — помним когда: за WEB_LOAD_SETTLE_MS отрезок растекается в черту у открытой и гаснет у прочих
    property var doneAt: ({})
    property var wasLoading: ({})
    property double now: Date.now()
    readonly property bool animating: {
        now
        return tabs.some(t => t.loading) || Object.keys(doneAt).some(id => now - doneAt[id] < DeskStyle.webLoadSettleMs)
    }
    onTabsChanged: {
        const t0 = Date.now(), done = {}, was = {}
        for (const t of tabs) {
            if (wasLoading[t.id] && !t.loading)
                done[t.id] = t0
            else if (doneAt[t.id] !== undefined && t0 - doneAt[t.id] < DeskStyle.webLoadSettleMs && !t.loading)
                done[t.id] = doneAt[t.id]
            was[t.id] = t.loading
        }
        doneAt = done
        wasLoading = was
        now = t0
        arrived()
    }
    onActiveChanged: arrived()
    // Пришла новая вкладка после «+» — полоса показывает её; ушёл адрес — «отправленное» больше не нужно. Вкладки и
    // открытая приходят одним сигналом (naomiWeb.tabsChanged), но tabs и active обновляются по очереди: в
    // onTabsChanged active был ещё прежний, ожидание не снималось, и черновик секунду стоял вкладкой «Новая вкладка»
    // рядом со строкой, пока не срабатывал pendingTimer (замер 20 ноября 2026). Поэтому — по обоим и с naomiWeb.active
    function arrived() {
        const id = naomiWeb.active
        if (newTabPending && id !== pendingFrom) {
            newTabPending = false
            pendingTimer.stop()
        }
        const t = tabs.find(o => o.id === id)
        if (sent && t && t.url !== "")
            sent = ""
    }
    readonly property Timer clock: Timer {   // 25 к/с, пока что-то грузится или растекается (WEB_LOAD_FRAME_MS)
        interval: 40
        repeat: true
        running: st.animating
        onTriggered: st.now = Date.now()
    }
    // черта под вкладкой: {a, b, edge, soft, peak} — отрезок a…b в долях черты (обрезается по ней), прозрачность
    // краёв, ширина «плато», пик; null — черты нет
    function line(t, isActive) {
        const cycle = DeskStyle.webLoadCycleMs
        const seg = (ms) => {
            const ph = (ms % cycle) / cycle
            const a = 0.6 * (-0.4 + 1.8 * ph)
            return { a: a, b: a + 0.4 }
        }
        if (t.loading) {
            const s = seg(now)
            return { a: s.a, b: s.b, edge: 0, soft: 0.5, peak: 1 }
        }
        const done = doneAt[t.id]
        if (done !== undefined && now - done < DeskStyle.webLoadSettleMs) {
            const e = 1 - Math.pow(1 - (now - done) / DeskStyle.webLoadSettleMs, 3)
            const s = seg(done)
            if (isActive)
                return { a: s.a + (0 - s.a) * e, b: s.b + (1 - s.b) * e, edge: e, soft: 0.5 * (1 - e), peak: 1 }
            return { a: s.a, b: s.b, edge: 0, soft: 0.5, peak: 1 - e }
        }
        return isActive ? { a: 0, b: 1, edge: 1, soft: 0, peak: 1 } : null
    }
}
