// Layer "weather" — the weather of a city (d.city: {name, lat, lon}; none — London), from
// Open-Meteo (open-meteo.com, no key, CC BY 4.0) — asked by the layer itself, again every 30 minutes, on the phone and in
// the studio alike; no answer — "—°". In one of four looks (d.look):
//   line  — capitals as the date's: "+7° · облачно · нью-йорк";
//   big   — the icon and the temperature in big light digits (Google Sans Flex), the sky and today's high and low under;
//   card  — a rounded card (Theme.surface): the icon, the temperature, the sky and the city, today's high and low;
//   hours — the next five hours: the hour, the icon, the temperature.
// Icons — Material Symbols Rounded by the WMO weather code. Props (the studio's state.js KINDS.weather): look, scale,
// color (the temperature, the icons), color2 (the rest), city; dx, dy. By default centered at 40 % of the height. Anchor —
// its middle.
import QtQuick
import Naomi.Ui

Item {
    id: root
    property var d: ({})
    property real sw: 393
    property real sh: 851
    property var now: new Date()
    property bool live: false
    signal tapped(int row)

    readonly property string look: d.look ?? "big"
    readonly property real k: d.scale ?? 1
    readonly property var city: d.city ?? ({ name: "Лондон", lat: 51.50853, lon: -0.12574 })
    readonly property color main: d.color ?? Theme.text
    readonly property color rest: d.color2 ?? Theme.muted
    readonly property real baseX: sw / 2
    readonly property real baseY: sh * 0.40
    width: body.item ? body.item.width : 0
    height: body.item ? body.item.height : 0
    x: baseX + (d.dx ?? 0) - width / 2
    y: baseY + (d.dy ?? 0) - height / 2

    // ---------- the data ----------
    property var wx: null          // {temp, code, day, hi, lo, hours: [{h, temp, code, day}]}
    readonly property string where: city.lat + "," + city.lon
    onWhereChanged: ask()
    Component.onCompleted: ask()
    Timer { interval: 30 * 60 * 1000; repeat: true; running: true; onTriggered: root.ask() }
    function ask() {
        const c = city
        const x = new XMLHttpRequest()
        x.onreadystatechange = () => {
            if (x.readyState !== XMLHttpRequest.DONE || x.status !== 200) return
            try {
                const j = JSON.parse(x.responseText), cur = j.current, hr = j.hourly
                const t0 = hr.time.indexOf(cur.time.slice(0, 13) + ":00"), hours = []
                for (let i = Math.max(0, t0 + 1); i < hr.time.length && hours.length < 5; i++)
                    hours.push({ h: hr.time[i].slice(11, 13), temp: hr.temperature_2m[i], code: hr.weather_code[i], day: hr.is_day[i] })
                root.wx = { temp: cur.temperature_2m, code: cur.weather_code, day: cur.is_day,
                            hi: j.daily.temperature_2m_max[0], lo: j.daily.temperature_2m_min[0], hours: hours }
            } catch (e) { console.warn("naomi: погода не разобрана:", e) }
        }
        x.open("GET", "https://api.open-meteo.com/v1/forecast?latitude=" + c.lat + "&longitude=" + c.lon
               + "&current=temperature_2m,weather_code,is_day&hourly=temperature_2m,weather_code,is_day"
               + "&daily=temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=2")
        x.send()
    }
    function deg(t) { return t === undefined || t === null ? "—°" : (Math.round(t) > 0 ? "+" : "") + Math.round(t) + "°" }
    function bare(t) { return t === undefined || t === null ? "—°" : Math.round(t) + "°" }
    // the WMO code → [icon, the sky in words]
    function sky(code, day) {
        if (code === undefined || code === null) return ["cloud", "погода…"]
        if (code === 0) return [day ? "sunny" : "clear_night", "ясно"]
        if (code <= 2) return [day ? "partly_cloudy_day" : "partly_cloudy_night", "малооблачно"]
        if (code === 3) return ["cloud", "облачно"]
        if (code === 45 || code === 48) return ["foggy", "туман"]
        if (code >= 51 && code <= 57) return ["rainy", "морось"]
        if (code >= 61 && code <= 67) return ["rainy", "дождь"]
        if (code >= 71 && code <= 77) return ["weather_snowy", "снег"]
        if (code >= 80 && code <= 82) return ["rainy", "ливень"]
        if (code >= 85 && code <= 86) return ["weather_snowy", "снегопад"]
        if (code >= 95) return ["thunderstorm", "гроза"]
        return ["cloud", ""]
    }
    readonly property var nowSky: sky(wx ? wx.code : null, wx ? wx.day : 1)

    Loader {
        id: body
        sourceComponent: ({ line: line, card: card, hours: hours })[root.look] ?? big
    }

    // (an icon is a Text here, not an inline component: one of those can't see the file's ids)

    Component {
        id: line
        Caps {
            text: [root.deg(root.wx ? root.wx.temp : null), root.nowSky[1], root.city.name].filter(s => s).join(" · ")
            font.pixelSize: Math.round(12 * root.k)
            font.letterSpacing: 1.2 * root.k
            color: root.rest
        }
    }

    Component {
        id: big
        Column {
            spacing: 4 * root.k
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 8 * root.k
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.nowSky[0]
                    font.family: "Material Symbols Rounded"
                    font.pixelSize: Math.round(38 * root.k)
                    font.variableAxes: ({ "FILL": 1, "wght": 400 })
                    color: root.main
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.bare(root.wx ? root.wx.temp : null)
                    font.family: Theme.clock
                    font.pixelSize: Math.round(56 * root.k)
                    font.variableAxes: ({ "wght": 300, "ROND": 100 })
                    color: root.main
                }
            }
            Caps {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.nowSky[1] + (root.wx ? " · " + root.bare(root.wx.hi) + " / " + root.bare(root.wx.lo) : "")
                font.pixelSize: Math.round(11 * root.k)
                font.letterSpacing: 1.2 * root.k
                color: root.rest
            }
        }
    }

    Component {
        id: card
        Rectangle {
            width: 300 * root.k; height: 84 * root.k
            radius: 22 * root.k
            color: Theme.surface
            Text {
                x: 18 * root.k
                anchors.verticalCenter: parent.verticalCenter
                text: root.nowSky[0]
                font.family: "Material Symbols Rounded"
                font.pixelSize: Math.round(34 * root.k)
                font.variableAxes: ({ "FILL": 1, "wght": 400 })
                color: root.main
            }
            Column {
                x: 64 * root.k
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2 * root.k
                Text {
                    text: root.bare(root.wx ? root.wx.temp : null)
                    font.family: Theme.sans; font.pixelSize: Math.round(28 * root.k); font.weight: Font.Medium
                    color: root.main
                }
                Text {
                    text: root.nowSky[1] + " · " + root.city.name
                    font.family: Theme.sans; font.pixelSize: Math.round(13 * root.k)
                    color: root.rest
                }
            }
            Column {
                anchors.right: parent.right; anchors.rightMargin: 18 * root.k
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4 * root.k
                Caps { anchors.right: parent.right; text: "макс " + root.bare(root.wx ? root.wx.hi : null); font.pixelSize: Math.round(10 * root.k); color: root.rest }
                Caps { anchors.right: parent.right; text: "мин " + root.bare(root.wx ? root.wx.lo : null); font.pixelSize: Math.round(10 * root.k); color: root.rest }
            }
        }
    }

    Component {
        id: hours
        Row {
            spacing: 18 * root.k
            Repeater {
                model: root.wx ? root.wx.hours : [{}, {}, {}, {}, {}]
                Column {
                    required property var modelData
                    spacing: 6 * root.k
                    Caps {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.h ?? "—"
                        font.pixelSize: Math.round(10 * root.k)
                        color: root.rest
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.sky(modelData.code, modelData.day ?? 1)[0]
                        font.family: "Material Symbols Rounded"
                        font.pixelSize: Math.round(22 * root.k)
                        font.variableAxes: ({ "FILL": 1, "wght": 400 })
                        color: root.main
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: root.bare(modelData.temp)
                        font.family: Theme.sans; font.pixelSize: Math.round(15 * root.k); font.weight: Font.Medium
                        color: root.main
                    }
                }
            }
        }
    }

    function box() { return [0, 0, width, height] }
    TapHandler { enabled: root.live && !!root.d.tap; onTapped: root.tapped(-1) }
}
