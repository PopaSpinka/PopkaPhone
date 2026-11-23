# Material 3 Expressive — точные токены с источниками (для веб-макета и Qt/QML-оболочки)

Дата сбора: 2026-11-10. Все числа взяты из исходного кода или со страниц Google; где источники расходятся — показаны оба значения.

## 0. Как читать эти заметки

**Пометки:**
- **[код]** — значение прочитано в исходном коде (указаны файл и строка).
- **[сайт]** — значение прочитано на m3.material.io или developer.android.com (страница рендерилась в браузере).
- **[?]** — не проверено: нашёл только косвенно или не нашёл совсем.

**Источники и их версии (ссылки закреплены на конкретные коммиты):**

| Код | Что | Версия / коммит |
|---|---|---|
| **AX** | androidx, Compose Material3 `compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/` | коммит `8b858f00703acca475c89c30729ec9f9cf893e15` (androidx-main, 2026-11-11). Последний выпуск на Maven: `material3 1.4.0` (стабильный), `1.5.0-rc01` |
| **MDC** | material-components-android `lib/java/com/google/android/material/` | коммит `66b0eeb53afeb6025ffbbd37734b08e5bce197ed` (2026-10-16). Выпуск `1.14.0` от 2026-05-13 |
| **MW** | material-web, сгенерированные Google токены `tokens/versions/latest/sass/` («Design system: Google Material 3, Version: 34.0.21») | коммит `37aa14b843dcaeb3a46f898679fa6fcd109b1905` |
| **MCU** | material-color-utilities `typescript/` | коммит `194144d75ed7d34f06264a625e086db81a7ab9aa` (2026-11-06) |
| **AOSP** | android.googlesource.com, ветка `main` | на 2026-11-10 |
| **M3** | m3.material.io (страницы указаны в каждом разделе) | на 2026-11-10 |

Базовые адреса для ссылок ниже:
- AX = `https://github.com/androidx/androidx/blob/8b858f00703acca475c89c30729ec9f9cf893e15/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3`
- MDC = `https://github.com/material-components/material-components-android/blob/66b0eeb53afeb6025ffbbd37734b08e5bce197ed/lib/java/com/google/android/material`
- MW = `https://github.com/material-components/material-web/blob/37aa14b843dcaeb3a46f898679fa6fcd109b1905/tokens/versions/latest/sass`
- MCU = `https://github.com/material-foundation/material-color-utilities/blob/194144d75ed7d34f06264a625e086db81a7ab9aa/typescript`
- AOSP = `https://android.googlesource.com/platform/frameworks/base/+/refs/heads/main`

Запись вида «AX `tokens/ExpressiveMotionTokens.kt#L22-L33`» означает ссылку `<AX>/tokens/ExpressiveMotionTokens.kt#L22-L33`.

**Главные расхождения между источниками (подробности в разделах):**
1. Кривая emphasized-accelerate: Compose, сайт и веб-токены дают `(0.3, 0, 0.8, 0.15)`, а MDC-Android — `(0.3, 0, 0.8, 0.2)`. Кривая emphasized-decelerate: везде `(0.05, 0.7, 0.1, 1)`, кроме MDC, где `(0.1, 0.7, 0.1, 1)`.
2. Межбуквенное расстояние (tracking): в Compose округлено (Display L −0.2, Body M 0.2, Title M 0.2). В MDC и вебе — −0.25 / 0.25 / 0.15.
3. Высота ручки слайдера размера M: на сайте 52dp, в коде MDC и в веб-токенах 44dp.
4. Минимальный масштаб при предиктивном «назад»: система и MDC сжимают до 0.9; новые переходы Compose `NavigationTransitions` — до 0.85 (1 − 0.15).
5. В шрифте Google Sans Flex на Google Fonts **нет кириллицы**: подмножества cyrillic не отдаются.

---

## 1. Движение (motion)

### 1.1 Пружины (motion physics system, M3 Expressive)

В двух схемах (standard и expressive) по 6 токенов: spatial или effects × fast, default или slow. Масса пружины равна 1 (так в Compose `spring()`).

| Токен | Standard: damping / stiffness | Expressive: damping / stiffness |
|---|---|---|
| `spring.fast.spatial` | **0.9 / 1400** | **0.6 / 800** |
| `spring.default.spatial` | **0.9 / 700** | **0.8 / 380** |
| `spring.slow.spatial` | **0.9 / 300** | **0.8 / 200** |
| `spring.fast.effects` | **1.0 / 3800** | **1.0 / 3800** |
| `spring.default.effects` | **1.0 / 1600** | **1.0 / 1600** |
| `spring.slow.effects` | **1.0 / 800** | **1.0 / 800** |

Источники **[код]**, все три совпадают:
- AX `tokens/StandardMotionTokens.kt#L20-L31` (VERSION v0_14_0)
- AX `tokens/ExpressiveMotionTokens.kt#L22-L33` (VERSION v0_14_0)
- MDC `motion/res/values/tokens.xml#L25-L52`: `m3_sys_motion_expressive_spring_*` и `m3_sys_motion_standard_spring_*`
- MW `_md-sys-motion.scss#L72-L105`: здесь только standard (0.9/700, 0.9/1400, 0.9/300, 1/1600, 1/3800, 1/800)
- Схемы в Compose: AX `MotionScheme.kt#L123` (`standard()`), `#L132` (`expressive()`), `#L139-L170` и `#L203-L234`. По умолчанию в `MaterialTheme` стоит `standard()` (AX `MaterialTheme.kt#L181`), в `MaterialExpressiveTheme` — `expressive()` (`#L262`).
- Ключи токенов: AX `tokens/MotionSchemeKeyTokens.kt#L23-L30`: DefaultSpatial=0, FastSpatial=1, SlowSpatial=2, DefaultEffects=3, FastEffects=4, SlowEffects=5.

**Когда что применять** **[сайт]**, https://m3.material.io/styles/motion/overview/how-it-works (пересказ):
- **Spatial** — всё, что двигает элемент на экране: положение x/y, поворот, размер, скругление углов. Пружина перелетает конечное значение и отскакивает.
- **Effects** — цвет и прозрачность, где перелёт недопустим (damping = 1).
- **Скорость:** default — для большинства движений: анимации, закрывающие часть экрана (нижний лист, развёрнутая навигационная панель). Fast — маленькие компоненты (переключатели, кнопки), например смена цвета ручки переключателя. Slow — полноэкранные анимации и полное обновление контента.
- Expressive — основная схема, особенно для ключевых моментов. Standard — для утилитарных продуктов, почти без отскока.
- По словам сайта, движение всех компонентов задают два токена: expressive fast spatial и expressive fast effects. По Compose это не совсем так, см. таблицу 1.3.
- Значения токенов могут отличаться у часов, телефона и планшета **[сайт]**. Конкретных значений для других устройств в открытом коде не нашёл **[?]**.

### 1.2 Пружины для веба: замена кривой и длительностью **[сайт]**

Источник: https://m3.material.io/styles/motion/overview/specs, раздел «Web: Convert springs to curves». Пригодится для CSS там, где нет жестов и прерываний.

| Пружина | cubic-bezier | Длительность |
|---|---|---|
| Expressive fast spatial | 0.42, 1.67, 0.21, 0.90 | 350ms |
| Expressive default spatial | 0.38, 1.21, 0.22, 1.00 | 500ms |
| Expressive slow spatial | 0.39, 1.29, 0.35, 0.98 | 650ms |
| Expressive fast effects | 0.31, 0.94, 0.34, 1.00 | 150ms |
| Expressive default effects | 0.34, 0.80, 0.34, 1.00 | 200ms |
| Expressive slow effects | 0.34, 0.88, 0.34, 1.00 | 300ms |
| Standard fast spatial | 0.27, 1.06, 0.18, 1.00 | 350ms |
| Standard default spatial | 0.27, 1.06, 0.18, 1.00 | 500ms |
| Standard slow spatial | 0.27, 1.06, 0.18, 1.00 | 750ms |
| Standard fast effects | 0.31, 0.94, 0.34, 1.00 | 150ms |
| Standard default effects | 0.34, 0.80, 0.34, 1.00 | 200ms |
| Standard slow effects | 0.34, 0.88, 0.34, 1.00 | 300ms |

