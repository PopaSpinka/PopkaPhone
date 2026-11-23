// «Браузер» — полоса прокрутки страниц «Нить» (docs/WEB.md, «Тёмные сайты и полоса „Нить“»): вариант 01 из десяти,
// выбран пользователем 21 ноября 2026 вместо белой полосы Chromium. Волосок 2 цвета muted .45 без желоба, хват 12;
// под мышью — 6 и светлее, тянут — янтарь. Слой @layer: правила сайта без слоя всегда сильнее — у кого своя полоса
// (YouTube) или она спрятана (карусели), так и остаётся. Толщина нарисована градиентом, а не прозрачной рамкой: сайт,
// задавший ползунку только цвет, рамкой сузился бы до нуля. Свой мир скрипта (ApplicationWorld): сайту не мешает.
(() => {
    const css = `@layer naomi-scrollbar {
        ::-webkit-scrollbar { width: 12px; height: 12px; background: transparent; }
        ::-webkit-scrollbar-track, ::-webkit-scrollbar-corner { background: transparent; }
        ::-webkit-scrollbar-button { display: none; width: 0; height: 0; }
        ::-webkit-scrollbar-thumb {
            min-height: 40px; min-width: 40px; border-radius: 6px;
            background: linear-gradient(to right, transparent 5px, rgba(169, 164, 154, .45) 5px,
                                        rgba(169, 164, 154, .45) 7px, transparent 7px);
        }
        ::-webkit-scrollbar-thumb:hover {
            background: linear-gradient(to right, transparent 3px, rgba(236, 233, 227, .7) 3px,
                                        rgba(236, 233, 227, .7) 9px, transparent 9px);
        }
        ::-webkit-scrollbar-thumb:active {
            background: linear-gradient(to right, transparent 3px, #d8b25a 3px, #d8b25a 9px, transparent 9px);
        }
        ::-webkit-scrollbar-thumb:horizontal {
            background: linear-gradient(to bottom, transparent 5px, rgba(169, 164, 154, .45) 5px,
                                        rgba(169, 164, 154, .45) 7px, transparent 7px);
        }
        ::-webkit-scrollbar-thumb:horizontal:hover {
            background: linear-gradient(to bottom, transparent 3px, rgba(236, 233, 227, .7) 3px,
                                        rgba(236, 233, 227, .7) 9px, transparent 9px);
        }
        ::-webkit-scrollbar-thumb:horizontal:active {
            background: linear-gradient(to bottom, transparent 3px, #d8b25a 3px, #d8b25a 9px, transparent 9px);
        }
    }`;
    try {
        const sheet = new CSSStyleSheet();
        sheet.replaceSync(css);
        document.adoptedStyleSheets = [...document.adoptedStyleSheets, sheet];
    } catch (e) {
        console.warn("naomi-scrollbar:", e);
    }
})();
