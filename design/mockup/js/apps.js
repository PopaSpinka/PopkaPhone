'use strict';
/* Программы макета и «Настройки». Общее состояние (Store) связывает шторку, «Настройки», строку состояния и пульт. */

const Store = (() => {
  const s = {
    // телефон
    wifi: true, bt: true, airplane: false, dnd: false, flashlight: false, rotate: false, dark: true, saver: false, location: true,
    hotspot: false, nfc: true, night: false, hz90: true, rec: false, adaptive: true, showPct: true, vibrate: true, mobile: true,
    brightness: 0.72, volMedia: 0.55, volRing: 0.7, volNotif: 0.6, volAlarm: 0.8, fontScale: 0.4, displayScale: 0.4,
    // варианты макета
    engine: 'spring', scheme: 'expressive', speed: 1,
    unlock: 'android', launch: 'icon', home: 'icon', back: 'predictive', forward: 'android', shade: 'single', volInShade: true,
    battery: 'new', lockClock: 'big', iconShape: 'circle', themed: true, grid: 4, font: 'gsans',
    wallpaper: 'dunes', seed: '#9a5b1e', variant: 'tonalSpot', nav: 'gesture', touches: false, blur: true,
  };
  const subs = [];
  return {
    get: k => s[k], all: () => ({ ...s }),
    set(k, v) { if (s[k] === v) return; s[k] = v; for (const [key, fn] of subs) if (key === k || key === '*') fn(k, v); },
    on(k, fn) { subs.push([k, fn]); },
  };
})();

