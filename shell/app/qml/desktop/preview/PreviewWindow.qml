// «Просмотр» рабочего стола — перенос naomi-preview НаомиОС (src/preview.c; docs/DESKTOP.md, «Просмотр»): пробел
// в «Файлах» (или двойной щелчок по фото и видео), щелчок по миниатюре скриншота. Окно: полоса 40 — имя, «4000 × 3000
// · JPEG · 3,9 МБ» и крестик; фото — по центру, окно под его пропорции до 80 % стола (не меньше 480 × 280), мелкое —
// в родном размере, пиксель фото в пиксель кадра; появляется посередине, сменили фото — центр стоит на месте.
// Видео играет сразу, со звуком: щелчок — пауза и снова (доиграло — с начала), у нижнего края — перемотка по полосе
// хода, на паузе — ▶; на полосе окна — «0:05 / 0:12». Остальное — карточка с именем и типом.
// Клавиши — как у НаомиОС: ← ↑ / → ↓ — соседний файл (открыли из «Файлов» — их выделение идёт следом, не фото и не
// видео пропускаются; иначе — фото и видео папки по имени), пробел и Esc — закрыть, F и двойной щелчок по фото — на
// весь стол (Esc — обратно), Alt+Backspace — в корзину и дальше соседний.
// Чтобы было мгновенно: окно живёт и после закрытия (прячется; память фото и видео отпускается); размер фото — из
// заголовка файла, окно сразу нужной формы; пока фото грузится — плитка «Файлов» того же файла из кэша Qt,
// растянутая; фото декодируется под размер стола (JPEG — сразу уменьшенным); соседние — заранее (до двух вперёд
// по ходу листания и одно назад). Видео — naomiPreview.video (GStreamer, кадры NV12 → шейдер nv12.frag).
import QtQuick
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import Naomi.Preview
import ".."