Для Qt/QML и для точного поведения в вебе лучше решать саму пружину: x'' = −k·(x − цель) − c·x′, где m = 1, k = stiffness, c = 2·ζ·√k, ζ = damping. Так устроен `spring()` в Compose и `SpringForce` в Android. Встроенный `SpringAnimation` в QML задаёт параметры иначе (`spring`/`damping`/`mass`) и **напрямую не совпадает** с этими токенами. Соответствие я не проверял **[?]**: надёжнее свой интегратор.

### 1.3 Какие пружины используют компоненты Compose **[код]**

Подсчитано по вхождениям `MotionSchemeKeyTokens.*` в файлах AX:

| Файл | Пружины |
|---|---|
| `Button.kt` | DefaultEffects ×2. Морфинг формы при нажатии намеренно идёт на effects, чтобы не было отскока (комментарий в `Button.kt#L253-L255`) |
| `ToggleButton.kt` | FastSpatial ×2, DefaultEffects ×1 |
| `IconButton.kt` | DefaultEffects ×4 |
| `ButtonGroup.kt` | FastSpatial ×1 (расширение нажатой кнопки) |
| `Switch.kt` | FastSpatial ×1 |
| `FloatingActionButtonMenu.kt` | FastSpatial ×2, FastEffects ×1, SlowEffects ×1 |
| `AppBar.kt` | FastSpatial ×1, DefaultEffects ×2 |
| `ListItem.kt` | FastSpatial ×2, DefaultEffects ×1 |
| `Slider.kt` | пружины схемы не использует |

### 1.4 Классические кривые (easing), «устаревшая» система