const Apps = (() => {
  const LIST = [
    { id: 'phone', name: 'Телефон', icon: 'call', h: 145 },
    { id: 'messages', name: 'Сообщения', icon: 'chat', h: 250 },
    { id: 'browser', name: 'Браузер', icon: 'public', h: 220 },
    { id: 'camera', name: 'Камера', icon: 'photo_camera', h: 25 },
    { id: 'settings', name: 'Настройки', icon: 'settings', h: 270 },
    { id: 'gallery', name: 'Галерея', icon: 'photo_library', h: 330 },
    { id: 'clock', name: 'Часы', icon: 'schedule', h: 200 },
    { id: 'calc', name: 'Калькулятор', icon: 'calculate', h: 300 },
    { id: 'notes', name: 'Заметки', icon: 'edit_note', h: 85 },
    { id: 'files', name: 'Файлы', icon: 'folder', h: 60 },
    { id: 'music', name: 'Музыка', icon: 'music_note', h: 0 },
    { id: 'naomi', name: 'Наоми', icon: 'auto_awesome', h: 290 },
    { id: 'weather', name: 'Погода', icon: 'partly_cloudy_day', h: 235 },
    { id: 'calendar', name: 'Календарь', icon: 'calendar_month', h: 265 },
    { id: 'maps', name: 'Карты', icon: 'map', h: 160 },
    { id: 'terminal', name: 'Терминал', icon: 'terminal', h: 180 },
    { id: 'contacts', name: 'Контакты', icon: 'contacts', h: 210 },
    { id: 'displaytest', name: 'Тест экрана', icon: 'monitor_heart', h: 350 },
  ];
  const BY = Object.fromEntries(LIST.map(a => [a.id, a]));
  const HOME = ['settings', 'clock', 'calendar', 'weather', 'gallery', 'notes', 'files', 'music', 'naomi', 'maps', 'calc', 'terminal'];
  const DOCK = ['phone', 'messages', 'browser', 'camera'];

  const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const icon = (n, f = '') => `<span class="ms ${f}">${n}</span>`;
  const iconHTML = a => `<div class="ic" style="--h:${a.h}">${icon(a.icon)}</div>`;
  const appIcon = (a, withLabel = true) => `<button class="app-icon" data-app="${a.id}">${iconHTML(a)}${withLabel ? `<span class="lbl">${a.name}</span>` : ''}</button>`;

  // ---- Органы управления ----
  const switchHTML = () => `<span class="sw"><span class="sw-th"><span class="ms">check</span></span></span>`;
  const sliderHTML = (key, ic = 'light_mode', size = 'm') =>
    `<div class="sl ${size}" data-slider="${key}"><div class="sl-a"></div><div class="sl-i"></div><div class="sl-h"></div>${ic ? `<span class="ms sl-icon">${ic}</span>` : ''}</div>`;

  function itemHTML(it) {
    if (it.html) return it.html;
    const ic = it.icon ? `<span class="iic" style="--h:${it.h ?? 250}">${icon(it.icon)}</span>` : '';
    const tx = `<span class="itx"><span class="it" style="display:block">${it.t}</span>${it.s ? `<span class="is" style="display:block">${it.s}</span>` : ''}</span>`;
    if (it.slider) return `<div class="item col"><span class="it">${it.t}</span>${sliderHTML(it.slider, it.sicon ?? 'light_mode', 'm')}</div>`;
    if (it.bind) return `<button class="item tap" data-bind="${it.bind}">${ic}${tx}${switchHTML()}</button>`;
    if (it.radio) return `<button class="item tap short" data-radio="${it.radio}" data-value="${it.value}">${ic}${tx}<span class="rd"></span></button>`;
    if (it.v !== undefined) return `<div class="item short">${ic}${tx}<span class="ival">${it.v}</span></div>`;
    return `<button class="item tap" data-nav="${it.nav || 'generic:' + it.t}">${ic}${tx}</button>`;
  }
  const groupHTML = g => `${g.title ? `<div class="grp-title">${g.title}</div>` : ''}<div class="grp">${g.items.map(itemHTML).join('')}</div>`;

  // ---- «Настройки» ----
  const S = {
    root: { title: 'Настройки', pre: `<div class="search-pill">${icon('search')}Поиск в настройках</div>`, groups: [
      { items: [{ t: 'Сеть и интернет', s: 'Wi‑Fi, мобильная сеть, точка доступа', icon: 'wifi', h: 250, nav: 'network' },
        { t: 'Подключённые устройства', s: 'Bluetooth, NFC', icon: 'devices', h: 195, nav: 'connected' }] },
      { items: [{ t: 'Приложения', s: 'Недавние, по умолчанию', icon: 'apps', h: 150 },
        { t: 'Уведомления', s: 'История, «Не беспокоить»', icon: 'notifications', h: 40 },
        { t: 'Батарея', s: '87% · хватит примерно на день', icon: 'battery_5_bar', h: 135, nav: 'battery' }] },
      { items: [{ t: 'Звук и вибрация', s: 'Громкость, мелодия', icon: 'volume_up', h: 300, nav: 'sound' },
        { t: 'Экран', s: 'Яркость, тёмная тема, 90 Гц', icon: 'brightness_6', h: 70, nav: 'display' },
        { t: 'Обои и стиль', s: 'Цвета, значки, их форма', icon: 'palette', h: 340, nav: 'style' },
        { t: 'Специальные возможности', s: 'Экран, звук, управление', icon: 'accessibility_new', h: 180 }] },
      { items: [{ t: 'Безопасность', s: 'Блокировка экрана, отпечаток', icon: 'shield', h: 145 },
        { t: 'Конфиденциальность', s: 'Разрешения, данные', icon: 'privacy_tip', h: 285 },
        { t: 'Местоположение', s: 'Вкл. · 3 приложения', icon: 'location_on', h: 25 }] },
      { items: [{ t: 'Система', s: 'Языки, жесты, время, обновления', icon: 'tune', h: 230, nav: 'system' },
        { t: 'О телефоне', s: 'Pixel 5 · НаомиФон 0.3', icon: 'smartphone', h: 260, nav: 'about' }] },
    ] },
    network: { title: 'Сеть и интернет', groups: [
      { items: [{ t: 'Wi‑Fi', s: 'HOME_WIFI', icon: 'wifi', h: 250, nav: 'wifi' }, { t: 'Мобильная сеть', s: '4G', icon: 'signal_cellular_alt', h: 250 }] },
      { items: [{ t: 'Режим полёта', icon: 'flight', h: 250, bind: 'airplane' }, { t: 'Точка доступа и модем', s: 'Выкл.', icon: 'wifi_tethering', h: 250 }] },
      { items: [{ t: 'Экономия трафика', s: 'Выкл.', icon: 'data_saver_on', h: 250 }, { t: 'VPN', s: 'Нет', icon: 'vpn_key', h: 250 }, { t: 'Частный DNS', s: 'Автоматически', icon: 'dns', h: 250 }] },
    ] },
    wifi: { title: 'Wi‑Fi', groups: [
      { items: [{ html: `<button class="item tap" data-bind="wifi" style="background:var(--primary-container);color:var(--on-primary-container);border-radius:28px;min-height:76px"><span class="itx"><span class="it" style="color:inherit;font-size:18px">Использовать Wi‑Fi</span></span>${switchHTML()}</button>` }] },
      { title: 'Сети', items: [
        { t: 'HOME_WIFI', s: 'Подключено', icon: 'wifi', h: 250, v: '' },
        { t: 'Наоми‑5G', s: 'Сохранена', icon: 'wifi', h: 250, nav: 'generic:Наоми‑5G' },
        { t: 'Дача', s: 'Защищена', icon: 'wifi_2_bar', h: 250, nav: 'generic:Дача' },
        { t: 'Кафе_гость', s: 'Открытая', icon: 'wifi_1_bar', h: 250, nav: 'generic:Кафе_гость' }] },
      { items: [{ t: 'Добавить сеть', icon: 'add', h: 250 }, { t: 'Сохранённые сети', s: '2 сети', icon: 'bookmark', h: 250 }, { t: 'Настройки Wi‑Fi', s: 'Включать автоматически', icon: 'settings', h: 250 }] },
    ] },
    connected: { title: 'Подключённые устройства', groups: [
      { items: [{ t: 'Bluetooth', icon: 'bluetooth', h: 195, bind: 'bt' }, { t: 'Подключить новое устройство', icon: 'add', h: 195 }] },
      { title: 'Ранее подключённые', items: [{ t: 'Наушники', s: 'Подключено', icon: 'headphones', h: 195 }, { t: 'Колонка на кухне', s: 'Не подключено', icon: 'speaker', h: 195 }] },
      { items: [{ t: 'NFC', s: 'Обмен данными на близком расстоянии', icon: 'nfc', h: 195, bind: 'nfc' }, { t: 'Печать', s: '1 служба', icon: 'print', h: 195 }] },
    ] },
    display: { title: 'Экран', groups: [
      { title: 'Яркость', items: [{ t: 'Уровень яркости', slider: 'brightness', sicon: 'light_mode' }, { t: 'Адаптивная яркость', s: 'Подстраивать под освещение', bind: 'adaptive' }] },
      { title: 'Внешний вид', items: [{ t: 'Тёмная тема', icon: 'dark_mode', h: 70, bind: 'dark' }, { t: 'Ночной режим', s: 'Тёплый оттенок экрана', icon: 'nightlight', h: 70, bind: 'night' }] },
      { title: 'Плавность', items: [{ t: '90 Гц', s: 'Плавнее прокрутка и анимации', radio: 'hz90', value: 'true' }, { t: '60 Гц', s: 'Дольше работает батарея', radio: 'hz90', value: 'false' }] },
      { items: [{ t: 'Размер шрифта и масштаб', s: 'По умолчанию', icon: 'format_size', h: 70, nav: 'fontsize' },
        { t: 'Отключение экрана', s: 'Через 1 минуту', icon: 'timer', h: 70 }, { t: 'Автоповорот экрана', icon: 'screen_rotation', h: 70, bind: 'rotate' }] },
    ] },
    fontsize: { title: 'Размер шрифта и масштаб', pre: `<div style="margin:0 16px 18px" class="preview-card"><div class="fs-prev" style="font-size:calc(15px + var(--fs,0.4) * 8px);line-height:1.4">Привет! Так будет выглядеть текст в программах и в «Настройках».</div></div>`, groups: [
      { items: [{ t: 'Размер шрифта', slider: 'fontScale', sicon: 'format_size' }, { t: 'Масштаб изображения', slider: 'displayScale', sicon: 'zoom_in' }] },
      { items: [{ t: 'Жирный шрифт', bind: 'boldText' }, { t: 'Сбросить', icon: 'restart_alt', h: 70 }] },
    ] },
    sound: { title: 'Звук и вибрация', groups: [
      { items: [{ t: 'Музыка и видео', slider: 'volMedia', sicon: 'music_note' }, { t: 'Звонок', slider: 'volRing', sicon: 'ring_volume' },
        { t: 'Уведомления', slider: 'volNotif', sicon: 'notifications' }, { t: 'Будильник', slider: 'volAlarm', sicon: 'alarm' }] },
      { items: [{ t: 'Вибрация при звонке', bind: 'vibrate' }, { t: 'Не беспокоить', s: 'Только важное', bind: 'dnd' }] },
      { items: [{ t: 'Мелодия звонка', s: 'Наоми', icon: 'music_note', h: 300 }, { t: 'Звук уведомлений', s: 'Капля', icon: 'water_drop', h: 300 }] },
    ] },
    battery: { title: 'Батарея', pre: `<div class="hero"><div class="big">87%</div><div class="t-bl">Хватит примерно до 22:00 завтра</div><div class="bars">${Array.from({ length: 24 }, (_, i) => `<i style="height:${Math.round(30 + 60 * Math.abs(Math.sin(i * 0.5 + 1)))}%" class="${i === 23 ? 'now' : ''}"></i>`).join('')}</div></div>`, groups: [
      { items: [{ t: 'Энергосбережение', s: 'Тёмная тема, меньше фоновой работы', icon: 'battery_saver', h: 135, bind: 'saver' }, { t: 'Заряд в процентах', s: 'В строке состояния', bind: 'showPct' }] },
      { items: [{ t: 'Использование батареи', s: 'С последней зарядки', icon: 'bar_chart', h: 135 }] },
    ] },
    style: { title: 'Обои и стиль', groups: [
      { title: 'Обои', items: [{ html: `<div class="item col"><div class="wp-row">${Object.entries(Theme.WALLPAPERS).map(([id]) => `<button class="wp-thumb" data-choice="wallpaper" data-value="${id}" data-wp="${id}"></button>`).join('')}</div></div>` }] },
      { title: 'Цвета', items: [{ html: `<div class="item col"><div class="seed-row">${['#9a5b1e', '#6750a4', '#0b6e8a', '#3f6c2a', '#b3261e', '#7d5260', '#8c7a00', '#3d4a86'].map(c => `<button class="seed" data-choice="seed" data-value="${c}" style="background:${c}"></button>`).join('')}</div>
        <div class="chips">${[['tonalSpot', 'Спокойная'], ['vibrant', 'Яркая'], ['expressive', 'Выразительная'], ['fidelity', 'Точная'], ['neutral', 'Нейтральная'], ['monochrome', 'Монохром']].map(([v, n]) => `<button class="chip" data-choice="variant" data-value="${v}">${n}</button>`).join('')}</div></div>` },
        { t: 'Тёмная тема', bind: 'dark' }] },
      { title: 'Значки', items: [{ html: `<div class="item col"><div class="shape-row">${Object.entries(Theme.SHAPES).map(([id, sh]) => `<button class="shape-opt" data-choice="iconShape" data-value="${id}" title="${sh.name}"><i style="clip-path:${sh.clip}"></i></button>`).join('')}</div></div>` },
        { t: 'Тематические значки', s: 'Цвет значков — из обоев', bind: 'themed' }] },
    ] },
    system: { title: 'Система', groups: [
      { items: [{ t: 'Языки и ввод', s: 'Русский', icon: 'language', h: 230 }, { t: 'Жесты', s: 'Навигация, быстрые жесты', icon: 'swipe_up', h: 230, nav: 'gestures' }, { t: 'Дата и время', s: 'GMT+03:00 Москва', icon: 'schedule', h: 230 }] },
      { items: [{ t: 'Резервное копирование', icon: 'backup', h: 230 }, { t: 'Обновление системы', s: 'НаомиФон 0.3', icon: 'system_update', h: 230 }, { t: 'Сброс настроек', icon: 'restart_alt', h: 230 }] },
    ] },
    gestures: { title: 'Жесты', groups: [
      { title: 'Навигация в системе', items: [
        { t: 'Навигация жестами', s: 'Вверх — домой, вверх и задержать — недавние, от края — назад', radio: 'nav', value: 'gesture' },
        { t: 'Три кнопки', s: 'Назад, главный экран и недавние — внизу', radio: 'nav', value: 'buttons' }] },
      { items: [{ t: 'Двойное нажатие питания', s: 'Открыть камеру', bind: 'dblPower' }, { t: 'Перевернуть для «Не беспокоить»', bind: 'flipDnd' }] },
    ] },
    about: { title: 'О телефоне', pre: `<div class="about-hero"><div class="phone"></div><div class="t-hs" style="margin-top:8px">НаомиФон</div><div class="muted t-bm">Pixel 5 · redfin</div></div>`, groups: [
      { items: [{ t: 'Название устройства', v: 'НаомиФон' }, { t: 'Модель', v: 'Pixel 5' }] },
      { items: [{ t: 'Система', v: 'НаомиФон 0.3' }, { t: 'Основа', v: 'Ubuntu 24.04' }, { t: 'Ядро', v: 'Linux 4.19' }] },
      { items: [{ t: 'Экран', v: '6,0″ · 90 Гц' }, { t: 'Процессор', v: 'Snapdragon 765G' }, { t: 'Память', v: '8 ГБ · 128 ГБ' }] },
    ] },
  };
  function settingsPage(route) {
    let P = S[route];
    if (!P) {
      const title = route.startsWith('generic:') ? route.slice(8) : 'Раздел';
      P = { title, groups: [{ items: [{ t: 'Основное', s: 'Пример строки', icon: 'tune', h: 250, nav: 'generic:Основное' }, { t: 'Дополнительно', s: 'Ещё одна страница', icon: 'more_horiz', h: 250, nav: 'generic:Дополнительно' }] },
        { items: [{ t: 'Включено', bind: 'demo_' + title }, { t: 'Подробнее', v: '' }] },
        { items: [{ html: `<div class="item col"><span class="is">Это заглушка: раздел «${esc(title)}» нужен, чтобы попробовать переходы вперёд и назад.</span></div>` }] }] };
    }
    const root = route === 'root';
    return `<div class="page set-page" data-route="${route}">
      <header class="tbar">${root ? '<span style="width:12px"></span>' : `<button class="ibtn" data-act="back">${icon('arrow_back')}</button>`}<div class="tbar-title">${P.title}</div>${root ? `<button class="ibtn">${icon('search')}</button>` : ''}</header>
      <div class="scroll"><h1 class="big-title">${P.title}</h1>${P.pre || ''}${P.groups.map(groupHTML).join('')}</div></div>`;
  }

  // ---- Другие программы ----
  const tbar = (title, back, actions = '', always = false) =>
    `<header class="tbar ${always ? 'always' : ''}">${back ? `<button class="ibtn" data-act="back">${icon('arrow_back')}</button>` : '<span style="width:12px"></span>'}<div class="tbar-title">${title}</div>${actions}</header>`;
  const CHATS = [['Наоми', 'Скинь фотки с выходных 📷', '14:02', 290], ['Мама', 'Позвони, как освободишься', '12:40', 25], ['Работа', 'Созвон перенесли на 16:00', '11:15', 220],
    ['Саша', 'Ок, договорились', 'Вчера', 145], ['Дача', 'Вода в бочке закончилась', 'Вчера', 85], ['Банк', 'Код подтверждения: ••••', 'Пн', 250]];
  const thread = name => `<div class="page" data-route="thread:${esc(name)}">${tbar(name, true, `<button class="ibtn">${icon('call')}</button><button class="ibtn">${icon('more_vert')}</button>`, true)}
    <div class="scroll"><div class="thread">${['in:Привет! Как дела?', 'out:Привет! Всё хорошо, собираю новый телефон 😄', 'in:На Линуксе?', 'out:Да, на Pixel 5. Своя оболочка, жесты как в новом Андроиде',
      'in:Скинь фотки с выходных 📷', 'out:Сейчас найду'].map(m => { const [d, t] = m.split(/:(.*)/s); return `<div class="bubble ${d}">${esc(t)}</div>`; }).join('')}</div></div>
    <div class="composer"><span class="grow">Сообщение</span><button class="send">${icon('send', 'f')}</button></div></div>`;
  const galColors = Array.from({ length: 30 }, (_, i) => `linear-gradient(${(i * 47) % 360}deg, oklch(0.7 0.12 ${(i * 37) % 360}), oklch(0.45 0.1 ${(i * 37 + 60) % 360}))`);

  function page(appId, route) {
    const a = BY[appId];
    if (appId === 'settings') return settingsPage(route);
    if (appId === 'messages') {
      if (route.startsWith('thread:')) return thread(route.slice(7));
      return `<div class="page" data-route="root">${tbar('Сообщения', false, `<button class="ibtn">${icon('search')}</button>`)}<div class="scroll"><h1 class="big-title">Сообщения</h1>
        ${CHATS.map(([n, t, tm, h]) => `<button class="list-row" style="width:100%" data-nav="thread:${n}"><span class="av" style="background:oklch(0.85 0.08 ${h});color:oklch(0.3 0.08 ${h})">${n[0]}</span><span style="flex:1;min-width:0"><span class="t-bl" style="display:block">${n}</span><span class="t-bm muted" style="display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis">${t}</span></span><span class="t-bs muted">${tm}</span></button>`).join('')}
        </div><button class="fab">${icon('chat')}Новый чат</button></div>`;
    }
    if (appId === 'phone') {
      return `<div class="page" data-route="root">${tbar('Телефон', false, `<button class="ibtn">${icon('more_vert')}</button>`)}<div class="scroll"><h1 class="big-title">Недавние</h1>
        ${[['Мама', 'call_received', 'Входящий · 12:41'], ['Наоми', 'call_made', 'Исходящий · 10:05'], ['+7 900 000-00-00', 'call_missed', 'Пропущенный · Вчера'], ['Саша', 'call_received', 'Входящий · Вчера'], ['Работа', 'call_made', 'Исходящий · Пн']]
          .map(([n, i, s], k) => `<div class="list-row"><span class="av" style="background:oklch(0.85 0.08 ${k * 70});color:oklch(0.3 0.08 ${k * 70})">${n[0] === '+' ? icon('person') : n[0]}</span><span style="flex:1"><span class="t-bl" style="display:block">${n}</span><span class="t-bm muted" style="display:flex;gap:4px;align-items:center"><span class="ms" style="font-size:16px">${i}</span>${s}</span></span><button class="ibtn">${icon('call')}</button></div>`).join('')}
        </div><button class="fab">${icon('dialpad')}Набрать</button></div>`;
    }
    if (appId === 'gallery') {
      if (route.startsWith('photo:')) { const i = +route.slice(6); return `<div class="page" data-route="${route}" style="background:#000"><div class="photo-view"><i style="background:${galColors[i]}"></i></div>${tbar('', true, `<button class="ibtn" style="color:#fff">${icon('share')}</button>`).replace('class="ibtn"', 'class="ibtn" style="color:#fff"')}</div>`; }
      return `<div class="page" data-route="root">${tbar('Галерея', false, `<button class="ibtn">${icon('search')}</button>`)}<div class="scroll"><h1 class="big-title">Фото</h1><div class="gal">${galColors.map((c, i) => `<i data-nav="photo:${i}" style="background:${c}"></i>`).join('')}</div></div></div>`;
    }
    if (appId === 'calc') {
      const keys = [['AC', 'fn'], ['()', 'op'], ['%', 'op'], ['÷', 'op'], ['7'], ['8'], ['9'], ['×', 'op'], ['4'], ['5'], ['6'], ['−', 'op'], ['1'], ['2'], ['3'], ['+', 'op'], ['0'], [','], ['⌫'], ['=', 'eq']];
      return `<div class="page" data-route="root"><div class="calc"><div class="calc-disp"><div class="calc-expr" data-calc-expr></div><div class="calc-out" data-calc-out>0</div></div>
        <div class="calc-keys">${keys.map(([k, c]) => `<button class="ck ${c || ''}" data-calc="${k}">${k}</button>`).join('')}</div></div></div>`;
    }
    if (appId === 'camera') {
      return `<div class="page" data-route="root" style="background:#000;color:#fff"><div class="cam"><div class="cam-view"></div><div class="cam-modes"><span>Видео</span><span class="on">Фото</span><span>Портрет</span></div><button class="cam-shutter" data-act="shutter"></button></div></div>`;
    }
    if (appId === 'clock') {
      return `<div class="page" data-route="root">${tbar('Часы', false, `<button class="ibtn">${icon('more_vert')}</button>`, true)}<div class="scroll"><div class="clock-big" data-live-clock>--:--</div><div class="t-bl muted" style="text-align:center;margin-bottom:24px" data-live-date></div>
        <div class="grp">${[['07:30', 'Будни', true], ['09:00', 'Выходные', false], ['14:45', 'Таблетки', true]].map(([t, s, on], i) => `<button class="item tap" data-bind="alarm${i}"><span class="itx"><span style="display:block;font-size:32px;line-height:40px">${t}</span><span class="is" style="display:block">${s}</span></span>${switchHTML()}</button>`).join('')}</div></div>
        <button class="fab">${icon('add')}</button></div>`;
    }
    // заглушка
    return `<div class="page" data-route="root">${tbar(a.name, false, `<button class="ibtn">${icon('more_vert')}</button>`)}<div class="scroll"><h1 class="big-title">${a.name}</h1>
      <div class="app-hero">${icon(a.icon, 'f')}<div class="t-tl" style="margin-top:12px">Здесь будет «${a.name}»</div><div class="t-bm" style="opacity:.8;margin-top:4px">Макет: на этой программе удобно пробовать запуск, жест «домой» и «назад».</div></div>
      ${['Первый пункт', 'Второй пункт', 'Третий пункт', 'Четвёртый пункт', 'Пятый пункт'].map((t, i) => `<button class="list-row" style="width:100%" data-nav="generic:${t}"><span class="av" style="background:var(--surface-container-highest)">${icon('article')}</span><span class="t-bl" style="flex:1">${t}</span><span class="ms muted">chevron_right</span></button>`).join('')}
      </div></div>`;
  }
  // Страница «вглубь» обычной программы
  function subPage(appId, route) {
    if (appId === 'settings' || appId === 'messages' || appId === 'gallery') return page(appId, route);
    const title = route.startsWith('generic:') ? route.slice(8) : route;
    return `<div class="page" data-route="${esc(route)}">${tbar(title, true, '', true)}<div class="scroll"><div class="app-hero" style="margin-top:16px"><div class="t-tl">${esc(title)}</div><div class="t-bm" style="opacity:.8;margin-top:4px">Вложенная страница. Вернитесь жестом от края или кнопкой «назад».</div></div>
      ${Array.from({ length: 8 }, (_, i) => `<div class="list-row"><span class="av" style="background:var(--surface-container-highest)">${icon('label')}</span><span class="t-bl">Строка ${i + 1}</span></div>`).join('')}</div></div>`;
  }

  return { LIST, BY, HOME, DOCK, iconHTML, appIcon, sliderHTML, switchHTML, page, subPage, icon, esc };
})();