DeskWindow {
    id: pv
    closeOnly: true
    chromeless: full
    minimumSize: Qt.size(480, 280)          // MIN_W × MIN_H
    property Item desk
    property Item source: null              // окно «Файлов», откуда открыли: листают его выделением
    property var folderFiles: []            // открыли не из «Файлов» — фото и видео папки по имени
    property Item back: null                // у кого был фокус клавиш (открыли не из «Файлов»)
    property string path
    property var info: ({})
    property var infos: ({})                // сведения о файлах, что уже смотрели и грузим (пока окно открыто)
    property bool full: false               // на весь стол
    property rect normalBox                 // окно до «на весь стол»
    property int lastStep: 1                // куда листали — туда и подгружаем
    property bool waitingFrame: false       // видео: окно ждёт первый кадр (его размер), не дольше 600 мс
    readonly property bool isOpen: shown && !closing
    readonly property string kind: info.kind || ""
    readonly property real s: naomiDesk.pixelScale   // пикселей кадра в точке — как сцена ложится в кадр
    readonly property PreviewVideo video: naomiPreview.video

    readonly property int fitPercent: 80    // FIT — фото занимает до такой доли стола
    readonly property int loadingMs: 300    // LOADING_MS — ещё грузится: подпись «ЗАГРУЖАЮ»
    readonly property int videoWaitMs: 600  // VIDEO_WAIT_MS
    readonly property int seekZone: 32      // VIDEO_SEEK_ZONE — щелчок у нижнего края — перемотка
    readonly property int playR: 26         // VIDEO_PLAY_R — ▶ на паузе

    function url(p) { return "file://" + encodeURIComponent(p).replace(/%2F/g, "/") }
    function infoOf(p) {
        if (!infos[p])
            infos[p] = naomiPreview.info(p)
        return infos[p]
    }

    // ---------- открыть, показать, закрыть ----------
    function open(src, p) {
        const fresh = !isOpen
        if (fresh) {
            parked = false   // создано спрятанным (Desktop.qml)
            back = src ? null : Window.activeFocusItem
            closing = false
            full = false
            if (maximized)
                toggleMaximize()
        }
        source = src
        folderFiles = src ? [] : naomiPreview.folder(p)
        show(p, fresh)
        shown = true
        keys.forceActiveFocus()
    }
    function show(p, fresh) {
        if (p === path && isOpen && !fresh)
            return
        video.close()
        info = infoOf(p)   // сначала сведения, потом путь: привязки по сведениям (GIF, плитка) берут путь из них же
        path = p
        shownAt = Date.now()
        loading = false
        loadingTimer.restart()
        if (kind === "image") {
            place(fitSize(info.width, info.height), fresh)
            loadImages()
        } else if (kind === "video") {
            video.open(p)
            if (fresh) {
                // окно — по первому кадру (он через 0,15–0,3 с), не дольше 600 мс
                place(Qt.size(Math.round(desk.width * 0.6), Math.round(desk.height * 0.6)), true)
                waitingFrame = true
                frameWait.restart()
            }
        } else {
            place(Qt.size(640, 340), fresh)   // CARD_W × CARD_H
        }
    }
    function hide() {
        if (!isOpen)
            return
        video.close()
        close()
        if (source)
            source.focusKeys()
        else if (back)
            back.forceActiveFocus()
        back = null
    }
    onClosed: {   // спрятано (анимация кончилась): окно остаётся, память — обратно
        closing = false
        full = false
        path = ""
        info = ({})
        infos = ({})
        source = null
        for (let i = 0; i < slots.count; ++i)
            slots.itemAt(i).file = ""
    }
    onActivated: keys.forceActiveFocus()
    function detachSource() {   // окно «Файлов» закрыли — дальше листаем папку сами
        if (!source)
            return
        source = null
        folderFiles = naomiPreview.folder(path)
    }

    // ---------- размер и место ----------
    // Окно под фото w × h пикселей: до 80 % стола, мелкое — в родном размере (window_fit_scaled)
    function fitSize(w, h) {
        const boxW = desk.width * fitPercent / 100, boxH = desk.height * fitPercent / 100 - 40
        const iw = w / s, ih = h / s
        const k = Math.min(1, boxW / iw, boxH / ih)
        return Qt.size(Math.max(Math.round(iw * k), 480), Math.max(Math.round(ih * k) + 40, 280))
    }
    // Новый размер — центр на месте (впервые — посередине стола); развёрнутое и на весь стол не трогаем
    function place(size, fresh) {
        if (full) {
            normalBox = boxAround(size, fresh ? null : Qt.point(normalBox.x + normalBox.width / 2,
                                                                normalBox.y + normalBox.height / 2))
            return
        }
        if (maximized)
            return
        const r = boxAround(size, fresh ? null : Qt.point(x + width / 2, y + height / 2))
        x = r.x; y = r.y; width = r.width; height = r.height
    }
    function boxAround(size, center) {
        const c = center || Qt.point(area.width / 2, area.height / 2)
        return Qt.rect(Math.round(Math.max(0, Math.min(area.width - size.width, c.x - size.width / 2))),
                       Math.round(Math.max(0, Math.min(area.height - size.height, c.y - size.height / 2))),
                       size.width, size.height)
    }
    function setFull(on) {
        if (on === full)
            return
        if (on) {
            normalBox = Qt.rect(x, y, width, height)
            full = true
            x = 0; y = -DeskStyle.panelH; width = desk.width; height = desk.height
        } else {
            full = false
            x = normalBox.x; y = normalBox.y; width = normalBox.width; height = normalBox.height
        }
    }
    Connections {   // стол сменил форму, а просмотр на весь стол — во весь новый
        target: pv.desk
        function onWidthChanged() { if (pv.full) pv.width = pv.desk.width }
        function onHeightChanged() { if (pv.full) pv.height = pv.desk.height }
    }

    // Где картинка pw × ph пикселей на холсте (pv_canvas_snapshot): вписана; не больше родного — и «почти влезла»
    // (до 2 %) тоже один к одному: ровно pw × ph пикселей кадра от целого пикселя; края — по пикселям кадра
    readonly property point origin: Qt.point(x + (parent ? parent.x : 0), y + (parent ? parent.y : 0) + barH)
    function rectFor(pw, ph) {
        const cw = canvas.width, ch = canvas.height, ox = origin.x, oy = origin.y
        if (pw <= 0 || ph <= 0 || cw <= 0 || ch <= 0)
            return Qt.rect(0, 0, 0, 0)
        const iw = pw / s, ih = ph / s
        let k = Math.min(cw / iw, ch / ih)
        if (k >= 0.98) {
            // во весь холст (снимок всего стола) — от угла: стол в точках бывает выше кадра на долю пикселя (1012 точек
            // × 1,33333 = 1349,3 при кадре 1348), и по центру снимок съезжал на строку, нижняя уходила за кадр
            const whole = Math.abs(pw - cw * s) <= 2 && Math.abs(ph - ch * s) <= 2
            const px = whole ? Math.round(ox * s) : Math.round((ox + cw / 2) * s - pw / 2)
            const py = whole ? Math.round(oy * s) : Math.round((oy + ch / 2) * s - ph / 2)
            return Qt.rect(px / s - ox, py / s - oy, pw / s, ph / s)
        }
        const dw = iw * k, dh = ih * k
        const x0 = Math.round((ox + (cw - dw) / 2) * s) / s - ox, y0 = Math.round((oy + (ch - dh) / 2) * s) / s - oy
        const x1 = Math.round((ox + (cw + dw) / 2) * s) / s - ox, y1 = Math.round((oy + (ch + dh) / 2) * s) / s - oy
        return Qt.rect(x0, y0, x1 - x0, y1 - y0)
    }

    // ---------- листание, корзина ----------
    function neighbour(p, d) {
        if (source)
            return source.previewNeighbour(p, d)
        const i = folderFiles.indexOf(p)
        return i >= 0 && i + d >= 0 && i + d < folderFiles.length ? folderFiles[i + d] : ""
    }
    function step(d) {
        lastStep = d
        const next = source ? source.previewStep(d) : neighbour(path, d)   // «Файлы» сдвигают выделение сами
        if (next)
            show(next, false)
    }
    function trash() {
        const gone = path
        if (!gone || kind === "missing")
            return
        const next = neighbour(gone, 1) || neighbour(gone, -1)
        if (source) {
            if (!source.previewTrash(gone))
                return   // выделено несколько — «Файлы» не убирают, остаёмся
        } else {
            naomiFiles.trash([gone])
            folderFiles = folderFiles.filter(o => o !== gone)
        }
        delete infos[gone]
        for (let i = 0; i < slots.count; ++i)
            if (slots.itemAt(i).file === gone)
                slots.itemAt(i).file = ""
        if (!next) {
            hide()
            return
        }
        if (source)
            source.selectPath(next)   // и «Файлы» пришлют его (previewFollow)
        show(next, false)
    }

    // ---------- клавиши (on_key) ----------
    Item {
        id: keys
        Keys.onPressed: (e) => {
            const ctrl = (e.modifiers & Qt.ControlModifier) !== 0, alt = (e.modifiers & Qt.AltModifier) !== 0
            const meta = (e.modifiers & Qt.MetaModifier) !== 0
            e.accepted = true
            if (e.key === Qt.Key_Backspace && alt && !ctrl && !meta)
                pv.trash()
            else if (ctrl || alt || meta)
                e.accepted = false
            else if (e.key === Qt.Key_Escape)
                pv.full ? pv.setFull(false) : pv.hide()
            else if (e.key === Qt.Key_Space)
                pv.hide()
            else if (e.key === Qt.Key_Left || e.key === Qt.Key_Up)
                pv.step(-1)
            else if (e.key === Qt.Key_Right || e.key === Qt.Key_Down)
                pv.step(1)
            else if (e.key === Qt.Key_F || e.key === Qt.Key_F11)
                pv.setFull(!pv.full)
            else
                e.accepted = false
        }
    }

    // ---------- полоса: имя, размеры, формат, вес ----------
    function clock(t) {
        const v = Math.max(0, Math.floor(t))
        const two = n => (n < 10 ? "0" : "") + n
        return v >= 3600 ? Math.floor(v / 3600) + ":" + two(Math.floor(v / 60) % 60) + ":" + two(v % 60)
                         : Math.floor(v / 60) + ":" + two(v % 60)
    }
    readonly property string meta: {
        const parts = []
        if (kind === "image") {
            parts.push(info.width + " × " + info.height)
            if (info.format) parts.push(info.format)
        } else if (kind === "video") {
            if (video.frameSize.width > 0) parts.push(video.frameSize.width + " × " + video.frameSize.height)
            parts.push(info.format || "ВИДЕО")
            if (video.duration > 0) parts.push(clock(video.position) + " / " + clock(video.duration))
        }
        if (info.sizeText) parts.push(info.sizeText)
        return parts.join("  ·  ")
    }
    barContent: [
        Text {
            id: nameText
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, parent.width - x - 120)
            text: pv.info.name || ""
            elide: Text.ElideMiddle
            font.family: DeskStyle.sans
            font.pixelSize: 14
            color: DeskStyle.text
        },
        Text {
            x: nameText.x + nameText.width + 14
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(0, parent.width - x - 12)
            text: pv.meta
            elide: Text.ElideRight
            font.family: DeskStyle.mono
            font.pixelSize: 12
            color: DeskStyle.dim
        }
    ]

    // ---------- холст ----------
    property bool loading: false
    property double shownAt: 0   // когда показали файл — журнал: за сколько фото на экране
    Timer { id: loadingTimer; interval: pv.loadingMs; onTriggered: pv.loading = true }
    Timer {
        id: frameWait
        interval: pv.videoWaitMs
        onTriggered: pv.waitingFrame = false
    }
    Item {
        id: canvas
        anchors.fill: parent
        clip: true
        // нижние углы — по контуру окна (как страница браузера, WebWindow): только когда кадр до них доходит —
        // мелкое фото посередине лишнего прохода отрисовки не стоит
        readonly property rect shownBox: pv.kind === "image" ? pv.rectFor(pv.info.width, pv.info.height)
                                       : pv.kind === "video" ? videoBox : Qt.rect(0, 0, 0, 0)
        layer.enabled: pv.corner > 0 && shownBox.y + shownBox.height > height - pv.corner
                       && (shownBox.x < pv.corner || shownBox.x + shownBox.width > width - pv.corner)
        layer.effect: OpacityMask { maskSource: canvasMask }

        // Фото: до четырёх в памяти — на экране и подгруженные заранее. Декодируется под размер стола (больше и
        // на весь стол не нужно): JPEG Qt читает сразу уменьшенным; на экране — мип-карты (большое — без ряби)
        Repeater {
            id: slots
            model: 4
            Image {
                property string file: ""
                readonly property var fi: file ? pv.infoOf(file) : null
                readonly property bool current: file !== "" && file === pv.path && pv.kind === "image" && !pv.info.animated
                readonly property rect box: current ? pv.rectFor(fi.width, fi.height) : Qt.rect(0, 0, 0, 0)
                visible: current && status === Image.Ready
                x: box.x; y: box.y; width: box.width; height: box.height
                source: file ? pv.url(file) : ""
                sourceSize: fi ? pv.decodeSize(fi.width, fi.height) : Qt.size(-1, -1)
                asynchronous: true
                cache: false
                autoTransform: true
                // уменьшено — мип-карты (трилинейно, без ряби); один к одному — простая текстура, ближайший пиксель:
                // ровно пиксели файла (с линейной выборкой — отличия на единицу из-за округления видеочипа)
                readonly property bool exact: fi !== null && Math.abs(box.width * pv.s - sourceSize.width) < 0.5
                smooth: !exact
                mipmap: !exact
                onStatusChanged: {
                    if (status === Image.Ready)
                        console.info("просмотр:", file.substring(file.lastIndexOf("/") + 1), sourceSize.width + "x" + sourceSize.height,
                                     current ? "на экране через " + (Date.now() - pv.shownAt) + " мс" : "заранее")
                    if (current && status !== Image.Loading)
                        pv.prefetch()
                }
                onCurrentChanged: if (current && status === Image.Ready)
                                      console.info("просмотр:", file.substring(file.lastIndexOf("/") + 1), "из памяти")
            }
        }
        // пока фото грузится — плитка «Файлов» того же файла (тот же адрес и размер — Qt отдаёт её из кэша), растянутая
        Image {
            readonly property rect box: pv.kind === "image" ? pv.rectFor(pv.info.width, pv.info.height) : Qt.rect(0, 0, 0, 0)
            visible: pv.kind === "image" && !pv.info.animated && !pv.imageReady && status === Image.Ready
            x: box.x; y: box.y; width: box.width; height: box.height
            source: pv.kind === "image" && !pv.imageReady ? pv.url(pv.info.path) : ""
            sourceSize: Qt.size(192, 144)   // как у плитки (FileIcon): тот же ключ кэша Qt
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true
        }
        AnimatedImage {   // GIF из нескольких кадров
            readonly property rect box: pv.kind === "image" ? pv.rectFor(pv.info.width, pv.info.height) : Qt.rect(0, 0, 0, 0)
            visible: pv.kind === "image" && pv.info.animated === true
            x: box.x; y: box.y; width: box.width; height: box.height
            source: visible ? pv.url(pv.info.path) : ""
            asynchronous: true
            cache: false
            smooth: true
        }

        // Видео: две плоскости кадра — текстуры, цвет — шейдер
        VideoPlane { id: lumaPlane; video: pv.video; plane: 0 }
        VideoPlane { id: chromaPlane; video: pv.video; plane: 1 }
        readonly property rect videoBox: pv.kind === "video" ? pv.rectFor(pv.video.frameSize.width, pv.video.frameSize.height)
                                                             : Qt.rect(0, 0, 0, 0)
        ShaderEffect {
            visible: pv.kind === "video" && pv.video.frameSize.width > 0
            x: canvas.videoBox.x; y: canvas.videoBox.y; width: canvas.videoBox.width; height: canvas.videoBox.height
            property variant luma: lumaPlane
            property variant chroma: chromaPlane
            property matrix4x4 colorMatrix: pv.video.colorMatrix
            fragmentShader: "shaders/nv12.frag.qsb"
        }
        // полоса хода по низу кадра (видимой его части): дорожка белая .06, пройденное — янтарь .75
        Item {
            visible: pv.kind === "video" && pv.video.frameSize.width > 0
            readonly property real x0: Math.max(canvas.videoBox.x, 0)
            readonly property real x1: Math.min(canvas.videoBox.x + canvas.videoBox.width, canvas.width)
            readonly property real y1: Math.min(canvas.videoBox.y + canvas.videoBox.height, canvas.height)
            x: x0; y: y1 - 3; width: x1 - x0; height: 3   // VIDEO_BAR_H
            Rectangle { anchors.fill: parent; color: Qt.rgba(1, 1, 1, 0.06) }
            Rectangle {
                height: parent.height
                width: parent.width * (pv.video.duration > 0 ? Math.min(1, pv.video.position / pv.video.duration) : 0)
                color: DeskStyle.amberA(0.75)
            }
        }
        Item {   // ▶ на паузе — по центру кадра
            visible: pv.kind === "video" && pv.video.active && !pv.video.playing && pv.video.frameSize.width > 0
            x: canvas.videoBox.x + canvas.videoBox.width / 2 - pv.playR
            y: canvas.videoBox.y + canvas.videoBox.height / 2 - pv.playR
            width: 2 * pv.playR; height: width
            Rectangle { anchors.fill: parent; radius: pv.playR; color: Qt.rgba(0.055, 0.059, 0.047, 0.6) }
            Shape {
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    fillColor: DeskStyle.text
                    strokeColor: "transparent"
                    startX: pv.playR * 0.7; startY: pv.playR * 0.55
                    PathLine { x: pv.playR * 1.5; y: pv.playR }
                    PathLine { x: pv.playR * 0.7; y: pv.playR * 1.45 }
                    PathLine { x: pv.playR * 0.7; y: pv.playR * 0.55 }
                }
            }
        }

        // Карточка: файл, который показать нельзя (и «НЕ ОТКРЫЛОСЬ»)
        Column {
            readonly property bool failed: pv.kind === "video" && pv.video.error !== ""
            visible: pv.kind === "card" || pv.kind === "missing" || failed
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: parent.failed ? "НЕ ОТКРЫЛОСЬ" : pv.info.type || ""
                font.family: DeskStyle.mono
                font.pixelSize: 13
                font.letterSpacing: 2
                color: DeskStyle.amber
            }
            Text {
                x: 32
                width: parent.width - 64
                topPadding: 10
                bottomPadding: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
                text: pv.info.name || ""
                font.family: DeskStyle.sans
                font.weight: Font.Light
                font.pixelSize: 26
                color: DeskStyle.text
            }
            Text {
                x: 32
                width: parent.width - 64
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: parent.failed ? pv.video.error.toLowerCase() : pv.info.hint || ""
                font.family: DeskStyle.mono
                font.pixelSize: 13
                color: DeskStyle.muted
            }
        }

        // «ЗАГРУЖАЮ» — фото или первый кадр не пришли за 300 мс
        Rectangle {
            visible: pv.loading && (pv.kind === "image" && !pv.info.animated && !pv.imageReady
                                    || pv.kind === "video" && pv.video.frameSize.width === 0 && pv.video.error === "")
            anchors.centerIn: parent
            width: loadingText.implicitWidth + 24
            height: loadingText.implicitHeight + 12
            color: Qt.rgba(14 / 255, 15 / 255, 12 / 255, 0.72)
            Text {
                id: loadingText
                anchors.centerIn: parent
                text: "ЗАГРУЖАЮ"
                font.family: DeskStyle.mono
                font.pixelSize: 12
                font.letterSpacing: 2
                color: DeskStyle.muted
            }
        }

        // Щелчок: видео — пауза и снова, у нижнего края — перемотка; двойной — на весь стол (видео — пауза обратно)
        MouseArea {
            anchors.fill: parent
            onPressed: keys.forceActiveFocus()
            onClicked: (m) => {
                if (pv.kind !== "video" || !pv.video.active)
                    return
                if (m.y >= height - pv.seekZone)
                    pv.video.seek(m.x / width)
                else
                    pv.video.toggle()
            }
            onDoubleClicked: {
                pv.setFull(!pv.full)
                if (pv.kind === "video" && pv.video.active)
                    pv.video.toggle()
            }
        }
    }
    Item {
        id: canvasMask
        visible: false
        width: canvas.width
        height: canvas.height
        layer.enabled: true
        Shape {
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                fillColor: "white"
                strokeColor: "transparent"
                PathPolyline { path: DeskStyle.outline(0, -pv.barH, pv.width, pv.height, pv.corner) }
            }
        }
    }
    // видео: первый кадр — окно по его размеру (впервые — подождав его, не дольше 600 мс)
    Connections {
        target: pv.video
        function onFrameSizeChanged() {
            if (pv.kind !== "video" || pv.video.frameSize.width <= 0)
                return
            pv.place(pv.fitSize(pv.video.frameSize.width, pv.video.frameSize.height), false)
            pv.waitingFrame = false
        }
        function onStateChanged() {
            if (pv.kind === "video" && pv.video.error !== "") {
                pv.place(Qt.size(640, 340), false)
                pv.waitingFrame = false
            }
        }
    }
    opacity: waitingFrame ? 0 : (shown && !minimized ? 1 : 0)

    // ---------- память: что грузить ----------
    readonly property bool imageReady: {
        for (let i = 0; i < slots.count; ++i) {
            const it = slots.itemAt(i)
            if (it && it.current && it.status === Image.Ready)
                return true
        }
        return false
    }
    // Декодировать под стол: влезает (с запасом 2 %) — в родном размере, больше — вписанным в кадр стола
    function decodeSize(w, h) {
        const dw = desk.width * s, dh = desk.height * s
        if (w <= dw * 1.02 && h <= dh * 1.02)
            return Qt.size(w, h)
        const k = Math.min(dw / w, dh / h)
        return Qt.size(Math.round(w * k), Math.round(h * k))
    }
    // Текущее — в ячейку (уже есть — сразу); соседи — когда текущее загрузилось (prefetch)
    function loadImages() {
        if (pv.info.animated)
            return
        for (let i = 0; i < slots.count; ++i)
            if (slots.itemAt(i).file === path)
                return prefetch()
        slotFor([path]).file = path
    }
    function prefetch() {   // у видео соседей заранее не грузим (как у НаомиОС)
        if (kind !== "image")
            return
        const want = [path]
        let p = path
        for (let k = 0; k < 2; ++k) {   // два вперёд по ходу листания
            p = neighbour(p, lastStep)
            if (!p)
                break
            want.push(p)
        }
        const behind = neighbour(path, -lastStep)
        if (behind)
            want.push(behind)
        const photos = want.filter(f => { const i = infoOf(f); return i.kind === "image" && !i.animated })
        for (const f of photos) {
            let has = false
            for (let i = 0; i < slots.count; ++i)
                has = has || slots.itemAt(i).file === f
            if (!has)
                slotFor(photos).file = f
        }
    }
    // ячейка под новый файл: пустая, иначе та, чей файл больше не нужен
    function slotFor(keep) {
        for (let i = 0; i < slots.count; ++i)
            if (slots.itemAt(i).file === "")
                return slots.itemAt(i)
        for (let i = 0; i < slots.count; ++i)
            if (keep.indexOf(slots.itemAt(i).file) < 0 && slots.itemAt(i).file !== path)
                return slots.itemAt(i)
        return slots.itemAt(0)
    }
}
