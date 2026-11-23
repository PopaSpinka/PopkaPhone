// Шильдик раздела страницы «Настроек»: номер, название янтарём, линия; справа на линии — значение раздела строчными
// приглушённым (info).
import QtQuick
import ".."

Item {
    id: head
    property string num
    property string title
    property string info
    height: DeskStyle.plateH
    Plate { width: parent.width; num: head.num; title: head.title; reserve: rightText.implicitWidth }
    Text {
        id: rightText
        anchors.right: parent.right
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        text: head.info
        font.family: DeskStyle.mono
        font.pixelSize: 12
        color: DeskStyle.muted
    }
}
