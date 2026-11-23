// Экраны проекта студии NaomiPhone поверх главного (docs/SHELL.md «Проект студии»). ShellView открывает их как
// программу "screen:<id>": тот же переход (без направления, 0,30 с), «домой» снизу и «назад» от края закрывают.
// Внутри — стопка экранов проекта, как Try студии (studio/qml/TryPhone.qml): касание слоя со ссылкой — следующий
// экран поверх (Naomi.Ui Axis без направления, 300 мс); «назад» от левого края — предыдущий (вбок, как «назад» из
// программы: 28 dp, 260 мс; backKind "sheet" — по отпусканию пальца). Ссылка на экран программы проекта или строка
// программы — настоящая программа телефона (openApp).
import QtQuick
import Naomi.Ui
import Naomi.Screens

Item {
    id: page
    property var project: null
    property string first: ""            // экран, с которого открыли
    property date now: new Date()
    signal openApp(string name)

    property var stack: []
    readonly property string topId: stack.length ? stack[stack.length - 1] : ""
    property string overId: ""           // верхний экран
    property string underId: ""          // тот, от которого или к которому идёт переход
    readonly property var apps: project && project.apps ? project.apps : []
    function reset() { stack = [first]; overId = first; underId = ""; ax.jump(1) }
    onFirstChanged: reset()
    Component.onCompleted: reset()          // (первое значение first приходит без onFirstChanged)

    Axis { id: ax; progress: 1; dir: -1; shift: 28; fade: true; onSettled: (at) => page.settled(at) }

    function screenById(id) {
        if (!project || !project.screens || !id) return null
        return project.screens.find(s => s.id === id) ?? null
    }
    // экран программы проекта (его строка ведёт туда) — значит, это программа телефона
    function appOf(to) {
        const a = apps.find(x => x.to === to)
        return a ? a.id : ""
    }
    function open(to, row) {
        if (row >= 0) {
            const a = apps[row]
            if (a && a.id) page.openApp(a.id)
            return
        }
        const app = appOf(to)
        if (app !== "") { page.openApp(app); return }
        if (!ax.atRest || !screenById(to) || to === topId) return
        underId = topId
        overId = to
        stack = stack.concat([to])
        ax.fade = true
        ax.jump(0)
        ax.goTo(1, 300, false)
    }
    function settled(at) {
        if (at === 1) { underId = ""; return }
        stack = stack.slice(0, -1)      // назад доехал: верхний — предыдущий
        overId = topId
        underId = ""
        ax.jump(1)
    }

    // жест «назад» оболочки (ShellView): свой экран назад, пока он есть; с первого — закрыть, как программу
    function backKind() { return stack.length > 1 && ax.atRest ? "sheet" : "" }
    function back() {
        if (stack.length < 2 || !ax.atRest) return
        overId = topId
        underId = stack[stack.length - 2]
        ax.fade = false
        ax.jump(1)
        ax.goTo(0, 260, false)
    }

    Rectangle { anchors.fill: parent; color: Theme.canvas }
    ScreenView {
        scr: page.screenById(page.underId)
        visible: page.underId !== "" && ax.oldAlpha > 0
        apps: page.apps
        device: ({ w: page.width, h: page.height, radius: 0, cutout: null })
        bezel: "transparent"
        now: page.now
        live: true
        opacity: ax.oldAlpha
        transform: Translate { x: ax.oldDelta }
        onOpen: (to, row) => page.open(to, row)
    }
    ScreenView {
        scr: page.screenById(page.overId)
        apps: page.apps
        device: ({ w: page.width, h: page.height, radius: 0, cutout: null })
        bezel: "transparent"
        now: page.now
        live: true
        opacity: page.underId !== "" ? ax.newAlpha : 1
        scale: page.underId !== "" ? ax.newScale : 1
        transform: Translate { x: page.underId !== "" ? ax.newDelta : 0 }
        onOpen: (to, row) => page.open(to, row)
    }
}
