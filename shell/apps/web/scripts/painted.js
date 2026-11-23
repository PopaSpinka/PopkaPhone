// «Браузер» — сайт нарисовал первое содержимое (docs/WEB.md, «Без белого экрана на холодную»). До этого Page.qml держит
// над страницей шторку цвета новой вкладки: фон страницы сайта по умолчанию белый, и на холодном запуске YouTube был
// пустой белый экран, пока сайт не нарисовался. Первое содержимое — текст, картинка, холст (first-contentful-paint,
// отмечается, когда кадр уже выведен) — строкой «naomi:painted» в консоль, её ловит Page.qml. Свой мир скрипта
// (ApplicationWorld): сайту он не мешает.
(() => {
    try {
        new PerformanceObserver((list, observer) => {
            if (!list.getEntriesByName("first-contentful-paint").length)
                return;
            observer.disconnect();
            console.log("naomi:painted");
        }).observe({ type: "paint", buffered: true });
    } catch (e) {
        console.log("naomi:painted");   // отметок нет — шторку не держать
    }
})();
