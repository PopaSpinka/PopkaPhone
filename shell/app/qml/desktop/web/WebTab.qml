// Вкладка на полосе браузера (webbar_tab в bcdesk.c): как программа на панели — без рамки, значок сайта 16 и имя
// моно 13; открытая — светлая подпись и янтарная черта снизу, грузится — по черте бежит отрезок, догрузилась —
// растекается (WebState.line). Под мышью — подложка .07 и у незакреплённой шириной от 90 — крестик. Нажатого вида
// нет: срабатывает на нажатии (WebState.tabPressed).
import QtQuick
import ".."

Item {
    id: tab
    property var st                  // WebState
    property var info: ({ id: -1, url: "", title: "", pinned: false, loading: false, icon: "" })
    property bool isActive: false
    height: DeskStyle.appH

    readonly property bool hovered: area.containsMouse
    readonly property bool closable: !info.pinned && width >= 90
    readonly property bool hasIcon: info.icon !== "" && icon.status !== Image.Error
    readonly property real closeX: width - 26
    readonly property bool overClose: closable && hovered && area.mouseX >= closeX && area.mouseX < closeX + 20
                                      && area.mouseY >= 5 && area.mouseY < 25

    Rectangle {
        anchors.fill: parent
        color: DeskStyle.amberA(DeskStyle.itemHover)
        visible: tab.hovered
    }
    Image {
        id: icon
        x: 12; y: 7
        width: 16; height: 16
        source: tab.info.icon
        sourceSize: Qt.size(32, 32)
        smooth: true
        mipmap: true
        visible: tab.hasIcon
    }
    Text {
        x: 12 + (tab.hasIcon ? 24 : 0)
        width: (tab.hovered && tab.closable ? tab.closeX - 4 : tab.width - 12) - x
        visible: width > 10
        height: tab.height
        verticalAlignment: Text.AlignVCenter
        text: tab.st.tabLabel(tab.info)
        elide: Text.ElideRight
        font.family: DeskStyle.mono
        font.pixelSize: 13
        color: tab.isActive || tab.hovered ? DeskStyle.text : DeskStyle.muted
    }

    // черта: открыта — сплошная; грузится — бегущий отрезок 40 %; догрузилась — растекается и гаснет
    Item {
        readonly property var ln: tab.st.line(tab.info, tab.isActive)
        x: 12; y: tab.height - 5
        width: tab.width - 24; height: 2
        clip: true
        visible: ln !== null
        Rectangle {
            id: seg
            readonly property var ln: parent.ln || { a: 0, b: 0, edge: 0, soft: 0, peak: 0 }
            x: ln.a * parent.width
            width: (ln.b - ln.a) * parent.width
            height: 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: DeskStyle.amberA(seg.ln.edge) }
                GradientStop { position: seg.ln.soft; color: DeskStyle.amberA(seg.ln.peak) }
                GradientStop { position: 1 - seg.ln.soft; color: DeskStyle.amberA(seg.ln.peak) }
                GradientStop { position: 1; color: DeskStyle.amberA(seg.ln.edge) }
            }
        }
    }

    // крестик: только под мышью; над самим крестиком — красный и подложка
    Item {
        visible: tab.closable && tab.hovered
        x: tab.closeX; y: 5
        width: 20; height: 20
        Rectangle {
            anchors.fill: parent
            radius: 2
            color: DeskStyle.badA(0.14)
            visible: tab.overClose
        }
        Glyph {
            x: 10; y: 10
            kind: "close"
            unit: 5 / 4.5
            line: 1.6
            color: tab.overClose ? DeskStyle.bad : DeskStyle.dim
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onPressed: (m) => {
            if (tab.info.id === -2)
                return   // ждём вкладку от браузера
            // крестик ловит щелчок всегда, даже невидимый (как у bcdesk)
            if (m.button === Qt.LeftButton && tab.closable && m.x >= tab.closeX && m.x < tab.closeX + 20)
                tab.st.close(tab.info.id)
            else
                tab.st.tabPressed(tab.info, m.button)
        }
    }
}
