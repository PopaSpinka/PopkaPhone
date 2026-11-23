// Пружина SwiftUI spring(response, dampingFraction) — общая для программ (чат, блокнот): масса 1, жёсткость (2π/response)², трение
// 4π·damping/response. Считается точной формулой затухающих колебаний от текущего положения и скорости —
// одинаково при любой частоте кадров; смена цели на лету сохраняет скорость (как в SwiftUI).
import QtQuick

Item {
    id: s
    visible: false

    property real response: 0.32
    property real damping: 0.88          // flowAnimation чата (ChatViewModel.swift:53)
    property real epsilon: 0.001         // ближе — пришли
    property real value: 0
    property real velocity: 0
    property real target: 0
    readonly property bool running: frames.running
    signal settled()

    function to(t) {
        target = t
        if (Math.abs(value - t) < epsilon && Math.abs(velocity) < epsilon * 10) {
            value = t
            velocity = 0
            return
        }
        frames.start()
    }
    function jump(t) {
        frames.stop()
        target = t
        value = t
        velocity = 0
    }

    function step(dt) {
        const w = 2 * Math.PI / response, z = damping
        const d = value - target, v = velocity
        let x, nv
        if (z < 1) {
            const wd = w * Math.sqrt(1 - z * z), e = Math.exp(-z * w * dt)
            const c = Math.cos(wd * dt), sn = Math.sin(wd * dt)
            x = e * (d * c + (v + z * w * d) / wd * sn)
            nv = e * (v * c - (z * w * v + w * w * d) / wd * sn)
        } else {
            const e = Math.exp(-w * dt)
            x = e * (d + (v + w * d) * dt)
            nv = e * (v - w * (v + w * d) * dt)
        }
        if (Math.abs(x) < epsilon && Math.abs(nv) < epsilon * 10) {
            frames.stop()
            value = target
            velocity = 0
            settled()
            return
        }
        value = target + x
        velocity = nv
    }

    FrameAnimation {
        id: frames
        onTriggered: s.step(frameTime)
    }
}
