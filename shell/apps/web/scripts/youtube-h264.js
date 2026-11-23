// «Браузер» — YouTube в H.264 (docs/WEB.md, «YouTube: H.264 вместо AV1»). Аппаратного видеодекодера телефона Chromium
// здесь не видит: всё — процессором, а YouTube по умолчанию шлёт AV1 — его разбор (dav1d) на 1080p ел 3,6–3,9 ядра и
// всё равно терял 11–22 % кадров. Страница YouTube (и её плеер, встроенный в другие сайты) слышит, что AV1 и VP9
// браузер не умеет, — и берёт H.264 (до 1080p), который разбирается в разы легче. Как расширение h264ify.
(() => {
    const host = location.hostname;
    if (!/(^|\.)(youtube\.com|youtube-nocookie\.com)$/.test(host))
        return;
    const heavy = (type) => /av01|vp0?9|vp8/i.test(String(type || ""));
    if (window.MediaSource) {
        const isTypeSupported = MediaSource.isTypeSupported.bind(MediaSource);
        MediaSource.isTypeSupported = (type) => !heavy(type) && isTypeSupported(type);
    }
    const canPlayType = HTMLMediaElement.prototype.canPlayType;
    HTMLMediaElement.prototype.canPlayType = function (type) {
        return heavy(type) ? "" : canPlayType.call(this, type);
    };
    if (navigator.mediaCapabilities) {
        const decodingInfo = navigator.mediaCapabilities.decodingInfo.bind(navigator.mediaCapabilities);
        navigator.mediaCapabilities.decodingInfo = (config) =>
            config && config.video && heavy(config.video.contentType)
                ? Promise.resolve({ supported: false, smooth: false, powerEfficient: false })
                : decodingInfo(config);
    }
})();
