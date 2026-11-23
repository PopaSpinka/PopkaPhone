'use strict';
/* Пульт: варианты переходов и вида, сцены, показы, «мой выбор». Выбор помнится в браузере (localStorage). */
const Panel = (() => {
  const panel = document.getElementById('panel');
  const KEY = 'naomiphone-mockup-v1';
  const SAVE = ['scheme', 'speed', 'unlock', 'launch', 'home', 'back', 'forward', 'shade', 'battery', 'lockClock', 'iconShape', 'themed', 'grid', 'font',
    'wallpaper', 'seed', 'variant', 'nav', 'touches', 'blur', 'dark', 'volInShade'];
  const OPT = {
    unlock: { label: 'Разблокировка', demo: [['unlock', 'показать']], opts: { android: 'Android', stagger: 'Ряды', zoom: 'Масштаб', sheet: 'Лист вверх', fade: 'Растворение' },
      note: { android: 'Android 17: экран под блокировкой растёт 0,95 → 1 и поднимается на 5 % высоты.', stagger: 'Ряды значков догоняют палец с задержкой, пружина M3E.', zoom: 'Блокировка уходит вперёд, главный выплывает из глубины.', sheet: 'Как сейчас в НаомиФоне: лист уезжает вверх.', fade: 'Просто растворение.' } },
    launch: { label: 'Запуск программы', demo: [['launch', 'показать']], opts: { icon: 'Android', iconx: 'Пружина M3E', zoom: 'Из центра', slide: 'Снизу' },
      note: { icon: 'Android 17: окно растёт из значка за 500 мс по кривой emphasized, главный экран отступает 1 → 0,97 и размывается.', iconx: 'То же из значка, но на пружинах Material 3 Expressive — с лёгким перелётом.', zoom: 'Окно проявляется из центра 0,86 → 1.', slide: 'Окно выезжает снизу.' } },
    home: { label: 'Домой (смахнуть вверх)', demo: [['home', 'показать'], ['recents', 'с задержкой → недавние']], opts: { icon: 'Android', iconx: 'Пружина M3E', shrink: 'Уменьшение', up: 'Улёт вверх' },
      note: { icon: 'Launcher3: окно летит в свой значок на пружинах X 450/0,965, Y 400/0,95, масштаб 500/0,99.', iconx: 'В значок на пружинах M3E Expressive (0,8/380) — заметный отскок.', shrink: 'Окно уменьшается и гаснет в центре (так Android делает, если значка нет на экране).', up: 'Окно улетает вверх и гаснет.' } },
    back: { label: 'Назад', demo: [['back', 'внутри «Настроек»'], ['backHome', 'на главный']], opts: { predictive: 'Android', material: 'Material', axis: 'Ось X', slide: 'Сдвиг' },
      note: { predictive: 'Системный предиктивный «назад» Android 17: страница сжимается до 0,85, предыдущая ждёт слева на −96 dp; с корня программы — окно 0,75 и домой.', material: 'Рекомендация Material для своих экранов: 100 → 90 %, входящая 110 → 100 %.', axis: 'Без предпросмотра: после жеста — общая ось X (30 dp).', slide: 'Страница идёт за пальцем целиком, как в iPhone.' } },
    forward: { label: 'Вперёд (внутри программы)', demo: [['forward', 'показать']], opts: { android: 'Настройки Android', axis: 'Ось X', container: 'Из пункта', fade: 'Сквозь', slide: 'Сдвиг' },
      note: { android: 'Как в «Настройках» Android 17: новая страница въезжает на 96 dp за 450 мс и проявляется за 83 мс.', axis: 'Material shared axis X: сдвиг 30 dp, смена прозрачности на 35 %.', container: 'Container transform: пункт раскрывается в страницу (500 мс).', fade: 'Fade through: старая гаснет, новая проявляется с 0,92 → 1.', slide: 'Полный сдвиг с затемнением старой страницы.' } },
    shade: { label: 'Шторка', demo: [['shade', 'показать']], opts: { single: 'Единая', dual: 'Две шторки' },
      note: { single: 'Android 16–17: первое вытягивание — 2 ряда плиток и уведомления, второе — яркость и все плитки.', dual: 'Dual Shade (есть в коде Android, на телефонах не вышла): слева — уведомления, справа — плитки.' } },
  };
  const LOOK = {
    scheme: { label: 'Пружины Material 3', opts: { expressive: 'Expressive', standard: 'Standard' } },
    speed: { label: 'Скорость анимаций', opts: { 1: '1×', 0.5: '½', 0.25: '¼', 0.1: '⅒' } },
    dark: { label: 'Тема', opts: { true: 'Тёмная', false: 'Светлая' } },
    variant: { label: 'Палитра Material You', opts: { tonalSpot: 'Спокойная', vibrant: 'Яркая', expressive: 'Выразительная', fidelity: 'Точная', neutral: 'Нейтральная', monochrome: 'Монохром' } },
    themed: { label: 'Значки', opts: { true: 'Тематические', false: 'Цветные' } },
    grid: { label: 'Сетка главного экрана', opts: { 4: '4 в ряд', 5: '5 в ряд' } },
    font: { label: 'Шрифт', opts: { gsans: 'Google Sans', roboto: 'Roboto Flex', system: 'Системный' } },
    battery: { label: 'Значок батареи', opts: { new: 'Новый (% внутри)', classic: 'Классический' } },
    lockClock: { label: 'Часы на блокировке', opts: { big: 'Крупные', flex: 'Узкие (Flex)', small: 'Малые + карточки' } },
    blur: { label: 'Размытие под шторкой', opts: { true: 'Есть', false: 'Нет' } },
    volInShade: { label: 'Громкость в шторке', opts: { true: 'Есть', false: 'Нет' } },
    nav: { label: 'Навигация', opts: { gesture: 'Жесты', buttons: '3 кнопки' } },
    touches: { label: 'Показывать касания', opts: { false: 'Нет', true: 'Да' } },
  };
  const play = '<svg viewBox="0 0 10 10"><path d="M2 1 L9 5 L2 9 Z" fill="currentColor"/></svg>';
  const parse = s => (s === 'true' ? true : s === 'false' ? false : /^\d+(\.\d+)?$/.test(s) ? +s : s);

  function load() {
    try {
      const d = JSON.parse(localStorage.getItem(KEY) || '{}');
      for (const k of SAVE) {
        if (!(k in d)) continue;
        const o = OPT[k] || LOOK[k];
        if (o && !(String(d[k]) in o.opts)) continue;   // устаревший вариант — пропускаем
        Store.set(k, d[k]);
      }
    } catch (e) { /* без памяти */ }
    if (Store.get('volInShade') === undefined) Store.set('volInShade', true);
  }
  function save() { try { const d = {}; for (const k of SAVE) d[k] = Store.get(k); localStorage.setItem(KEY, JSON.stringify(d)); } catch (e) { /* нет доступа */ } }

  const segHTML = (key, o) => `<div class="seg" role="group" aria-label="${o.label}">${Object.entries(o.opts).map(([v, n]) => `<button type="button" data-opt="${key}" data-val="${v}">${n}</button>`).join('')}</div>`;
  function rowHTML(key, o) {
    const demos = (o.demo || []).map(([d, n]) => `<button type="button" class="p-play" data-demo="${d}">${play}${n}</button>`).join('');
    return `<div class="p-row"><div class="p-row-top"><span class="p-label">${o.label}</span>${demos}</div>${segHTML(key, o)}${o.note ? `<div class="p-note" data-note="${key}"></div>` : ''}</div>`;
  }
  function build() {
    const seeds = ['#9a5b1e', '#6750a4', '#0b6e8a', '#3f6c2a', '#b3261e', '#7d5260', '#8c7a00', '#3d4a86'];
    panel.innerHTML = `
      <header class="p-head"><h1>Макет НаомиФона</h1>
        <p>Интерфейс в духе Android 17 и Material 3 Expressive для нашего Pixel 5. Водите мышью или пальцем прямо по экрану: жесты настоящие, числа — из исходников Android.</p>
        <div class="p-src">Цвет: <b data-color-src>…</b> · <span class="kbd">←</span> назад <span class="kbd">H</span> домой <span class="kbd">R</span> недавние <span class="kbd">P</span> питание</div></header>
      <section class="p-card"><h2>Сцены</h2>
        <div class="p-grid">${[['lock', 'Блокировка'], ['home', 'Главный'], ['shade1', 'Шторка'], ['shade2', 'Все плитки'], ['settings', 'Настройки'], ['display', 'Настройки › Экран'], ['recents', 'Недавние']].map(([s, n]) => `<button type="button" class="p-btn wide" data-scene="${s}">${n}</button>`).join('')}</div>
        <div class="p-sys"><button type="button" class="p-btn" data-sys="back" title="Назад">◁</button><button type="button" class="p-btn" data-sys="home" title="Домой">○</button><button type="button" class="p-btn" data-sys="recents" title="Недавние">▢</button>
          <button type="button" class="p-btn" data-sys="power" title="Питание">⏻</button><button type="button" class="p-btn" data-sys="voldown" title="Тише">−</button><button type="button" class="p-btn" data-sys="volup" title="Громче">+</button></div></section>
      <section class="p-card"><h2>Переходы</h2>${Object.entries(OPT).map(([k, o]) => rowHTML(k, o)).join('')}</section>
      <section class="p-card"><h2>Движение</h2>${rowHTML('scheme', LOOK.scheme)}<div class="spec" data-spec></div>${rowHTML('speed', LOOK.speed)}</section>
      <section class="p-card"><h2>Вид</h2>
        ${rowHTML('dark', LOOK.dark)}
        <div class="p-row"><span class="p-label">Обои (цвет берётся из них)</span><div class="wps">${Object.entries(Theme.WALLPAPERS).map(([id, w]) => `<button type="button" data-opt="wallpaper" data-val="${id}" data-wp="${id}" title="${w.name}" aria-label="${w.name}"></button>`).join('')}</div></div>
        <div class="p-row"><span class="p-label">Цвет-источник</span><div class="swatches">${seeds.map(c => `<button type="button" data-opt="seed" data-val="${c}" style="background:${c}" aria-label="${c}"></button>`).join('')}</div></div>
        ${rowHTML('variant', LOOK.variant)}
        ${rowHTML('themed', LOOK.themed)}
        <div class="p-row"><span class="p-label">Форма значков (Android 16 QPR2)</span><div class="shapes">${Object.entries(Theme.SHAPES).map(([id, s]) => `<button type="button" data-opt="iconShape" data-val="${id}" title="${s.name}" aria-label="${s.name}"><i style="clip-path:${s.clip}"></i></button>`).join('')}</div></div>
        ${['grid', 'font', 'battery', 'lockClock', 'blur', 'volInShade', 'nav', 'touches'].map(k => rowHTML(k, LOOK[k])).join('')}
      </section>
      <section class="p-card"><h2>Мой выбор</h2><div class="choice" data-choice-text></div>
        <div class="p-grid"><button type="button" class="p-btn" data-copy>Скопировать</button><button type="button" class="p-btn" data-reset>Сбросить к Android</button></div>
        <div class="p-note">Скопируйте и пришлите в чат — так я узнаю, что собирать на телефоне.</div></section>
      <section class="p-card"><h2>Жесты</h2><ul class="gest">
        <li><b>Шторка</b><span>от верхнего края вниз; ещё раз вниз — все плитки. На главном экране — вниз где угодно</span></li>
        <li><b>Разблокировка</b><span>на блокировке — вверх</span></li>
        <li><b>Все программы</b><span>на главном — вверх</span></li>
        <li><b>Домой</b><span>от нижнего края вверх (с броском)</span></li>
        <li><b>Недавние</b><span>от нижнего края вверх и задержать палец; карточку — смахнуть вверх</span></li>
        <li><b>Назад</b><span>от левого или правого края внутрь; отпустить, когда стрелка станет кругом</span></li>
        <li><b>Уведомление</b><span>смахнуть в сторону</span></li>
        <li><b>Корпус</b><span>кнопки справа: питание и громкость</span></li></ul></section>
      <section class="p-card"><h2>Откуда взято</h2><div class="p-note">Числа движения — из исходников Android 17 (AOSP, Apache 2.0) и токенов Material 3 Expressive (androidx, Apache 2.0). Шрифты Google Sans, Google Sans Flex, Roboto Flex — OFL 1.1; значки Material Symbols — Apache 2.0; цвет — Material Color Utilities (Apache 2.0). Обои и программы нарисованы для макета. Подробно — docs/UI.md.</div></section>`;
    panel.querySelectorAll('[data-wp]').forEach(el => (el.style.backgroundImage = `url(${Theme.wallpaper(el.dataset.wp, 66, 140, 1)})`));
    refresh();
  }
  function refresh() {
    panel.querySelectorAll('[data-opt]').forEach(b => b.classList.toggle('on', String(Store.get(b.dataset.opt)) === b.dataset.val));
    for (const [k, o] of Object.entries(OPT)) { const n = panel.querySelector(`[data-note="${k}"]`); if (n) n.textContent = o.note[Store.get(k)] || ''; }
    const sp = Motion.SCHEMES[Store.get('scheme')], f = ([z, k]) => `${z}/${k}`;
    const spec = panel.querySelector('[data-spec]');
    if (spec) spec.textContent = `ζ/k: spatial ${f(sp.spatial)}, fast ${f(sp.spatialFast)}, slow ${f(sp.spatialSlow)}; effects ${f(sp.effects)}. Ими двигаются шторка, список программ, недавние, переключатели и всё «Пружина M3E».`;
    const ct = panel.querySelector('[data-choice-text]'); if (ct) ct.textContent = choiceText();
    document.documentElement.style.setProperty('--hue', Theme.hexToOklch(Store.get('seed')).H.toFixed(0));
  }
  function choiceText() {
    const n = (k, src) => src[k].opts[String(Store.get(k))] || Store.get(k);
    return [
      `Разблокировка: ${n('unlock', OPT)}`, `Запуск: ${n('launch', OPT)}`, `Домой: ${n('home', OPT)}`, `Назад: ${n('back', OPT)}`, `Вперёд: ${n('forward', OPT)}`, `Шторка: ${n('shade', OPT)}`,
      `Пружины: ${n('scheme', LOOK)}`, `Тема: ${n('dark', LOOK)}, палитра ${n('variant', LOOK)}, цвет ${Store.get('seed')}, обои «${Theme.WALLPAPERS[Store.get('wallpaper')].name}»`,
      `Значки: ${n('themed', LOOK)}, форма «${Theme.SHAPES[Store.get('iconShape')].name}», сетка ${n('grid', LOOK)}`,
      `Шрифт: ${n('font', LOOK)}; батарея: ${n('battery', LOOK)}; часы: ${n('lockClock', LOOK)}; навигация: ${n('nav', LOOK)}`,
    ].join('\n');
  }
  const scenes = {
    lock: () => Shell.sceneLock(), home: () => Shell.sceneHome(),
    shade1: () => { Shell.sceneHome(); Shell.openShade(1); }, shade2: () => { Shell.sceneHome(); Shell.openShade(Store.get('shade') === 'dual' ? 1 : 2, Store.get('shade') === 'dual' ? 'qs' : 'single'); },
    settings: () => Shell.sceneApp('settings'), display: () => Shell.sceneApp('settings', ['display']),
    recents: () => { Shell.ensureRecentApps(); Shell.sceneApp('settings'); Shell.enterRecents(); },
  };
  const sys = {
    back: () => Shell.navBack(), home: () => Shell.navHome(), power: () => Shell.power(), volup: () => Shell.showVolume(1 / 15), voldown: () => Shell.showVolume(-1 / 15),
    recents: () => (Shell.state.recents ? Shell.navHome() : Shell.enterRecents()),
  };
  panel.addEventListener('click', e => {
    const o = e.target.closest('[data-opt]'); if (o) { Store.set(o.dataset.opt, parse(o.dataset.val)); return; }
    const d = e.target.closest('[data-demo]'); if (d) { Shell.demos[d.dataset.demo](); return; }
    const s = e.target.closest('[data-scene]'); if (s) { scenes[s.dataset.scene](); return; }
    const y = e.target.closest('[data-sys]'); if (y) { sys[y.dataset.sys](); return; }
    if (e.target.closest('[data-copy]')) {
      const txt = choiceText(), btn = e.target.closest('[data-copy]');
      const ok = () => { btn.textContent = 'Скопировано'; setTimeout(() => (btn.textContent = 'Скопировать'), 1500); };
      if (navigator.clipboard) navigator.clipboard.writeText(txt).then(ok, () => selectChoice()); else selectChoice();
      return;
    }
    if (e.target.closest('[data-reset]')) {
      const def = { unlock: 'android', launch: 'icon', home: 'icon', back: 'predictive', forward: 'android', shade: 'single', scheme: 'expressive', speed: 1, battery: 'new', lockClock: 'big', themed: true, iconShape: 'circle', grid: 4, font: 'gsans', nav: 'gesture', blur: true, volInShade: true };
      for (const [k, v] of Object.entries(def)) Store.set(k, v);
    }
  });
  function selectChoice() { const el = panel.querySelector('[data-choice-text]'); const r = document.createRange(); r.selectNodeContents(el); const s = getSelection(); s.removeAllRanges(); s.addRange(r); }
  document.addEventListener('keydown', e => {
    if (e.target.closest && e.target.closest('input, textarea')) return;
    const k = e.key.toLowerCase();
    if (k === 'arrowleft' || k === 'backspace' || k === 'escape') { Shell.navBack(); e.preventDefault(); }
    else if (k === 'h' || k === 'р') Shell.navHome();
    else if (k === 'r' || k === 'к') sys.recents();
    else if (k === 'p' || k === 'з') Shell.power();
  });

  // Подгоняем телефон под окно
  function fit() {
    const wrap = document.getElementById('deviceWrap'), dev = document.getElementById('device'), stage = document.getElementById('stage');
    const narrow = window.innerWidth <= 900;
    const availH = narrow ? Math.min(window.innerHeight - 24, 900) : window.innerHeight - 28;
    const availW = (narrow ? document.documentElement.clientWidth - 32 : stage.clientWidth) - 8;
    const k = Math.max(0.3, Math.min(availH / 871, availW / 413, 1.15));
    dev.style.transform = `scale(${k})`;
    wrap.style.width = 413 * k + 'px'; wrap.style.height = 871 * k + 'px';
  }

  function onTheme(source) { const el = panel.querySelector('[data-color-src]'); if (el) el.textContent = source === 'mcu' ? 'Material Color Utilities 0.3.0' : 'приближение OKLCH (библиотека не загрузилась)'; }

  load();
  Store.on('*', (k) => { refresh(); if (SAVE.includes(k)) save(); });
  build();
  window.addEventListener('resize', fit);
  return { fit, onTheme, refresh };
})();

Shell.init();
Shell.setThemeHook(Panel.onTheme);
Panel.fit();
