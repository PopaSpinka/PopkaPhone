// Подпись капсом с разрядкой — главный «служебный» шрифт НаомиОС (Google Sans Medium).
import QtQuick
import Naomi.Ui

Text {
    font.family: Theme.sans
    font.weight: Font.Medium
    font.pixelSize: 11
    font.letterSpacing: 1.2
    font.capitalization: Font.AllUppercase
    color: Theme.dim
}