Сайт **[сайт]** (https://m3.material.io/styles/motion/easing-and-duration/tokens-specs) прямо пишет: в expressive-обновлении компоненты перешли на пружины. Easing/duration больше не развиваются, но **по-прежнему применяются в переходах между экранами**.

| Токен | CSS / Compose | Примечание |
|---|---|---|
| `easing.emphasized` | Android: `path(M 0,0 C 0.05, 0, 0.133333, 0.06, 0.166666, 0.4 C 0.208333, 0.82, 0.25, 1, 1, 1)`. В CSS одной кривой нет: сайт советует standard. Compose: `CubicBezierEasing(0.2, 0, 0, 1)` | Путь из двух сегментов. Тот же путь — `fast_out_extra_slow_in` / `Interpolators.EMPHASIZED` в AOSP |
| `easing.emphasized.decelerate` | **cubic-bezier(0.05, 0.7, 0.1, 1.0)** | В MDC: (0.1, 0.7, 0.1, 1), см. ниже |
| `easing.emphasized.accelerate` | **cubic-bezier(0.3, 0.0, 0.8, 0.15)** | В MDC: (0.3, 0, 0.8, 0.2), см. ниже |
| `easing.standard` | **cubic-bezier(0.2, 0.0, 0, 1.0)** | |
| `easing.standard.decelerate` | **cubic-bezier(0, 0, 0, 1)** | |
| `easing.standard.accelerate` | **cubic-bezier(0.3, 0, 1, 1)** | |
| `easing.legacy` | cubic-bezier(0.4, 0, 0.2, 1) | |
| `easing.legacy.decelerate` | cubic-bezier(0, 0, 0.2, 1) | |
| `easing.legacy.accelerate` | cubic-bezier(0.4, 0, 1, 1) | |
| `easing.linear` | cubic-bezier(0, 0, 1, 1) | |

Источники:
- **[код]** AX `tokens/MotionTokens.kt#L40-L68`: Emphasized (0.2,0,0,1), EmphasizedAccelerate (0.3,0,0.8,0.15), EmphasizedDecelerate (0.05,0.7,0.1,1), Legacy…, Standard (0.2,0,0,1), StandardAccelerate (0.3,0,1,1), StandardDecelerate (0,0,0,1).
- **[код]** MW `_md-sys-motion.scss#L50-L67`: те же значения, `easing-emphasized: $easing-standard`.
- **[код] Расхождение:** MDC `motion/res/values/tokens.xml#L55-L71`. Строка 59: `emphasized_accelerate = cubic-bezier(0.3, 0, 0.8, 0.2)`. Строка 66: `emphasized_decelerate = cubic-bezier(0.1, 0.7, 0.1, 1)`. Значения перепроверены побайтно, в файле нет «0.15». Для оболочки советую значения сайта и Compose.
- **[сайт]** Таблица на странице tokens-specs совпадает с Compose: `PathInterpolator(0.05f, 0.7f, 0.1f, 1f)` и `PathInterpolator(0.3f, 0f, 0.8f, 0.15f)`.

### 1.5 Длительности **[код]** + **[сайт]**

| Токен | мс | | Токен | мс |
|---|---|---|---|---|
| short1 | 50 | | long1 | 450 |
| short2 | 100 | | long2 | 500 |
| short3 | 150 | | long3 | 550 |
| short4 | 200 | | long4 | 600 |
| medium1 | 250 | | extra-long1 | 700 |
| medium2 | 300 | | extra-long2 | 800 |
| medium3 | 350 | | extra-long3 | 900 |
| medium4 | 400 | | extra-long4 | 1000 |

Источники: AX `tokens/MotionTokens.kt#L24-L39`, MDC `motion/res/values/tokens.xml#L122-L137`, MW `_md-sys-motion.scss#L18-L49`, сайт tokens-specs.

Примеры с сайта: элементы выбора — 200ms со standard. FAB, раскрывающийся в лист, — 400ms с emphasized. Карточка на весь экран — 500ms с emphasized. Фоновая автопрокрутка карусели — 1000ms.

Устаревшие длительности в `transition/res/values/integers.xml` у MDC (75/150/200/250/300/350) — от M2, их **не использовать**. Темы M3 переопределяют атрибуты на `m3_sys_motion_duration_*`: MDC `theme/res/values/themes_base.xml#L495-L503`.

### 1.6 Справочно: системные константы пружин

- Compose: `Spring.StiffnessMediumLow` = 400, `DampingRatioNoBouncy` = 1. Это пружина по умолчанию у `fadeIn`/`slideIn*` (https://github.com/androidx/androidx/blob/8b858f00703acca475c89c30729ec9f9cf893e15/compose/animation/animation/src/commonMain/kotlin/androidx/compose/animation/EnterExitTransition.kt#L327-L328 и #L800-L805). Числа 400 и 1 взяты из публичных констант Compose; в этой сессии сам файл `Spring.kt` не открыл **[?]**.
- Android `SpringForce`: STIFFNESS_LOW = 200, MEDIUM = 1500; DAMPING_RATIO_LOW_BOUNCY = 0.75, NO_BOUNCY = 1. Используются в предиктивном «назад» системы (п. 3.3). Значения общеизвестные, в этой сессии по коду не сверял **[?]**.

### 1.7 Стандартный переход между активити в Android (что видит пользователь «по умолчанию») **[код]**

AOSP `core/res/res/anim/activity_open_enter.xml`, `activity_open_exit.xml`, `activity_close_enter.xml`, `activity_close_exit.xml`:
- Сдвиг по X на **96dp**, длительность **450ms**, интерполятор `fast_out_extra_slow_in` (то есть emphasized-путь).
- Новый экран появляется альфой 0→1 за **83ms**, линейно, с задержкой **50ms**. Старый при закрытии гаснет 1→0 за 83ms с задержкой 35ms.
- Края расширяются (`<extend>`) на 96dp, чтобы при сдвиге не было пустой полосы.

---

## 2. Переходы между экранами (transition patterns)

### 2.1 MDC-Android: `com.google.android.material.transition` **[код]**

| Переход | Параметры |
|---|---|
| **MaterialSharedAxis X** | Сдвиг `SlideDistanceProvider` по горизонтали на **30dp** (`mtrl_transition_shared_axis_slide_distance`). Вперёд — к END, назад — к START. Плюс прозрачность через `FadeThroughProvider` с порогом **0.35** |
| **MaterialSharedAxis Y** | То же по вертикали: 30dp, вперёд — BOTTOM, назад — TOP; fade through 0.35 |
| **MaterialSharedAxis Z** | Масштаб через `ScaleProvider(growing=forward)`. Вперёд: входящий **0.8→1.0**, уходящий **1.0→1.1**. Назад: входящий 1.1→1.0, уходящий 1.0→0.8. Плюс fade through 0.35 |
| Shared axis, время | `motionDurationLong1` = **450ms**, `motionEasingEmphasizedInterpolator` (emphasized-путь) |
| **MaterialFadeThrough** | Уходящий: альфа 1→0 на отрезке прогресса **[0, 0.35]**. Входящий: альфа 0→1 на **[0.35, 1]**, масштаб **0.92→1.0**; уходящий не масштабируется. 450ms (long1), emphasized |
| **MaterialFade** (появление компонента в пределах экрана) | Входящий: альфа 0→1 за первые **30%** прогресса (`incomingEndThreshold 0.3`), масштаб **0.8→1.0**. Уходящий: только альфа 1→0, без масштаба. Вход **400ms** (medium4), emphasized decelerate. Выход **150ms** (short3), emphasized accelerate |
| **MaterialElevationScale** | Масштаб 1.0↔**0.85** и прозрачность |
| **MaterialContainerTransform** | Вход: `motionDurationLong2` = **500ms**. Возврат: `motionDurationMedium4` = **400ms**. Кривая emphasized. Затемнение `0x52000000` (чёрный, альфа 0x52 = 82/255 ≈ 32%). Режим прозрачности по умолчанию `FADE_MODE_IN`, режим вписывания `FIT_MODE_AUTO` |
| Container transform, пороги прогресса (без дуги) | Вход: fade **0–0.25**, scale 0–1, scaleMask 0–1, shapeMask **0–0.75**. Возврат: fade **0.60–0.90**, scale 0–1, scaleMask 0–0.90, shapeMask **0.30–0.90** |
| Container transform, пороги (по дуге, ArcMotion) | Вход: fade 0.10–0.40, scale 0.10–1, scaleMask 0.10–1, shapeMask 0.10–0.90. Возврат: fade 0.60–0.90, scale 0–0.90, scaleMask 0–0.90, shapeMask 0.20–0.90 |

Ссылки:
- MDC `transition/MaterialSharedAxis.java#L86-L121`
- MDC `transition/res/values/dimens.xml#L18` (30dp)
- MDC `transition/SlideDistanceProvider.java#L116-L123`
- MDC `transition/ScaleProvider.java#L38-L41` (1.0, 1.1, 0.8, 1.0) и `#L148-L170` (логика growing)
- MDC `transition/FadeThroughProvider.java#L38` (0.35F) и `#L66-L84`
- MDC `transition/MaterialFadeThrough.java#L33-L51` (0.92f)
- MDC `transition/MaterialFade.java#L36-L66`
- MDC `transition/MaterialElevationScale.java#L28`
- MDC `transition/MaterialContainerTransform.java#L200-L226` (пороги), `#L240` (scrim), `#L242-L243`, `#L1008-L1011` (длительности)
- Длительности тем M3: MDC `theme/res/values/themes_base.xml#L495-L503` → `motion/res/values/tokens.xml#L122-L137`

### 2.2 Compose Material3: `NavigationTransitions.kt` (новое в androidx-main) **[код]**

AX `NavigationTransitions.kt`:
- `HorizontalSharedAxisNavigationTransitions` (`#L59`). Вперёд (`#L82`): входящий выезжает с **+20% ширины** (`SlideFraction 0.2f`, `#L170`), уходящий уезжает на −20%. Прозрачность tween **90ms**, задержка **50ms**, линейно (`#L175-L177`). Сдвиг идёт пружиной Compose по умолчанию (stiffness 400, damping 1). Назад (`#L95`) — зеркально, уходящий слой ниже (zIndex −10).
- `FadeThroughNavigationTransitions` (`#L194`): уход — fadeOut **90ms** линейно и масштаб к **1.05**. Вход — fadeIn **210ms** линейно с задержкой 90ms и масштаб от **0.8** (`#L256-L259`).
- Сайт **[сайт]** (https://m3.material.io/styles/motion/transitions/transition-patterns) отмечает: переходы M3 всё ещё на старой системе easing/duration и позже перейдут на пружины. Android-переход «вперёд/назад» — сдвиг с затуханием. Переход между разделами верхнего уровня — fade through. Lateral (вкладки, карусель) — сдвиг без затухания.

---

## 3. Предиктивный «назад» (predictive back)

### 3.1 Общая кривая прогресса жеста **[код]**, все источники совпадают

Сырой прогресс жеста (0..1) пропускается через **cubic-bezier(0.1, 0.1, 0, 1)**:
- AOSP `core/java/android/view/animation/BackGestureInterpolator.java`: `super(0.1f, 0.1f, 0f, 1f)`
- AX `internal/BackHandler.kt#L47-L51`: `PredictiveBackEasing = CubicBezierEasing(0.1f, 0.1f, 0f, 1f)`
- MDC `motion/MaterialBackAnimationHelper.java#L46`: `new PathInterpolator(0.1f, 0.1f, 0, 1)`
- **[сайт]** developer.android.com (раздел «Full screen surfaces → Interpolation»): параметры (.1, .1, 0, 1), как в SystemUI. На той же странице для shared element советуют STANDARD_DECELERATE (0,0,0,1) — это более старая рекомендация.

### 3.2 Полноэкранные поверхности внутри приложения

**[сайт]** https://developer.android.com/design/ui/mobile/guides/patterns/predictive-back:

| Параметр | Начало | Конец | Контекст |
|---|---|---|---|
| Масштаб уходящего | 100% | **90%** | |
| Масштаб входящего | **110%** | 100% | |
| Прозрачность уходящего | 100% | 0% | гаснет полностью к **35%** прогресса |
| Прозрачность входящего | 0% | 100% | начинает проявляться с **35%** прогресса |

В точке 35% не виден ни один экран. После порога «commit» содержимое сменяется через fade through.

Для «отклеенной» поверхности (shared element или back preview) на той же странице:
- Сдвиг по X: максимум **((ширина экрана / 20) − 8) dp**, остаётся отступ **8dp**. Пример: при ширине 1280 сдвиг 56dp.
- Сдвиг по Y: максимум **((доступная высота / 20) − 8) dp**, отступ 8dp сохраняется. Сдвиг идёт через decelerate-кривую до предела, чтобы поверхность не упиралась резко.
- Минимальный масштаб окна **90%**. Если поверхность уходит за экран, уменьшать не более чем на 50%.
- **Fling:** при быстром жесте поверхность сначала доходит до максимального состояния предпросмотра, затем идёт анимация завершения.
- **Отмена:** если отпустить до порога, поверхность быстро возвращается к исходному размеру и положению.

**[код]** MDC `motion/MaterialMainContainerBackHelper.java`. Используется в SearchView и других полноэкранных контейнерах.
- `MIN_SCALE = 0.9f` (`#L57`); масштаб = lerp(1, 0.9, p).
- `minEdgeGap` = `m3_back_progress_main_container_min_edge_gap` = **8dp**; `maxTranslationY` = **24dp**. Значения в `motion/res/values/dimens.xml#L26-L27`.
- translationX = lerp(0, max(0, (w − 0.9w)/2 − 8dp), p) в сторону жеста (`#L127-L130`). translationY = lerp(0, min(свободное место, 24dp), |Δy|/h) по знаку Δy (`#L132-L137`).
- **Радиус углов во время жеста** плавно меняется от «развёрнутого» к `collapsedCornerSize` (`#L139-L144`). «Развёрнутый» радиус — это скругление углов **самого дисплея** из `WindowInsets.getRoundedCorner()`, но только у углов, которые касаются краёв экрана (`#L256-L297`). У поиска `collapsedCornerSize` — радиус свёрнутой строки поиска.
- Времена (`MaterialBackAnimationHelper.java#L41-L67`): скрытие после commit — lerp(**300ms**, **150ms**, прогресс): `motionDurationMedium2` → `motionDurationShort3`. Отмена — **100ms** (`motionDurationShort2`).

### 3.3 Системная анимация между активити (AOSP WM Shell) **[код]**

AOSP `libs/WindowManager/Shell/src/com/android/wm/shell/back/CrossActivityBackAnimation.kt` и `DefaultCrossActivityBackAnimation.kt`:
- `MAX_SCALE = 0.9f`. Затемнение предыдущей активити: чёрный, альфа **0.2** в светлой теме и **0.8** в тёмной (`MAX_SCRIM_ALPHA_LIGHT/DARK`). После commit гаснет до 0.
- `displayBoundsMargin` = `cross_task_back_vertical_margin` = **8dp**. Сдвиг по Y ограничен так, чтобы не заходить за поле 8dp; кривая `DecelerateInterpolator`, учитывается не больше половины высоты экрана.
- Свайп от левого края: уходящее окно масштабируется до 0.9 и прижимается к **правому** краю с отступом 8dp. Свайп справа: масштаб от центра.
- Входящее (предыдущее) окно стартует сдвинутым на **−96dp** (`cross_activity_back_entering_start_offset`) и масштабируется синхронно.
- Скругление углов окна = `ScreenDecorationsUtils.getWindowCornerRadius(context)`, то есть радиус углов дисплея.
- После commit: **450ms**, интерполятор EMPHASIZED. Уходящее окно уезжает дальше на 96dp, его альфа = max(1 − 5·p, 0), то есть исчезает за первые 20%.
- Fling: пружина `STIFFNESS_LOW` / `DAMPING_RATIO_LOW_BOUNCY`, режим FLING_BOUNCE.
- Размеры: AOSP `libs/WindowManager/Shell/res/values/dimen.xml#642-647` (14dp — зазор между окнами для cross-task, 8dp, 96dp).

### 3.4 Compose `NavigationTransitions.predictiveBackTransform` **[код]**

AX `NavigationTransitions.kt#L115-L177`, `#L223-L267`, `#L269-L300`:
- Масштаб текущего = 1 − eased(p)·**0.15** (`ScaleProgressMultiplier`, `#L266`), то есть до **0.85**. Это меньше, чем 0.9 у системы и MDC.
- Точка опоры по X: 0.9 при свайпе слева и 0.5 при свайпе справа. Сдвиг X = (1 − s)·(pivot − 0.5)·w.
- Поле `DisplayBoundsMargin = 8.dp` (`#L267`). Сдвиг Y = maxΔY·(1 − (1 − r)²), где r = min(1, |Δy|/(h/2)), maxΔY = max(0, (h − h·s)/2 − 8dp).
- Предыдущий экран (shared axis): масштаб = s − **0.05**, сдвиг X = **−20%** ширины. Поверх него вуаль цвета `colorScheme.scrim` с альфой **0.2** в светлой теме и **0.8** в тёмной (`#L168-L169`).
- Fade through при жесте: предыдущий экран невидим (альфа 0, масштаб 0.8), текущий сжимается.

### 3.5 Нижний лист (bottom sheet)

- **[сайт]** https://m3.material.io/components/bottom-sheets/guidelines: при жесте лист отделяется от левого и правого краёв, под ним виден предыдущий экран.
- **[код]** AX `BottomSheet.kt#L443-L487` и MDC `motion/res/values/dimens.xml#L23-L24`. Ширина уменьшается максимум на **48dp**, высота — на **24dp**: scaleX = 1 − lerp(0, 48dp, p)/w, scaleY = 1 − lerp(0, 24dp, p)/h. Точка опоры — нижний край листа. Содержимое масштабируется обратно (scaleY = sx/sy, опора сверху), чтобы не искажалось.
- MDC `MaterialBottomContainerBackHelper.java#L84-L146`: после commit lerp(300ms, 150ms, p), отмена 100ms.

### 3.6 Боковой лист и навигационный ящик

- **[сайт]** https://m3.material.io/components/side-sheets/guidelines: лист отделяется от верхнего и нижнего краёв; лист и его содержимое всегда масштабируются в сторону жеста.
- **[код]** MDC `motion/res/values/dimens.xml#L19-L21`: по X сжатие до **24dp** или рост до **12dp**, по Y — **48dp**. Compose ящик: AX `NavigationDrawer.kt#L1345-L1350` (12dp рост, 24dp сжатие, 48dp по Y).

### 3.7 Поиск (search view)

- **[сайт]** https://m3.material.io/components/search/guidelines: полноэкранный поиск отделяется от края экрана и масштабируется назад в сторону жеста, под ним виден предыдущий экран.
- **[код]** AX `SearchBar.kt#L4187-L4192`:
  - `SearchBarPredictiveBackMinScale = 9f/10f` (**0.9**), `MinMargin = 8.dp`.
  - `MaxOffsetXRatio = 1/20`, то есть maxOffsetX = w/20 − 8dp. `MaxOffsetY = 24.dp`.
  - Анимации (`#L4195-L4213`): вход **600ms** (DurationLong4) с emphasized decelerate и задержкой 100ms. Выход **350ms** (DurationMedium3) с `CubicBezierEasing(0, 1, 0, 1)`.

### 3.8 Сводка: commit и cancel

| | Commit (отпустил после порога или fling) | Cancel (отпустил до порога) |
|---|---|---|
| Полный экран (сайт) | Fade through к цели, порог 35% | Быстрый возврат масштаба и положения |
| MDC-контейнеры | Скрытие за lerp(300→150ms) по прогрессу | Возврат за 100ms |
| Система (cross-activity) | 450ms emphasized, затемнение гаснет, fling-пружина | Возврат через `BackProgressAnimator`: пружина MEDIUM, NO_BOUNCY |

Источник для `BackProgressAnimator`: AOSP `core/java/android/window/BackProgressAnimator.java#L53-L70`.

---

## 4. Форма (shape)

### 4.1 Шкала скруглений **[код]** + **[сайт]**

| Токен | dp | Новое в M3E |
|---|---|---|
| `corner.none` | 0 | |
| `corner.extra-small` | 4 | |
| `corner.small` | 8 | |
| `corner.medium` | 12 | |
| `corner.large` | 16 | |
| `corner.large-increased` | **20** | да |
| `corner.extra-large` | 28 | |
| `corner.extra-large-increased` | **32** | да |
| `corner.extra-extra-large` | **48** | да |
| `corner.full` | полное скругление (Compose `CircleShape`, в вебе 9999px) | в M3E «full» — отдельный стиль; раньше считали как 50% размера |

Асимметричные варианты: `extra-small-top` (4,4,0,0), `large-top` (16,16,0,0), `large-start` (16,0,0,16), `large-end` (0,16,16,0), `extra-large-top` (28,28,0,0).

Источники:
- AX `tokens/ShapeTokens.kt#L28-L81` (VERSION 14_1_0)
- MW `_md-sys-shape.scss#L18-L35` (corner-value)
- **[сайт]** https://m3.material.io/styles/shape/corner-radius-scale (10 ступеней) и https://m3.material.io/styles/shape/overview-principles: в мае 2025 добавлены 20/32/48 и «full».

**Оптическое скругление** **[сайт]**: внутренний радиус = внешний радиус − отступ. Пример с сайта: 48 − 14 = 34dp.

### 4.2 Библиотека форм Material Shapes (35 форм) и морфинг

- **[сайт]** В обновлении M3E (май 2025) добавлено 35 форм и морфинг между ними. Морфинг используется в **стандартной группе кнопок** и **индикаторе загрузки**. По умолчанию он идёт на expressive-схеме. На вебе API нет: «Web is not currently available», https://m3.material.io/styles/shape/shape-morph.
- **[код]** AX `MaterialShapes.kt#L192-L328`, ровно 35 форм: Circle, Square, Slanted, Arch, Fan, Arrow, SemiCircle, Oval, Pill, Triangle, Diamond, ClamShell, Pentagon, Gem, Sunny, VerySunny, Cookie4Sided, Cookie6Sided, Cookie7Sided, Cookie9Sided, Cookie12Sided, Ghostish, Clover4Leaf, Clover8Leaf, Burst, SoftBurst, Boom, SoftBoom, Flower, Puffy, PuffyDiamond, PixelCircle, PixelTriangle, Bun, Heart.
- Формы строятся через `androidx.graphics.shapes` (`RoundedPolygon`, `CornerRounding`, `Morph`). На Maven последняя версия **graphics-shapes 1.1.0**.
- Примеры описаний (AX `MaterialShapes.kt#L331-L590`):
  - `Circle` = `RoundedPolygon.circle(numVertices=10)`.
  - `Square` = прямоугольник 1×1 со скруглением 0.3.
  - `Sunny` = звезда, 8 лучей, innerRadius 0.8, скругление 0.15.
  - `Cookie9Sided` = звезда, 9 лучей, innerRadius 0.8, скругление 0.5, повёрнута на −90°.
  - `Oval` = круг, сжатый по Y до 0.64 и повёрнутый на −45°.
  - `SoftBurst` = 2 точки × 10 повторов: (0.193, 0.277) со скруглением 0.053 и (0.176, 0.055) со скруглением 0.053.
  - Остальные формы заданы списками точек `PointNRound` с повторами и отражением. Их можно перенести 1:1.
- **Где морфинг в компонентах** **[код]**:
  - Кнопки: при нажатии форма меняется на `PressedContainerShape`, например у S-кнопки с full на 8dp. У переключаемых (toggle) кнопок выбранное состояние скругляет иначе. В `Button.kt` форма анимируется пружиной DefaultEffects, без отскока.
  - Соединённая группа кнопок: внутренние углы 8dp (S), при нажатии 4dp, у выбранной 50%.
  - Стандартная группа кнопок: нажатая кнопка расширяется на 15% ширины пружиной fast spatial. Таблица в п. 6.3.
  - Индикатор загрузки — п. 6.7.

---

## 5. Типографика

### 5.1 Базовая шкала (15 стилей)

| Стиль | Размер sp | Межстрочный sp | Tracking sp (MDC и веб) | Tracking в Compose | Вес | Гарнитура |
|---|---|---|---|---|---|---|
| Display L | 57 | 64 | **−0.25** | −0.2 | 400 | Brand |
| Display M | 45 | 52 | 0 | 0 | 400 | Brand |
| Display S | 36 | 44 | 0 | 0 | 400 | Brand |
| Headline L | 32 | 40 | 0 | 0 | 400 | Brand |
| Headline M | 28 | 36 | 0 | 0 | 400 | Brand |
| Headline S | 24 | 32 | 0 | 0 | 400 | Brand |
| Title L | 22 | 28 | 0 | 0 | 400 | Brand |
| Title M | 16 | 24 | **0.15** | 0.2 | 500 | Plain |
| Title S | 14 | 20 | 0.1 | 0.1 | 500 | Plain |
| Body L | 16 | 24 | 0.5 | 0.5 | 400 | Plain |
| Body M | 14 | 20 | **0.25** | 0.2 | 400 | Plain |
| Body S | 12 | 16 | 0.4 | 0.4 | 400 | Plain |
| Label L | 14 | 20 | 0.1 | 0.1 | 500 | Plain |
| Label M | 12 | 16 | 0.5 | 0.5 | 500 | Plain |
| Label S | 11 | 16 | 0.5 | 0.5 | 500 | Plain |

Источники:
- AX `tokens/TypeScaleTokens.kt#L24-L247` (VERSION v0_103). Округлённые −0.2/0.2/0.2 стоят там с 2022 года (коммиты `b938e956bf69`, `d95b985e08c1`).
- MW `_md-sys-typescale.scss`: tracking в rem, например `#L111` display-large −0.015625rem = −0.25px, `#L63` body-medium 0.25px, `#L355` title-medium 0.15px.
- MDC `typography/res/values/tokens.xml#L25-L133`: letterSpacing в em. DisplayLarge −0.00438596 × 57 = −0.25; TitleMedium 0.009375 × 16 = 0.15; BodyMedium 0.01785714 × 14 = 0.25.
- В вебе в каждом стиле заданы и оси переменного шрифта: `wght` (400/500), `wdth` 100, `ROND` 0, `opsz` = размеру кегля, `GRAD` 0, `slnt` 0 (MW `_md-sys-typescale.scss`).

**Перевод единиц** **[сайт]** (https://m3.material.io/styles/typography/type-scale-tokens): rem = sp/16; letter-spacing в em = tracking/размер. Пример с сайта: 0.2/16 = 0.0125em.

### 5.2 Emphasized-стили (M3E, ещё 15)

| Стиль | Размер / межстрочный | Вес в Compose | Веб: static weight / `wght` | Tracking: Compose / веб |
|---|---|---|---|---|
| Display L/M/S Emph. | как базовые | Medium (500) | medium / 500 | 0 / −0.25, 0, 0 |
| Headline L/M/S Emph. | как базовые | 500 | medium / 500 | 0 / 0 |
| Title L Emph. | 22/28 | 500 | medium / 500 | 0 / 0 |
| Title M Emph. | 16/24 | **Bold (700)** | bold / **600** | 0.15 / 0.15 |
| Title S Emph. | 14/20 | 700 | bold / 600 | 0.1 / 0.1 |
| Body L Emph. | 16/24 | 500 | medium / 500 | **0.15 / 0.5** |
| Body M Emph. | 14/20 | 500 | medium / 500 | 0.25 / 0.25 |
| Body S Emph. | 12/16 | 500 | medium / 500 | 0.4 / 0.4 |
| Label L/M/S Emph. | 14/20, 12/16, 11/16 | 700 | bold / 600 | 0.1, 0.5, 0.5 |

Источники:
- AX `tokens/TypeScaleTokens.kt#L249-L473`. В коде пометка «TODO update with the generated tokens once available», поэтому tracking в Compose ненадёжен.
- MW `_md-sys-typescale-emphasized.scss` (v34.0.21).
- **[сайт]** Emphasized-стили — для выделения: выбранные пункты, кнопки главного действия, бейджи, extended FAB, непрочитанное. Компоненты по умолчанию их **не** используют.

### 5.3 Шрифты и их оси

- **[сайт]** https://m3.material.io/styles/typography/fonts: по умолчанию **Roboto** (статический) и для brand, и для plain. Переменный Roboto Flex «пока не входит в шкалу M3». На странице про форму сказано, что формы M3 перекликаются по скруглённости с **Google Sans Flex**.
- **[код]** Compose: `TypefaceTokens.Brand` и `Plain` = `FontFamily.SansSerif` (AX `tokens/TypefaceTokens.kt#L25-L29`). Веб: `$brand: Roboto; $plain: Roboto` (MW `_md-ref-typeface.scss`).
- **Roboto Flex** (лицензия OFL; google/fonts `ofl/robotoflex/METADATA.pb`): оси GRAD −200..150, XOPQ 27..175, XTRA 323..603, YOPQ 25..135, YTAS 649..854, YTDE −305..−98, YTFI 560..788, YTLC 416..570, YTUC 528..760, opsz 8..144, slnt −10..0, wdth 25..151, wght 100..1000. **Кириллица есть** (cyrillic, cyrillic-ext).
- **Google Sans Flex** (лицензия OFL-1.1; google/fonts `ofl/googlesansflex/METADATA.pb`; версия v5.000 добавлена 2026-10-09; исходники https://github.com/googlefonts/googlesans-flex):
  - Оси в сборке Google Fonts: **ROND 0..100, opsz 1..144 (по умолчанию 18), slnt −10..0, wdth 25..151, wght 1..1000**.
  - В исходниках ещё **GRAD 0..100**, но в файл для Google Fonts ось не вошла.
  - **Кириллицы НЕТ:** подмножества canadian-aboriginal, cherokee, latin, latin-ext, math, nushu, symbols, syriac, tifinagh, vietnamese. Проверено по METADATA и по CSS `fonts.googleapis.com/css2?family=Google+Sans+Flex`. Для русского интерфейса НаомиФона Google Sans Flex не подходит: нужен Roboto Flex или Roboto.
- Каким шрифтом рисует сам Android 16 / Pixel (Google Sans Flex в SystemUI) — **[?]**, по коду не проверял.

---

## 6. Компоненты M3 Expressive (размеры)

### 6.1 Слайдер

| Размер | Высота дорожки | Ручка (высота × ширина) | Скругление дорожки | Иконка внутри | Отступ иконки |
|---|---|---|---|---|---|
| **XS** (по умолчанию) | 16dp | 44 × 4dp | 8dp | — | — |
| **S** | 24dp | 44 × 4dp | 8dp | — | — |
| **M** | 40dp | **44 × 4dp** в коде (на сайте **52dp**) | 12dp | 24dp | 6dp |
| **L** | 56dp | 68 × 4dp | 16dp | 24dp | 6dp |
| **XL** | 96dp | 108 × 4dp | 28dp | 32dp | 8dp |

Общие значения:
- Ручка шириной 4dp; при нажатии и фокусе — 2dp.
- Зазор между ручкой и дорожкой 6dp (`ActiveHandleLeadingSpace`/`TrailingSpace`).
- Внутренний угол дорожки у ручки 2dp. Точка-ограничитель (stop indicator) 4dp, отступ от края 6dp в Compose (в вебе 4dp).
- Выноска значения: отступ снизу 12dp, текст `LabelLarge`, фон inverseSurface. На сайте контейнер выноски 44×48dp.
- Цвета: активная дорожка primary, неактивная secondaryContainer, ручка primary, stop indicator — onSecondaryContainer или onPrimary.

Источники:
- **[код]** AX `tokens/SliderTokens.kt#L24-L165` (v2_3_5) и AX `Slider.kt#L3377-L3390`. В Compose есть только XS; размеры S–XL — только токенами.
- **[код]** MDC `slider/res/values/tokens.xml#L57-L94` и `slider/res/values/styles.xml#L21-L51` (`Widget.Material3Expressive.Slider.Xsmall…Xlarge`).
- **[код]** MW `_md-comp-slider-{xsmall,small,medium,large,xlarge}.scss#L19-L31`, `_md-comp-slider.scss`.
- **[сайт]** https://m3.material.io/components/sliders/specs: таблица Measurements (там M = 52dp).

### 6.2 Переключатель (switch)

| Параметр | Значение |
|---|---|
| Дорожка | **52 × 32dp**, полное скругление, обводка 2dp (когда выключен) |
| Ручка выключена | **16dp** |
| Ручка выключена, с иконкой | 24dp |
| Ручка включена | **24dp** |
| Ручка нажата | **28dp** |
| Иконка | 16dp |
| Слой состояния (state layer) | 40dp |
| Цвета включённого | дорожка primary, ручка onPrimary, иконка onPrimaryContainer; при нажатии, наведении и фокусе ручка primaryContainer |
| Цвета выключенного | дорожка surfaceContainerHighest, обводка и ручка outline, иконка surfaceContainerHighest; при нажатии и наведении ручка onSurfaceVariant |
| Движение | ручка — пружина FastSpatial (п. 1.3) |

Источники: AX `tokens/SwitchTokens.kt#L24-L185` (v0_210); MW `_md-comp-switch.scss#L41-L73`.

### 6.3 Кнопки, иконочные кнопки, группы кнопок

**Кнопки XS–XL** (AX `tokens/Button{XSmall,Small,Medium,Large,XLarge}Tokens.kt#L24-L54`):

| Размер | Высота | Иконка | Отступы по краям | Иконка–текст | Скругление: квадратная / нажатая | Обводка |
|---|---|---|---|---|---|---|
| XS | 32 | 20 | 16 | 8 | 12 / 8 | 1 |
| S | 40 | 20 | 16 | 8 | 12 / 8 | 1 |
| M | 56 | 24 | 24 | 8 | 16 / 12 | 1 |
| L | 96 | 32 | 48 | 12 | 28 / 16 | 2 |
| XL | 136 | 40 | 64 | 16 | 28 / 16 | 3 |

Круглая форма — full у всех размеров.

**Иконочные кнопки XS–XL** (AX `tokens/{XSmall,Small,Medium,Large,XLarge}IconButtonTokens.kt`):

| Размер | Высота | Иконка | Ширина narrow / default / wide (отступ с каждой стороны) | Квадратная | Нажатая | Выбранная круглая → |
|---|---|---|---|---|---|---|
| XS | 32 | 20 | 4 / 6 / 10 | 12 | 8 | 12 |
| S | 40 | 24 | 4 / 8 / 14 | 12 | 8 | 12 |
| M | 56 | 24 | 12 / 16 / 24 | 16 | 12 | 16 |
| L | 96 | 32 | 16 / 32 (uniform) / 48 | 28 | 16 | 28 |
| XL | 136 | 40 | 32 / 48 / 72 | 28 | 16 | 28 |

Выбранная квадратная кнопка становится full.

**Группы кнопок** (MW `_md-comp-button-group-{standard,connected}-{size}.scss`; AX `tokens/ButtonGroupSmallTokens.kt#L24-L27`, `tokens/ConnectedButtonGroupSmallTokens.kt#L25-L36`, `ButtonGroup.kt#L188-L195`):

| Размер | Стандартная: зазор | Соединённая: зазор | Соединённая: внутренний угол / нажатый / выбранный |
|---|---|---|---|
| XS (32) | 18 | 2 | 8 / 4 / 50% |
| S (40) | 12 | 2 | 8 / 4 / 50% |
| M (56) | 8 | 2 | 8 / 4 / 50% |
| L (96) | 8 | 2 | 16 / 12 / 50% |
| XL (136) | 8 | 2 | 20 / 16 / 50% |

В стандартной группе нажатая кнопка расширяется на **15%** своей ширины (`pressed-item-width-multiplier`, `ExpandedRatio = 0.15f`) пружиной **fast spatial**. Внешние углы соединённой группы — full.

### 6.4 FAB и меню FAB

- FAB 56dp: углы 16, иконка 24. Medium FAB 80dp: углы **20** (large-increased), иконка 28. Large FAB 96dp: углы 28, иконка 32. Источники: AX `tokens/Fab{Baseline,Medium,Large}Tokens.kt`; форма medium — MW `_md-comp-fab-medium.scss`.
- Меню FAB (AX `tokens/FabMenuBaselineTokens.kt#L24-L63`, MW `_md-comp-fab-menu.scss`):
  - Кнопка закрытия 56dp, круглая, иконка 20, отступ до списка 8.
  - Пункт высотой 56dp, круглый (full), отступы 24/24, иконка 24, иконка–текст 8, между пунктами 4.
  - Тень пункта: в Compose Level3 (6dp), в вебе level0.
- Движение меню: FastSpatial ×2, FastEffects, SlowEffects (п. 1.3).

### 6.5 Верхние панели (top app bar)

| Вариант | Свёрнута | Развёрнута | Заголовок | Подзаголовок |
|---|---|---|---|---|
| Small | 64dp | — | TitleLarge | LabelMedium |
| **Medium flexible** (M3E) | 64dp | **112dp**, с подзаголовком **136dp** | **HeadlineMedium** | LabelLarge |
| **Large flexible** (M3E) | 64dp | **120dp**, с подзаголовком **152dp** | **DisplaySmall** | TitleMedium |
| Medium (старая, deprecated) | 64 | 112 | HeadlineSmall | — |
| Large (старая) | 64 | 152 | HeadlineMedium | — |

Ещё размеры:
- Отступы по краям 4dp, иконки 24, аватар 32.
- Отступ заголовка 16dp от края, то есть 12dp после 4dp-отступа.
- Нижний отступ заголовка: 24dp у medium, 28dp у large.
- Тень при прокрутке Level2 (3dp). Строка поиска в панели 56dp, full.

Источники:
- AX `tokens/AppBarTokens.kt`, `AppBarSmallTokens.kt#L25-L31`, `AppBarMediumFlexibleTokens.kt#L25-L34`, `AppBarLargeFlexibleTokens.kt#L25-L34` (VERSION 14_0_0)
- AX `AppBar.kt#L1987-L2015` (TopAppBarDefaults…Height) и `#L3604-L3615`
- MW `_md-comp-app-bar-*.scss`

### 6.6 Списки

| Параметр | Значение |
|---|---|
| Высота строки: одна / две / три строки | **56 / 72 / 88dp** |
| Отступы | слева 16, справа 16, сверху 10, снизу 10, между элементами 12 |
| Иконки | 24dp; в expressive-варианте **20dp** |
| Аватар | 40, круглый |
| Картинка | 56×56, в expressive скругление 8 |
| Видео | 100×56 или 114×64, скругление 8 |
| Тексты | label BodyLarge, supporting BodyMedium, overline и trailing LabelSmall |
| **Сегментированный (grouped) список** | зазор **2dp**. Внешние углы первого и последнего пункта **16dp** (`ContainerShape` = large). Внутренние углы **4dp** (`ItemContainerExpressiveShape` = extra-small). Нажатый, выбранный и в фокусе — **16dp**, при наведении 12dp. Цвет пункта `surface` (`ItemSegmentedContainerColor`), выбранного — secondaryContainer |

Источники:
- AX `tokens/ListTokens.kt#L24-L353` (VERSION 29.0.0): `SegmentedGap` `#L353`, высоты `#L180/#L347/#L323`
- AX `ListItemDefaults.kt#L449-L537` (`segmentedShapes`, `SegmentedGap`)
- MW `_md-comp-list.scss#L26-L324`

Фон, на котором лежат сегменты: в токенах не нашёл **[?]**. Обычно это surfaceContainer у экрана.

### 6.7 Индикатор загрузки (loading indicator)

- Контейнер 48×48dp, full; активная фигура **38dp**, масштаб 38/48. Вариант «contained»: фон primaryContainer, фигура onPrimaryContainer; без контейнера фигура primary. Источники: AX `tokens/LoadingIndicatorTokens.kt#L24-L42`, MW `_md-comp-loading-indicator.scss`.
- Неопределённый режим, последовательность из 7 форм: **SoftBurst → Cookie9Sided → Pentagon → Pill → Sunny → Cookie4Sided → Oval** (AX `LoadingIndicator.kt#L550-L556`).
- Морф каждые **650ms** пружиной **damping 0.6 / stiffness 200** (visibilityThreshold 0.1). После каждого морфа фигура поворачивается ещё на **+90°**. Общее вращение **360° за 4666ms**, линейно, бесконечно (`#L406-L440`, `#L706-L711`).
- Определённый режим: Circle (повёрнут на 18°) ↔ SoftBurst по прогрессу (`#L568-L573`).

### 6.8 Нижняя навигационная панель (navigation bar, M3E)

- Высота **64dp** (раньше в M3 было 80dp: `TallContainerHeight`). Фон surfaceContainer, тень Level2.
- Вертикальный пункт: индикатор **56×32dp**, full; иконка 24; иконка–подпись 4; отступ сверху 6.
- Горизонтальный пункт (на широких экранах): индикатор высотой 40, отступы 16/16.
- Подпись LabelMedium. Активная: иконка onSecondaryContainer, индикатор secondaryContainer, подпись secondary. Неактивная — onSurfaceVariant.

Источники: AX `tokens/NavigationBarTokens.kt#L24-L64`, `NavigationBarVerticalItemTokens.kt#L24-L33`, `NavigationBarHorizontalItemTokens.kt#L24-L33`; MW `_md-comp-nav-bar*.scss`.

### 6.9 Панели инструментов (toolbars, M3E)

- **Плавающая**: высота 64, full, отступы внутри 8/8, между элементами 4, от края экрана 16 (вертикальная — 24), тень level3. Цвета: standard — surfaceContainer; vibrant — primaryContainer, кнопки onPrimaryContainer. Источники: AX `tokens/FloatingToolbarTokens.kt#L24-L60`, MW `_md-comp-toolbar-floating.scss`.
- **Прикреплённая (docked)**: высота 64, без скругления, края 16/16, промежутки от 4 до 32. Она же `FlexibleBottomAppBar` (AX `AppBar.kt#L2352`). Источники: AX `tokens/DockedToolbarTokens.kt#L28-L43`, MW `_md-comp-toolbar-docked.scss`.

### 6.10 Карточки

Скругление у всех **12dp** (medium):
- Elevated: surfaceContainerLow, Level1 (1dp).
- Filled: surfaceContainerHighest, Level0.
- Outlined: surface, обводка outlineVariant 1dp.

Источники: AX `tokens/{Elevated,Filled,Outlined}CardTokens.kt`.

Справочно — высоты теней Level0..5 = 0, 1, 3, 6, 8, 12dp (AX `tokens/ElevationTokens.kt#L24-L39`). Непрозрачность слоя состояния: hover 0.08, focus 0.10, pressed 0.10, dragged 0.16 (AX `tokens/StateTokens.kt#L22-L25`).

---

## 7. Цвет: dynamic color

### 7.1 Варианты схем (Variant) **[код]**

MCU `dynamiccolor/variant.ts`: MONOCHROME, NEUTRAL, TONAL_SPOT, VIBRANT, EXPRESSIVE, FIDELITY, CONTENT, RAINBOW, FRUIT_SALAD и новый **CMF** (его не разбирал **[?]**). Версии спецификации: `'2021' | '2025' | '2026'` (`color_spec.ts#L27`). По умолчанию в TS — `'2021'` и платформа `'phone'` (`dynamic_scheme.ts#L120-L121`).

В Android по умолчанию **TONAL_SPOT**. Если у цвета-источника chroma < 5 (кроме CONTENT), вместо него берётся GOOGLE_BLUE. В AOSP вариант Neutral называется `SPRITZ`, Monochrome — `MONOCHROMATIC`; есть системные CLOCK и CLOCK_VIBRANT. Источник: AOSP `frameworks/libs/systemui/monet/src/com/android/systemui/monet/ColorScheme.java#L68-L107`. Схемы там создаются **без указания версии спецификации**. Какую версию (2021 или 2025) берёт libmonet в Android 16 — **[?]**, не проверил.

**Палитры по спецификации 2021** (MCU `dynamiccolor/dynamic_scheme.ts#L632-L810`; h — оттенок источника, c — его chroma):

| Вариант | primary | secondary | tertiary | neutral | neutralVariant |
|---|---|---|---|---|---|
| TONAL_SPOT | h, **36** | h, **16** | h+60, **24** | h, **6** | h, **8** |
| VIBRANT | h, 200 | поворот по таблице, 24 | поворот, 32 | h, 10 | h, 12 |
| EXPRESSIVE | h+240, 40 | поворот, 24 | поворот, 32 | h+15, 8 | h+15, 12 |
| NEUTRAL | h, 12 | h, 8 | h, 16 | h, 2 | h, 2 |
| MONOCHROME | h, 0 | h, 0 | h, 0 | h, 0 | h, 0 |
| FIDELITY / CONTENT | h, c (как у источника) | h, max(c−32, c/2) | дополнительный (FIDELITY) или аналогичный (CONTENT) цвет с исправлением «неприятных» | h, c/8 | h, c/8+4 |
| RAINBOW | h, 48 | h, 16 | h+60, 24 | h, 0 | h, 0 |
| FRUIT_SALAD | h−50, 48 | h−50, 36 | h, 36 | h, 10 | h, 16 |

Таблицы поворота оттенка для VIBRANT и EXPRESSIVE — в `#L686-L740`. Палитра error по 2021: hue 25, chroma 84 (`#L270`).

**Изменения спецификации 2025 для телефона** (MCU `dynamic_scheme.ts#L811-L1120`):
- TONAL_SPOT: primary chroma **32** в светлой теме и **26** в тёмной; tertiary chroma **28** с поворотом оттенка по таблице; neutral **5**, neutralVariant 5×1.7.
- EXPRESSIVE: primary h, chroma 48 в светлой и 36 в тёмной.
- VIBRANT: primary chroma 74.
- NEUTRAL: primary 12 для синих оттенков, иначе 8; neutral 1.4.
- Палитра error: оттенок кусочно 12, 22 или 32, chroma 60 (TONAL_SPOT).

### 7.2 Роли → тона по спецификации 2021 **[код]**

Источник: MCU `dynamiccolor/color_spec_2021.ts#L141-L700`.

`ContrastCurve(low, normal, medium, high)`: при contrastLevel 0 берётся **normal**, второе число (`contrast_curve.ts#L36-L51`).

| Роль | Светлая | Тёмная | Палитра |
|---|---|---|---|
| primary | **40** | **80** | primary |
| onPrimary | 100 | 20 | primary |
| primaryContainer | 90 | 30 | primary |
| onPrimaryContainer | **30** | 90 | primary |
| inversePrimary | 80 | 40 | primary |
| secondary / onSecondary | 40 / 100 | 80 / 20 | secondary |
| secondaryContainer / on… | 90 / 30 | 30 / 90 | secondary |
| tertiary / onTertiary | 40 / 100 | 80 / 20 | tertiary |
| tertiaryContainer / on… | 90 / 30 | 30 / 90 | tertiary |
| error / onError | 40 / 100 | 80 / 20 | error |
| errorContainer / on… | 90 / 30 | 30 / 90 | error |
| background / onBackground | 98 / 10 | 6 / 90 | neutral |
| surface / onSurface | **98 / 10** | **6 / 90** | neutral |
| surfaceDim | **87** | 6 | neutral |
| surfaceBright | 98 | **24** | neutral |
| surfaceContainerLowest | **100** | **4** | neutral |
| surfaceContainerLow | **96** | **10** | neutral |
| surfaceContainer | **94** | **12** | neutral |
| surfaceContainerHigh | **92** | **17** | neutral |
| surfaceContainerHighest | **90** | **22** | neutral |
| surfaceVariant / onSurfaceVariant | 90 / 30 | 30 / 80 | neutralVariant |
| inverseSurface / inverseOnSurface | 20 / 95 | 90 / 20 | neutral |
| outline / outlineVariant | 50 / 80 | 60 / 30 | neutralVariant |
| shadow, scrim | 0 | 0 | neutral |
| surfaceTint | 40 | 80 | primary |
| primaryFixed / Dim / onFixed / onFixedVariant | 90 / 80 / 10 / 30 | то же | primary (так же для secondary и tertiary) |

Особенности:
- **MONOCHROME**: primary 0/100, onPrimary 90/10, primaryContainer 25/85.
- **FIDELITY**: primaryContainer = тон источника, onPrimaryContainer подбирается до контраста 4.5.
- Пары тонов дополнительно выравниваются по контрасту (`toneDeltaPair`, `contrastCurve`), поэтому итоговые тона могут немного сдвигаться.

### 7.3 Спецификация 2025, телефон **[код]**

Источник: MCU `dynamiccolor/color_spec_2025.ts`.

- surface: светлая **98** (99 у жёлтых нейтралей, 97 у VIBRANT), тёмная **4** (`#L220`).
- surfaceDim: 87 / 4 (`#L247`). surfaceBright: 98 / 18 (`#L283`).
- surfaceContainerLowest/Low/—/High/Highest:
  - светлая **100 / 96 / 94 / 92 / 90**;
  - тёмная **0 / 6 / 9 / 12 / 15** (`#L319-L449`).
- inverseSurface 4 / 98 (`#L603`).
- primary: TONAL_SPOT — тёмная 80, светлая `tMaxC` (тон с максимальной chroma); NEUTRAL — 40/80 (`#L627`).
- primaryContainer: TONAL_SPOT — светлая tMaxC(0..90), тёмная tMinC(35..93) (`#L708`). Появилась новая роль `primaryDim` (`#L666`).
- secondaryContainer: TONAL_SPOT — 90 / 25 (`#L928`).
- error: светлая tMaxC, тёмная tMinC(0..98). errorContainer: tMaxC(0..90) / tMinC(30..93) (`#L1251-L1307`).

Вывод для макета:
- Если повторяете «классический» Android 12–15 — спецификация 2021.
- Если хотите «новейший» вид — 2025.
- Что именно показывает Android 16 на Pixel — **[?]**.

### 7.4 Material Color Utilities: реализации

| Язык | Где | Версия | Лицензия | Спецификации 2025/2026 |
|---|---|---|---|---|
| **TypeScript** | npm **`@material/material-color-utilities`** | **0.4.0**, опубликован **2026-01-21T18:35:58Z** (dist-tag latest; до неё 0.3.0 от 2024-06-24) | Apache-2.0 | есть (`color_spec_2025.ts`, `color_spec_2026.ts`). Последний коммит в репозитории — 2026-11-06, то есть npm отстаёт от main |
| Dart | pub.dev `material_color_utilities` | **0.13.1** (2026-08-10) | Apache-2.0 | файлов spec нет — вероятно только 2021 **[?]** |
| Java | исходники в репозитории; в составе MDC-Android (`com.google.android.material.color.utilities`) | отдельного артефакта нет | Apache-2.0 | есть (`ColorSpec2025.java`, `ColorSpec2026.java`) |
| Kotlin | исходники в репозитории | отдельного артефакта не нашёл **[?]** | Apache-2.0 | есть (`ColorSpec2025.kt`, `ColorSpec2026.kt`) |
| C++ | исходники в репозитории | — | Apache-2.0 | файлов spec нет — вероятно только 2021 **[?]** |
| Swift | Swift Package (`swift/Package.swift`) | — | Apache-2.0 | не проверял **[?]** |

Сам репозиторий: https://github.com/material-foundation/material-color-utilities, лицензия Apache-2.0. Данные npm: `https://registry.npmjs.org/@material/material-color-utilities`. Для Qt/C++ ближе всего порт C++, но там нет спецификации 2025: для неё придётся переносить TS или Kotlin.

---

## 8. Что не проверено или противоречит

1. **Высота ручки слайдера M:** сайт 52dp, код MDC и веб-токены 44dp.
2. **emphasized accelerate/decelerate в MDC** расходятся с остальными источниками (п. 1.4).
3. **Tracking в Compose** округлён (п. 5.1). У emphasized-стилей в Compose есть TODO, Body L Emph. 0.15 против 0.5 в вебе.
4. **Минимальный масштаб предиктивного «назад»:** 0.9 у системы, MDC и в гайде; 0.85 в новых `NavigationTransitions` Compose.
5. **Значения пружин для часов и планшетов:** сайт пишет, что они другие; в коде не нашёл.
6. **Шрифт системы Android 16 / Pixel** и **версия цветовой спецификации на Android 16** — не проверено.
7. **Радиус углов экрана Pixel 5** (от него зависит скругление окна при «назад») не известен. Мерить на телефоне: `WindowInsets.getRoundedCorner()`; на Linux — по панели.
8. Страницы m3.material.io — одностраничное приложение (SPA). Тексты сняты через браузер; таблицы токенов на сайте — интерактивный виджет, их значения брались из кода.
9. Пружины Compose по умолчанию (StiffnessMediumLow = 400) и константы Android `SpringForce` приведены по известным публичным значениям, файлы `Spring.kt` и `SpringForce.java` в этой сессии не открывал.
10. Вариант CMF и спецификацию 2026 в MCU не разбирал.

## 9. Страницы, которые читал (кроме ссылок на код выше)

- https://m3.material.io/styles/motion/overview/how-it-works
- https://m3.material.io/styles/motion/overview/specs
- https://m3.material.io/styles/motion/easing-and-duration/tokens-specs
- https://m3.material.io/styles/motion/transitions/transition-patterns
- https://m3.material.io/foundations/interaction/gestures (раздел Predictive back: совместимы bottom sheet, navigation bar, navigation rail, search bar, side sheet)
- https://m3.material.io/components/bottom-sheets/guidelines, …/side-sheets/guidelines, …/search/guidelines
- https://m3.material.io/styles/shape/overview-principles, …/corner-radius-scale, …/shape-morph
- https://m3.material.io/styles/typography/fonts, …/type-scale-tokens
- https://m3.material.io/components/sliders/specs
- https://developer.android.com/design/ui/mobile/guides/patterns/predictive-back
- https://github.com/google/fonts/tree/main/ofl/googlesansflex, https://github.com/google/fonts/tree/main/ofl/robotoflex, https://github.com/googlefonts/googlesans-flex
- Maven: `dl.google.com/android/maven2/androidx/{graphics/graphics-shapes,compose/material3/material3}/maven-metadata.xml`, `com/google/android/material/material`
