// Выбор «‹ значение ›» из нескольких: кнопки — как у уставки (Stepper), между ними значение моно-капсом янтарём;
// щелчок по кнопке — следующее или предыдущее по кругу. options — [{ v, name }], value — выбранное v.
import QtQuick
import ".."

Item {
    id: c
    property var options: []
    property var value
    signal moved(var value)
    height: 30
    readonly property int index: {
        for (let i = 0; i < options.length; ++i)
            if (options[i].v === value)
                return i
        return 0
    }
    function go(dir) {
        const n = options.length
        if (n)
            moved(options[(index + dir + n) % n].v)
    }

    Stepper.Btn { label: "‹"; onClicked: c.go(-1) }
    Text {
        x: 34
        width: c.width - 68
        height: c.height
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        text: c.options.length ? c.options[c.index].name.toUpperCase() : ""
        font.family: DeskStyle.mono
        font.pixelSize: 12
        font.letterSpacing: 12 * 0.1
        color: DeskStyle.amber
    }
    Stepper.Btn { x: c.width - width; label: "›"; onClicked: c.go(1) }
}
