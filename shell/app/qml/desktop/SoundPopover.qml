// Окно звука под значком на панели — перенос POPUP_SOUND НаомиОС (bcdesk sound_popover_draw, docs/DESKTOP.md «Звук»):
// модуль 288 × 110. Шильдик «01 ЗВУК 45%», справа в нём — MUTE (UNMUTE, янтарём, когда без звука) и значок выхода:
// щелчок — следующий выход. Выходов у стола два (своё, НаомиФона): «Динамики» — монитор приставки (колонки) и
// «Телефон»; монитора нет (смотрят с Mac) — один телефон. Ниже — эквалайзер (85 столбиков точками, SoundEqualizer) и
// громкость (DeskMeter): нажать и тянуть — даже за краем окна. Колесо над громкостью, MUTE и значком — ±5 за щелчок.
// Появляется как модули НаомиОС (anim_curve): растёт от верхнего края с 0,98 за 110 мс, уходит за 150 мс — сжимается
// до 0,99 и тает. Эквалайзер — с первого кадра окна, вместе с ним: звук «Динамиков» он берёт у трансляции (stream),
// столбики уже на месте; закрывается — столбики стоят и тают с окном.
import QtQuick
import Naomi.Desk

Item {
    id: pop
    property bool open: false
    property string output: ""        // naomid Output: desk — «Динамики» (монитор), speaker/earpiece — телефон
    property bool available: true     // naomid на связи (выход звука есть)
    property int volume: 0
    property bool muted: false
    property bool softVolume: false   // громкость выхода — в отсчётах (naomid SoftVolume): монитор выхода — после неё
    property QtObject stream          // трансляция (naomiStream): звук «Динамиков» для эквалайзера
    signal volumeSet(int percent)
    signal muteToggled()
    signal outputToggled()
    signal wheel(real delta)   // колесо: сдвиг в точках стола (щелчок колеса — 100), шаги считает стол

    width: 288
    height: 110

    // ---------- появление (anim_curve): t — ход открытия или закрытия, 0…1 ----------
    property real openT: 1
    property real closeT: 1
    onOpenChanged: {
        if (open) {
            closeAnim.stop()
            closeT = 1
            openT = 0
            openAnim.restart()
        } else {
            openAnim.stop()
            closeT = 0
            closeAnim.restart()
        }
    }
    NumberAnimation { id: openAnim; target: pop; property: "openT"; from: 0; to: 1; duration: 110 }
    NumberAnimation { id: closeAnim; target: pop; property: "closeT"; from: 0; to: 1; duration: 150 }
    visible: open || closeAnim.running
    transformOrigin: Item.Top
    scale: open ? 0.98 + 0.02 * (1 - Math.pow(1 - openT, 3)) : 1 - 0.01 * (1 - Math.pow(1 - closeT, 3))
    opacity: open ? 1 : (Math.exp(-closeT * 150 / 38) - Math.exp(-150 / 38)) / (1 - Math.exp(-150 / 38))

    MouseArea { anchors.fill: parent }   // щелчок по пустому месту окна — ничего, и не мимо него

    DeskModule { anchors.fill: parent }

    readonly property string glyph: output === "desk" ? "speaker"
                                  : output === "speaker" || output === "earpiece" ? "phone" : "headphones"

    // ---------- шильдик, MUTE и значок выхода ----------
    Text {
        id: muteText
        visible: false
        font.family: DeskStyle.mono
        font.pixelSize: 12   // у bcdesk 11,5 (cairo); у Qt размер в точках — целый
        font.letterSpacing: 11.5 * 0.08
        text: pop.muted ? "UNMUTE" : "MUTE"
    }
    readonly property real muteW: Math.ceil(muteText.implicitWidth) + 2 * 6
    readonly property real sourceX: 16 + 256 - 24
    Plate {
        x: 16; y: 14
        width: 256
        num: "01"
        title: "ЗВУК"
        value: pop.available ? pop.volume + "%" : ""
        valueColor: pop.muted ? DeskStyle.dim : DeskStyle.text
        reserve: 24 + pop.muteW
    }
    Item {
        id: mute
        visible: pop.available
        x: pop.sourceX - pop.muteW; y: 11
        width: pop.muteW; height: 24
        Text {
            x: 6
            height: parent.height
            verticalAlignment: Text.AlignVCenter
            font: muteText.font
            text: muteText.text
            color: pop.muted ? DeskStyle.amber : muteArea.containsMouse ? DeskStyle.text : DeskStyle.muted
        }
        // MouseArea, а не TapHandler: подложка окна (MouseArea) забирала нажатие себе, и щелчок не доходил
        MouseArea {
            id: muteArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: pop.muteToggled()
            onWheel: (e) => pop.wheel(e.pixelDelta.y !== 0 ? e.pixelDelta.y : e.angleDelta.y / 120 * 100)
        }
    }
    Item {
        id: source
        x: pop.sourceX; y: 11
        width: 24; height: 24
        Glyph {
            kind: pop.glyph
            unit: (DeskStyle.icon - DeskStyle.iconLine) / 13.4
            x: 12; y: 12 + (sourceArea.pressed ? 1 : 0)
            color: sourceArea.containsMouse ? DeskStyle.text : DeskStyle.muted
        }
        MouseArea {
            id: sourceArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: pop.outputToggled()
            onWheel: (e) => pop.wheel(e.pixelDelta.y !== 0 ? e.pixelDelta.y : e.angleDelta.y / 120 * 100)
        }
    }

    // ---------- эквалайзер ----------
    SoundEqualizer {
        x: 16; y: 46
        width: 256 + 1; height: 30 + 1
        visible: pop.available
        running: pop.available && pop.open
        output: pop.output
        stream: pop.stream
        // громкость в отсчётах («Монитор», динамик телефона): звук снят уже после неё — вернуть (кубом, как PulseAudio)
        gainDb: pop.softVolume && pop.volume > 0 ? -60 * Math.log10(pop.volume / 100) : 0
        color: DeskStyle.amber
    }
    Text {
        visible: !pop.available
        x: 16; y: 46
        height: 30
        verticalAlignment: Text.AlignVCenter
        font.family: DeskStyle.mono
        font.pixelSize: 13
        font.letterSpacing: 0.52
        text: "выход звука не найден"
        color: DeskStyle.muted
    }

    // ---------- громкость ----------
    DeskMeter {
        x: 16; y: 90
        width: 256
        value: pop.available ? pop.volume / 100 : 0
        fill: pop.muted ? DeskStyle.dim : DeskStyle.amber
        fillAlpha: pop.muted ? 0.5 : 0.75
        tick: pop.muted ? DeskStyle.muted : DeskStyle.amber
        tall: volumeArea.containsMouse || volumeArea.pressed
    }
    MouseArea {   // HIT_VOLUME: шире шкалы на 10 и выше на 14 — по ней легко попасть
        id: volumeArea
        enabled: pop.available
        x: 16 - 10; y: 90 - 14
        width: 256 + 20; height: 6 + 28
        hoverEnabled: true
        preventStealing: true
        function apply(mx) {
            const v = Math.round(Math.max(0, Math.min(1, (mx + x - 16) / 256)) * 100)
            if (Math.abs(v - pop.volume) >= 0.5 || pop.muted)
                pop.volumeSet(v)
        }
        onPressed: (e) => apply(e.x)
        onPositionChanged: (e) => { if (pressed) apply(e.x) }
        onWheel: (e) => pop.wheel(e.pixelDelta.y !== 0 ? e.pixelDelta.y : e.angleDelta.y / 120 * 100)
    }
}
