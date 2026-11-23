// «Терминал» стола — общее у полосы окна и панели (режим дока), как WebState у браузера: щелчки по вкладкам, «+»,
// переименование. Вкладки — naomiTerm (term_link.h).
import QtQuick

QtObject {
    id: st
    property int renaming: -1      // id вкладки, имя которой правят
    signal tabMenu(int id)         // правая кнопка по вкладке — меню у курсора (Desktop.qml)
    signal raiseRequested()        // окно терминала — вперёд, клавиатура — тексту

    readonly property var activeTab: naomiTerm.tabs.find(t => t.id === naomiTerm.active) || null

    // нажатие по вкладке: левая — открыть, средняя — закрыть, правая — меню
    function pressed(id, button) {
        if (button === Qt.MiddleButton) {
            close(id)
            return
        }
        if (button === Qt.RightButton) {
            tabMenu(id)
            return
        }
        if (renaming >= 0 && renaming !== id)
            renaming = -1
        naomiTerm.activate(id)
        raiseRequested()
    }
    function close(id) {
        if (renaming === id)
            renaming = -1
        naomiTerm.closeTab(id)
    }
    function startRename(id) {
        naomiTerm.activate(id)
        renaming = id
    }
    function rename(id, name) {
        naomiTerm.rename(id, name)
        renaming = -1
        raiseRequested()
    }
    function cancelRename() {
        renaming = -1
        raiseRequested()
    }
    // «+» — новая вкладка дома; Ctrl+Shift+T и «Дублировать» — в папке открытой
    function plus() {
        naomiTerm.newTab(-1, "")
        raiseRequested()
    }
    function newHere() {
        naomiTerm.newTab(naomiTerm.active, activeTab ? activeTab.cwd : "")
        raiseRequested()
    }
}
