// НаомиФон — оболочка: главный экран, программы, строка состояния, шторка, HUD. Экрана блокировки нет (решение
// пользователя, 12 ноября 2026): включили экран — главный; AOD — он же, притушенный (AodScreen).
// Слои снизу вверх: обои → главный экран → программа → строка состояния и зоны жестов → HUD → шторка → AOD.
// Программа — страница оболочки («Настройки», экраны проекта студии — "screen:<id>", ProjectPage) или окно
// отдельного процесса («Наоми», «Заметки»)
// (Wayland-клиент, Main.qml → addWindow). Жесты: снизу вверх — домой (программа уменьшается за пальцем;
// процесс остаётся жить); от левого края — назад (сначала клавиатура, потом back() страницы — её лист или
// раздел, потом закрыть программу); от строки состояния вниз — шторка.
// Клавиатура (Keyboard.qml) выезжает, когда поле ввода просит её (naomiKeyboard); окно программы на это
// время становится ниже. Кнопка питания гасит экран (C++: naomiPower); с AOD поверх всего — AodScreen, переход в
// него и обратно — aodAmount (см. там). Погас экран — оболочка на главном (программа живёт дальше, как после «домой»).
import QtQuick
import QtCore
import QtWayland.Compositor
import Naomi.Ui
import Naomi.Screens

