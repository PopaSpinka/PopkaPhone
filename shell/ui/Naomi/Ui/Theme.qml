// НаомиФон — дизайн: палитра и типографика НаомиОС (docs/DESIGN.md там) на телефоне.
// Фон — чистый чёрный (экран AMOLED: чёрная точка не светится), янтарь, подписи капсом с разрядкой.
// Шрифты — Google (мягкие, скруглённые; SIL OFL, ставит tools/naomi fonts, docs/UI.md «Шрифты»).
// Размеры — в dp (логические точки Qt, 393 × 851).
pragma Singleton
import QtQuick

QtObject {
    readonly property color canvas: "#000000"     // фон всех экранов и панелей
    readonly property color surface: "#161614"
    readonly property color surfaceHigh: "#1b1b18"
    readonly property color text: "#ece9e3"
    readonly property color muted: "#a9a49a"
    readonly property color dim: "#7d786e"
    readonly property color amber: "#d8b25a"
    readonly property color amberEdge: Qt.rgba(216 / 255, 178 / 255, 90 / 255, 0.36)
    readonly property color amberFaint: Qt.rgba(216 / 255, 178 / 255, 90 / 255, 0.12)
    readonly property color teal: "#63b6b0"
    readonly property color bad: "#d9705f"
    readonly property color good: "#7fc98f"

    readonly property string sans: "Google Sans"         // текст и подписи: с кириллицей, насыщенность 400–700
    readonly property string clock: "Google Sans Flex"   // цифры часов: скругление (ROND) и любая насыщенность; кириллицы нет
    readonly property string mono: "Ubuntu Sans Mono"    // цифры столбиком: HUD

    readonly property int margin: 22          // поля экрана
    // полоса выреза камеры сверху (136 пикселей = 49,5 точки): строки состояния в ней нет — пустая; окна
    // программ, «Настройки» и шторка начинаются под ней (окно чата — от края: лента идёт под полосу)
    readonly property int statusBarHeight: 50

    // состояние сотовой сети (naomiPhone.cell.state, служба naomid) — словом
    function cellLabel(state) {
        return ({ nosim: "нет sim", searching: "поиск", unregistered: "нет сети", denied: "отказ сети",
                  off: "модем выкл", unknown: "неизвестно" })[state] ?? state
    }

    // столбики силы сигнала (Bars.level) по 0…100: 0 — ни одного, дальше по четверти
    function signalLevel(strength) { return strength <= 0 ? 0 : Math.min(4, 1 + Math.floor(strength / 25)) }
    // Wi-Fi одним словом (naomiPhone.wifi)
    function wifiLabel(wifi) {
        return !wifi.enabled ? "выкл" : wifi.state === "connected" ? "подключено"
             : wifi.state === "connecting" ? "подключаюсь" : "нет сети"
    }

    // куда идёт звук (naomiPhone.audio.output) — словом
    function outputLabel(output) {
        return ({ speaker: "динамик", earpiece: "разговорный", headphones: "наушники", bluetooth: "bluetooth",
                  usb: "usb", desk: "монитор" })[output] ?? "звук"
    }
    // вид устройства Bluetooth по значку BlueZ
    function deviceKind(icon) {
        return ({ "audio-headset": "гарнитура", "audio-headphones": "наушники", "audio-card": "колонка",
                  phone: "телефон", computer: "компьютер", "input-keyboard": "клавиатура", "input-mouse": "мышь",
                  "input-gaming": "геймпад", "input-tablet": "планшет", printer: "принтер",
                  "camera-video": "камера", "camera-photo": "камера" })[icon] ?? ""
    }

    // движение: открыть быстрее, чем закрыть; кривые без «пружинного» отскока
    readonly property int openMs: 240
    readonly property int closeMs: 200
    readonly property int easing: Easing.OutCubic
}
