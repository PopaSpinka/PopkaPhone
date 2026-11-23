'use strict';
/* Цвет (Material You), формы значков и обои.
   Цвет: Material Color Utilities (Apache 2.0, npm @material/material-color-utilities 0.3.0) — грузится с jsDelivr;
   без сети — приближение тональных палитр через OKLCH. */
const Theme = (() => {
  const ROLES = ['primary', 'onPrimary', 'primaryContainer', 'onPrimaryContainer', 'secondary', 'onSecondary', 'secondaryContainer', 'onSecondaryContainer',
    'tertiary', 'onTertiary', 'tertiaryContainer', 'onTertiaryContainer', 'error', 'onError', 'errorContainer', 'onErrorContainer',
    'surface', 'onSurface', 'onSurfaceVariant', 'surfaceDim', 'surfaceBright', 'surfaceContainerLowest', 'surfaceContainerLow', 'surfaceContainer',
    'surfaceContainerHigh', 'surfaceContainerHighest', 'outline', 'outlineVariant', 'inverseSurface', 'inverseOnSurface', 'inversePrimary',
    'primaryFixed', 'primaryFixedDim', 'onPrimaryFixed', 'onPrimaryFixedVariant', 'secondaryFixed', 'onSecondaryFixed', 'tertiaryFixed', 'onTertiaryFixed'];
  const VARIANTS = { tonalSpot: 'SchemeTonalSpot', vibrant: 'SchemeVibrant', expressive: 'SchemeExpressive', neutral: 'SchemeNeutral',
    monochrome: 'SchemeMonochrome', fidelity: 'SchemeFidelity', content: 'SchemeContent', rainbow: 'SchemeRainbow', fruitSalad: 'SchemeFruitSalad' };
  let mcu = null, loading = null;
  const kebab = s => s.replace(/[A-Z]/g, m => '-' + m.toLowerCase());

  function load() {
    if (!loading) loading = import('https://cdn.jsdelivr.net/npm/@material/material-color-utilities@0.3.0/+esm')
      .then(m => (mcu = m)).catch(() => null);
    return loading;
  }

  // sRGB hex → OKLCH
  function hexToOklch(hex) {
    const n = parseInt(hex.slice(1), 16);
    const lin = c => (c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4));
    const r = lin(((n >> 16) & 255) / 255), g = lin(((n >> 8) & 255) / 255), b = lin((n & 255) / 255);
    const l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    const L = 0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s;
    const A = 1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s;
    const B = 0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s;
    return { L, C: Math.hypot(A, B), H: ((Math.atan2(B, A) * 180) / Math.PI + 360) % 360 };
  }
  // «Тон» HCT (≈ L* CIELAB, 0..100) → цвет OKLCH с заданной насыщенностью
  function tone(h, c, t) {
    const L = t <= 0 ? 0 : t >= 100 ? 1 : Math.min(1, (t / 100 + 0.16) / 1.16);
    const edge = 1 - Math.pow(Math.abs(t - 50) / 50, 3);
    return `oklch(${L.toFixed(4)} ${(c * Math.max(0.15, edge)).toFixed(4)} ${h.toFixed(1)})`;
  }
  function palettes(seedHex, variant) {
    const { H } = hexToOklch(seedHex);
    const v = {
      tonalSpot: [[0, .095], [0, .042], [60, .062], [0, .012], [0, .02]],
      vibrant: [[0, .17], [15, .07], [45, .09], [0, .03], [0, .04]],
      expressive: [[240, .11], [15, .06], [100, .08], [15, .022], [15, .03]],
      neutral: [[0, .032], [0, .016], [0, .022], [0, .005], [0, .008]],
      monochrome: [[0, 0], [0, 0], [0, 0], [0, 0], [0, 0]],
      fidelity: [[0, .16], [0, .06], [60, .08], [0, .015], [0, .024]],
      content: [[0, .14], [0, .05], [60, .07], [0, .014], [0, .022]],
      rainbow: [[0, .13], [0, .045], [60, .06], [0, 0], [0, 0]],
      fruitSalad: [[-50, .13], [-50, .1], [0, .1], [0, .025], [0, .035]],
    }[variant] || [[0, .095], [0, .042], [60, .062], [0, .012], [0, .02]];
    return v.map(([dh, c]) => t => tone((H + dh + 360) % 360, c, t));
  }
  function fallback(seedHex, dark, variant) {
    const [P, S, T, N, NV] = palettes(seedHex, variant);
    const E = t => tone(27, 0.16, t);
    const d = dark;
    return {
      primary: P(d ? 80 : 40), onPrimary: P(d ? 20 : 100), primaryContainer: P(d ? 30 : 90), onPrimaryContainer: P(d ? 90 : 10),
      secondary: S(d ? 80 : 40), onSecondary: S(d ? 20 : 100), secondaryContainer: S(d ? 30 : 90), onSecondaryContainer: S(d ? 90 : 10),
      tertiary: T(d ? 80 : 40), onTertiary: T(d ? 20 : 100), tertiaryContainer: T(d ? 30 : 90), onTertiaryContainer: T(d ? 90 : 10),
      error: E(d ? 80 : 40), onError: E(d ? 20 : 100), errorContainer: E(d ? 30 : 90), onErrorContainer: E(d ? 90 : 10),
      surface: N(d ? 6 : 98), onSurface: N(d ? 90 : 10), onSurfaceVariant: NV(d ? 80 : 30), surfaceDim: N(d ? 6 : 87), surfaceBright: N(d ? 24 : 98),
      surfaceContainerLowest: N(d ? 4 : 100), surfaceContainerLow: N(d ? 10 : 96), surfaceContainer: N(d ? 12 : 94),
      surfaceContainerHigh: N(d ? 17 : 92), surfaceContainerHighest: N(d ? 22 : 90),
      outline: NV(d ? 60 : 50), outlineVariant: NV(d ? 30 : 80), inverseSurface: N(d ? 90 : 20), inverseOnSurface: N(d ? 20 : 95), inversePrimary: P(d ? 40 : 80),
      primaryFixed: P(90), primaryFixedDim: P(80), onPrimaryFixed: P(10), onPrimaryFixedVariant: P(30),
      secondaryFixed: S(90), onSecondaryFixed: S(10), tertiaryFixed: T(90), onTertiaryFixed: T(10),
    };
  }
  function compute(seedHex, dark, variant = 'tonalSpot') {
    if (mcu) {
      try {
        const Cls = mcu[VARIANTS[variant]] || mcu.SchemeTonalSpot;
        const scheme = new Cls(mcu.Hct.fromInt(mcu.argbFromHex(seedHex)), dark, 0);
        const out = {};
        for (const r of ROLES) { const dc = mcu.MaterialDynamicColors[r]; if (dc) out[r] = mcu.hexFromArgb(dc.getArgb(scheme)); }
        if (out.primary && out.surface) return { colors: out, source: 'mcu' };
      } catch (e) { /* упадём на приближение */ }
    }
    return { colors: fallback(seedHex, dark, variant), source: 'oklch' };
  }
  function apply(el, colors, dark) {
    for (const [k, v] of Object.entries(colors)) el.style.setProperty('--' + kebab(k), v);
    el.style.setProperty('--scrim-tint', dark ? colors.surfaceContainerLowest : colors.surfaceContainer);
    el.style.setProperty('--tile-off-bg', dark ? colors.surfaceContainerHighest : colors.surfaceContainerLowest);
    el.style.setProperty('--ic-l-bg', dark ? '0.36' : '0.90');
    el.style.setProperty('--ic-c-bg', dark ? '0.07' : '0.06');
    el.style.setProperty('--ic-l-fg', dark ? '0.90' : '0.38');
  }

  // ---- Формы значков (Material shapes; Android 16 QPR2 даёт выбор формы значков) ----
  function poly(pts) { return `polygon(${pts.map(([x, y]) => `${(x * 100).toFixed(2)}% ${(y * 100).toFixed(2)}%`).join(', ')})`; }
  function radial(fn, n = 120) {
    const raw = [];
    for (let i = 0; i < n; i++) { const a = (i / n) * Math.PI * 2 - Math.PI / 2; const r = fn(a); raw.push([Math.cos(a) * r, Math.sin(a) * r]); }
    const max = Math.max(...raw.map(([x, y]) => Math.max(Math.abs(x), Math.abs(y))));
    return poly(raw.map(([x, y]) => [0.5 + (x / max) * 0.5, 0.5 + (y / max) * 0.5]));
  }
  const cookie = (k, amp) => radial(a => 1 + amp * Math.cos(k * (a + Math.PI / 2)));
  function squircle(p = 4.2) {
    const pts = [];
    for (let i = 0; i < 120; i++) {
      const a = (i / 120) * Math.PI * 2, c = Math.cos(a), s = Math.sin(a);
      pts.push([0.5 + 0.5 * Math.sign(c) * Math.pow(Math.abs(c), 2 / p), 0.5 + 0.5 * Math.sign(s) * Math.pow(Math.abs(s), 2 / p)]);
    }
    return poly(pts);
  }
  function arch() {
    const pts = [];
    for (let i = 0; i <= 40; i++) { const a = Math.PI + (i / 40) * Math.PI; pts.push([0.5 + 0.5 * Math.cos(a), 0.5 + 0.5 * Math.sin(a)]); }
    const r = 0.22;
    for (let i = 0; i <= 12; i++) { const a = (i / 12) * (Math.PI / 2); pts.push([1 - r + r * Math.cos(a), 1 - r + r * Math.sin(a)]); }
    for (let i = 0; i <= 12; i++) { const a = Math.PI / 2 + (i / 12) * (Math.PI / 2); pts.push([r + r * Math.cos(a), 1 - r + r * Math.sin(a)]); }
    return poly(pts);
  }
  const SHAPES = {
    circle: { name: 'Круг', clip: 'circle(50%)', radius: 0.5 },
    squircle: { name: 'Сквиркл', clip: squircle(), radius: 0.36 },
    rounded: { name: 'Квадрат', clip: 'inset(0 round 26%)', radius: 0.26 },
    cookie4: { name: 'Печенье-4', clip: cookie(4, 0.11), radius: 0.42 },
    cookie7: { name: 'Печенье-7', clip: cookie(7, 0.075), radius: 0.46 },
    arch: { name: 'Арка', clip: arch(), radius: 0.3 },
  };

  // ---- Обои: рисуем сами (свои, без чужих картинок). Каждые — со своим цветом-источником. ----
  const WALLPAPERS = {
    dunes: { name: 'Дюны', seed: '#9a5b1e' },
    shapes: { name: 'Формы', seed: '#6750a4' },
    sea: { name: 'Море', seed: '#0b6e8a' },
    garden: { name: 'Сад', seed: '#3f6c2a' },
    night: { name: 'Ночь', seed: '#3d4a86' },
  };
  const wpCache = {};
  function wallpaper(id, w = 393, h = 851, scale = 2) {
    const key = `${id}:${w}:${scale}`;
    if (wpCache[key]) return wpCache[key];
    const cv = document.createElement('canvas'); cv.width = w * scale; cv.height = h * scale;
    const g = cv.getContext('2d'); g.scale(scale, scale);
    const [P, S, T, N] = palettes(WALLPAPERS[id].seed, 'vibrant');
    const fill = (c) => { g.fillStyle = c; };
    const grad = (y0, y1, stops) => { const gr = g.createLinearGradient(0, y0, 0, y1); stops.forEach(([o, c]) => gr.addColorStop(o, c)); return gr; };
    if (id === 'dunes') {
      g.fillStyle = grad(0, h, [[0, P(30)], [0.45, P(62)], [0.62, T(78)]]); g.fillRect(0, 0, w, h);
      fill(T(92)); g.globalAlpha = .9; g.beginPath(); g.arc(w * 0.68, h * 0.36, 46, 0, Math.PI * 2); g.fill(); g.globalAlpha = 1;
      [[0.56, 60, P(48), 1.3], [0.64, 50, P(38), 2.1], [0.73, 44, P(28), 1.6], [0.84, 36, P(18), 2.6]].forEach(([y, amp, c, f], i) => {
        fill(c); g.beginPath(); g.moveTo(0, h);
        for (let x = 0; x <= w; x += 4) g.lineTo(x, h * y + Math.sin((x / w) * Math.PI * f + i) * amp * 0.5 + Math.sin((x / w) * 7 + i * 2) * 6);
        g.lineTo(w, h); g.fill();
      });
    } else if (id === 'shapes') {
      fill(P(22)); g.fillRect(0, 0, w, h);
      const blob = (cx, cy, R, k, amp, rot, c) => {
        fill(c); g.beginPath();
        for (let i = 0; i <= 160; i++) { const a = (i / 160) * Math.PI * 2; const r = R * (1 + amp * Math.cos(k * a)); g.lineTo(cx + Math.cos(a + rot) * r, cy + Math.sin(a + rot) * r); }
        g.fill();
      };
      blob(w * 0.15, h * 0.18, 150, 9, 0.06, 0.2, P(34));
      blob(w * 0.95, h * 0.45, 170, 4, 0.13, 0.5, T(40));
      blob(w * 0.25, h * 0.78, 190, 7, 0.08, 0.1, S(30));
      fill(P(70)); g.beginPath(); g.roundRect(w * 0.55, h * 0.72, 220, 110, 55); g.fill();
      blob(w * 0.62, h * 0.2, 46, 6, 0.1, 0.3, T(80));
    } else if (id === 'sea') {
      g.fillStyle = grad(0, h, [[0, P(20)], [0.5, P(45)], [1, P(15)]]); g.fillRect(0, 0, w, h);
      for (let i = 0; i < 14; i++) {
        g.strokeStyle = i % 3 ? P(60 + (i % 4) * 6) : T(80); g.globalAlpha = 0.35 + (i % 3) * 0.15; g.lineWidth = 10 - (i % 4) * 2;
        g.beginPath(); const y0 = h * 0.15 + i * 48;
        for (let x = -10; x <= w + 10; x += 6) g.lineTo(x, y0 + Math.sin(x / 38 + i * 0.7) * 14 + Math.sin(x / 90 + i) * 10);
        g.stroke();
      }
      g.globalAlpha = 1;
    } else if (id === 'garden') {
      g.fillStyle = grad(0, h, [[0, P(28)], [1, P(14)]]); g.fillRect(0, 0, w, h);
      let seed = 7; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
      for (let i = 0; i < 36; i++) {
        const r = 14 + rnd() * 70; g.globalAlpha = 0.18 + rnd() * 0.45;
        fill([P(70), T(80), S(60), P(50)][i % 4]); g.beginPath(); g.arc(rnd() * w, rnd() * h, r, 0, Math.PI * 2); g.fill();
      }
      g.globalAlpha = 1;
    } else {
      g.fillStyle = grad(0, h, [[0, P(8)], [0.6, P(24)], [1, T(30)]]); g.fillRect(0, 0, w, h);
      let seed = 11; const rnd = () => (seed = (seed * 16807) % 2147483647) / 2147483647;
      fill('#fff'); for (let i = 0; i < 90; i++) { g.globalAlpha = 0.3 + rnd() * 0.7; g.fillRect(rnd() * w, rnd() * h * 0.6, 1.4, 1.4); }
      g.globalAlpha = 1; fill(T(92)); g.beginPath(); g.arc(w * 0.72, h * 0.2, 34, 0, Math.PI * 2); g.fill();
      fill(P(8)); g.beginPath(); g.arc(w * 0.72 + 14, h * 0.2 - 8, 30, 0, Math.PI * 2); g.fill();
      [[0.72, P(16)], [0.82, P(10)]].forEach(([y, c], i) => {
        fill(c); g.beginPath(); g.moveTo(0, h);
        for (let x = 0; x <= w; x += 6) g.lineTo(x, h * y - Math.abs(Math.sin(x / (60 + i * 30) + i)) * (90 - i * 30));
        g.lineTo(w, h); g.fill();
      });
    }
    return (wpCache[key] = cv.toDataURL('image/jpeg', 0.9));
  }

  return { load, compute, apply, hexToOklch, tone, palettes, SHAPES, WALLPAPERS, wallpaper, get mcuLoaded() { return !!mcu; } };
})();
