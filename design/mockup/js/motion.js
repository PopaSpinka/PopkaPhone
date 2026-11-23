'use strict';
/* Движение: пружины и кривые Material 3 Expressive.
   Пружина — как в Jetpack Compose: dampingRatio (ζ), stiffness (k), масса 1.
   Значения токенов — androidx compose material3 tokens (MotionSchemeKeyTokens, Expressive/StandardMotionTokens). */
const Motion = (() => {
  const SCHEMES = {
    expressive: { spatialFast: [0.6, 800], spatial: [0.8, 380], spatialSlow: [0.8, 200], effectsFast: [1, 3800], effects: [1, 1600], effectsSlow: [1, 800] },
    standard:   { spatialFast: [0.9, 1400], spatial: [0.9, 700], spatialSlow: [0.9, 300], effectsFast: [1, 3800], effects: [1, 1600], effectsSlow: [1, 800] },
  };
  // Кривые M3 (до Expressive). emphasized в Android — составная path-кривая; здесь её обычное приближение.
  const EASING = {
    emphasized: [0.2, 0, 0, 1], emphasizedDecelerate: [0.05, 0.7, 0.1, 1], emphasizedAccelerate: [0.3, 0, 0.8, 0.15],
    standard: [0.2, 0, 0, 1], standardDecelerate: [0, 0, 0, 1], standardAccelerate: [0.3, 0, 1, 1], linear: [0, 0, 1, 1],
  };
  const DURATION = { short1: 50, short2: 100, short3: 150, short4: 200, medium1: 250, medium2: 300, medium3: 350, medium4: 400,
    long1: 450, long2: 500, long3: 550, long4: 600, extraLong1: 700, extraLong2: 800, extraLong3: 900, extraLong4: 1000 };

  let scheme = 'expressive';
  let speed = 1;            // замедление: 1 — как на телефоне, 0.1 — в десять раз медленнее
  let engine = 'spring';    // spring | curve

  function spec(s) {
    if (s && typeof s === 'object') return s;
    const [z, k] = SCHEMES[scheme][s] || SCHEMES[scheme].spatial;
    return { z, k };
  }

  // Точное решение затухающего осциллятора: смещение x и скорость v через t секунд.
  function springState(x0, v0, k, z, t) {
    const w0 = Math.sqrt(k);
    if (z < 1) {
      const wd = w0 * Math.sqrt(1 - z * z), e = Math.exp(-z * w0 * t);
      const A = x0, B = (v0 + z * w0 * x0) / wd, c = Math.cos(wd * t), s = Math.sin(wd * t);
      return [e * (A * c + B * s), e * (-z * w0 * (A * c + B * s) + (-A * wd * s + B * wd * c))];
    }
    if (z === 1) {
      const e = Math.exp(-w0 * t), B = v0 + w0 * x0;
      return [e * (x0 + B * t), e * (B - w0 * (x0 + B * t))];
    }
    const r = Math.sqrt(z * z - 1), s1 = -w0 * (z - r), s2 = -w0 * (z + r);
    const C2 = (v0 - s1 * x0) / (s2 - s1), C1 = x0 - C2, e1 = Math.exp(s1 * t), e2 = Math.exp(s2 * t);
    return [C1 * e1 + C2 * e2, s1 * C1 * e1 + s2 * C2 * e2];
  }

  function bezier(x1, y1, x2, y2) {
    const cx = 3 * x1, bx = 3 * (x2 - x1) - cx, ax = 1 - cx - bx, cy = 3 * y1, by = 3 * (y2 - y1) - cy, ay = 1 - cy - by;
    const sx = t => ((ax * t + bx) * t + cx) * t, sy = t => ((ay * t + by) * t + cy) * t, dx = t => (3 * ax * t + 2 * bx) * t + cx;
    const solve = x => {
      let t = x;
      for (let i = 0; i < 8; i++) { const e = sx(t) - x; if (Math.abs(e) < 1e-6) return t; const d = dx(t); if (Math.abs(d) < 1e-6) break; t -= e / d; }
      let lo = 0, hi = 1; t = x;
      for (let i = 0; i < 40; i++) { const v = sx(t); if (Math.abs(v - x) < 1e-6) break; if (x > v) lo = t; else hi = t; t = (lo + hi) / 2; }
      return t;
    };
    return x => (x <= 0 ? 0 : x >= 1 ? 1 : sy(solve(x)));
  }
  // emphasized в Android — путь из двух кубических сегментов:
  // M0,0 C0.05,0 0.133333,0.06 0.166666,0.4 C0.208333,0.82 0.25,1 1,1
  function cubicPath(p0, p1, p2, p3) {
    const bx = t => { const u = 1 - t; return u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0]; };
    const by = t => { const u = 1 - t; return u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]; };
    return x => { let lo = 0, hi = 1, t = 0.5; for (let i = 0; i < 30; i++) { t = (lo + hi) / 2; if (bx(t) < x) lo = t; else hi = t; } return by(t); };
  }
  const emphSeg1 = cubicPath([0, 0], [0.05, 0], [0.133333, 0.06], [0.166666, 0.4]);
  const emphSeg2 = cubicPath([0.166666, 0.4], [0.208333, 0.82], [0.25, 1], [1, 1]);
  const emphasizedPath = x => (x <= 0 ? 0 : x >= 1 ? 1 : x < 0.166666 ? emphSeg1(x) : emphSeg2(x));
  const easeFns = { emphasized: emphasizedPath };
  function ease(name) {
    if (typeof name === 'function') return name;
    if (!easeFns[name]) easeFns[name] = bezier(...(EASING[name] || EASING.emphasized));
    return easeFns[name];
  }
  // Кривая прогресса предиктивного «назад» (система, Compose и MDC): (0.1, 0.1, 0, 1)
  const backProgress = bezier(0.1, 0.1, 0, 1);

  // ---- Цикл кадров: все бегущие значения + отложенная отрисовка (не чаще раза за кадр) ----
  const running = new Set();
  const renders = new Set();
  let raf = 0, last = 0;
  function frame(now) {
    const dt = Math.min(0.05, Math.max(0, (now - last) / 1000)) * speed;
    last = now;
    for (const v of [...running]) v._step(dt);
    const list = [...renders]; renders.clear();
    for (const fn of list) fn();
    raf = (running.size || renders.size) ? requestAnimationFrame(frame) : 0;
  }
  function kick() { if (!raf) { last = performance.now(); raf = requestAnimationFrame(frame); } }
  function schedule(fn) { renders.add(fn); kick(); }

  class Value {
    constructor(v = 0, render = null, threshold = 0.01) { this.v = v; this.vel = 0; this.render = render; this.thr = threshold; this._a = null; }
    get running() { return !!this._a; }
    _changed() { if (this.render) schedule(this.render); }
    set(v, vel = 0) { this.stop(); this.v = v; this.vel = vel; this._changed(); }
    stop() { if (this._a) { const r = this._a.resolve; this._a = null; running.delete(this); r(false); } }
    // Пружина к цели. v0 — начальная скорость (единиц в секунду «времени анимации»).
    spring(to, s = 'spatial', v0) {
      const p = spec(s);
      this.stop();
      return new Promise(resolve => {
        this._a = { type: 'spring', to, z: p.z, k: p.k, x0: this.v - to, v0: v0 === undefined ? this.vel : v0, t: 0, resolve };
        running.add(this); kick();
      });
    }
    tween(to, ms, e = 'emphasized') {
      this.stop();
      return new Promise(resolve => {
        this._a = { type: 'tween', from: this.v, to, dur: Math.max(0.001, ms / 1000), ease: ease(e), t: 0, resolve };
        running.add(this); kick();
      });
    }
    // «Как выбрано в пульте»: пружина или кривая той же роли.
    animate(to, role = 'spatial', v0) {
      if (engine === 'spring') return this.spring(to, role, v0);
      const dur = { spatialFast: DURATION.medium2, spatial: DURATION.long1, spatialSlow: DURATION.long4, effectsFast: DURATION.short4, effects: DURATION.medium1, effectsSlow: DURATION.medium4 }[role] || DURATION.long1;
      const e = role.startsWith('effects') ? 'standard' : 'emphasized';
      return this.tween(to, dur, e);
    }
    _step(dt) {
      const a = this._a;
      if (!a) { running.delete(this); return; }
      a.t += dt;
      if (a.type === 'spring') {
        const [x, v] = springState(a.x0, a.v0, a.k, a.z, a.t);
        this.v = a.to + x; this.vel = v;
        if (Math.abs(x) < this.thr && Math.abs(v) < this.thr * 12) { this.v = a.to; this.vel = 0; this._finish(); }
      } else {
        const p = Math.min(1, a.t / a.dur), nv = a.from + (a.to - a.from) * a.ease(p);
        this.vel = dt > 0 ? (nv - this.v) / dt : 0; this.v = nv;
        if (p >= 1) { this.vel = 0; this._finish(); }
      }
      this._changed();
    }
    _finish() { const r = this._a.resolve; this._a = null; running.delete(this); r(true); }
  }

  // Пружина для CSS-переходов: кривая linear() по точному решению + длительность до успокоения.
  function settle(z, k) {
    let t = 0, calm = 0; const dt = 1 / 240;
    while (t < 3) {
      t += dt;
      const [x, v] = springState(-1, 0, k, z, t);
      if (Math.abs(x) < 0.001 && Math.abs(v) < 0.02) { calm += dt; if (calm > 0.02) return t; } else calm = 0;
    }
    return t;
  }
  function cssSpring(name) {
    const { z, k } = spec(name), T = settle(z, k), N = 50, pts = [];
    for (let i = 0; i <= N; i++) { const [x] = springState(-1, 0, k, z, (T * i) / N); pts.push(+(1 + x).toFixed(4)); }
    return { easing: `linear(${pts.join(', ')})`, ms: Math.round((T * 1000) / speed), settleMs: Math.round(T * 1000) };
  }
  function applyCss(el) {
    for (const name of Object.keys(SCHEMES.expressive)) {
      const kebab = name.replace(/[A-Z]/g, m => '-' + m.toLowerCase());
      let easing, ms;
      if (engine === 'spring') ({ easing, ms } = cssSpring(name));
      else {
        const c = EASING[name.startsWith('effects') ? 'standard' : 'emphasized'];
        easing = `cubic-bezier(${c.join(',')})`;
        ms = Math.round(({ spatialFast: 300, spatial: 450, spatialSlow: 600, effectsFast: 200, effects: 250, effectsSlow: 400 }[name]) / speed);
      }
      el.style.setProperty(`--sp-${kebab}`, easing);
      el.style.setProperty(`--sp-${kebab}-ms`, ms + 'ms');
    }
  }

  // Скорость пальца по последним ~100 мс, в dp/с реального времени.
  class Tracker {
    constructor() { this.s = []; }
    add(x, y, t = performance.now()) { this.s.push({ x, y, t }); while (this.s.length > 2 && t - this.s[0].t > 100) this.s.shift(); }
    velocity() {
      const s = this.s; if (s.length < 2) return { vx: 0, vy: 0 };
      const a = s[0], b = s[s.length - 1], dt = (b.t - a.t) / 1000;
      if (dt <= 0.004) return { vx: 0, vy: 0 };
      return { vx: (b.x - a.x) / dt, vy: (b.y - a.y) / dt };
    }
  }

  const sleep = ms => new Promise(r => setTimeout(r, ms / speed));

  return {
    SCHEMES, EASING, DURATION, Value, Tracker, spec, ease, schedule, cssSpring, applyCss, springState, sleep, backProgress,
    get scheme() { return scheme; }, set scheme(v) { scheme = v; },
    get speed() { return speed; }, set speed(v) { speed = v; },
    get engine() { return engine; }, set engine(v) { engine = v; },
    // скорость пальца (dp/с реального времени) → единицы/с времени анимации
    sim: v => v / speed,
  };
})();

const clamp = (v, a = 0, b = 1) => Math.min(b, Math.max(a, v));
const lerp = (a, b, t) => a + (b - a) * t;
const prog = (v, a, b) => clamp((v - a) / (b - a));
