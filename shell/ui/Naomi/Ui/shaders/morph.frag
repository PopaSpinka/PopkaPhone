#version 440
// Значок-трансформер НаомиФона (Naomi.Ui/MorphIcon): плюс ↔ крутилка «Морфинг» одной фигурой. Фигуры заданы
// полем расстояний до контура (в точках интерфейса): плюс — два отрезка, кольцо — круг крутилки, «Морфинг» —
// квадрат со своими скруглениями углов. Переход — перетекание одного поля в другое, поэтому каждый кадр — цельная
// гладкая фигура, а не наложение двух картинок; край сглажен ровно на пиксель экрана.
// Новая форма (стрелка, стоп, галочка…) — ещё одна функция расстояния здесь и свой отрезок morph в main().
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float morph;     // 0 — плюс, 1 — кольцо (круг, с которого начинается крутилка)
    float phase;     // 0…1 — идёт круг крутилки «Морфинг» (1,8 с); вне — крутилки нет, видна фигура morph.
                     // Начало и конец круга — то же кольцо, что morph = 1: стык без шва
    float span;      // сторона элемента в точках фигуры
    float px;        // сторона элемента в пикселях экрана
    vec4 tint;
};

const float PI = 3.14159265;
const float ARM = 10.5;    // луч плюса (плюс 21 × 2)
const float RING = 9.0;    // середина контура крутилки 20 × 20 толщиной 2
const float HALF = 1.0;    // полутолщина линии

vec2 turnBy(vec2 p, float a) { float c = cos(a), s = sin(a); return vec2(c * p.x + s * p.y, -s * p.x + c * p.y); }

float segment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
    return length(pa - ba * h);
}

// плюс с лучами arm: при arm = 0 — точка толщины линии
float plusShape(vec2 p, float arm) {
    return min(segment(p, vec2(-arm, 0.0), vec2(arm, 0.0)), segment(p, vec2(0.0, -arm), vec2(0.0, arm))) - HALF;
}

// кольцо радиуса r: при r = 0 — та же точка
float ringShape(vec2 p, float r) { return abs(length(p) - r) - HALF; }

// «Морфинг» (Naomi.Ui/Spinner, ПопкаТВ CardLoading): квадрат 20 со скруглениями углов, поворотом и масштабом по
// ключевым кадрам (полуволна косинуса между ними); контур 2 внутри квадрата
float spinner(vec2 p, float ph) {
    // ключи: скругления (верх-лево, верх-право, низ-право, низ-лево), поворот, масштаб
    vec4 r0 = vec4(10.0), r1 = vec4(3.0), r2 = vec4(3.0, 10.0, 3.0, 10.0), r3 = vec4(10.0);
    float x = ph * 3.0;
    float part = min(2.0, floor(x));
    float e = (1.0 - cos(PI * (x - part))) / 2.0;
    vec4 ra = part < 0.5 ? r0 : part < 1.5 ? r1 : r2;
    vec4 rb = part < 0.5 ? r1 : part < 1.5 ? r2 : r3;
    float ta = part < 0.5 ? 0.0 : part < 1.5 ? 90.0 : 180.0;
    float tb = part < 0.5 ? 90.0 : part < 1.5 ? 180.0 : 360.0;
    float sa = part < 0.5 ? 1.0 : part < 1.5 ? 0.85 : 1.0;
    float sb = part < 0.5 ? 0.85 : 1.0;
    vec4 rad = mix(ra, rb, e) - HALF;
    float sc = mix(sa, sb, e);
    vec2 q = turnBy(p, radians(mix(ta, tb, e))) / sc;
    // угол — по месту точки (экран: y вниз)
    float rc = q.x < 0.0 ? (q.y < 0.0 ? rad.x : rad.w) : (q.y < 0.0 ? rad.y : rad.z);
    vec2 w = abs(q) - vec2(RING) + rc;
    float box = min(max(w.x, w.y), 0.0) + length(max(w, 0.0)) - rc;
    return (abs(box) - HALF) * sc;
}

void main() {
    vec2 p = (qt_TexCoord0 - 0.5) * span;
    float m = clamp(morph, 0.0, 1.0);
    // втягивание (выбор пользователя 12 ноября 2026): лучи стягиваются в точку, и точка тут же, без остановки,
    // раздувается в кольцо — одно движение: разгон и торможение — у всего перехода (MorphIcon), а не у половин,
    // иначе фигура замирала в точке и виден был «конец и начало»
    float d = m < 0.5 ? plusShape(p, ARM * (1.0 - m / 0.5)) : ringShape(p, RING * (m - 0.5) / 0.5);
    if (phase >= 0.0 && phase < 1.0)
        d = spinner(p, phase);
    float alpha = clamp(0.5 - d * px / span, 0.0, 1.0);
    fragColor = vec4(tint.rgb * tint.a, tint.a) * alpha * qt_Opacity;
}