Item {
    id: root

    property string app: ""                 // открытая программа; "" — главный экран
    property string settingsSection: ""     // раздел «Настроек» ("wifi", "bluetooth"), пока их страница ещё создаётся
    // Программа над главным — Naomi.Ui Axis (переходы Попка Студио для 7″, docs/SHELL.md «Переходы»):
    // 0 — главный, 1 — программа. Открыть — без направления (fade through, как экран поверх циферблата с
    // переходом Fade: 0,30 с, программа растёт с 0,92); «назад» — вбок, как назад из галереи (0,26 с по оставшемуся,
    // обратно — 0,16 с; сдвиг 28 dp ≈ 30 px платы); «домой» снизу вверх — по вертикали, как уход на циферблат
    // (0,38 с, 37 dp). Вид выбирается в начале перехода (useAxis)
    Axis { id: appAx; dir: -1; shift: 28; onSettled: (at) => root.appSettled(at) }
    property bool appAxisY: false
    property bool hudOn: false              // HUD разработчика — включается в шторке
    property date now: new Date()

    readonly property var external: []      // программы — отдельные процессы (их имена; сейчас таких нет)
    // страница открытой программы-страницы («Настройки», экран проекта) — ей жест «назад» (backKind …)
    readonly property var appPage: app === "settings" ? settingsLoader.item : appLoader.item

    // Проект студии NaomiPhone (naomiProject, docs/SHELL.md «Проект студии»): первый экран — главный (у проектов,
    // сделанных, пока была блокировка, первый был ею — тогда главный — их main); нет проекта — свои экраны. Шторка
    // тянется, только если на главном проекта есть её слой. Load студии меняет файл — экраны меняются сразу
    readonly property var project: naomiProject.loaded ? naomiProject.data : null
    readonly property var projectScreens: project && project.screens ? project.screens : []
    readonly property var projectHome: (project && project.main ? projectScreens.find(s => s.id === project.main) : null)
                                       ?? (projectScreens.length ? projectScreens[0] : null)
    // AOD проекта: его экран Always On (ready "aod"), нет — главный, притушенный (AodScreen)
    readonly property var projectAod: projectScreens.find(s => s.ready === "aod") ?? null
    // Обои проекта (System → Wallpaper студии; Naomi.Screens Wallpaper) — под главным: стоят на месте, пока над ним
    // открывается программа; в AOD (wall.aod, по умолчанию да) — притушенные, ещё темнее слоёв; иначе гаснут.
    // Картинки проекта — рядом с его файлом, в assets/
    readonly property var wall: {
        const w = project && project.functions ? project.functions.wallpaper : null
        return w && w.on && w.file ? w : null
    }
    readonly property bool wallHome: wall !== null && projectHome !== null
    readonly property bool wallAod: wall !== null && wall.aod !== false
    readonly property string assetBase: "file://" + naomiProject.path.replace(/[^\/]*$/, "") + "assets/"
    readonly property bool shadeAllowed: project === null
                                         || (projectHome !== null && projectHome.layers.some(l => l.type === "shade" && !l.hidden))
    // Экран проекта по ссылке слоя (с главного): поверх главного, как программа ("screen:<id>", ProjectPage);
    // экран программы проекта (его строка ведёт туда) — сама программа телефона
    function openScreen(id) {
        const apps = project && project.apps ? project.apps : []
        const a = apps.find(x => x.to === id)
        if (a && a.id) openApp(a.id)
        else if (projectScreens.some(s => s.id === id)) openApp("screen:" + id)
    }
    onProjectChanged: {
        if (project === null && app.startsWith("screen:")) closeApp()
        applyFunctions()
    }

    // Функции System проекта (Always On, кнопка питания, Wi-Fi, Bluetooth, клавиатура) — настройки телефона. Только
    // те, что заданы в проекте (в студии их меняли руками: project.functions[id], лишь тронутые поля); остальные —
    // как на телефоне. Load (studio.loaded новый, без live) ставит все заданные; Live и перезапуск оболочки — только
    // изменившиеся с прошлого раза (studio.ini): что поменяли на самом телефоне, остаётся до новой правки в студии
    Settings {
        id: applied
        location: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation) + "/naomi/studio.ini"
        category: "applied"
        property string functions: ""     // функции проекта, поставленные в прошлый раз (JSON)
        property string load: ""          // studio.loaded того Load
    }
    function applyFunctions() {
        if (!project || !naomiPhone.available) return   // (служба железа ещё не ответила — когда ответит)
        const fns = project.functions || {}
        const meta = project.studio || {}
        let before = {}
        try { before = applied.functions ? JSON.parse(applied.functions) : {} } catch (e) {}
        const freshLoad = !meta.live && (meta.loaded || "") !== applied.load
        for (const id of ["aod", "power", "wifi", "bt", "keyboard"]) {
            const f = fns[id]
            if (f === undefined || f === null) continue
            if (!freshLoad && JSON.stringify(f) === JSON.stringify(before[id])) continue
            applyFunction(id, f)
        }
        applied.functions = JSON.stringify(fns)
        if (!meta.live) applied.load = meta.loaded || ""
    }
    function applyFunction(id, f) {
        console.info("naomi: функция проекта", id, JSON.stringify(f))
        if (id === "aod") {
            if ("on" in f) naomiPower.aod = !!f.on
            if ("deep" in f) naomiPower.aodDeep = !!f.deep
            if ("after" in f) naomiPower.screenTimeout = f.after
        } else if (id === "power") {
            naomiPower.powerKey = f.on === false ? "none" : (f.act || "lock")
        } else if (id === "wifi") {
            if ("on" in f && !!f.on !== naomiPhone.wifi.enabled) naomiPhone.wifi.call("SetEnabled", [!!f.on])
        } else if (id === "bt") {
            if ("on" in f && !!f.on !== naomiPhone.bluetooth.powered) naomiPhone.bluetooth.call("SetPowered", [!!f.on])
        } else if (id === "keyboard") {
            if (f.langs) keyboard.langs = f.langs
        }
    }

    // окна программ-процессов: имя → { pid, xdgSurface, toplevel }
    property var windows: ({})
    readonly property var appWindow: windows[app] ?? null
    // у программы-процесса открыт свой раздел (заметка в блокноте): заголовок её окна начинается с «‹». Тогда
    // жест «назад» от края оболочка не берёт — касание достаётся окну, и раздел закрывает сама программа
    readonly property bool appOwnsBack: appWindow !== null && appWindow.toplevel.title.startsWith("\u2039")
    // программы во весь экран, и под вырезом камеры: окно — от верхнего края, что попадёт под вырез, программа
    // решает сама. Остальные — под полосой
    readonly property var underCutout: []
    // верх окна программы: под полосой выреза камеры, край на целом пикселе панели (50 точек — это 137,5
    // пикселя, встаёт на 138); у программ под вырезом — 0
    function appTop(name) {
        return underCutout.includes(name) || name.startsWith("screen:") ? 0
             : Math.round(Theme.statusBarHeight * naomiDensity) / naomiDensity
    }

    // Размер окна программы — в целых пикселях панели (у программы QT_SCALE_FACTOR = плотность,
    // docs/SHELL.md, «Окна программ»): всё ниже appTop, кроме клавиатуры. Окно оболочки в
    // точках округлено вверх (393 × 851 = 1080,75 × 2340,25 пикселя), поэтому экран — floor.
    function windowSize(name) {
        const d = naomiDensity
        return Qt.size(Math.floor(width * d),
                       Math.floor(height * d) - Math.round(appTop(name) * d) - (keyboardUp ? Math.round(keyboard.height * d) : 0))
    }
    function fitWindow() {
        if (appWindow)
            appWindow.toplevel.sendFullscreen(windowSize(app))
    }

    // Main.qml: новое окно клиента. Программу узнаём по pid процесса.
    function addWindow(toplevel, xdgSurface) {
        const pid = xdgSurface.surface.client.processId
        const name = Object.keys(running).find(n => running[n] === pid)
        if (!name) {
            console.warn("naomi: окно неизвестного процесса", pid)
            return
        }
        toplevel.sendFullscreen(windowSize(name))
        const w = Object.assign({}, windows)
        w[name] = { pid: pid, xdgSurface: xdgSurface, toplevel: toplevel }
        windows = w
    }
    function removeWindow(name) {
        const w = Object.assign({}, windows)
        delete w[name]
        windows = w
        if (app === name)
            closeApp()
    }
    property var running: ({})        // имя → pid запущенных программ

    // Фокус клавиатуры Wayland — только у открытой программы: у кого он есть, та активна. Без него поле
    // программы теряет фокус — курсор не мигает, клавиатура прячется, программа ничего не рисует.
    // Дома и при погасшем экране фокуса нет ни у кого.
    function setAppActive(active) {
        if (active) {
            if (appSurface.visible)
                appSurface.takeFocus()
        } else {
            root.forceActiveFocus()
            naomiApps.releaseFocus()
        }
    }
    Connections {
        target: naomiApps
        function onExited(pid, name, code) {
            const r = Object.assign({}, root.running)
            delete r[name]
            root.running = r
            root.removeWindow(name)
        }
    }

    // Открыть программу — один путь для всех: готова — сразу переход; нет — переход, когда будет готова.
    // Готова: у процесса («Наоми», «Заметки») уже есть окно с картинкой; страница «Настроек» создана (это
    // делается один раз, в фоне после запуска оболочки, — дальше она живёт, как процесс); страница экрана
    // проекта или заставка программы, которая только запускается, загрузилась (appLoader, в фоне: создание
    // прямо во время анимации стоило пропущенного кадра на каждое открытие).
    function openApp(name, section = "") {
        if (app.startsWith("screen:") && name !== app && appAx.atRest)
            appAx.jump(0)                   // с экрана проекта в программу: переход открытия заново, от главного
        if (name === "settings") {
            settingsSection = section
            if (settingsLoader.item && section !== "")   // на экране — переходом, спрятанные — сразу
                settingsLoader.item.openSection(section, app === "settings")
        }
        if (external.includes(name) && running[name] === undefined) {
            const pid = naomiApps.launch(name)
            if (pid < 0)
                return
            const r = Object.assign({}, running)
            r[name] = pid
            running = r
        }
        app = name
        if (appReady(name))
            startOpen()
    }
    function appReady(name) {
        if (name === "settings")
            return settingsLoader.status === Loader.Ready
        return drawn(name) || appLoader.status === Loader.Ready
    }
    function drawn(name) {          // у программы-процесса есть окно с картинкой
        const w = windows[name]
        return w !== undefined && w.xdgSurface.surface !== null && w.xdgSurface.surface.hasContent
    }
    // программа стала готова, пока ждали (окно ещё открывается — не трогать)
    function openWhenReady(name) {
        if (app === name && appAx.atRest && appAx.progress === 0)
            startOpen()
    }
    function useAxis(vertical, fade) {      // только с места: на ходу вид не меняется
        if (!appAx.atRest) return
        appAxisY = vertical
        appAx.fade = !!fade
        appAx.dir = vertical ? 1 : -1
        appAx.shift = vertical ? 37 : 28
    }
    function startOpen() {
        useAxis(false, true)
        appAx.goTo(1, 300, false)
    }
    // закрыть: «назад» — вбок, «домой» (byHome) — по вертикали
    function closeApp(byHome) {
        if (app === "")
            return
        setAppActive(false)
        useAxis(!!byHome)
        appAx.goTo(0, byHome ? 380 : 260, true)
    }
    function appSettled(at) {
        if (at === 0) app = ""
        else setAppActive(true)
    }
    // жест «домой» или «назад» с программы: палец ведёт переход (appAx), отпустили — решает Axis
    function appGestureBegin(byHome) {
        appAx.finish()                // прошлый переход ещё доезжал — доиграть сразу: новый жест — новый переход
        if (app === "" || appAx.progress < 1) return false
        useAxis(byHome)
        setAppActive(false)
        appAx.dragBegin()
        return true
    }
    function appGestureEnd(byHome, v) {
        if (appAx.decide(v) === 0) closeApp(byHome)
        else appAx.goTo(1, byHome ? 380 : 160, byHome)
    }

    // часы: обновляем раз в минуту и только при включённом экране или AOD — без лишних кадров
    Timer {
        interval: 1000; repeat: true; triggeredOnStart: true
        running: naomiPower.displayOn || naomiPower.dozing
        onTriggered: {
            const d = new Date()
            if (d.getMinutes() !== root.now.getMinutes() || d.getHours() !== root.now.getHours()
                    || d.getDate() !== root.now.getDate())
                root.now = d
        }
    }
    // датчики (naomid): свет — для автояркости, пока горит экран; движение — в AOD со второй ступенью (сдвинули —
    // панель из малого питания, отсчёт заново); экран погашен — датчики стоят
    readonly property bool wantLight: naomiPower.displayOn && naomiPower.autoBrightness
    readonly property bool wantMotion: naomiPower.watchMotion
    function watchSensors() {
        naomiPhone.sensors.call("Watch", [(wantLight ? ["light"] : []).concat(wantMotion ? ["motion"] : [])])
    }
    onWantLightChanged: watchSensors()
    onWantMotionChanged: watchSensors()
    readonly property var motion: naomiPhone.sensors.motion   // счётчик движений службы
    onMotionChanged: naomiPower.moved()
    Component.onCompleted: { watchSensors(); applyFunctions() }   // (Load перезапустил оболочку — его функции)
    Connections {
        target: naomiPhone
        function onAvailableChanged() { if (naomiPhone.available) root.watchSensors(), root.applyFunctions() }   // naomid перезапустилась
    }
    Binding { target: naomiPower; property: "ambientLux"; value: naomiPhone.sensors.light }

    // Переход в AOD и обратно: 0 — обычный экран, 1 — AOD; ход ведёт naomiPower на каждом кадре (вместе с
    // яркостью). Главный тускнеет на месте под чёрным поверх всего (AodScreen)
    readonly property real aodAmount: naomiPower.aodAmount

    Connections {
        target: naomiPower
        function onDisplayOnChanged() {
            if (!naomiPower.displayOn)
                root.goHome()
        }
    }
    // на главный сразу, без перехода (экран погас или ушёл в чёрный по пути в AOD): программа живёт дальше
    function goHome() {
        setAppActive(false)
        quick.hideNow()
        Qt.inputMethod.hide()
        if (app !== "") {
            appAx.jump(0)
            app = ""
        }
    }

    // главный и программа уходят из-под шторки (QuickSettings: contentDy, contentAlpha)
    Wallpaper {   // обои главного
        anchors.fill: parent
        visible: root.wallHome && quick.contentAlpha > 0 && opacity > 0
        wall: root.wall
        assetBase: root.assetBase
        opacity: quick.contentAlpha * (root.wallAod ? aod.wallIn : 1 - root.aodAmount)
    }
    Rectangle {   // в AOD обои ещё темнее слоёв (часы читаются)
        anchors.fill: parent
        color: Theme.canvas
        visible: root.wallHome && root.wallAod && opacity > 0
        opacity: 0.35 * root.aodAmount
    }
    HomeScreen {
        anchors.fill: parent
        // касания — только когда главный сверху: обработчики касаний слоёв не забирают касание себе целиком, и
        // касание по пустому месту экрана проекта доходило до строки программы под ним
        enabled: root.app === ""
        screen: root.projectHome
        projectApps: root.project && root.project.apps ? root.project.apps : []
        now: root.now
        glass: root.wallHome ? "transparent" : Theme.canvas
        shiftX: aod.shiftX; shiftY: aod.shiftY   // (в AOD — против выгорания)
        backdrop: aod.wallIn                     // (вторая ступень AOD: без панели «Background»)
        visible: appAx.oldAlpha > 0 && quick.contentAlpha > 0
        opacity: appAx.oldAlpha * quick.contentAlpha
        transform: Translate {
            x: root.appAxisY ? 0 : appAx.oldDelta
            y: (root.appAxisY ? appAx.oldDelta : 0) + quick.contentDy
        }
        onOpen: (name) => root.openApp(name)
        onOpenScreen: (id) => root.openScreen(id)
    }

    Item {
        id: appLayer
        anchors.fill: parent
        visible: root.app !== "" && appAx.newAlpha > 0 && quick.contentAlpha > 0
        opacity: appAx.newAlpha * quick.contentAlpha
        scale: appAx.newScale
        transform: Translate {
            x: root.appAxisY ? 0 : appAx.newDelta
            y: (root.appAxisY ? appAx.newDelta : 0) + quick.contentDy
        }
        // «Настройки» — как программа-процесс: создаются один раз, в фоне после запуска оболочки, и после
        // закрытия не выгружаются, а прячутся (на экране — только пока открыты; спрятанные ничего не опрашивают)
        Loader {
            id: settingsLoader
            anchors.fill: parent
            asynchronous: true
            visible: root.app === "settings"
            sourceComponent: SettingsPage {
                open: root.app === "settings"
                still: root.app === "settings" && appAx.atRest && appAx.progress === 1
                hudOn: root.hudOn
                onHudToggled: root.hudOn = !root.hudOn
                keyboardHeight: keyboard.height * root.keyboardShown
            }
            onLoaded: {
                item.openSection(root.settingsSection, false)
                root.openWhenReady("settings")
            }
        }
        // страница экрана проекта или заставка программы-процесса — только пока у той нет картинки (запускается)
        Loader {
            id: appLoader
            anchors.fill: parent
            active: root.app !== "" && root.app !== "settings" && !root.drawn(root.app)
            asynchronous: true
            sourceComponent: root.app.startsWith("screen:") ? projectPage : appSplash
            onLoaded: root.openWhenReady(root.app)
        }
        // окно программы-процесса поверх её заставки; касания и клавиатура — ему. Размер — по буферу
        // программы в её пикселях, на экран — уменьшено в плотность раз: изображение 1:1, а координаты
        // внутри элемента и есть координаты окна программы. Касания Qt отдаёт программе как есть, не
        // пересчитывая (мышь — пересчитывая); при размере в точках палец попадал в 2,75 раза ближе к углу.
        ShellSurfaceItem {
            id: appSurface
            visible: root.appWindow !== null && surface !== null && surface.hasContent
            shellSurface: root.appWindow ? root.appWindow.xdgSurface : null
            // xdg-интеграция Qt ставит окно «на весь экран» в угол вывода (0, 0) — двигаем пустышку,
            // а место окна задаёт оболочка
            moveItem: Item {}
            y: root.appTop(root.app)
            width: surface ? surface.destinationSize.width : 0
            height: surface ? surface.destinationSize.height : 0
            scale: 1 / naomiDensity
            transformOrigin: Item.TopLeft
            onSurfaceDestroyed: root.removeWindow(root.app)
            onVisibleChanged: if (visible) root.setAppActive(true)
        }
    }
    // заставка, пока программа запускается и не нарисовала первый кадр (потом её нет — appLoader: в переходах окно
    // программы полупрозрачно, и надпись заставки проступала сквозь него)
    Component {
        id: appSplash
        Rectangle {
            color: Theme.canvas
            Spinner { anchors.centerIn: parent }   // крутилка по центру, без надписи (просьба пользователя)
        }
    }
    Component {
        id: projectPage
        ProjectPage {
            project: root.project
            first: root.app.startsWith("screen:") ? root.app.slice(7) : ""
            now: root.now
            onOpenApp: (name) => root.openApp(name)
        }
    }

    // клавиатура оболочки — для её собственных полей («Настройки»); у программ-процессов клавиатура своя, в их
    // окне (Naomi.Ui: KeyboardPanel, docs/KEYBOARD.md) — оболочка их окна под клавиатуру не сжимает
    readonly property bool keyboardUp: naomiKeyboard !== null && naomiKeyboard.visible && naomiPower.displayOn
    property real keyboardShown: keyboardUp ? 1 : 0
    Behavior on keyboardShown {
        NumberAnimation { duration: root.keyboardUp ? keyboard.openMs : keyboard.closeMs; easing.type: Theme.easing }
    }
    onKeyboardUpChanged: {
        naomiKeyboard.setRect(0, keyboardUp ? height - keyboard.height : height, width, keyboardUp ? keyboard.height : 0)
        fitWindow()
    }
    Item {   // клавиатура оболочки — в тех же переходах, что и её страница (appLayer): ездит с ней одним окном
        anchors.fill: parent
        visible: root.app === "" ? root.keyboardUp : appLayer.visible
        opacity: root.app === "" ? 1 : appLayer.opacity
        scale: root.app === "" ? 1 : appLayer.scale
        transform: Translate {
            x: root.app === "" || root.appAxisY ? 0 : appAx.newDelta
            y: root.app === "" ? 0 : (root.appAxisY ? appAx.newDelta : 0) + quick.contentDy
        }
        Keyboard {
            id: keyboard
            width: parent.width
            height: implicitHeight
            y: parent.height - height * root.keyboardShown
            visible: root.keyboardShown > 0
            onHideRequested: Qt.inputMethod.hide()
        }
    }

    // жест «домой»: снизу вверх
    Item {
        anchors.bottom: parent.bottom
        width: parent.width; height: 30
        visible: root.app !== ""
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom; anchors.bottomMargin: 9
            width: 108; height: 4; radius: 2
            color: Theme.muted
            opacity: 0.7
        }
        // палец ведёт переход (путь 260 dp от места, где жест его подхватил); смахнул вверх — домой.
        // Путь — от положения пальца при подхвате (centroid.scenePosition), не translation: в Qt 6.10 она в
        // момент подхвата 0, а со следующим движением сразу считается от точки касания — переход прыгал на
        // весь порог жеста (10 dp) за кадр. Так во всех жестах оболочки
        DragHandler {
            target: null
            xAxis.enabled: false
            property bool on: false
            property real ty0: 0
            property real vy: 0       // последняя на ходу: к концу жеста Qt её обнуляет
            onActiveChanged: {
                if (active) {
                    on = root.appGestureBegin(true)
                    ty0 = centroid.scenePosition.y; vy = 0
                } else if (on) {
                    on = false
                    root.appGestureEnd(true, vy)
                }
            }
            onTranslationChanged: if (active && on) { vy = centroid.velocity.y; appAx.dragMove(centroid.scenePosition.y - ty0) }
        }
    }

    // жест «назад»: от левого края вправо (зона 30 dp, как в Попка Студио и Android 16). Что закрывает — по
    // порядку: клавиатуру, лист страницы, раздел программы (backKind() у страницы: "sheet", "page"), саму
    // программу. Раздел и программу палец ведёт (переход вбок); клавиатуру и лист — по отпусканию. Решение —
    // как у Axis: бросок вправо быстрее 650 dp/с или палец ушёл вправо хотя бы на 13 dp (5 % пути) — назад
    Item {
        y: Theme.statusBarHeight
        width: 30; height: parent.height - Theme.statusBarHeight - 30
        visible: root.app !== "" && !root.appOwnsBack
        DragHandler {
            target: null
            yAxis.enabled: false
            property string kind: ""   // keyboard, sheet, page, app; "" — жест ничей
            property real tx0: 0
            property real dx: 0        // по ходу: к onActiveChanged(false) Qt сдвиг уже обнулил
            property real vx: 0
            onActiveChanged: {
                if (active) {
                    tx0 = centroid.scenePosition.x; dx = 0; vx = 0
                    const page = root.appPage
                    kind = root.keyboardUp ? "keyboard" : page && page.backKind ? page.backKind() : ""
                    if (kind === "page") page.backBegin()
                    else if (kind === "") kind = root.appGestureBegin(false) ? "app" : ""
                    return
                }
                const go = vx > 650 || (vx > -650 && dx >= 13)
                if (kind === "keyboard") { if (go) Qt.inputMethod.hide() }
                else if (kind === "sheet") { if (go) root.appPage.back() }
                else if (kind === "page") root.appPage.backEnd(vx)
                else if (kind === "app") root.appGestureEnd(false, -vx)
                kind = ""
            }
            onTranslationChanged: {
                if (!active) return
                dx = centroid.scenePosition.x - tx0; vx = centroid.velocity.x
                if (kind === "page") root.appPage.backMove(dx)
                else if (kind === "app") appAx.dragMove(-dx)
            }
        }
    }

    // шторка: от строки состояния вниз (переход и решение, куда, — в QuickSettings)
    Item {
        width: parent.width; height: Theme.statusBarHeight + 12
        enabled: root.shadeAllowed
        DragHandler {
            target: null
            xAxis.enabled: false
            property real vy: 0
            onActiveChanged: active ? quick.dragBegin(centroid.scenePosition.y) : quick.dragEnd(vy)
            onTranslationChanged: if (active) { vy = centroid.velocity.y; quick.dragMove(centroid.scenePosition.y) }
        }
    }

    DevHud {
        visible: root.hudOn
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 2 * Theme.margin
        y: Theme.statusBarHeight + 2
    }

    QuickSettings {
        id: quick
        anchors.fill: parent
        hudOn: root.hudOn
        onHudToggled: root.hudOn = !root.hudOn
        onWifiRequested: {
            quick.close()
            root.openApp("settings", "wifi")
        }
        onBluetoothRequested: {
            quick.close()
            root.openApp("settings", "bluetooth")
        }
        onScreensRequested: {
            quick.close()
            root.openApp("settings", "screens")
        }
    }

    // кнопки громкости (ловит naomiPower до программ) → служба звука; громкость поверх всего
    Connections {
        target: naomiPower
        function onVolumeKey(direction) {
            naomiPhone.audio.call(direction > 0 ? "VolumeUp" : "VolumeDown")
            volumeOsd.show()
        }
    }
    VolumeOsd {
        id: volumeOsd
        anchors.horizontalCenter: parent.horizontalCenter
        y: Theme.statusBarHeight + 46
    }

    AodScreen {
        id: aod
        anchors.fill: parent
        visible: root.aodAmount > 0
        amount: root.aodAmount
        screen: root.projectAod
        // на экране главный, ничего поверх: AOD — на месте; иначе — через чёрный, под ним — на главный
        homeUnder: root.app === "" && appAx.atRest && quick.contentAlpha >= 1
        deep: naomiPower.deep
        now: root.now
        onCovered: root.goHome()
    }
}
