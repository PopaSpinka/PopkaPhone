'use strict';
/* Оболочка макета: экраны, окна программ, жесты. Числа «как в Android» — из исходников Android 17
   (Launcher3, SystemUI, WM Shell); источники — в docs/UI.md. Размеры — dp экрана Pixel 5 (393 × 851). */
const Shell = (() => {
  const W = 393, H = 851, SB = 48, R = 36, CW = Math.round(W * 0.64), CGAP = 18, CY = 404;
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const screen = $('#screen'), world = $('#world'), wallEl = $('#wallpaper'), home = $('#home'), homeScrim = $('#homeScrim'),
    drawer = $('#drawer'), recBg = $('#recentsBg'), winsEl = $('#windows'), recUi = $('#recentsUi'), lock = $('#lock'),
    sbar = $('#statusbar'), shade = $('#shade'), vol = $('#volPanel'), navbar = $('#navbar'), arrow = $('#backArrow'),
    dimEl = $('#dim'), nightEl = $('#night'), offEl = $('#screenOff'), touch = $('#touchDot'), toastEl = $('#toast'), privacy = $('#privacy');
  const V = Motion.Value;
  const st = { locked: true, on: true, fg: null, apps: [], recents: false, recLive: false, shadeKind: 'single', homeRows: [], homeMin: 0.92, homeBlur: 0, backPeek: 0 };
  const LAUNCHER = { x: { z: 0.965, k: 450 }, y: { z: 0.95, k: 400 }, s: { z: 0.99, k: 500 } };   // Launcher3, «окно → иконка»
  const TILE_SPRING = { z: 1, k: 1500 };
  let themeHook = null;

  // ---------- Значения, которые двигаем ----------
  const lockP = new V(0, renderLock, 0.0005);
  const homeZ = new V(0, renderHome, 0.0005);
  const drawerP = new V(0, renderDrawer, 0.0005);
  const shadeP = new V(0, renderShade, 0.0005);
  const recP = new V(0, renderRecents, 0.0005);
  const recScroll = new V(0, renderRecents, 0.0005);
  const volP = new V(0, renderVol, 0.001);

  const shapeR = () => (Theme.SHAPES[Store.get('iconShape')] || Theme.SHAPES.circle).radius;
  const scaleOf = () => screen.getBoundingClientRect().width / W;
  function screenRect(el) {
    const r = el.getBoundingClientRect(), s = screen.getBoundingClientRect(), k = s.width / W;
    return { x: (r.left - s.left) / k, y: (r.top - s.top) / k, w: r.width / k, h: r.height / k };
  }
  function layoutRect(el, stop = home) {   // без учёта transform предков — куда встанет значок в покое
    let x = 0, y = 0, n = el;
    while (n && n !== stop) { x += n.offsetLeft; y += n.offsetTop; n = n.offsetParent; }
    return { x, y, w: el.offsetWidth, h: el.offsetHeight };
  }
  const homeIcon = id => home.querySelector(`.app-icon[data-app="${id}"]`);

  // ================= Главный экран и список программ =================
  function buildHome() {
    const cols = +Store.get('grid');
    const gridApps = cols === 5 ? Apps.LIST.filter(a => !Apps.DOCK.includes(a.id)).map(a => a.id) : Apps.HOME;
    home.innerHTML = `<div class="glance" data-row="0"><div class="glance-date" data-date-short></div><div class="glance-sub"><span class="ms">cloud</span><span>4 °C · облачно</span></div></div>
      <div class="home-grid ${cols === 5 ? 'cols5' : ''}">${gridApps.map(id => Apps.appIcon(Apps.BY[id])).join('')}</div>
      <div class="dock" data-row="5">${Apps.DOCK.map(id => Apps.appIcon(Apps.BY[id], false)).join('')}</div>
      <div class="home-search" data-row="6"><span class="ms">search</span><span class="grow">Поиск</span><button class="round" data-act="mic"><span class="ms">mic</span></button></div>`;
    st.homeRows = [];
    st.homeRows.push([$('.glance', home), 0]);
    $$('.home-grid .app-icon', home).forEach((el, i) => st.homeRows.push([el, 1 + Math.floor(i / cols)]));
    st.homeRows.push([$('.dock', home), 5], [$('.home-search', home), 6]);
    const sorted = [...Apps.LIST].sort((a, b) => a.name.localeCompare(b.name, 'ru'));
    drawer.innerHTML = `<div class="drawer-sheet"><div class="drawer-head"><div class="drawer-grab"></div><div class="drawer-search"><span class="ms">search</span>Поиск программ</div></div>
      <div class="drawer-grid">${sorted.map(a => Apps.appIcon(a)).join('')}</div></div>`;
    tickClock(); renderHome(); renderDrawer();
  }

  function renderHome() {
    const z = homeZ.v, d = clamp(drawerP.v), L = lockP.v, Lc = clamp(L), uv = Store.get('unlock');
    const lockVis = st.locked || Lc < 1;
    // открытие из блокировки (меняем весь «мир»: главный экран и окна)
    let op = 1, tf = '', origin = '50% 50%';
    if (lockVis) {
      if (uv === 'android') { op = prog(Lc, 0.05, 0.6); tf = `translateY(${(1 - L) * H * 0.05}px) scale(${lerp(0.95, 1, L)})`; origin = '50% 66%'; }
      else if (uv === 'stagger') op = prog(Lc, 0.05, 0.5);
      else if (uv === 'zoom') { op = prog(Lc, 0, 0.65); tf = `scale(${lerp(0.86, 1, L)})`; }
      else if (uv === 'fade') op = Lc;
    }
    world.style.opacity = op; world.style.transform = tf; world.style.transformOrigin = origin;
    const rise = lockVis && uv === 'stagger' ? 1 - L : 0;
    for (const [el, row] of st.homeRows) el.style.transform = rise ? `translateY(${rise * (40 + row * 26)}px)` : '';
    // главный экран позади программы / под списком
    const zc = clamp(z, -0.3, 1.3), sc = 1 - (1 - st.homeMin) * zc;
    home.style.transform = `translateY(${-d * 50}px) scale(${sc})`;
    home.style.opacity = 1 - prog(d, 0, 0.6);
    homeScrim.style.opacity = clamp(z) * 0.45 + st.backPeek;
    const blur = st.homeBlur * clamp(z);
    home.style.filter = blur > 0.3 ? `blur(${blur.toFixed(1)}px)` : '';
    wallEl.style.filter = home.style.filter;
    home.style.visibility = (z >= 0.999 && !st.recents && st.fg && !drawerP.running) ? 'hidden' : 'visible';
  }

  function renderDrawer() {
    const d = drawerP.v, sheet = drawer.firstElementChild;
    drawer.style.visibility = d > 0.002 ? 'visible' : 'hidden';
    if (!sheet) return;
    sheet.style.transform = `translateY(${(1 - d) * H * 0.5}px) scale(${st.drawerBack ? 1 - 0.06 * st.drawerBack : 1})`;
    sheet.style.opacity = prog(d, 0, 0.3);
    sheet.style.borderRadius = `${lerp(28, 0, prog(d, 0.85, 1)) + (st.drawerBack ? 28 * st.drawerBack : 0)}px`;
    updateStatus();
  }

  // ================= Блокировка =================
  function buildLock() {
    lock.innerHTML = `<div class="lock-bg"></div><div class="lock-scrim"></div><div class="lock-content">
      <div class="lock-clock"><span class="l1" data-hh></span><span class="sep">:</span><span class="l2" data-mm></span></div>
      <div class="lock-small"><div class="t" data-time></div><div class="d"><span data-date-long></span> · <span class="ms" style="font-size:18px">cloud</span> 4 °C</div></div>
      <div class="lock-date"><span data-date-long></span> · <span class="w"><span class="ms">cloud</span>4 °C</span></div>
      <div class="lock-shelf"><span class="dotic"><span class="ms">chat</span></span><span class="dotic"><span class="ms">system_update</span></span><span class="dotic"><span class="ms">partly_cloudy_day</span></span></div>
      <div class="lock-notifs notifs">${NOTIFS.map(notifHTML).join('')}</div>
      <div class="lock-bottom"><button class="lock-sc l" data-bind="flashlight" title="Фонарик"><span class="ms">flashlight_on</span></button>
        <span class="ms lock-icon">lock</span><div class="lock-hint">Проведите вверх, чтобы разблокировать</div>
        <button class="lock-sc r" data-act="lockcam" title="Камера"><span class="ms">photo_camera</span></button></div></div>`;
    applyWallpaper(); applyLockStyle(); tickClock(); renderLock();
  }
  function applyLockStyle() {
    lock.classList.remove('style-big', 'style-small', 'style-flex');
    lock.classList.add('style-' + Store.get('lockClock'));
    lock.classList.toggle('sheet', Store.get('unlock') === 'sheet');
  }
  function renderLock() {
    const p = lockP.v, pc = clamp(p), v = Store.get('unlock'), content = lock.querySelector('.lock-content'), scrim = lock.querySelector('.lock-scrim');
    lock.style.visibility = (!st.locked && pc >= 0.999) ? 'hidden' : 'visible';
    let tf = '', op = 1, cTf = '', cOp = 1, scr = 1 - pc;
    if (v === 'android' || v === 'stagger') { cTf = `translateY(${-p * H * 0.22}px)`; cOp = 1 - prog(pc, 0, 0.5); }
    else if (v === 'zoom') { cTf = `scale(${1 + 0.14 * p})`; cOp = 1 - prog(pc, 0, 0.55); }
    else if (v === 'sheet') { tf = `translateY(${-p * H}px)`; scr = 1; }
    else { op = 1 - pc; }
    lock.style.transform = tf; lock.style.opacity = op;
    if (content) { content.style.transform = cTf; content.style.opacity = cOp; }
    if (scrim) scrim.style.opacity = scr;
    renderHome();
  }
  function lockNow() {
    st.locked = true;
    stopAll(); shadeP.set(0); drawerP.set(0); hideVolume(true);
    if (st.recents) leaveRecentsInstant();
    lockP.set(0); renderLock(); updateStatus();
  }
  async function unlockCommit(v0 = 0) {
    st.locked = false; updateStatus();
    const v = Store.get('unlock');
    const ok = v === 'android' ? await lockP.tween(1, 300 * (1 - clamp(lockP.v)) + 120, 'emphasizedDecelerate') : await lockP.spring(1, 'spatial', v0);
    if (ok) renderLock();
  }

  // ================= Строка состояния =================
  function batteryHTML(forShade = false) {
    const pct = 87, show = Store.get('showPct') || forShade, saver = Store.get('saver');
    if (Store.get('battery') === 'new') return `<span class="bat new ${saver ? 'saver' : ''}"><span class="bat-fill" style="width:${pct}%"></span>${show ? `<span class="bat-num">${pct}</span>` : ''}</span>`;
    return `${show ? `<span class="bat-pct">${pct}%</span>` : ''}<span class="bat classic"><span class="ms f">${saver ? 'battery_saver' : 'battery_5_bar'}</span></span>`;
  }
  function sysIconsHTML() {
    const s = Store.all(), out = [];
    if (s.dnd) out.push('do_not_disturb_on');
    if (s.airplane) out.push('flight');
    if (s.bt && !s.airplane) out.push('bluetooth');
    if (!s.airplane && s.mobile) out.push('signal_cellular_4_bar');   // Android 16 QPR1: мобильная сеть левее Wi-Fi
    if (s.wifi) out.push('wifi');
    return out.map(i => `<span class="ms">${i}</span>`).join('');
  }
  function updateStatus() {
    const light = !Store.get('dark');
    const appUp = st.fg && !st.recents;
    const darkIcons = light && !st.locked && (appUp && st.fg.app.id !== 'camera' || (!st.fg && !st.recents && drawerP.v > 0.5));
    screen.style.setProperty('--sb-fg', darkIcons ? '#1d1b20' : '#ffffff');
    screen.style.setProperty('--sb-inv', darkIcons ? '#ffffff' : '#1d1b20');
    screen.style.setProperty('--nav-fg', darkIcons ? '#1d1b20' : '#ffffff');
    sbar.innerHTML = `<div class="sb-left">${st.locked ? '' : '<span class="sb-clock" data-time></span>'}<span class="sb-notifs">${st.locked ? '' : '<span class="ms">chat</span><span class="ms">system_update</span>'}</span><span class="sb-rec ${Store.get('rec') ? 'on' : ''}"></span></div>
      <div class="sb-right">${sysIconsHTML()}${batteryHTML()}</div>`;
    const hi = shade.querySelector('[data-sh-icons]');
    if (hi) hi.innerHTML = sysIconsHTML() + batteryHTML(true);
    const cam = st.fg && st.fg.app.id === 'camera' && !st.recents && !st.locked;
    if (cam && !privacy.classList.contains('on')) {
      privacy.classList.add('on'); privacy.classList.remove('dot');
      clearTimeout(st.privT); st.privT = setTimeout(() => privacy.classList.add('dot'), 2000);
    } else if (!cam) privacy.classList.remove('on', 'dot');
    tickClock();
  }

  // ================= Часы и даты =================
  const cap = s => s.charAt(0).toUpperCase() + s.slice(1);
  function tickClock() {
    const d = new Date(), hh = String(d.getHours()).padStart(2, '0'), mm = String(d.getMinutes()).padStart(2, '0');
    const short = cap(new Intl.DateTimeFormat('ru-RU', { weekday: 'short', day: 'numeric', month: 'short' }).format(d));
    const long = cap(new Intl.DateTimeFormat('ru-RU', { weekday: 'long', day: 'numeric', month: 'long' }).format(d));
    $$('[data-time]', screen).forEach(e => (e.textContent = `${hh}:${mm}`));
    $$('[data-hh]', screen).forEach(e => (e.textContent = hh));
    $$('[data-mm]', screen).forEach(e => (e.textContent = mm));
    $$('[data-date-short]', screen).forEach(e => (e.textContent = short));
    $$('[data-date-long]', screen).forEach(e => (e.textContent = long));
    $$('[data-live-clock]', screen).forEach(e => (e.textContent = `${hh}:${mm}`));
    $$('[data-live-date]', screen).forEach(e => (e.textContent = long));
  }

  // ================= Шторка =================
  const NOTIFS = [
    { app: 'Сообщения', icon: 'chat', title: 'Наоми', text: 'Скинь фотки с выходных 📷', time: '2 мин', av: 'Н' },
    { app: 'Система', icon: 'system_update', title: 'Доступно обновление', text: 'НаомиФон 0.4 — новый интерфейс', time: '1 ч' },
    { app: 'Погода', icon: 'partly_cloudy_day', title: 'Завтра +6°, без осадков', text: 'Сегодня облачно, вечером ветер', time: '3 ч' },
  ];
  function notifHTML(n) {
    return `<div class="notif">${n.av ? `<span class="avatar">${n.av}</span>` : `<span class="nic"><span class="ms">${n.icon}</span></span>`}
      <div class="nbody"><div class="nhead">${n.app} · ${n.time}</div><div class="nt">${n.title}</div><div class="nx">${n.text}</div></div></div>`;
  }
  const TILES = {
    wifi: { t: 'Wi‑Fi', on: 'wifi', off: 'wifi_off', s: v => (v ? 'HOME_WIFI' : 'Выкл.') },
    mobile: { t: 'Мобильный интернет', on: 'signal_cellular_alt', off: 'signal_cellular_off', s: v => (v ? '4G' : 'Выкл.') },
    bt: { t: 'Bluetooth', on: 'bluetooth', off: 'bluetooth_disabled', s: v => (v ? 'Наушники' : 'Выкл.') },
    flashlight: { t: 'Фонарик', on: 'flashlight_on', off: 'flashlight_off' },
    dnd: { t: 'Не беспокоить', on: 'do_not_disturb_on', off: 'do_not_disturb_off' },
    hz90: { t: 'Плавность', on: 'speed', off: 'speed', s: v => (v ? '90 Гц' : '60 Гц') },
    dark: { t: 'Тёмная тема', on: 'dark_mode', off: 'light_mode' },
    rotate: { t: 'Автоповорот', on: 'screen_rotation', off: 'screen_lock_rotation' },
    airplane: { t: 'Режим полёта', on: 'flight', off: 'flight' },
    saver: { t: 'Энергосбережение', on: 'battery_saver', off: 'battery_saver' },
    location: { t: 'Геолокация', on: 'location_on', off: 'location_off' },
    night: { t: 'Ночной режим', on: 'nightlight', off: 'nightlight' },
  };
  const tileHTML = (key, large) => {
    const d = TILES[key];
    return large ? `<button class="tile large" data-bind="${key}" data-tile="${key}"><span class="tile-ic"><span class="ms">${d.on}</span></span><span class="tile-tx"><span class="tile-t">${d.t}</span><span class="tile-s"></span></span></button>`
      : `<button class="tile small" data-bind="${key}" data-tile="${key}" title="${d.t}"><span class="ms">${d.on}</span></button>`;
  };
  function buildShade() {
    shade.innerHTML = `<div class="shade-scrim"></div><div class="shade-layer">
      <div class="sh-head" data-sh="head"><div class="sh-time" data-time></div><div class="sh-date" data-date-short></div><div class="sh-icons" data-sh-icons></div></div>
      <div class="sh-group" data-sh="bright">${Apps.sliderHTML('brightness', 'light_mode', 'm')}</div>
      <div class="sh-group" data-sh="vol">${Apps.sliderHTML('volMedia', 'volume_up', 'm')}</div>
      <div class="sh-group tiles" data-sh="rows12">${tileHTML('wifi', 1)}${tileHTML('mobile', 1)}${tileHTML('bt', 1)}${tileHTML('flashlight')}${tileHTML('dnd')}</div>
      <div class="sh-group tiles" data-sh="rows34">${tileHTML('hz90', 1)}${tileHTML('dark')}${tileHTML('rotate')}${tileHTML('airplane')}${tileHTML('saver')}${tileHTML('location')}${tileHTML('night')}</div>
      <div class="sh-group sh-foot" data-sh="foot"><div class="grow" style="justify-content:flex-start;padding-left:8px"><i class="pg-dot on"></i><i class="pg-dot"></i></div>
        <button class="rbtn" data-act="qs-edit" title="Изменить"><span class="ms">edit</span></button><button class="rbtn" data-act="qs-user" title="Пользователь"><span class="ms">person</span></button>
        <button class="rbtn" data-act="open-settings" title="Настройки"><span class="ms">settings</span></button><button class="rbtn pw" data-act="power-menu" title="Питание"><span class="ms">power_settings_new</span></button></div>
      <div class="sh-handle" data-sh="handle"></div>
      <div class="sh-group" data-sh="notifs">
        <div class="media" style="--mh:${Theme.hexToOklch(Store.get('seed')).H.toFixed(0)}"><div class="mrow"><span class="ms">music_note</span><span class="t-ls" style="opacity:.8">Музыка · Наушники</span></div>
          <div><div class="mt">Тишина</div><div class="ma">Наоми</div></div>
          <div class="mrow"><svg viewBox="0 0 200 20" preserveAspectRatio="none"><path class="squig" d="${squiggle()}" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round"/><path d="M118 10 H200" stroke="currentColor" stroke-opacity=".35" stroke-width="3" stroke-linecap="round"/></svg>
          <button class="play playing" data-act="play"><span class="ms f">pause</span></button></div></div>
        <div class="notifs">${NOTIFS.map(notifHTML).join('')}</div>
        <div class="notif-actions"><button class="pill-btn" data-act="notif-history">История</button><button class="pill-btn" data-act="clear-notifs">Очистить все</button></div>
      </div></div>`;
    tickClock(); updateStatus(); refreshAll(shade); renderShade();
  }
  function squiggle() { let d = 'M0 10'; for (let x = 0; x <= 116; x += 2) d += ` L${x} ${(10 + Math.sin(x / 6) * 4).toFixed(2)}`; return d; }
  function shadeGeom() {
    const volOn = Store.get('volInShade') !== false;
    const bright = 80, volTop = 128, tiles = volOn ? 184 : 136, tilesBottom = tiles + 4 * 80 - 8, foot = tilesBottom + 18;
    return { bright, volTop, tiles, tilesBottom, foot, handle: foot + 60, volOn };
  }
  function renderShade() {
    const s = shadeP.v, kind = st.shadeKind, g = shadeGeom();
    const open = s > 0.002;
    shade.classList.toggle('open', open);
    if (!open && !shadeP.running) st.shadeKind = Store.get('shade') === 'dual' ? 'notif' : 'single';
    const s1 = clamp(s), s2 = kind === 'single' ? clamp(s - 1) : (kind === 'qs' ? 1 : 0);
    const q = k => shade.querySelector(`[data-sh="${k}"]`);
    const set = (k, top, op, extra = '') => { const el = q(k); if (!el) return; el.style.transform = `translateY(${top}px) ${extra}`; el.style.opacity = op; el.style.visibility = op > 0.01 ? 'visible' : 'hidden'; };
    const scrim = shade.querySelector('.shade-scrim'); if (scrim) scrim.style.opacity = s1;
    sbar.style.opacity = 1 - prog(s1, 0, 0.35);
    set('head', -(1 - s1) * 24, prog(s1, 0.2, 1));
    const drop = -(1 - s1) * 120;
    if (kind === 'notif') {
      ['bright', 'vol', 'rows12', 'rows34', 'foot', 'handle'].forEach(k => set(k, 0, 0));
      set('notifs', 72 + drop, prog(s1, 0.15, 1));
      return;
    }
    const late = prog(s2, 0.43, 1);   // остальное проявляется с 43 % второго вытягивания
    const rowsTop = lerp(72, g.tiles, s2);
    set('rows12', rowsTop + drop, prog(s1, 0.2, 1));
    set('rows34', g.tiles + 160 + drop * 0.5 - (1 - s2) * 40, late);
    set('bright', g.bright - (1 - s2) * 40 + drop, late);
    set('vol', g.volTop - (1 - s2) * 40 + drop, g.volOn ? late : 0);
    set('foot', g.foot - (1 - s2) * 60 + drop, late);
    set('handle', lerp(232, g.handle, s2) + drop, prog(s1, 0.5, 1));
    if (kind === 'qs') set('notifs', 0, 0);
    else set('notifs', lerp(244, g.handle + 24, s2) + drop * 1.15, prog(s1, 0.3, 1) * (1 - prog(s2, 0, 0.6)));
  }
  const openShade = (to = 1, kind) => { if (kind) st.shadeKind = kind; else if (shadeP.v < 0.01) st.shadeKind = Store.get('shade') === 'dual' ? 'qs' : 'single'; return shadeP.spring(to, 'spatial'); };
  const closeShade = () => shadeP.spring(0, 'spatial');

  // ================= Окна программ =================
  function createInst(app) {
    const el = document.createElement('div');
    el.className = 'win'; el.dataset.app = app.id;
    el.innerHTML = `<div class="win-inner"><div class="pages"></div></div><div class="win-splash"><div class="win-splash-bg"></div>${Apps.iconHTML(app)}</div>`;
    winsEl.appendChild(el);
    const inst = { app, el, inner: $('.win-inner', el), pagesEl: $('.pages', el), sbg: $('.win-splash-bg', el), sic: $('.win-splash .ic', el), stack: [], phase: 'live', iconSize: 56 };
    const r = () => renderWin(inst);
    inst.cx = new V(W / 2, r, 0.05); inst.cy = new V(H / 2, r, 0.05); inst.w = new V(W, r, 0.05);
    inst.sq = new V(0, r, 0.0005); inst.r = new V(R, r, 0.05); inst.a = new V(1, r, 0.0005); inst.lq = new V(1, r, 0.0005);
    inst.slot = new V(0, renderRecents, 0.0005); inst.dy = new V(0, renderRecents, 0.05);
    inst.stack.push(makePage(inst, Apps.page(app.id, 'root')));
    return inst;
  }
  function makePage(inst, html) {
    const t = document.createElement('div'); t.innerHTML = html.trim();
    const pg = t.firstElementChild;
    pg.insertAdjacentHTML('beforeend', '<div class="page-scrim"></div>');
    inst.pagesEl.appendChild(pg);
    const sc = $('.scroll', pg), bar = $('.tbar', pg);
    if (sc && bar) sc.addEventListener('scroll', () => bar.classList.toggle('scrolled', sc.scrollTop > 40), { passive: true });
    refreshAll(pg); paintThumbs(pg); tickClock();
    return pg;
  }
  function renderWin(inst) {
    const w = Math.max(1, inst.w.v), sq = clamp(inst.sq.v), h = lerp((w * H) / W, w, sq);
    const el = inst.el.style;
    el.transform = `translate(${inst.cx.v - w / 2}px, ${inst.cy.v - h / 2}px)`;
    el.width = w + 'px'; el.height = h + 'px';
    el.borderRadius = Math.max(0, inst.r.v) + 'px';
    el.opacity = clamp(inst.a.v);
    const s = Math.max(w / W, h / H);
    inst.inner.style.transform = `translate(${(w - W * s) / 2}px, ${(h - H * s) / 2}px) scale(${s})`;
    let bgA = 0, icA, cA;
    if (inst.phase === 'launch') { bgA = prog(w, inst.iconSize, inst.iconSize + 40); icA = clamp(inst.lq.v); cA = 0; }
    else { icA = prog(w, 170, inst.iconSize + 4); cA = 1 - prog(w, 130, inst.iconSize + 2); }
    inst.inner.style.opacity = cA; inst.sbg.style.opacity = bgA; inst.sic.style.opacity = icA;
  }
  function stopInst(i) { [i.cx, i.cy, i.w, i.sq, i.r, i.a, i.lq, i.slot, i.dy].forEach(v => v.stop()); }
  function stopAll() { for (const i of st.apps) stopInst(i); }
  const showWin = i => i.el.classList.add('shown');
  const hideWin = i => i.el.classList.remove('shown');
  const instOf = el => st.apps.find(i => i.el.contains(el));
  function fullNow(i) { stopInst(i); i.phase = 'live'; i.cx.set(W / 2); i.cy.set(H / 2); i.w.set(W); i.sq.set(0); i.r.set(R); i.a.set(1); showWin(i); }

  async function launch(appId, fromEl) {
    if (!st.on) return;
    const app = Apps.BY[appId];
    let inst = st.apps.find(i => i.app.id === appId);
    const warm = !!inst;
    if (!inst) inst = createInst(app);
    if (st.recents) leaveRecentsInstant();
    if (shadeP.v > 0.01) closeShade();
    st.apps = [inst, ...st.apps.filter(i => i !== inst)];
    for (const o of st.apps) if (o !== inst) { stopInst(o); hideWin(o); }
    st.fg = inst; const token = (inst.token = {});
    stopInst(inst); showWin(inst);
    const icEl = fromEl && (fromEl.querySelector('.ic') || fromEl);
    const src = icEl ? screenRect(icEl) : null;
    inst.iconSize = src ? Math.round(src.w) : 56;
    inst.sic.style.width = inst.sic.style.height = inst.iconSize + 'px';
    const variant = Store.get('launch');
    const anims = [];
    const appIconEl = fromEl && fromEl.closest('.app-icon');
    st.homeBlur = 0;
    if ((variant === 'icon' || variant === 'iconx') && src) {
      inst.phase = warm ? 'live' : 'launch';
      inst.cx.set(src.x + src.w / 2); inst.cy.set(src.y + src.h / 2); inst.w.set(src.w); inst.sq.set(1); inst.r.set(src.w * shapeR()); inst.a.set(1); inst.lq.set(1);
      if (appIconEl) appIconEl.classList.add('hidden-ic');
      if (variant === 'icon') {
        // Android: 500 мс, emphasized; углы от круга к радиусу экрана; значок гаснет на 25–75 мс; дом 1 → 0,97 за 350 мс; размытие за окном до 30 dp
        st.homeMin = 0.97; st.homeBlur = 12;
        const t = new V(0, () => {
          const q = t.v;
          inst.cx.v = lerp(src.x + src.w / 2, W / 2, q); inst.cy.v = lerp(src.y + src.h / 2, H / 2, q);
          inst.w.v = lerp(src.w, W, q); inst.sq.v = 1 - q; inst.r.v = lerp(src.w / 2, R, q);
          inst.lq.v = 1 - prog(t.raw, 25 / 500, 75 / 500);
          renderWin(inst);
        });
        const lin = new V(0, () => { t.raw = lin.v; t.v = Motion.ease('emphasized')(lin.v); t.render(); });
        t.raw = 0;
        anims.push(lin.tween(1, 500, 'linear'), homeZ.tween(1, 350, 'standard'));
      } else {
        st.homeMin = 0.92;
        anims.push(inst.cx.spring(W / 2, 'spatial'), inst.cy.spring(H / 2, 'spatial'), inst.w.spring(W, 'spatial'), inst.sq.spring(0, 'spatial'), inst.r.spring(R, 'spatial'), homeZ.spring(1, 'spatial'));
      }
    } else if (variant === 'slide') {
      inst.phase = 'live'; inst.cx.set(W / 2); inst.cy.set(H / 2 + H); inst.w.set(W); inst.sq.set(0); inst.r.set(R); inst.a.set(1);
      st.homeMin = 0.92;
      anims.push(inst.cy.spring(H / 2, 'spatial'), homeZ.spring(1, 'spatial'));
    } else {
      inst.phase = 'live'; inst.cx.set(W / 2); inst.cy.set(H / 2); inst.w.set(W * 0.86); inst.sq.set(0); inst.r.set(R); inst.a.set(0);
      st.homeMin = 0.92;
      anims.push(inst.w.spring(W, 'spatial'), inst.a.spring(1, 'effects'), homeZ.spring(1, 'spatial'));
    }
    updateStatus();
    await Promise.all(anims);
    if (appIconEl) appIconEl.classList.remove('hidden-ic');
    if (inst.token !== token) return;
    if (inst.phase === 'launch') { inst.el.classList.add('fading'); inst.phase = 'live'; renderWin(inst); setTimeout(() => inst.el.classList.remove('fading'), 260); }
    drawerP.set(0); renderHome();
  }

  async function goHome(inst, vel = { vx: 0, vy: 0 }, opts = {}) {
    if (!inst) { if (st.recents) return recentsToHome(); if (drawerP.v > 0.01) return drawerP.spring(0, 'spatial'); if (shadeP.v > 0.01) return closeShade(); return; }
    const token = (inst.token = {});
    st.fg = null; inst.phase = 'live'; updateStatus();
    const variant = opts.variant || Store.get('home');
    for (const o of st.apps) if (o !== inst && o.el.classList.contains('shown')) o.a.spring(0, 'effects').then(() => { if (st.fg !== o && !st.recents) hideWin(o); });
    const iconEl = homeIcon(inst.app.id);
    const anims = [];
    const k = (W * 0.55) / (H * 0.6);
    if ((variant === 'icon' || variant === 'iconx') && iconEl && drawerP.v < 0.01) {
      const icEl = $('.ic', iconEl), t = layoutRect(icEl);
      inst.iconSize = t.w; inst.sic.style.width = inst.sic.style.height = t.w + 'px';
      iconEl.classList.add('hidden-ic');
      const sx = variant === 'icon' ? LAUNCHER.x : 'spatial', sy = variant === 'icon' ? LAUNCHER.y : 'spatial', ss = variant === 'icon' ? LAUNCHER.s : 'spatial';
      anims.push(inst.cx.spring(t.x + t.w / 2, sx, Motion.sim(vel.vx)), inst.cy.spring(t.y + t.h / 2, sy, Motion.sim(vel.vy)),
        inst.w.spring(t.w, ss, Motion.sim(vel.vy * k)), inst.sq.spring(1, ss), inst.r.spring(t.w * shapeR(), ss));
      if (variant === 'icon') { st.homeMin = Math.min(st.homeMin, 0.85); anims.push(homeZ.tween(0, opts.homeMs || 1000, 'emphasizedDecelerate')); }
      else anims.push(homeZ.spring(0, 'spatial'));
    } else if (variant === 'up') {
      anims.push(inst.cy.spring(-H * 0.6, 'spatial', Motion.sim(Math.min(vel.vy, -900))), inst.a.spring(0, 'effectsSlow'), inst.w.spring(W * 0.7, 'spatial'), homeZ.spring(0, 'spatial'));
    } else {
      anims.push(inst.cx.spring(W / 2, 'spatial'), inst.cy.spring(H * 0.46, 'spatial'), inst.w.spring(W * 0.5, 'spatial'), inst.a.spring(0, 'effects'), homeZ.spring(0, 'spatial'));
    }
    await Promise.all(anims.slice(0, 3));
    st.homeBlur = 0; renderHome();
    if (inst.token !== token) { iconEl && iconEl.classList.remove('hidden-ic'); return; }
    hideWin(inst); inst.a.set(1); stopInst(inst);
    if (iconEl) { iconEl.classList.remove('hidden-ic', 'land'); void iconEl.offsetWidth; iconEl.classList.add('land'); }
  }
  function restoreFg(inst, vel = { vx: 0, vy: 0 }) {
    st.fg = inst; updateStatus();
    for (const o of st.apps) if (o !== inst && o.el.classList.contains('shown')) o.a.spring(0, 'effects').then(() => { if (st.fg !== o && !st.recents) hideWin(o); });
    inst.cx.spring(W / 2, 'spatial', Motion.sim(vel.vx)); inst.cy.spring(H / 2, 'spatial', Motion.sim(vel.vy));
    inst.w.spring(W, 'spatial'); inst.r.spring(R, 'spatial'); inst.sq.spring(0, 'spatial'); inst.a.spring(1, 'effects');
    homeZ.spring(1, 'spatial');
  }

  // ---------- Страницы внутри программы ----------
  function resetPage(pg) {
    pg.style.transform = ''; pg.style.opacity = ''; pg.style.clipPath = ''; pg.style.borderRadius = ''; pg.style.boxShadow = ''; pg.style.removeProperty('--cin');
    const s = $('.page-scrim', pg); if (s) s.style.opacity = 0;
  }
  const scrimOf = (pg, a) => { const s = $('.page-scrim', pg); if (s) s.style.opacity = a; };
  function relRect(el, inst) {
    const r = screenRect(el), w = screenRect(inst.inner);
    return { x: r.x - w.x, y: r.y - w.y, w: r.w, h: r.h };
  }
  function renderFwd(v, out, inn, q, fr) {
    const qc = clamp(q);
    if (v === 'android') {        // Settings Android 17: новая с +96 dp, проявление 83 мс после 50 мс; старая уезжает на −96 dp, не гаснет
      inn.style.transform = `translateX(${96 * (1 - q)}px)`; inn.style.opacity = prog(st.fwdT || 0, 50, 133);
      out.style.transform = `translateX(${-96 * q}px)`;
    } else if (v === 'axis') {    // Material shared axis X: 30 dp, смена прозрачности на 35 %
      inn.style.transform = `translateX(${30 * (1 - q)}px)`; inn.style.opacity = prog(q, 0.35, 1);
      out.style.transform = `translateX(${-30 * q}px)`; out.style.opacity = 1 - prog(q, 0, 0.35);
    } else if (v === 'slide') {
      inn.style.transform = `translateX(${W * (1 - q)}px)`; inn.style.boxShadow = '-10px 0 30px rgba(0,0,0,.25)';
      out.style.transform = `translateX(${-W * 0.3 * q}px)`; scrimOf(out, 0.3 * qc);
    } else if (v === 'fade') {    // fade through: 0,35; 0,92 → 1
      inn.style.transform = `scale(${0.92 + 0.08 * q})`; inn.style.opacity = prog(q, 0.35, 1);
      out.style.opacity = 1 - prog(q, 0, 0.35);
    } else if (v === 'container' && fr) {   // container transform: 500 мс, затемнение 0x52
      const m = x => Math.max(0, x).toFixed(1);
      inn.style.clipPath = `inset(${m(lerp(fr.y, 0, q))}px ${m(lerp(W - fr.x - fr.w, 0, q))}px ${m(lerp(H - fr.y - fr.h, 0, q))}px ${m(lerp(fr.x, 0, q))}px round ${lerp(22, 0, qc).toFixed(1)}px)`;
      inn.style.setProperty('--cin', prog(q, 0.2, 0.6)); scrimOf(out, 0.32 * qc);
    } else { inn.style.transform = `translateX(${30 * (1 - q)}px)`; inn.style.opacity = prog(q, 0.35, 1); }
  }
  async function pushPage(inst, route, fromEl) {
    if (inst.navBusy) return;
    inst.navBusy = true;
    const v = Store.get('forward'), out = inst.stack[inst.stack.length - 1];
    const fr = fromEl ? relRect(fromEl, inst) : null;
    const pg = makePage(inst, Apps.subPage(inst.app.id, route));
    inst.stack.push(pg);
    const dur = v === 'container' ? 500 : 450;
    st.fwdT = 0;
    const lin = new V(0, () => { st.fwdT = lin.v * dur; renderFwd(v, out, pg, Motion.ease('emphasized')(lin.v), fr); });
    renderFwd(v, out, pg, 0, fr);
    await lin.tween(1, dur, 'linear');
    resetPage(out); out.style.visibility = 'hidden'; resetPage(pg);
    inst.navBusy = false;
  }
  // «Назад» без жеста (кнопкой): тот же вариант, что и для жеста, но с нуля
  async function pageBack(inst, from = null) {
    if (inst.navBusy && !from) return;
    inst.navBusy = true;
    const v = Store.get('back'), cur = inst.stack[inst.stack.length - 1], prev = inst.stack[inst.stack.length - 2];
    prev.style.visibility = 'visible';
    const f = from || { pe: 0, ty: 0, side: 1 };
    const dark = Store.get('dark');
    const lin = new V(0, () => {
      const t = lin.v, q = Motion.ease('emphasized')(t);
      if (v === 'predictive') {   // система: закрываемое ещё на +96 dp и гаснет за 90 мс, предыдущее разворачивается
        const s0 = 1 - 0.15 * f.pe, tx0 = f.side * Math.max(0, (W - W * s0) / 2 - 8);
        cur.style.transform = `translate(${tx0 + f.side * 96 * q}px, ${f.ty * (1 - q)}px) scale(${s0})`; cur.style.opacity = Math.max(1 - 5 * t, 0);
        cur.style.borderRadius = '28px';
        const ps = lerp(lerp(0.95, 0.85, f.pe), 1, q), ptx = lerp(-96 * f.side * (1 - 0.2 * f.pe), 0, q);
        prev.style.transform = `translate(${ptx}px, ${f.ty * (1 - q)}px) scale(${ps})`; prev.style.borderRadius = `${28 * (1 - q)}px`;
        scrimOf(prev, (dark ? 0.8 : 0.2) * f.pe * (1 - q) + (from ? 0 : (dark ? 0.5 : 0.2) * (1 - q) * 0.5));
      } else if (v === 'material') {   // Material: уходящий 100 → 90 %, входящий 110 → 100 %, смена на 35 %
        const s0 = 1 - 0.1 * f.pe;
        cur.style.transform = `translate(${f.side * (W / 20 - 8) * f.pe}px, ${f.ty}px) scale(${lerp(s0, 0.9, q)})`; cur.style.opacity = 1 - prog(q, 0, 0.35); cur.style.borderRadius = '28px';
        prev.style.transform = `scale(${lerp(1.1, 1, q)})`; prev.style.opacity = prog(q, 0.35, 1);
      } else if (v === 'slide') {
        const x0 = f.slideX || 0;
        cur.style.transform = `translateX(${lerp(x0, W, q)}px)`; cur.style.boxShadow = '-10px 0 30px rgba(0,0,0,.25)';
        prev.style.transform = `translateX(${lerp(-W * 0.3 * (1 - x0 / W), 0, q)}px)`; scrimOf(prev, 0.3 * (1 - x0 / W) * (1 - q));
      } else {    // ось X назад
        cur.style.transform = `translateX(${30 * q}px)`; cur.style.opacity = 1 - prog(q, 0, 0.35);
        prev.style.transform = `translateX(${-30 * (1 - q)}px)`; prev.style.opacity = prog(q, 0.35, 1);
      }
    });
    lin.render();
    await lin.tween(1, v === 'slide' ? 350 : 450, 'linear');
    cur.remove(); inst.stack.pop(); resetPage(prev);
    inst.navBusy = false;
  }
  function navBack() {
    if (shadeP.v > 0.01) return closeShade();
    if (volP.v > 0.01) return hideVolume();
    if (st.recents) return recentsToHome();
    if (st.fg) return st.fg.stack.length > 1 ? pageBack(st.fg) : goHome(st.fg, { vx: 0, vy: 0 });
    if (drawerP.v > 0.01) return drawerP.spring(0, 'spatial');
  }
  function navHome() {
    if (st.locked) return;
    if (shadeP.v > 0.01) closeShade();
    if (st.fg) return goHome(st.fg, { vx: 0, vy: -1200 });
    if (st.recents) return recentsToHome();
    if (drawerP.v > 0.01) return drawerP.spring(0, 'spatial');
  }

  // ================= Недавние =================
  const cardX = slot => W / 2 - (slot - recScroll.v) * (CW + CGAP);
  const CH = Math.round((CW * H) / W);
  function buildRecUi() {
    recUi.innerHTML = st.apps.map(i => `<div class="chip-card" data-chip="${i.app.id}">${Apps.iconHTML(i.app)}<span>${i.app.name}</span></div>`).join('') +
      `<div class="rec-actions"><button class="rec-btn" data-act="screenshot"><span class="ms">screenshot</span>Снимок экрана</button><button class="rec-btn" data-act="select"><span class="ms">touch_app</span>Выбрать</button></div>
       <button class="rec-btn rec-clear" data-act="clear-all">Очистить всё</button><div class="rec-empty">Нет недавних приложений</div>`;
  }
  async function enterRecents() {
    if (st.locked || !st.on) return;
    if (shadeP.v > 0.01) closeShade();
    if (drawerP.v > 0.01) drawerP.spring(0, 'spatial');
    const from = st.fg;
    st.recents = true; st.recLive = false; st.fg = null; screen.classList.add('in-recents'); updateStatus();
    buildRecUi(); recScroll.set(0);
    recUi.style.visibility = 'visible';
    st.homeMin = 0.92;
    recP.spring(1, 'effects'); homeZ.spring(1, 'spatial');
    const anims = [];
    st.apps.forEach((i, n) => {
      stopInst(i); i.slot.set(n); i.dy.set(0); i.phase = 'live';
      if (i !== from && !i.el.classList.contains('shown')) { i.cx.set(cardX(n) - 80); i.cy.set(CY); i.w.set(CW * 0.92); i.sq.set(0); i.r.set(20); i.a.set(0); }
      showWin(i);
      anims.push(i.cx.spring(cardX(n), 'spatial'), i.cy.spring(CY, 'spatial'), i.w.spring(CW, 'spatial'), i.r.spring(20, 'spatial'), i.sq.spring(0, 'spatial'), i.a.spring(1, 'effects'));
    });
    const token = (st.recToken = {});
    renderRecents();
    await Promise.all(anims);
    if (st.recToken === token && st.recents) { st.recLive = true; renderRecents(); }
  }
  function renderRecents() {
    const p = clamp(recP.v);
    recBg.style.opacity = p; recUi.style.opacity = p;
    if (!st.recents && p < 0.01) recUi.style.visibility = 'hidden';
    const n = st.apps.length;
    if (st.recLive) {
      for (const i of st.apps) {
        i.cx.v = cardX(i.slot.v); i.cy.v = CY + i.dy.v; i.w.v = CW; i.sq.v = 0; i.r.v = 20;
        i.a.v = 1 - clamp(-i.dy.v / (H * 0.7)) * 0.9; renderWin(i);
      }
    }
    for (const i of st.apps) {
      const chip = recUi.querySelector(`[data-chip="${i.app.id}"]`);
      if (!chip) continue;
      const x = i.cx.v - (i.w.v / 2), h = lerp((i.w.v * H) / W, i.w.v, clamp(i.sq.v));
      chip.style.transform = `translate(${x}px, ${i.cy.v - h / 2 - 40}px)`; chip.style.opacity = clamp(i.a.v);
    }
    const clr = recUi.querySelector('.rec-clear'), emp = recUi.querySelector('.rec-empty');
    if (clr) { clr.style.display = n ? '' : 'none'; clr.style.transform = `translate(${cardX(n) - 60}px, ${CY - 20}px)`; }
    if (emp) emp.style.display = n ? 'none' : '';
  }
  function cardAt(x, y) {
    if (!st.recents) return null;
    for (const i of st.apps) { const cx = cardX(i.slot.v); if (Math.abs(x - cx) < CW / 2 && Math.abs(y - (CY + i.dy.v)) < CH / 2) return i; }
    return null;
  }
  function leaveRecentsInstant() {
    st.recents = false; st.recLive = false; st.recToken = null; screen.classList.remove('in-recents');
    recP.set(0); for (const i of st.apps) if (i !== st.fg) { stopInst(i); hideWin(i); i.a.set(1); }
    renderRecents();
  }
  async function openFromRecents(inst) {
    st.recents = false; st.recLive = false; st.recToken = null; screen.classList.remove('in-recents');
    recP.spring(0, 'effects');
    st.apps = [inst, ...st.apps.filter(i => i !== inst)];
    st.fg = inst; updateStatus();
    const token = (inst.token = {});
    for (const o of st.apps) if (o !== inst) {
      const dir = o.cx.v < inst.cx.v ? -1 : 1;
      o.cx.spring(o.cx.v + dir * W * 0.5, 'spatial'); o.a.spring(0, 'effects').then(() => { if (st.fg !== o && !st.recents) hideWin(o); });
    }
    await Promise.all([inst.cx.spring(W / 2, 'spatial'), inst.cy.spring(H / 2, 'spatial'), inst.w.spring(W, 'spatial'), inst.r.spring(R, 'spatial'), inst.a.spring(1, 'effects')]);
    if (inst.token === token) renderHome();
  }
  async function recentsToHome() {
    st.recents = false; st.recLive = false; st.recToken = null; screen.classList.remove('in-recents');
    recP.spring(0, 'effects'); homeZ.spring(0, 'spatial');
    await Promise.all(st.apps.map(i => { i.w.spring(CW * 0.9, 'spatial'); return i.a.spring(0, 'effects'); }));
    if (!st.recents) for (const i of st.apps) if (st.fg !== i) { hideWin(i); i.a.set(1); }
    updateStatus();
  }
  async function dismissCard(inst, vy = -1500) {
    const idx = st.apps.indexOf(inst);
    await inst.dy.spring(-H, 'spatial', Motion.sim(Math.min(vy, -1200)));
    inst.el.remove(); st.apps.splice(st.apps.indexOf(inst), 1);
    recUi.querySelector(`[data-chip="${inst.app.id}"]`)?.remove();
    st.apps.forEach((i, n) => { if (n >= idx) i.slot.spring(n, 'spatial'); });
    if (recScroll.v > Math.max(0, st.apps.length - 1)) recScroll.spring(Math.max(0, st.apps.length - 1), 'spatial');
    renderRecents();
    if (!st.apps.length) setTimeout(() => st.recents && recentsToHome(), 500);
  }
  async function clearAll() {
    const list = [...st.apps];
    list.forEach((i, n) => setTimeout(() => i.dy.spring(-H, 'spatial', Motion.sim(-1600)), (n * 45) / Motion.speed));
    await Motion.sleep(450 + list.length * 45);
    list.forEach(i => i.el.remove()); st.apps = []; recUi.innerHTML = ''; buildRecUi(); renderRecents();
    recentsToHome();
  }

  // ================= Громкость =================
  function renderVol() { vol.style.transform = `translateX(${(1 - volP.v) * 110}px)`; vol.style.opacity = clamp(volP.v * 1.5); }
  function showVolume(delta = 0) {
    if (!st.on) return;
    if (delta) Store.set('volMedia', clamp(Math.round((Store.get('volMedia') + delta) * 15) / 15));
    vol.classList.add('shown'); volP.spring(1, 'spatialFast');
    clearTimeout(st.volT); st.volT = setTimeout(() => hideVolume(), 2600 / Motion.speed);
  }
  function hideVolume(now) {
    clearTimeout(st.volT);
    if (now) { volP.set(0); vol.classList.remove('shown'); return; }
    volP.spring(0, 'effects').then(ok => ok && vol.classList.remove('shown'));
  }

  // ================= Привязки к состоянию =================
  const parse = s => (s === 'true' ? true : s === 'false' ? false : /^\d+(\.\d+)?$/.test(s) ? +s : s);
  function refreshKey(key, root = screen) {
    const v = Store.get(key);
    root.querySelectorAll(`[data-bind="${key}"]`).forEach(el => {
      el.classList.toggle('on', !!v);
      const sw = el.querySelector('.sw'); if (sw) sw.classList.toggle('on', !!v);
      if (el.dataset.tile) { const d = TILES[key]; const ic = el.querySelector('.ms'); if (ic) ic.textContent = v ? d.on : d.off; const s = el.querySelector('.tile-s'); if (s) s.textContent = d.s ? d.s(v) : (v ? 'Вкл.' : 'Выкл.'); }
    });
    root.querySelectorAll(`[data-radio="${key}"]`).forEach(el => { const on = String(v) === el.dataset.value; el.querySelector('.rd')?.classList.toggle('on', on); });
    root.querySelectorAll(`[data-choice="${key}"]`).forEach(el => el.classList.toggle('on', String(v) === el.dataset.value));
    root.querySelectorAll(`[data-slider="${key}"]`).forEach(el => paintSlider(el, v));
    root.querySelectorAll(`[data-vslider="${key}"]`).forEach(el => el.style.setProperty('--v', v));
  }
  function refreshAll(root = screen) {
    const keys = new Set();
    root.querySelectorAll('[data-bind],[data-radio],[data-choice],[data-slider],[data-vslider]').forEach(el => keys.add(el.dataset.bind || el.dataset.radio || el.dataset.choice || el.dataset.slider || el.dataset.vslider));
    keys.forEach(k => refreshKey(k, root));
  }
  function paintSlider(el, v) {
    el.style.setProperty('--v', v);
    const ic = el.querySelector('.sl-icon'); if (!ic) return;
    const w = el.offsetWidth || 361, act = v * (w - 16);
    if (act >= 46) { ic.classList.remove('out'); ic.style.left = ''; }
    else { ic.classList.add('out'); ic.style.left = `${act + 16 + 8}px`; }
  }
  function paintThumbs(root) {
    root.querySelectorAll('[data-wp]').forEach(el => (el.style.backgroundImage = `url(${Theme.wallpaper(el.dataset.wp, 120, 260, 1)})`));
  }
  function applyWallpaper() {
    const url = `url(${Theme.wallpaper(Store.get('wallpaper'))})`;
    wallEl.style.backgroundImage = url;
    const lb = lock.querySelector('.lock-bg'); if (lb) lb.style.backgroundImage = url;
  }
  function applyTheme() {
    const dark = Store.get('dark');
    const { colors, source } = Theme.compute(Store.get('seed'), dark, Store.get('variant'));
    Theme.apply(screen, colors, dark);
    st.colorSource = source;
    screen.classList.toggle('light', !dark);
    const m = shade.querySelector('.media'); if (m) m.style.setProperty('--mh', Theme.hexToOklch(Store.get('seed')).H.toFixed(0));
    updateStatus();
    if (themeHook) themeHook(source);
  }
  function applyMotion() {
    Motion.scheme = Store.get('scheme'); Motion.speed = +Store.get('speed');
    Motion.applyCss(screen);
    const t = Motion.cssSpring(TILE_SPRING);
    screen.style.setProperty('--sp-tile', t.easing); screen.style.setProperty('--sp-tile-ms', t.ms + 'ms');
  }
  function applyIcons() {
    screen.style.setProperty('--ic-clip', (Theme.SHAPES[Store.get('iconShape')] || Theme.SHAPES.circle).clip);
    screen.classList.toggle('icons-themed', !!Store.get('themed'));
    screen.classList.toggle('icons-colored', !Store.get('themed'));
  }
  function applyNav() {
    const buttons = Store.get('nav') === 'buttons';
    navbar.classList.toggle('buttons', buttons);
  }
  function applyFont() {
    screen.classList.remove('font-roboto', 'font-system');
    if (Store.get('font') === 'roboto') screen.classList.add('font-roboto');
    if (Store.get('font') === 'system') screen.classList.add('font-system');
  }

  Store.on('*', (k, v) => {
    refreshKey(k);
    switch (k) {
      case 'dark': case 'seed': case 'variant': applyTheme(); break;
      case 'wallpaper': applyWallpaper(); Store.set('seed', Theme.WALLPAPERS[v].seed); break;
      case 'iconShape': case 'themed': applyIcons(); break;
      case 'font': applyFont(); break;
      case 'grid': buildHome(); break;
      case 'brightness': dimEl.style.opacity = (1 - v) * 0.62; break;
      case 'night': nightEl.style.opacity = v ? 0.16 : 0; break;
      case 'nav': applyNav(); break;
      case 'scheme': case 'speed': applyMotion(); break;
      case 'lockClock': case 'unlock': applyLockStyle(); renderLock(); break;
      case 'blur': screen.classList.toggle('no-blur', !v); break;
      case 'shade': if (shadeP.v > 0.01) closeShade(); st.shadeKind = v === 'dual' ? 'notif' : 'single'; break;
      case 'volInShade': renderShade(); break;
      case 'battery': case 'showPct': case 'saver': case 'wifi': case 'bt': case 'airplane': case 'dnd': case 'rec': case 'mobile': updateStatus(); break;
      case 'fontScale': $$('.fs-prev', screen).forEach(e => e.style.setProperty('--fs', v)); break;
    }
  });

  // ================= Жесты =================
  let G = null, suppressClick = false;
  function loc(e) { const r = screen.getBoundingClientRect(), s = r.width / W; return { x: (e.clientX - r.left) / s, y: (e.clientY - r.top) / s }; }
  function showTouch(p, on) {
    if (!Store.get('touches')) { touch.classList.remove('on'); return; }
    if (!p) { touch.classList.remove('on'); return; }
    touch.style.transform = `translate(${p.x}px, ${p.y}px)`; if (on) touch.classList.add('on');
  }
  function startG(type, e, p, immediate) {
    G = { type, id: e.pointerId, x0: p.x, y0: p.y, x: p.x, y: p.y, t0: performance.now(), decided: immediate, tr: new Motion.Tracker(), target: e.target, pointerType: e.pointerType };
    G.tr.add(p.x, p.y);
    if (immediate) { G.h = H_[type]; G.h.begin(G); }
  }
  screen.addEventListener('pointerdown', e => {
    suppressClick = false;
    if (e.button > 0 || G || !st.on) return;
    const p = loc(e);
    showTouch(p, true);
    if (e.target.closest('.volpanel') && !e.target.closest('[data-vslider]')) return;
    const vs = e.target.closest('[data-vslider]'), sl = e.target.closest('[data-slider]');
    if (vs) { e.stopPropagation(); startG('vslider', e, p, true); G.el = vs; G.h.begin(G); return; }
    let type = null;
    const gest = Store.get('nav') === 'gesture';
    if (p.y < SB - 4) type = 'shade';
    else if (gest && p.y > H - 26 && !st.locked) type = 'home';
    else if ((p.x < 16 || p.x > W - 16) && !st.locked) type = 'back';
    if (type) { e.preventDefault(); e.stopPropagation(); startG(type, e, p, true); return; }
    if (sl) { e.stopPropagation(); startG('slider', e, p, false); G.el = sl; G.decided = true; G.h = H_.slider; G.h.begin(G); return; }
  }, true);
  screen.addEventListener('pointerdown', e => {
    if (e.button > 0 || G || !st.on) return;
    const p = loc(e);
    let cand = null;
    if (shadeP.v > 0.01) cand = 'shadeArea';
    else if (st.locked) cand = 'lockArea';
    else if (st.recents) cand = 'recentsArea';
    else if (st.fg) cand = e.pointerType === 'mouse' && e.target.closest('.scroll') ? 'mouseScroll' : null;
    else if (drawerP.v > 0.01) cand = e.target.closest('.drawer-head') ? 'drawerArea' : e.target.closest('.drawer-grid') ? 'drawerGrid' : 'drawerArea';
    else cand = 'homeArea';
    if (cand) startG(cand, e, p, false);
  });
  window.addEventListener('pointermove', e => {
    if (!G || e.pointerId !== G.id) return;
    const p = loc(e); G.x = p.x; G.y = p.y; G.tr.add(p.x, p.y); showTouch(p);
    if (!G.decided) {
      const dx = p.x - G.x0, dy = p.y - G.y0;
      if (Math.hypot(dx, dy) < 8) return;
      const h = decide(G, dx, dy);
      if (!h) { G = null; return; }
      G.decided = true; G.type = h; G.h = H_[h]; G.h.begin(G); suppressClick = true;
    }
    G.h.move(G, p);
  });
  const endG = cancel => e => {
    if (!G || e.pointerId !== G.id) return;
    showTouch(null);
    const g = G; G = null;
    if (!g.decided) { if (g.type === 'recentsArea' && !cancel) recentsTap(g, e); return; }
    if (Math.hypot(g.x - g.x0, g.y - g.y0) > 6) suppressClick = true;
    g.h.end(g, cancel ? { vx: 0, vy: 0 } : g.tr.velocity(), cancel);
  };
  window.addEventListener('pointerup', endG(false));
  window.addEventListener('pointercancel', endG(true));
  screen.addEventListener('click', e => { if (suppressClick) { e.stopPropagation(); e.preventDefault(); suppressClick = false; } }, true);

  function decide(g, dx, dy) {
    const vert = Math.abs(dy) > Math.abs(dx);
    switch (g.type) {
      case 'shadeArea': if (!vert && g.target.closest('.notif')) return 'notif'; return vert ? 'shadeDrag' : null;
      case 'lockArea': return vert ? (dy < 0 ? 'unlock' : 'shade') : null;
      case 'homeArea': return vert ? (dy < 0 ? 'drawer' : 'shade') : null;
      case 'drawerArea': return vert && dy > 0 ? 'drawer' : null;
      case 'drawerGrid': { const grid = $('.drawer-grid', drawer); if (!vert) return null; if (dy > 0 && grid.scrollTop <= 0) return 'drawer'; return g.pointerType === 'mouse' ? 'mouseScroll' : null; }
      case 'recentsArea': if (!vert) return 'recScroll'; return dy < 0 && cardAt(g.x0, g.y0) ? 'recDismiss' : null;
      case 'mouseScroll': return vert ? 'mouseScroll' : null;
    }
    return null;
  }

  const shadeG = {
    begin(g) {
      shadeP.stop();
      if (shadeP.v < 0.01) st.shadeKind = Store.get('shade') === 'dual' ? (g.x0 < W / 2 ? 'notif' : 'qs') : 'single';
      g.start = shadeP.v; g.max = st.shadeKind === 'single' ? 2 : 1;
    },
    move(g, p) { let v = g.start + (p.y - g.y0) / 300; if (v > g.max) v = g.max + (v - g.max) * 0.15; shadeP.set(Math.max(0, v)); },
    end(g, vel) {
      const vv = vel.vy / 300, v = shadeP.v;
      let t;
      if (vv > 1.2) t = Math.min(g.max, Math.floor(v + 0.001) + 1);
      else if (vv < -1.2) t = Math.max(0, Math.ceil(v - 0.001) - 1);
      else t = clamp(Math.round(v), 0, g.max);
      if (g.start < 0.01 && t === 2) t = 1;
      shadeP.spring(t, 'spatial', Motion.sim(vv));
    },
  };
  const unlockG = {
    begin(g) { lockP.stop(); g.start = lockP.v; },
    move(g, p) { lockP.set(clamp(g.start + (g.y0 - p.y) / (H * 0.42), 0, 1)); },
    end(g, v) {
      const vv = -v.vy / (H * 0.42);
      if (lockP.v > 0.4 || vv > 1.0) unlockCommit(Motion.sim(Math.max(vv, 0)));
      else lockP.spring(0, 'spatial', Motion.sim(vv));
    },
  };
  const drawerG = {
    begin(g) { drawerP.stop(); g.start = drawerP.v; },
    move(g, p) { drawerP.set(clamp(g.start + (g.y0 - p.y) / (H * 0.55), 0, 1.04)); },
    end(g, v) {
      const vv = -v.vy / (H * 0.55), d = drawerP.v;
      const t = vv > 0.8 ? 1 : vv < -0.8 ? 0 : (g.start > 0.5 ? (d > 0.7 ? 1 : 0) : (d > 0.3 ? 1 : 0));
      drawerP.spring(t, 'spatial', Motion.sim(vv));
      if (t === 0) { const grid = $('.drawer-grid', drawer); if (grid) grid.scrollTop = 0; }
    },
  };
  const homeG = {
    begin(g) {
      if (shadeP.v > 0.01) { g.sub = shadeG; g.sub.begin(g); return; }
      if (st.recents) { g.sub = { begin() {}, move() {}, end: (gg, v) => { if (gg.y0 - gg.y > 40 || v.vy < -300) recentsToHome(); } }; return; }
      const inst = st.fg;
      if (!inst) {
        if (drawerP.v > 0.01) { g.sub = { begin() {}, move() {}, end: (gg, v) => { if (gg.y0 - gg.y > 30 || v.vy < -300) drawerP.spring(0, 'spatial'); } }; return; }
        g.sub = drawerG; g.sub.begin(g); return;
      }
      g.inst = inst; stopInst(inst); homeZ.stop(); inst.phase = 'live';
      st.homeMin = 0.85; st.homeBlur = 0;
      g.armed = false; g.slowSince = 0; g.peek = new V(0, () => homeG.move(g, { x: g.x, y: g.y }), 0.001);
    },
    move(g, p) {
      if (g.sub) return g.sub.move(g, p);
      const inst = g.inst, up = Math.max(0, g.y0 - p.y), dx = p.x - g.x0;
      const q = up / (H * 0.6), qe = q < 1 ? q : 1 + (q - 1) * 0.25;
      const w = W * (1 - 0.55 * Math.min(qe, 1.1)), h = (w * H) / W, bottom = H - up;
      inst.cx.set(W / 2 + dx * 0.85); inst.cy.set(bottom - h / 2); inst.w.set(w); inst.sq.set(0); inst.r.set(lerp(R, 24, clamp(q * 3))); inst.a.set(1);
      homeZ.set(1 - clamp(q * 1.6));
      // задержка пальца ≥ 300 мс (Android) → недавние
      const now = performance.now(), v = g.tr.velocity(), sp = Math.hypot(v.vx, v.vy);
      if (up > 100 && sp < 140) { if (!g.slowSince) g.slowSince = now; else if (!g.armed && now - g.slowSince > 300) { g.armed = true; g.peek.spring(1, 'spatial'); } }
      else { g.slowSince = 0; if (g.armed && sp > 1100 && v.vy < 0) { g.armed = false; g.peek.spring(0, 'spatial'); } }
      const pk = g.peek.v;
      st.apps.forEach((o, n) => {
        if (o === inst || n === 0) return;
        if (pk < 0.01) { if (o.el.classList.contains('shown') && !g.armed) hideWin(o); return; }
        showWin(o); o.phase = 'live';
        o.cx.set(inst.cx.v - n * (w + 16) - (1 - pk) * 90); o.cy.set(inst.cy.v); o.w.set(w); o.sq.set(0); o.r.set(inst.r.v); o.a.set(pk);
      });
    },
    end(g, v) {
      if (g.sub) return g.sub.end(g, v);
      const inst = g.inst, up = g.y0 - g.y;
      if (g.armed) return enterRecents();
      if ((up > 40 && v.vy < -500) || (up > H * 0.2 && v.vy < 250)) return goHome(inst, v);
      restoreFg(inst, v);
    },
  };
  // «Назад» от края
  const ARM = 64;
  const backG = {
    begin(g) {
      g.side = g.x0 < W / 2 ? 1 : -1;
      g.kind = shadeP.v > 0.01 ? 'shade' : volP.v > 0.01 ? 'vol' : st.recents ? 'recents' : st.fg ? (st.fg.stack.length > 1 ? 'page' : 'app') : drawerP.v > 0.01 ? 'drawer' : 'none';
      g.variant = Store.get('back'); g.armed = false; g.dist = 0;
      arrow.style.transition = 'none'; arrow.style.opacity = 0;
      arrow.querySelector('.ms').textContent = g.side === 1 ? 'chevron_left' : 'chevron_right';
      if (g.kind === 'page') { const i = st.fg; g.inst = i; g.cur = i.stack[i.stack.length - 1]; g.prev = i.stack[i.stack.length - 2]; i.navBusy = true; }
      if (g.kind === 'app') { g.inst = st.fg; stopInst(g.inst); homeZ.stop(); st.homeMin = 0.85; st.homeBlur = 10; }
      g.shade0 = shadeP.v; shadeP.stop();
    },
    move(g, p) {
      const dist = Math.max(0, (p.x - g.x0) * g.side);
      g.dist = dist;
      const raw = clamp(dist / W), pe = Motion.backProgress(raw);
      g.pe = pe; g.ty = clamp((p.y - g.y0) * 0.25, -24, 24);
      // стрелка: до порога «лепесток» у края, после — круг 48 dp в 14 dp от края
      const armed = dist > ARM;
      if (armed !== g.armed) { g.armed = armed; arrow.style.transition = 'width 220ms var(--sp-spatial-fast), height 220ms var(--sp-spatial-fast), border-radius 220ms, transform 220ms var(--sp-spatial-fast), background-color 120ms'; setTimeout(() => (arrow.style.transition = 'none'), 230); }
      arrow.classList.toggle('armed', armed);
      const w = armed ? 48 : clamp(dist * 0.8, 6, 51), h = armed ? 48 : 46, edge = armed ? 14 : 0;
      arrow.style.width = w + 'px'; arrow.style.height = h + 'px'; arrow.style.marginTop = -h / 2 + 'px';
      arrow.style.borderRadius = armed ? '24px' : (g.side === 1 ? '0 20px 20px 0' : '20px 0 0 20px');
      const x = g.side === 1 ? edge : W - edge - w;
      arrow.style.transform = `translate(${x}px, ${Math.max(64, p.y)}px)`; arrow.style.opacity = clamp(dist / 20);
      const dark = Store.get('dark');
      if (g.kind === 'page') {
        const { cur, prev } = g; prev.style.visibility = 'visible';
        if (g.variant === 'predictive') {     // система: 0,85, поле 8 dp, предыдущее с −96 dp, скрим 0,8/0,2
          const s = 1 - 0.15 * pe, tx = g.side * Math.max(0, (W - W * s) / 2 - 8);
          cur.style.transform = `translate(${tx}px, ${g.ty * pe}px) scale(${s})`; cur.style.borderRadius = `${28 * clamp(pe * 4)}px`;
          const ps = lerp(0.95, 0.85, pe);
          prev.style.transform = `translate(${-96 * g.side * (1 - 0.2 * pe)}px, ${g.ty * pe}px) scale(${ps})`; prev.style.borderRadius = '28px';
          scrimOf(prev, (dark ? 0.8 : 0.2) * pe);
        } else if (g.variant === 'material') {
          const s = 1 - 0.1 * pe;
          cur.style.transform = `translate(${g.side * (W / 20 - 8) * pe}px, ${g.ty * pe}px) scale(${s})`; cur.style.borderRadius = `${28 * clamp(pe * 4)}px`;
          prev.style.opacity = 0;
        } else if (g.variant === 'slide') {
          g.slideX = Math.max(0, dist);
          cur.style.transform = `translateX(${g.slideX}px)`; cur.style.boxShadow = '-10px 0 30px rgba(0,0,0,.25)';
          prev.style.transform = `translateX(${-W * 0.3 * (1 - g.slideX / W)}px)`; scrimOf(prev, 0.3 * (1 - g.slideX / W));
        }
      } else if (g.kind === 'app') {
        const inst = g.inst;
        if (g.variant === 'predictive' || g.variant === 'material') {   // назад на главный: 1 → 0,75, углы 40 dp, поле 10 dp
          const s = 1 - 0.25 * pe, w = W * s;
          inst.w.set(w); inst.cx.set(W / 2 + g.side * Math.max(0, (W - w) / 2 - 10 * pe)); inst.cy.set(H / 2 + g.ty * pe * 2); inst.r.set(lerp(R, 40, clamp(pe * 3))); inst.sq.set(0);
          homeZ.set(1 - 0.0001); st.backPeek = (dark ? 0.8 : 0.2) * pe * 0.6; renderHome();
        }
      } else if (g.kind === 'drawer') { st.drawerBack = pe; renderDrawer(); }
      else if (g.kind === 'shade') shadeP.set(g.shade0 * (1 - 0.12 * pe));
    },
    end(g, v, cancel) {
      const commit = !cancel && g.armed && !(v.vx * g.side < -400);
      arrow.style.transition = 'opacity 160ms, transform 220ms var(--sp-spatial-fast)';
      if (commit) { arrow.style.transform += ' scale(0.86)'; }
      arrow.style.opacity = 0; arrow.classList.remove('armed');
      if (g.kind === 'page') {
        const { inst, cur, prev } = g;
        if (commit) {
          if (g.variant === 'predictive' || g.variant === 'material' || g.variant === 'slide') pageBack(inst, { pe: g.pe || 0, ty: g.ty || 0, side: g.side, slideX: g.slideX || 0 });
          else { inst.navBusy = false; pageBack(inst); }
        } else {
          const c = new V(1, () => {
            const k = c.v;
            if (g.variant === 'slide') { cur.style.transform = `translateX(${(g.slideX || 0) * k}px)`; prev.style.transform = `translateX(${-W * 0.3 * (1 - ((g.slideX || 0) * k) / W)}px)`; scrimOf(prev, 0.3); }
            else { const pe = (g.pe || 0) * k, s = 1 - (g.variant === 'predictive' ? 0.15 : 0.1) * pe; cur.style.transform = `translate(${g.side * Math.max(0, (W - W * s) / 2 - 8)}px, ${(g.ty || 0) * pe}px) scale(${s})`; scrimOf(prev, 0.5 * pe); }
          });
          c.spring(0, 'spatialFast').then(() => { resetPage(cur); resetPage(prev); prev.style.visibility = 'hidden'; inst.navBusy = false; });
        }
      } else if (g.kind === 'app') {
        const inst = g.inst;
        const bp = new V(st.backPeek, () => { st.backPeek = bp.v; renderHome(); });
        bp.tween(0, 233, 'standard');   // скрим и размытие гаснут за 233 мс
        if (commit) goHome(inst, { vx: g.side * 300, vy: 0 }, { homeMs: 450 });
        else restoreFg(inst);
      } else if (g.kind === 'drawer') {
        const d0 = st.drawerBack || 0;
        if (commit) { st.drawerBack = 0; drawerP.spring(0, 'spatial'); }
        else { const c = new V(d0, () => { st.drawerBack = c.v; renderDrawer(); }); c.spring(0, 'spatialFast'); }
      } else if (g.kind === 'shade') { if (commit) closeShade(); else shadeP.spring(g.shade0, 'spatialFast'); }
      else if (g.kind === 'recents') { if (commit) recentsToHome(); }
      else if (g.kind === 'vol') { if (commit) hideVolume(); }
    },
  };
  const notifG = {
    begin(g) {
      g.el = g.target.closest('.notif'); g.el.classList.add('dragging');
      const sib = [...g.el.parentElement.children]; const i = sib.indexOf(g.el);
      g.n1 = [sib[i - 1], sib[i + 1]].filter(Boolean); g.n2 = [sib[i - 2], sib[i + 2]].filter(Boolean);
      g.xv = new V(0, () => {
        const x = g.xv.v; g.el.style.transform = `translateX(${x}px)`; g.el.style.opacity = 1 - prog(Math.abs(x), 120, W);
        g.n1.forEach(n => (n.style.transform = `translateX(${x * 0.12}px)`)); g.n2.forEach(n => (n.style.transform = `translateX(${x * 0.04}px)`));
      }, 0.05);
    },
    move(g, p) {   // до 72 dp — половина скорости пальца, потом догоняет пружиной 800/0,95
      const dx = p.x - g.x0, t = Math.abs(dx) < 72 ? dx * 0.5 : dx;
      g.xv.spring(t, { z: 0.95, k: 800 });
    },
    end(g, v) {
      const dx = g.x - g.x0;
      if (Math.abs(dx) > 150 || Math.abs(v.vx) > 900) {
        const dir = Math.sign(dx || v.vx);
        g.xv.spring(dir * W * 1.1, { z: 0.95, k: 800 }, Motion.sim(v.vx)).then(() => {
          g.n1.concat(g.n2).forEach(n => (n.style.transform = ''));
          const el = g.el; el.style.transition = 'max-height 260ms, padding 260ms, margin 260ms'; el.style.maxHeight = el.offsetHeight + 'px';
          requestAnimationFrame(() => { el.style.maxHeight = '0px'; el.style.paddingTop = el.style.paddingBottom = '0px'; el.style.overflow = 'hidden'; });
          setTimeout(() => el.remove(), 280);
        });
      } else {
        g.xv.spring(0, { z: 0.6, k: 550 }).then(() => { g.el.classList.remove('dragging'); g.n1.concat(g.n2).forEach(n => (n.style.transform = '')); });
      }
    },
  };
  const recScrollG = {
    begin(g) { recScroll.stop(); g.start = recScroll.v; st.recLive = true; },
    move(g, p) {
      let v = g.start + (p.x - g.x0) / (CW + CGAP); const max = st.apps.length;
      if (v < 0) v *= 0.3; if (v > max) v = max + (v - max) * 0.3;
      recScroll.set(v);
    },
    end(g, vel) {
      const vv = vel.vx / (CW + CGAP), proj = recScroll.v + vv * 0.25;
      recScroll.spring(clamp(Math.round(proj), 0, st.apps.length), 'spatial', Motion.sim(vv));
    },
  };
  const recDismissG = {
    begin(g) { g.inst = cardAt(g.x0, g.y0); st.recLive = true; },
    move(g, p) { if (g.inst) g.inst.dy.set(Math.min(0, p.y - g.y0) + Math.max(0, p.y - g.y0) * 0.1); },
    end(g, v) { if (!g.inst) return; if (g.inst.dy.v < -150 || v.vy < -800) dismissCard(g.inst, v.vy); else g.inst.dy.spring(0, 'spatial', Motion.sim(v.vy)); },
  };
  function recentsTap(g, e) {
    if (e.target.closest('button')) return;
    const i = cardAt(g.x0, g.y0);
    if (i) openFromRecents(i); else recentsToHome();
  }
  const mouseScrollG = {
    begin(g) { g.sc = g.target.closest('.scroll, .drawer-grid'); g.st0 = g.sc ? g.sc.scrollTop : 0; cancelAnimationFrame(g.sc?._mom); },
    move(g, p) { if (g.sc) g.sc.scrollTop = g.st0 - (p.y - g.y0); },
    end(g, v) {
      const sc = g.sc; if (!sc) return;
      let vel = -v.vy, last = performance.now();
      const step = now => { const dt = (now - last) / 1000; last = now; sc.scrollTop += vel * dt; vel *= Math.pow(0.04, dt); if (Math.abs(vel) > 20) sc._mom = requestAnimationFrame(step); };
      sc._mom = requestAnimationFrame(step);
    },
  };
  const sliderG = {
    begin(g) { g.el.classList.add('active'); sliderG.move(g, { x: g.x0, y: g.y0 }); },
    move(g, p) { const r = screenRect(g.el); Store.set(g.el.dataset.slider, clamp((p.x - r.x - 8) / (r.w - 16))); },
    end(g) { g.el.classList.remove('active'); },
  };
  const vsliderG = {
    begin(g) { clearTimeout(st.volT); vsliderG.move(g, { x: g.x0, y: g.y0 }); },
    move(g, p) { const r = screenRect(g.el); Store.set(g.el.dataset.vslider, clamp(1 - (p.y - r.y) / r.h)); },
    end() { st.volT = setTimeout(() => hideVolume(), 2000 / Motion.speed); },
  };
  const H_ = { shade: shadeG, shadeDrag: shadeG, unlock: unlockG, drawer: drawerG, home: homeG, back: backG, notif: notifG,
    recScroll: recScrollG, recDismiss: recDismissG, mouseScroll: mouseScrollG, slider: sliderG, vslider: vsliderG };

  // ================= Нажатия =================
  function toast(msg) {
    toastEl.textContent = msg; toastEl.classList.add('on');
    clearTimeout(st.toastT); st.toastT = setTimeout(() => toastEl.classList.remove('on'), 1800);
  }
  function flash(el) {
    const f = document.createElement('div');
    f.style.cssText = 'position:absolute;inset:0;background:#fff;opacity:.85;z-index:97;pointer-events:none;transition:opacity 300ms';
    screen.appendChild(f); requestAnimationFrame(() => requestAnimationFrame(() => (f.style.opacity = 0))); setTimeout(() => f.remove(), 350);
  }
  const calc = { expr: '' };
  function calcPress(k, page) {
    if (k === 'AC') calc.expr = '';
    else if (k === '⌫') calc.expr = calc.expr.slice(0, -1);
    else if (k === '=') { const r = calcEval(calc.expr); calc.expr = r === null ? '' : String(r).replace('.', ','); }
    else if (k === '()') calc.expr += (calc.expr.split('(').length > calc.expr.split(')').length) ? ')' : '(';
    else calc.expr += k;
    $('[data-calc-expr]', page).textContent = calc.expr;
    const r = calcEval(calc.expr);
    $('[data-calc-out]', page).textContent = r === null ? (calc.expr ? '…' : '0') : String(r).replace('.', ',');
  }
  function calcEval(s) {   // свой разбор: числа, + − × ÷ %, скобки
    const t = s.replace(/,/g, '.').match(/\d+(\.\d+)?|[+\-−×÷%()]/g);
    if (!t) return null;
    let i = 0;
    const num = () => { const x = t[i]; if (x === '(') { i++; const v = add(); if (t[i] === ')') i++; return v; } if (x === '−' || x === '-') { i++; return -num(); } i++; const v = parseFloat(x); if (isNaN(v)) throw 0; if (t[i] === '%') { i++; return v / 100; } return v; };
    const mul = () => { let v = num(); while (t[i] === '×' || t[i] === '÷') { const o = t[i++]; const r = num(); v = o === '×' ? v * r : v / r; } return v; };
    const add = () => { let v = mul(); while (t[i] === '+' || t[i] === '−' || t[i] === '-') { const o = t[i++]; const r = mul(); v = o === '+' ? v + r : v - r; } return v; };
    try { const v = add(); if (i < t.length || !isFinite(v)) return null; return Math.round(v * 1e10) / 1e10; } catch { return null; }
  }
  screen.addEventListener('click', e => {
    const t = e.target;
    const b = t.closest('[data-bind]');
    if (b) {
      const k = b.dataset.bind, nv = !Store.get(k);
      if (k === 'dark' && document.startViewTransition && !b.closest('.panel')) document.startViewTransition(() => Store.set(k, nv));
      else Store.set(k, nv);
      if (b.classList.contains('tile')) { const ic = b.querySelector('.ms'); ic.animate([{ transform: 'scale(1)' }, { transform: 'scale(1.1)' }, { transform: 'scale(1)' }], { duration: 334 / Motion.speed, easing: 'ease-out' }); }
      return;
    }
    const r = t.closest('[data-radio]'); if (r) { Store.set(r.dataset.radio, parse(r.dataset.value)); return; }
    const c = t.closest('[data-choice]'); if (c) { Store.set(c.dataset.choice, parse(c.dataset.value)); return; }
    const calcK = t.closest('[data-calc]'); if (calcK) { calcPress(calcK.dataset.calc, calcK.closest('.page')); return; }
    const act = t.closest('[data-act]');
    if (act) {
      const a = act.dataset.act;
      if (a === 'back') { const i = instOf(act); if (i) i.stack.length > 1 ? pageBack(i) : goHome(i); }
      else if (a === 'lockcam') { const sc = act; unlockCommit(0); launch('camera', sc); }
      else if (a === 'open-settings') { closeShade(); if (st.locked) return toast('Сначала разблокируйте'); launch('settings', null); }
      else if (a === 'clear-notifs') { $$('.notifs .notif', shade).forEach((n, k) => { n.style.transition = `transform 300ms ${k * 60}ms, opacity 300ms ${k * 60}ms`; n.style.transform = `translateX(${W}px)`; n.style.opacity = 0; }); setTimeout(() => { $('.notifs', shade).innerHTML = ''; }, 500); }
      else if (a === 'play') { act.classList.toggle('playing'); act.querySelector('.ms').textContent = act.classList.contains('playing') ? 'pause' : 'play_arrow'; }
      else if (a === 'clear-all') clearAll();
      else if (a === 'screenshot' || a === 'shutter') { flash(); toast(a === 'shutter' ? 'Снимок сохранён' : 'Снимок экрана сохранён'); }
      else if (a === 'power-menu') toast('Меню питания — в следующей версии макета');
      else if (a === 'qs-edit') toast('Правка плиток — в следующей версии макета');
      else if (a === 'qs-user') toast('Пользователи — позже');
      else if (a === 'select') toast('Выделение текста — позже');
      else if (a === 'mic') toast('Голосовой ввод — позже');
      else if (a === 'notif-history') toast('История уведомлений — позже');
      else if (a === 'nav-back') navBack();
      else if (a === 'nav-home') navHome();
      else if (a === 'nav-recents') st.recents ? recentsToHome() : enterRecents();
      return;
    }
    const nav = t.closest('[data-nav]');
    if (nav) { const i = instOf(nav); if (i) pushPage(i, nav.dataset.nav, nav); return; }
    const ai = t.closest('.app-icon');
    if (ai && !st.locked && !st.recents) launch(ai.dataset.app, ai);
  });

  // ================= Питание =================
  async function power() {
    if (st.on) {
      st.on = false; offEl.classList.add('on');
      const v = new V(0, () => (offEl.style.opacity = v.v)); await v.tween(1, 250, 'standard');
      lockNow();
    } else {
      st.on = true; lockNow();
      const v = new V(1, () => (offEl.style.opacity = v.v)); await v.tween(0, 300, 'standard');
      offEl.classList.remove('on');
    }
  }

  // ================= Сцены и показы для пульта =================
  function sceneHome() {
    stopAll(); hideVolume(true); shadeP.set(0); drawerP.set(0);
    if (st.recents) leaveRecentsInstant();
    if (st.fg) { hideWin(st.fg); st.fg = null; }
    for (const i of st.apps) { hideWin(i); i.navBusy = false; }
    st.locked = false; st.on = true; offEl.style.opacity = 0; offEl.classList.remove('on');
    lockP.set(1); homeZ.set(0); st.backPeek = 0; st.homeBlur = 0; st.drawerBack = 0;
    $$('.app-icon.hidden-ic', home).forEach(e => e.classList.remove('hidden-ic'));
    renderLock(); updateStatus();
  }
  function sceneApp(id, routes = []) {
    sceneHome();
    let inst = st.apps.find(i => i.app.id === id);
    if (inst) { inst.stack.slice(1).forEach(p => p.remove()); inst.stack.length = 1; resetPage(inst.stack[0]); inst.stack[0].style.visibility = ''; }
    else inst = createInst(Apps.BY[id]);
    st.apps = [inst, ...st.apps.filter(i => i !== inst)];
    for (const r of routes) { const prev = inst.stack[inst.stack.length - 1]; prev.style.visibility = 'hidden'; inst.stack.push(makePage(inst, Apps.subPage(id, r))); }
    fullNow(inst); st.fg = inst; homeZ.set(1); st.homeMin = 0.92; updateStatus(); renderHome();
    return inst;
  }
  function sceneLock() { sceneHome(); lockNow(); }
  function ensureRecentApps() {
    for (const id of ['messages', 'calc', 'gallery']) if (!st.apps.find(i => i.app.id === id)) { const i = createInst(Apps.BY[id]); st.apps.push(i); }
  }
  async function simulate(type, path, holdMs = 0) {
    if (G) return;
    const sp = Motion.speed;
    const g = { type, id: -1, x0: path[0][0], y0: path[0][1], x: path[0][0], y: path[0][1], decided: true, tr: new Motion.Tracker(), target: screen, synthetic: true };
    G = g; g.h = H_[type];
    const t0 = performance.now();
    g.tr.add(g.x0, g.y0, t0); showTouch({ x: g.x0, y: g.y0 }, true);
    g.h.begin(g);
    const total = path[path.length - 1][2] / sp;
    await new Promise(res => {
      const step = now => {
        const t = (now - t0) * sp;
        let k = 1; while (k < path.length - 1 && path[k][2] < t) k++;
        const a = path[k - 1], b = path[k], u = clamp((t - a[2]) / Math.max(1, b[2] - a[2]));
        const e = u < 0.5 ? 2 * u * u : 1 - Math.pow(-2 * u + 2, 2) / 2;
        g.x = lerp(a[0], b[0], e); g.y = lerp(a[1], b[1], e);
        g.tr.add(g.x, g.y, now); showTouch({ x: g.x, y: g.y });
        g.h.move(g, { x: g.x, y: g.y });
        if (now - t0 >= total) res(); else requestAnimationFrame(step);
      };
      requestAnimationFrame(step);
    });
    if (holdMs) {
      const hEnd = performance.now() + holdMs / sp;
      await new Promise(res => { const step = now => { g.tr.add(g.x, g.y, now); g.h.move(g, { x: g.x, y: g.y }); if (now >= hEnd) res(); else requestAnimationFrame(step); }; requestAnimationFrame(step); });
    }
    G = null; showTouch(null);
    g.h.end(g, holdMs ? { vx: 0, vy: 0 } : g.tr.velocity(), false);
  }
  const wait = ms => Motion.sleep(ms);
  const demos = {
    async unlock() { sceneLock(); await wait(500); await simulate('unlock', [[196, 690, 0], [196, 430, 260]]); },
    async launch() { sceneHome(); await wait(350); launch('settings', homeIcon('settings')); },
    async home() { const i = sceneApp('settings'); await wait(450); await simulate('home', [[196, 846, 0], [190, 560, 230]]); },
    async recents() { ensureRecentApps(); sceneApp('settings'); await wait(450); await simulate('home', [[196, 846, 0], [196, 600, 380]], 420); },
    async back() { sceneApp('settings', ['display']); await wait(450); await simulate('back', [[3, 420, 0], [80, 424, 260], [150, 430, 700]], 250); },
    async backHome() { sceneApp('settings'); await wait(450); await simulate('back', [[3, 420, 0], [150, 430, 700]], 250); },
    async forward() { const i = sceneApp('settings'); await wait(450); const item = $('[data-nav="display"]', i.stack[0]); pushPage(i, 'display', item); },
    async shade() { sceneHome(); await wait(350); await simulate('shade', [[200, 12, 0], [200, 300, 420]]); await wait(1100); if (Store.get('shade') === 'single') { await simulate('shade', [[200, 300, 0], [200, 620, 420]]); await wait(1300); } closeShade(); },
  };

  // ================= Запуск =================
  function init() {
    navbar.innerHTML = `<div class="nav-handle"></div>
      <button class="nb" data-act="nav-back" title="Назад"><svg width="20" height="20" viewBox="0 0 20 20"><path d="M14 3.5 L5 10 L14 16.5 Z" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"/></svg></button>
      <button class="nb" data-act="nav-home" title="Главный экран"><svg width="20" height="20" viewBox="0 0 20 20"><circle cx="10" cy="10" r="7" fill="none" stroke="currentColor" stroke-width="2"/></svg></button>
      <button class="nb" data-act="nav-recents" title="Недавние"><svg width="20" height="20" viewBox="0 0 20 20"><rect x="4" y="4" width="12" height="12" rx="2.5" fill="none" stroke="currentColor" stroke-width="2"/></svg></button>`;
    applyMotion(); applyIcons(); applyNav(); applyFont();
    buildHome(); buildLock(); buildShade();
    applyTheme(); applyWallpaper();
    screen.classList.toggle('no-blur', !Store.get('blur'));
    dimEl.style.opacity = (1 - Store.get('brightness')) * 0.62;
    nightEl.style.opacity = Store.get('night') ? 0.16 : 0;
    vol.innerHTML = `<button class="vol-btn on" data-act="ring"><span class="ms f">notifications_active</span></button>
      <div class="vsl" data-vslider="volMedia"><div class="vsl-a"></div><span class="ms f">music_note</span></div>
      <button class="vol-btn" data-act="vol-more"><span class="ms">more_horiz</span></button>`;
    refreshAll(); renderVol(); renderShade(); renderRecents(); renderLock(); updateStatus();
    setInterval(tickClock, 5000);
    if (document.fonts) document.fonts.load('24px "Material Symbols Rounded"', 'home').then(() => screen.classList.add('icons-ready')).catch(() => screen.classList.add('icons-ready'));
    setTimeout(() => screen.classList.add('icons-ready'), 2500);
    Theme.load().then(() => applyTheme());
  }

  return {
    init, launch, goHome, navBack, navHome, enterRecents, power, showVolume, lockNow, sceneHome, sceneApp, sceneLock, demos, toast,
    openShade, closeShade, ensureRecentApps, setThemeHook(fn) { themeHook = fn; if (st.colorSource) fn(st.colorSource); },
    get state() { return st; },
  };
})();
