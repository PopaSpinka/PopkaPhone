// НаомиФон — снимок датчиков через QtSensors (бэкенд sensorfw).
// Слушает все датчики `seconds` секунд и печатает строку «NAOMI_JSON {…}»:
// для каждого — сколько показаний пришло, последнее, минимум и максимум.
// Запуск: QT_QPA_PLATFORM=offscreen qml sensors.qml   (часть device/hwtest.py)
import QtQuick 2.0
import QtSensors 5.2

Item {
    id: root
    property int seconds: 4
    property var results: ({})

    function rec(name, values) {
        var d = results[name] || { count: 0, last: null, min: {}, max: {} };
        d.count += 1;
        d.last = values;
        for (var k in values) {
            var v = values[k];
            if (typeof v !== "number") continue;
            if (d.min[k] === undefined || v < d.min[k]) d.min[k] = v;
            if (d.max[k] === undefined || v > d.max[k]) d.max[k] = v;
        }
        results[name] = d;
    }
    function xyz(r) { return { x: r.x, y: r.y, z: r.z }; }

    Accelerometer     { active: true; onReadingChanged: root.rec("accelerometer", root.xyz(reading)) }
    Gyroscope         { active: true; onReadingChanged: root.rec("gyroscope", root.xyz(reading)) }
    Magnetometer      { active: true; onReadingChanged: root.rec("magnetometer", root.xyz(reading)) }
    RotationSensor    { active: true; onReadingChanged: root.rec("rotation", root.xyz(reading)) }
    Compass           { active: true; onReadingChanged: root.rec("compass", { azimuth: reading.azimuth, calibration: reading.calibrationLevel }) }
    LightSensor       { active: true; onReadingChanged: root.rec("light", { lux: reading.illuminance }) }
    ProximitySensor   { active: true; onReadingChanged: root.rec("proximity", { near: reading.near ? 1 : 0 }) }
    PressureSensor    { active: true; onReadingChanged: root.rec("pressure", { pa: reading.pressure }) }
    OrientationSensor { active: true; onReadingChanged: root.rec("orientation", { orientation: reading.orientation }) }

    Timer {
        interval: root.seconds * 1000; running: true
        onTriggered: { console.log("NAOMI_JSON " + JSON.stringify(root.results)); Qt.quit(); }
    }
}
