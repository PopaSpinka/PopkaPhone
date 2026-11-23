// НаомиФон — снимок с камеры через QtMultimedia (бэкенд aalcamera → Android camera HAL).
// Нужен экран: окно с видоискателем на секунды появляется поверх Lomiri.
// @INDEX@ и @OUT@ (номер камеры и путь JPEG) подставляет device/hwtest.py перед запуском.
// Печатает «NAOMI_JSON {…}»: список камер, разрешение, путь снимка или ошибку.
import QtQuick 2.0
import QtMultimedia 5.6

Rectangle {
    id: root
    width: 540; height: 1170
    color: "black"
    property int index: @INDEX@
    property string out: "@OUT@"
    property bool shot: false

    function finish(extra) {
        var cams = [];
        for (var i = 0; i < QtMultimedia.availableCameras.length; i++) {
            var c = QtMultimedia.availableCameras[i];
            cams.push({ id: c.deviceId, name: c.displayName, position: c.position });
        }
        extra.cameras = cams;
        extra.index = index;
        console.log("NAOMI_JSON " + JSON.stringify(extra));
        Qt.quit();
    }

    Camera {
        id: cam
        deviceId: QtMultimedia.availableCameras.length > root.index
                  ? QtMultimedia.availableCameras[root.index].deviceId : ""
        captureMode: Camera.CaptureStillImage
        imageCapture.onImageSaved: root.finish({ saved: path, resolution: [cam.imageCapture.resolution.width, cam.imageCapture.resolution.height] })
        imageCapture.onCaptureFailed: root.finish({ error: "capture: " + message })
        onError: root.finish({ error: "camera " + errorCode + ": " + errorString })
        onCameraStatusChanged: {
            if (cameraStatus === Camera.ActiveStatus && !root.shot) {
                root.shot = true;
                shotDelay.start();  // дать автоэкспозиции секунду
            }
        }
    }
    Timer { id: shotDelay; interval: 1500; onTriggered: cam.imageCapture.captureToLocation(root.out) }
    VideoOutput { source: cam; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit }
    Timer { interval: 20000; running: true; onTriggered: root.finish({ error: "timeout", status: cam.cameraStatus }) }
}
