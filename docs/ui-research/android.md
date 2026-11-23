# Android System UI + Launcher: как это выглядит и двигается (состояние на 10.11.2026)

Заметки для мокапа оболочки НаомиФон (Pixel 5, 393×851 dp, 440 dpi = 2,75 px/dp, 90 Гц).

## Легенда достоверности

- **VERIFIED-CODE**: прочитано в исходниках AOSP, ветка `android17-release` (тег `android-17.0.0_r1`,
  это Android 17, июнь 2026). Ссылки ведут на конкретные файлы. Это значения AOSP: Pixel может
  переопределять их своими оверлеями (SystemUIGoogle и Pixel Launcher закрыты).
- **VERIFIED-DOC**: официальная документация Google (developer.android.com, source.android.com,
  support.google.com, blog.google).
- **PRESS**: 9to5Google, Android Authority и т. п. «(поиск)» значит, что факт взят из выдачи поиска,
  а саму статью я не открывал.
- **UNCERTAIN**: не подтверждено или противоречиво.

**Важно про исходники.** С 2026 года Google публикует исходники AOSP только дважды в год, во 2-м и
4-м кварталах (PRESS, P8). На source.android.com написано, что `android-latest-release` указывает на
`android17-release` (VERIFIED-DOC, O4). Поэтому **кода Android 17 QPR1 (сентябрь 2026) в AOSP нет**.
Всё, что поменялось в 17 QPR1 (например, «пружинистое» открытие Quick Settings), известно только из
прессы. В код Android 17 уже вошли все изменения 16 QPR1–QPR3.

Ссылки вида [C12] указывают на файлы кода, [O3] на официальные документы, [P5] на прессу; список в конце.

---

## 0. Какая версия сейчас последняя

| Версия | Стабильный выпуск | Что важно для System UI | Источник |
|---|---|---|---|
| Android 16 | 10.06.2025 | Material 3 Expressive (M3E) ещё не в системе; авто-группировка уведомлений | O11, P2 |
| **16 QPR1** | 03.09.2025 (Pixel Drop) | **Главная M3E-переделка**: QS с плитками разного размера и сменой формы, размытие шторки, новые иконки строки состояния (батарея с % внутри, сегментные сигналы, мобильная сеть и Wi-Fi поменялись местами), часы на блокировке с «Dynamic clock», компактные уведомления на блокировке, редизайн Settings, громкости и ползунка яркости, ящик приложений в виде листа, At a Glance меньше, «магнитное» смахивание уведомлений | P2, P3, P6, O7 |
| 16 QPR2 | начало дек. 2025 | 5 форм иконок, авто-тематические иконки, виджеты на блокировке телефона (бета), «Reduce blur», расширенная тёмная тема, стрелки в медиа-карусели | P4 |
| 16 QPR3 | 03.03.2026 | Оконный режим на внешнем дисплее; индикатор геолокации в виде синей «пилюли»; ползунок силы фонарика (PRESS) | P5, P12 (поиск) |
| Android 17 | 16.06.2026 | Плитка Internet снова разделена на Wi-Fi и Mobile data; Live Updates в стиле «Metric»; размытие в выборе виджетов; на домашнем экране можно скрыть подписи; поисковая строка стала виджетом Google | P5, P11 (поиск), C35 |
| **17 QPR1** | **15.09.2026 — последний стабильный** | Пружинистое открытие QS, кнопка правки QS справа внизу, больше размытия, полупрозрачная громкость с Dynamic Color, обновлённое меню питания с размытием фона, медиа-плеер в виде M3-карусели, меню долгого нажатия на иконку разделено на Shortcuts и Actions, анимация камеры с экрана блокировки | P1, O1, O9 |
| 17 QPR2 | бета; Beta 7.1 вышла 16.10.2026, стабильная ожидается в декабре 2026 | (в бете) выбор видимых иконок строки состояния, перестановка раскладки QS и др. | O2, P5 |
| 17 QPR3 | идёт Beta 1 | — | O3 |

**Ответ:** последняя стабильная версия на 10.11.2026 — **Android 17 QPR1** (сентябрьский Pixel Drop
2026, финальная бета Beta 9, сборка CP31.260623.012) [O1, P1]. Последний опубликованный код AOSP —
Android 17 (`android-17.0.0_r1`).

**Dual Shade** (уведомления тянутся слева, QS справа) **UNCERTAIN / на телефонах, видимо, не выпущен.**
В коде AOSP 17 он есть: `config_enableTwoFingerSwipeDownShade`, переходы `ToNotificationsShadeTransition`,
`ToQuickSettingsShadeTransition`, радиус панели `overlay_shade_panel_shape_radius 46dp` [C29, C30, C35].
Пресса знает о нём только по утечкам: настройка «Notifications & Quick Settings: Separate / Combined
(classic)» [P10 (поиск)], а в списке Android Authority он значится как «not yet shipped» [P5].

### 0.1 Геометрия Pixel 5, взятая из device tree (VERIFIED-CODE [C52])
- Плотность `TARGET_SCREEN_DENSITY := 440`, то есть 2,75 px/dp; экран 1080×2340 px ≈ 392,7×850,9 dp.
- Вырез под камеру `config_mainBuiltInDisplayCutout`: `M 41,77 a 40,40 0 1 0 80,0 …` — круг радиусом 40 px
  с центром (81, 77) px, то есть **центр (29,5; 28,0) dp, радиус 14,5 dp**, прижат влево (`@left`).
  Прямоугольная аппроксимация выреза 145×136 px.
- **Высота строки состояния в портрете: `status_bar_height_portrait 136px` ≈ 49,5 dp** (равна нижней
  границе выреза), `status_bar_height_default 28dp`. В AOSP по умолчанию 24 dp [C13].
- Скругление углов экрана: `rounded_corner_radius 108px` (≈39,3 dp), поправка `8px`. Эффективный
  радиус окна ≈ 100 px ≈ **36 dp** — это моя оценка формулы `ScreenDecorationsUtils` (UNCERTAIN).

---

## 1. Шторка уведомлений и Quick Settings

### 1.1 Стадии вытягивания (VERIFIED-CODE, AOSP 17; Compose-реализация QS [C36, C53, C35])
- **Первое вытягивание (QQS):** верхняя полоса (часы, дата, иконки), затем **2 ряда плиток**
  (`QuickQuickSettingsRowInteractor.defaultRows = 2`) сетки из **4 колонок**
  (`quick_settings_infinite_grid_num_columns 4`), то есть до 8 маленьких или 4 больших плиток; ниже
  медиа-плеер, ещё ниже уведомления. **Ползунка яркости в QQS нет**: в `QuickQuickSettingsLayout`
  только `tiles()` и `media()`, отступ между ними `qs_tile_margin_vertical 8dp`.
- **Второе вытягивание (полные QS):** сверху **ползунок яркости**, под ним страницы плиток
  (4 колонки × `quick_settings_paginated_grid_num_rows 4` ряда, то есть до 16 маленьких плиток на
  странице), под ними медиа, внизу полоса кнопок (footer). Индикатор страниц: `qs_page_indicator 16×8dp`, точка 6,4 dp [C12].
- **Переход QQS → QS** (`quickQuickSettingsToQuickSettings` [C36]) идёт по прогрессу жеста. Плитки
  QQS — общие элементы: они переезжают и растягиваются в свои места в полной сетке. Остальное
  содержимое QS проявляется (fade) начиная с 43 % прогресса, footer — с 90 %. Плитки QQS гаснут в
  диапазоне 0–50 %.
- PRESS [P2]: в 16 QPR1 максимальная сетка выросла с 8 (2×4) до 16 (4×4) плиток, а первое
  вытягивание показывает до 8 плиток. PRESS [P1, P7 (поиск)]: в 17 QPR1 у панели QS при открытии
  появилась «bouncy» анимация, как у шторки уведомлений; первое вытягивание пружинит заметнее.
  Цифр нет, потому что кода QPR1 нет.
- UNCERTAIN: возможно, «пружина» реализована через `VerticalExpandContainerSpec` [C40]. В AOSP 17
  он используется для оверлейных шторок (Dual Shade). Как он устроен: первые **80 dp** панель идёт
  за пальцем с коэффициентом **0,25** (`PreDetachRatio`), затем «отрывается» и догоняет палец
  пружиной **k=380, ζ=0,9**. Обратно прилипает на высоте 40 dp. Прозрачность становится 1 после 24 dp
  (пружина k=1200, ζ=0,99). Ширина по краям сжата на 28 dp и расширяется к 80 dp. Радиус 46 dp (минимум 28 dp).
- Длительности переходов сцен (если в сборке включён scene container; включён ли он на Pixel —
  UNCERTAIN) [C29]: открытие шторки `toSingleShadeTransition` **500 мс**; шторка → QS **500 мс**,
  footer гаснет за первые 83 мс; оверлейные шторки (Dual Shade) **300 мс**.

### 1.2 Плитки (VERIFIED-CODE [C30, C31, C32, C33, C34])
| Параметр | Значение |
|---|---|
| Высота плитки | **72 dp** (`common_tile_default_tile_height`) |
| Зазор между плитками | 8 dp по горизонтали и вертикали (`qs_tile_margin_horizontal/vertical`) [C12] |
| Большая плитка | 2 колонки из 4: иконка + подпись + вторая строка. Иконка **28 dp**, отступ слева 8 dp, справа 12 dp, между элементами 6 dp |
| Маленькая плитка | 1 колонка, только иконка **32 dp** |
| Скругление ВКЛ | **24 dp** (`common_tile_default_active_tile_corner_radius`), фон иконки 16 dp |
| Скругление ВЫКЛ / недоступна | **50 dp**, то есть полная «пилюля» (`…inactive_tile_corner_radius`) |
| Смена формы | `animateDpAsState(24↔50dp)`: Compose-пружина по умолчанию, **k=1500 (StiffnessMedium), ζ=1** (это значения Compose `spring()` по умолчанию, VERIFIED по API, не по коду SystemUI); цвет — `animateColorAsState` |
| Цвета ВКЛ | фон `primary`, текст и иконка `onPrimary` |
| Цвета ВЫКЛ | фон `surfaceEffect1` (полупрозрачная поверхность), текст и иконка `onSurface` |
| Недоступна | фон `surface` с альфой 0,18; текст и иконка `onSurfaceVariant` с альфой 0,38 |
| Плитка с двумя зонами нажатия (BT, Modes, Wi-Fi в 17) | фон плитки `surfaceEffect1`, у включённой окрашен только круг иконки (`primary`); зона нажатия 56 dp |
| Отклик на нажатие («bounce») | плитка **расширяется на 8 dp** в сторону соседей, соседи в ряду на столько же сужаются (`BounceableImpl(8.dp)`, `spring()` по умолчанию), минимум 75 % амплитуды даже при быстром тапе. Затем контент: иконка ×**1,1** (у маленьких плиток) или текст ×**1,06** (у больших), keyframes **334 мс**: пик на 167 мс с изингом (0,05, 0, 0, 1), возврат с изингом (1, 0, 0,95, 1) |

PRESS [P2, P3, P6]: в режиме правки плитки меняют размер (2×1 ↔ 1×1), в редакторе есть категории и
кнопка Reset. Выключенные плитки «стеклянные, почти овальные», включённые «квадратнее» и в
Dynamic Color. Плитки по умолчанию в AOSP 17 после разделения Internet: `wifi,bt,cell,cast,dnd,
wallet,airplane,rotation,flashlight,battery,alarm,controls,screenrecord`, большие из них `wifi, bt,
cell, cast, dnd, wallet` [C35].

### 1.3 Ползунок яркости (VERIFIED-CODE [C38])
M3E-слайдер: трек **40 dp** высотой, скругление трека 12 dp, на стыке с ползунком 2 dp; ползунок —
**вертикальная палочка 4×52 dp**, зазор от трека 6 dp; иконка 28 dp внутри трека; рамка вокруг
скруглена на 24 dp. Иконка появляется за 100 мс после задержки 33 мс и исчезает за 50 мс.
PRESS [P3, P6]: «менее пузатый» дизайн, «огромная перетаскиваемая линия».
Ползунок громкости в самой шторке в AOSP есть только для оверлейной раскладки (`overlay_qs_layout_volume_*` [C30]);
есть ли он в обычной шторке Pixel — UNCERTAIN.

### 1.4 Нижняя полоса QS (footer) (VERIFIED-CODE [C37])
Слева текстовые «чипы» (управляемое устройство, фоновые службы), они занимают оставшуюся ширину.
Справа **круглые кнопки: выбор пользователя, Settings, Power**; высота 40 dp, иконки 20 dp.
Кнопка правки: PRESS [P1] — в 17 QPR1 значок правки в правом нижнем углу.
Отступы footer `qs_footer_actions_top_padding 8dp`, `bottom 4dp` [C12].

### 1.5 Медиа-плеер
VERIFIED-CODE [C12]: внутренние отступы 16 dp, скругление обложки 14 dp, кнопка play/pause шириной 72 dp, иконка приложения 24 dp.
PRESS [P2]: пауза — прямоугольник, play — пилюля. В 16 QPR2 появились стрелки между сессиями [P4].
В 17 QPR1 — M3-карусель: соседние сессии видны сбоку, текущая карточка сжимается [P1].

### 1.6 Уведомления в шторке (VERIFIED-CODE [C12, C39])
- Углы карточки **28 dp** (`notification_corner_radius`). У соседних карточек внутренние углы
  **4 dp** (`notification_corner_radius_small`), между карточками **2 dp** (`notification_divider_height`).
  Получается «стопка» со скруглёнными краями. Между секциями 16 dp, на блокировке 4 dp. Поля по бокам 16 dp.
- Полка (shelf) 48 dp. Heads-up: максимум 136 dp, въезжает с 32 dp над экраном, отступ от строки
  состояния 8 dp. Скругление скрима шторки 32 dp. Размытие окна шторки до **34 dp**
  (`max_shade_window_blur_radius`), размытие содержимого до 23 px.
- **«Магнитное» смахивание** (16 QPR1, [C39]): пока палец не ушёл на **72 dp**, карточка едет со
  скоростью **0,5×** пальца, соседи ±1 — 0,12×, ±2 — 0,04×. Углы соседей скругляются, до 0,8 от
  полного радиуса. После 72 dp карточка «отрывается» и пружиной **k=800, ζ=0,95** догоняет палец,
  а соседи возвращаются на место пружиной **k=550, ζ=0,6** (с отскоком ~9,5 %). Если вернуть палец
  ближе 56 dp, карточка прилипает обратно пружиной k=850, ζ=0,95. Скорость смахивания 500 dp/с.
  На каждом пороге — хаптика. Официальное описание: соседи «subtly respond to your drag» [O7].
- Группировка: Android 16 сам группирует уведомления приложения, если их 4 и больше без группы [O11].
  Группа визуально — одна карточка, дети внутри через 8 dp (`notification_children_padding`) [C12].

---

## 2. Строка состояния

- **Порядок системных иконок (слева направо) — VERIFIED-CODE [C41]:** `… alarm_clock, rotate,
  headset, data_saver, … bluetooth, camera, microphone, location, mute, volume, zen, screen_record,
  cast, …, hotspot, stacked_mobile, mobile, wifi, airplane, battery, sensors_off`. В Android 16 было
  `wifi, …, mobile, airplane, battery`; **в 16 QPR1 мобильная сеть стала левее Wi-Fi** (сравнение
  веток `android16-release` и `android16-qpr1-release`) [C41]. Слева: часы, за ними иконки
  уведомлений и «чипы» Live Updates и звонка (PRESS [P3]).
- Часы: **14 sp**, шрифт `variable-label-large-emphasized` [C12, C42]; отступы по 4 dp.
  PRESS [P3]: «крупнее и жирнее» с 16 QPR1.
- Иконки: системные рисуются **15 dp**, контейнер 22 sp; отступы строки 4 dp слева и справа;
  между сигналом и батареей 3 sp [C12, C13].
- **Батарея:** горизонтальная **20,6×12 sp, процент внутри** (`status_bar_battery_unified_icon_*`)
  [C12]. Вышла в **16 QPR1** (PRESS [P13 (поиск), P6]): красная при низком заряде, зелёная при
  зарядке, молния или «+» рядом.
- Wi-Fi и сотовая связь: сегментные иконки, состояние «нет данных» — PRESS [P3]. Цифры — UNCERTAIN.
- **Точка приватности:** диаметр **6 dp**, зелёная `#3ddc84` для камеры и микрофона; для геолокации
  синяя пилюля `#4e8ff8` [C12]. Синяя пилюля появилась в 16 QPR3 (PRESS [P5]). Чипы приложений
  (privacy chip): высота 24 sp, радиус 28 dp, ширина 56–76 dp. Чипы Live Updates: радиус 28 dp,
  иконка 16 dp, текст 12–74 dp.
- Высота: AOSP 24 dp; **Pixel 5 — 136 px ≈ 49,5 dp** (см. 0.1).

---

## 3. Экран блокировки

- **Часы:** большие — **150 dp** (`large_clock_text_size`), маленькие — 86 dp; высота блока
  маленьких 114 dp, отступ сверху 28 dp, слева 28 dp; дата и погода — блок 48 dp, умный блок 104 dp
  [C43]. VERIFIED-DOC [O8]: настройка «**Dynamic clock**» меняет размер часов в зависимости от
  содержимого экрана; есть стили со слайдером ширины, выбор цвета и переключатель Large.
  PRESS [P2]: дата и погода стоят под большими часами, а когда есть уведомления — справа от них.
  Начертание Rounded или Sharp — PRESS [P3].
- **Смена больших часов на маленькие** (VERIFIED-CODE [C44]): старые гаснут за 133 мс линейно,
  новые проявляются за 167 мс после задержки 133 мс (`LINEAR_OUT_SLOW_IN`). Блок даты и погоды едет
  с изингом `EMPHASIZED`: вниз 467 мс, вверх 967 мс. Сдвиг часов 14 dp.
- **Уведомления на блокировке:** «Compact View» или «Full list» (VERIFIED-DOC [O8]). Compact — одно
  уведомление плюс полка с иконками остальных (PRESS [P14 (поиск)]). Есть опции «скрывать
  просмотренные» и «скрывать беззвучные» [P6].
- **Ярлыки внизу:** в AOSP круг **48 dp**, 16 dp от края и 32 dp от низа; сработка по долгому
  нажатию, при коротком тапе встряска на 8 dp [C12]. Варианты на Pixel: DND, Home, QR, Wallet,
  Camera, Flashlight, Mute, Video camera [O8]. Размытие за ярлыками 15 dp есть в коде [C12];
  в 17 QPR2-бете оно включено (PRESS [P5]).
- **Разблокировка свайпом вверх:**
  - Путь через scene container [C29]: за 500 мс, в первые 30 % с изингом PredictiveBack, гаснут
    верхняя часть, иконка замка и нижняя часть; верхняя часть (часы) уезжает **на −48 dp**,
    стопка уведомлений **на −72 dp**.
  - Старый путь `KeyguardUnlockAnimationController` [C28]: окно под блокировкой (лаунчер или
    приложение) растёт со **0,95 до 1** с опорой в точке (W/2; 0,66·H) и поднимается **с +5 % высоты**.
    При свайпе это окно проявляется за 175 мс, когда «dismiss amount» ≥ 0,15, а блокировка
    снимается при 0,3. «Консервированная» анимация (отпечаток, PIN): 300 мс + 25 мс задержки.
    Анимация иконок внутри лаунчера — 633 мс. Если обои разные, обои блокировки гаснут за 150 мс
    после задержки 150 мс.
  - Какой путь включён на Pixel — UNCERTAIN.
  - Длительности переходов keyguard [C45]: LOCKSCREEN→GONE 633 мс, LOCKSCREEN→AOD 500, AOD→LOCKSCREEN 500, GONE→AOD 1300.
- **AOD:** при переходе GONE→AOD элементы въезжают с −100 dp [C12]. Размытые обои на AOD — только
  Pixel 10 Pro / Pro XL [O8]; по прессе с 16 QPR2 [P5].
- **Виджеты на блокировке:** в 16 QPR2 для телефонов (бета), открываются свайпом влево, до 3 виджетов
  на странице (PRESS [P4]).
- PRESS [P1]: в 17 QPR1 новая анимация запуска камеры двойным нажатием питания.

---

## 4. Главный экран, лаунчер, жесты

### 4.1 Раскладка
- Pixel Launcher закрыт; ниже AOSP Launcher3 и пресса.
- PRESS [P9, P2]: At a Glance уменьшили в 16 QPR1, из-за этого появился лишний ряд иконок. Сетки
  теперь называются Small / Medium / Large / XL вместо «5×5»/«4×5». Поисковая строка — пилюля с
  кругом AI Mode справа. Ящик приложений — **плавающий лист**, а не весь экран.
  Точные размеры сетки (4×6?) — UNCERTAIN.
- **Формы иконок** (16 QPR2, PRESS [P4]; пути VERIFIED-CODE [C27], viewBox 100×100):
  - circle: `M50 0A50 50,0,1,1,50 100A50 50,0,1,1,50 0`
  - square (скругл. квадрат, радиус папки 1/3, shapeRadius 17,33): `M53.689 0.82 L53.689 .82 C67.434 .82 74.306 .82 79.758 2.978 87.649 6.103 93.897 12.351 97.022 20.242 99.18 25.694 99.18 32.566 99.18 46.311 V53.689 C99.18 67.434 99.18 74.306 97.022 79.758 93.897 87.649 87.649 93.897 79.758 97.022 74.306 99.18 67.434 99.18 53.689 99.18 H46.311 C32.566 99.18 25.694 99.18 20.242 97.022 12.351 93.897 6.103 87.649 2.978 79.758 .82 74.306 .82 67.434 .82 53.689 L.82 46.311 C.82 32.566 .82 25.694 2.978 20.242 6.103 12.351 12.351 6.103 20.242 2.978 25.694 .82 32.566 .82 46.311 .82Z`
  - 4-sided cookie (shapeRadius 13,5): `M39.888,4.517C46.338 7.319 53.662 7.319 60.112 4.517L63.605 3C84.733 -6.176 106.176 15.268 97 36.395L95.483 39.888C92.681 46.338 92.681 53.662 95.483 60.112L97 63.605C106.176 84.732 84.733 106.176 63.605 97L60.112 95.483C53.662 92.681 46.338 92.681 39.888 95.483L36.395 97C15.267 106.176 -6.176 84.732 3 63.605L4.517 60.112C7.319 53.662 7.319 46.338 4.517 39.888L3 36.395C -6.176 15.268 15.267 -6.176 36.395 3Z`
  - 7-sided cookie: длинный путь, см. [C27] (`SEVEN_SIDED_COOKIE_PATH`).
  - arch (снизу почти прямоугольник, shapeRadius 7,8, радиус папки 1/4): `M50 0C77.614 0 100 22.386 100 50C100 85.471 100 86.476 99.9 87.321 99.116 93.916 93.916 99.116 87.321 99.9 86.476 100 85.471 100 83.46 100H16.54C14.529 100 13.524 100 12.679 99.9 6.084 99.116 .884 93.916 .1 87.321 0 86.476 0 85.471 0 83.46L0 50C0 22.386 22.386 0 50 0Z`
  - Форма действует только на домашнем экране и в папках (PRESS [P4]). Авто-тематические иконки
    (монохромный фильтр) — 16 QPR2 [P4].
- PRESS [P5, P1]: в 17 можно скрыть подписи под иконками. В 17 QPR1 меню долгого нажатия разделено
  на Shortcuts и системные Actions; меню на пустом месте экрана без промежутков, с более круглыми углами.
- **Полоска жестов:** **108×4 dp** (радиус 2 dp), **10 dp от низа** [C12]; инсет навигации в
  жестовом режиме 24 dp, рамка 48 dp, зона жеста снизу 32 dp, зона «назад» по краю 30 dp [C14].

### 4.2 Запуск приложения: иконка превращается в окно (VERIFIED-CODE [C18, C19, C20, C26])
- **500 мс** (`APP_LAUNCH_DURATION`). Позиция Y, размер и обрезка идут с изингом **emphasized**:
  путь `M0,0 C0.05,0 0.133333,0.06 0.166666,0.4 C0.208333,0.82 0.25,1 1,1`. Позиция X — отдельный
  путь `app_open_x`: `M0,0 C0.1217,0.0462 0.15,0.4686 0.1667,0.66 C0.1834,0.8878 0.1667,1 1,1`.
- Окно стартует из прямоугольника иконки. Радиус углов идёт от **круга** (max(w,h)/2) до радиуса углов окна.
- Иконка гаснет за **50 мс**, начиная с 25 мс.
- Домашний экран (рабочий стол и док) уменьшается **1 → 0,97** за **350 мс** (`DECELERATE_1_5`).
  Если запуск из ящика: ящик гаснет за 217 мс линейно и масштабируется за 350 мс с изингом (0,2, 0, 0, 1).
- За окном нарастает размытие **0 → 30 dp** (`max_depth_blur_radius_enhanced`) плюс скрим,
  `DECELERATE_1_5` (`config_enableAppLaunchBlur=true`).
- Полоска жестов гаснет за 133 мс с изингом (0,2, 0, 1, 1) и проявляется в последние 266 мс с (0, 0, 0, 1).
  Строка состояния переключается за 120 мс с задержкой 96 мс до конца.
- Запуск из Recents — 336 мс; обычное закрытие окна без жеста — 250 мс.

### 4.3 Закрытие свайпом вверх: окно за пальцем, уменьшается и влетает в иконку
- Пока палец тянет [C55]: окно уменьшается к размеру карточки Recents. Прогресс = смещение /
  `transitionDragLength` (длина, на которой окно доходит до размера карточки); дальше можно
  дотянуть до высоты экрана.
- Решение при отпускании [C23, C24, C25]:
  - вертикальный «бросок» быстрее **0,5 dp/мс** (500 dp/с, `quickstep_fling_threshold_speed`) вверх → **HOME**;
  - палец остановился → **RECENTS**;
  - медленный свайп вверх без паузы → HOME;
  - горизонтальный бросок или прокрутка к другой задаче → **NEW_TASK** (quick switch).
- **Пауза** (`MotionPauseDetector`): нет движения **300 мс** — значит, пауза. Пороги скорости:
  very slow 0,0285, slow 0,15, somewhat fast 0,285, fast 1,4 dp/мс; резкое торможение — 0,6 от
  прежней скорости; окно должно уйти минимум на 36 dp от исходного места.
  `MIN_PROGRESS_FOR_OVERVIEW 0.7`, присоединение к Recents 300 мс.
- **Полёт в иконку** (`RectFSpringAnim`, [C21, C16]) — три независимые пружины:
  **X k=450, ζ=0,965**; **Y k=400, ζ=0,95**; масштаб (прогресс прямоугольника) **k=500, ζ=0,99**.
  Начальная скорость берётся у пальца и зажимается: X 300–500 px/с, Y 2000–5000 px/с (с мягким
  ограничением, коэффициент 1,4). Если иконка в доке — прямоугольник ведётся по центру, если в
  верхней трети экрана — по верхнему краю.
- **Проявление домашнего экрана** (`ScalingWorkspaceRevealAnim` [C22]; в Android 17 используется
  всегда [C54]): рабочий стол и док **0,85 → 1,0** за **1000 мс** по своей кривой
  `M0,0 C0.045,0.0356 0.0975,0.2055 0.15,0.3952 C0.235,0.6855 0.235,1 1,1`; прозрачность 0 → 1 за
  первые **200 мс**; размытие 30 dp → 0 с изингом EMPHASIZED за первые 66,6 % (≈ 666 мс); глубина
  обоев — по той же кривой масштаба.

### 4.4 Recents и быстрое переключение
- Карточки: поля 16 dp; панель действий 48 dp высотой, отступ сверху 24 dp, кнопки скруглены на 28 dp
  [C25]. Чип приложения над карточкой: свёрнут 156×36 dp, развёрнут 216×52 dp (PRESS [P3] —
  «пилюля» с выпадающим меню, 16 QPR1).
- Смахивание карточки, «expressive»: Y k=850, ζ=0,65; X k=2800, ζ=0,8; эффекты k=1600, ζ=1 [C17].
- Быстрое переключение вдоль полоски: порог масштабирования при прокрутке 100 dp,
  `MAX_QUICK_SWITCH_RECENTS_SCALE_PROGRESS 0.07` [C23, C25].
- Ящик приложений (свайп вверх по домашнему экрану): вертикальный прогресс с изингом EMPHASIZED,
  рабочий стол уменьшается по `DECELERATED_EASE`; возврат при отмене 200 мс (AOSP Launcher3).

---

## 5. Жест «Назад» (predictive back) — VERIFIED-CODE

Системные анимации (домой, между Activity, между задачами) работают с Android 15 у приложений,
которые включили predictive back [O6].

### 5.1 Прогресс жеста [C7, C8, C9, C13, C14]
- Ширина зоны у края: **30 dp** (`config_backGestureInset`, умножается на чувствительность).
  Нижние 32 dp (`navigation_bar_gesture_height`) — зона жеста «домой». Touch slop ×0,75.
- `progress = dx / currentTarget`, где `linearDistance = min(ширина экрана, 412 dp)`,
  `maxDistance = ширина экрана`, нелинейная часть 0,2. **На Pixel 5 (393 dp < 412 dp) прогресс
  просто линейный: dx / 393 dp.**
- Прогресс сглаживается пружиной **k=1500, ζ=1** (`BackProgressAnimator`), затем проходит кривую
  **BACK_GESTURE = cubic-bezier(0,1, 0,1, 0, 1)**.
- В последних 20 % после отпускания (`UPDATE_SYSUI_FLAGS_THRESHOLD 0.2`) строка состояния переключается на вид следующего окна.

### 5.2 Стрелка «назад» [C10, C11, C12]
- Срабатывает после **16 dp** движения (`navigation_edge_action_drag_threshold`); если повести
  обратно на **32 dp**, отменяется (так же 32 dp на повторную активацию).
- Стрелка рисуется линией толщиной **4 dp** внутри «подушки», которая растёт от края:
  - вход: ширина 0 → , высота 48 dp, углы 6 dp, отступ 4 dp, масштаб 0,98;
  - до порога: 51×46 dp, угол у края 16 dp, дальний угол 20 dp, стрелка 8×5,6 dp;
  - **после порога: круг 48×48 dp (углы 24 dp), 14 dp от края**, стрелка 6,4×7,2 dp;
  - полностью растянута: ширина 60 dp, 18 dp от края, стрелка 5,6×8 dp;
  - фиксация: масштаб **0,86** (пружина 5700/1), фон гаснет (1400/1).
- Пружины (k/ζ): горизонталь при входе 800/0,76; в активном состоянии 1000/0,8; масштаб 325/0,55;
  ширина фона 850/0,75; дальний угол 1200/0,30; длина и высота стрелки при активации 1500/0,29
  («щелчок»). Прозрачность стрелки ступенчатая: порог 0,165, пружины 180/0,9 и 2000/0,6.
- Стрелка держится минимум на 64 dp от верха; панель 105×268 dp.

### 5.3 Назад на домашний экран (Launcher, `LauncherBackAnimationController` [C15, C16, C17])
- Окно приложения уменьшается **1 → 0,75** по прогрессу.
- Если жест с левого края, окно прижимается вправо с отступом `p·10dp`, с правого края — наоборот.
  По Y окно идёт за пальцем (DecelerateInterpolator, предел — половина высоты экрана, поле 10 dp).
- Углы: радиус окна → **40 dp** (`swipe_back_window_corner_radius`).
- Домашний экран за окном сразу ставится в масштаб **0,85**. Поверх него чёрный скрим с альфой
  **0,8 в тёмной теме и 0,2 в светлой**, размытие **30 dp → 10 px** по прогрессу.
- После фиксации окно уходит обычной анимацией «окно → иконка» от текущего прямоугольника и радиуса;
  скрим и размытие гаснут за **233 мс**. UNCERTAIN: вызывает ли при этом путь back
  `ScalingWorkspaceRevealAnim` 0,85 → 1, я не проверял.

### 5.4 Назад между Activity (`DefaultCrossActivityBackAnimation` [C1, C2, C4, C5])
- **Закрываемое окно:** масштаб **1 → 0,85** от центра. При жесте с левого края оно ещё сдвигается
  вправо, пока правый край не встанет в **8 dp** от края экрана. На Pixel 5 это сдвиг ≈ 21,5 dp от центра.
- **Предыдущее окно:** стартует левее на **max(0,15·W, 96 dp)** — на Pixel 5 это 96 dp — и
  масштабируется вместе с закрываемым до 0,85 (начальный масштаб ≤ 0,95). По Y оба идут за пальцем,
  с полем 8 dp.
- Скрим над предыдущим окном: 0,8 в тёмной теме и 0,2 в светлой; углы = радиус углов окна; за окнами фон.
- **После отпускания: 450 мс, изинг EMPHASIZED.** Предыдущее окно разворачивается на весь экран.
  Закрываемое уезжает вправо ещё на 96 dp и гаснет по `alpha = max(1 − 5t, 0)`, то есть исчезает
  за первые 90 мс. Скорость броска добавляет «дожим» масштаба пружиной **k=200 (LOW), ζ=0,75**;
  для жестов от края скорость ×2.

### 5.5 Назад между задачами (`CrossTaskBackAnimation` [C3, C4])
- Оба окна уменьшаются **1 → 0,8**. Предыдущая задача стоит **слева, через 14 dp**
  (`cross_task_back_inter_window_margin`). При жесте с левого края правый край текущего окна
  отходит на `p·8dp`; по Y окна идут за пальцем (поле 8 dp).
- После отпускания: **500 мс, EMPHASIZED**. Входящее окно разворачивается на весь экран, уходящее
  встаёт по центру в масштабе 0,8 позади него. Пружина броска **k=320, ζ=1**, скорость по умолчанию 320.

### 5.6 Назад внутри приложения (Material, рекомендации) [O5]
- Полноэкранная поверхность: уходящий экран **100 → 90 %**, входящий **110 → 100 %**,
  смена через fade-through на **35 %** прогресса. Кривая из гайда (0,1, 0,1, 0, 1).
- Общий элемент: масштаб до 90 %, сдвиг по X `(W/20 − 8) dp` (поле 8 dp), сдвиг по Y
  `(H/20 − 8) dp`; прогресс прогонять через STANDARD_DECELERATE (0, 0, 0, 1). При броске сначала
  «добирается» превью, потом идёт анимация фиксации. При отмене поверхность быстро возвращается в
  полный размер. Компоненты MDC (BottomSheet, SideSheet, Search) делают это сами начиная с 1.10.0.

---

## 6. Приложение Settings

- **Редизайн M3E в 16 QPR1** (PRESS [P2, P3]): цветные иконки разделов, каждый пункт — отдельная
  карточка, у пунктов с подстраницей стрелка, заголовок страницы сверху; толще полосы и ползунки, в
  переключателях галочка или крестик.
- **Карточки** (VERIFIED-CODE [C46]): отступ от краёв экрана **16 dp**, текст начинается в 32 dp от
  края; цвет `surfaceBright`. Одиночная карточка — углы **20 dp**. Группа: у первой верхние углы
  20 dp, нижние 4 dp; у средних все 4 dp; у последней наоборот. Зазор между карточками группы —
  UNCERTAIN, вероятно 2 dp (`settingslib_expressive_space_extrasmall1`).
- **Большой сворачивающийся заголовок** (VERIFIED-CODE [C47]): развёрнутый текст **36 dp**, свёрнутый
  22 dp; поля 24 dp по бокам и 32 dp снизу; высота блока 179 dp; при сворачивании заголовок гаснет
  (`titleCollapseMode fade`), скрим за 50 мс, до 3 строк.
- **Переход между страницами:** `SettingsTransitionHelper` в AOSP 13–17 пустой [C48]. Settings
  использует **системный переход между Activity** [C49]:
  - открытие: новая страница въезжает **с +96 dp** за 450 мс с изингом `fast_out_extra_slow_in`
    (тот же путь, что emphasized) и проявляется за 83 мс после задержки 50 мс; старая уезжает на
    −96 dp и не гаснет; края экрана «дотягиваются» (`<extend>`);
  - закрытие — зеркально (уходящая страница уезжает на +96 dp и гаснет за 83 мс после задержки 35 мс).
  - Это **не** Material shared-axis с проявлением через ноль, а «сдвиг на 96 dp + быстрое проявление».
- **Predictive back между страницами:** Settings включил `enableOnBackInvokedCallback="true"` на
  уровне приложения [C50]. Значит, работает системная анимация между Activity из 5.4
  (0,85, 96 dp, 450 мс).

---

## 7. Громкость и меню питания

- **Панель громкости** (VERIFIED-CODE [C12]): вертикальная, ширина 60 dp, углы фона **30 dp**,
  слайдер **52×254 dp**, кнопки режима звонка 48 dp (фон выбранной скруглён на 20 dp), окно в 12 dp
  от края, размытие поверхности 23 dp; в альбомной ориентации горизонтальный слайдер 296×56 dp.
  PRESS: в 16 QPR1 вкладки ползунков, точка-метка максимума, «менее пузато», волна в слайдере при
  воспроизведении [P6, P2]. В 17 QPR1 полупрозрачность с Dynamic Color [P1].
- **Меню питания** (VERIFIED-CODE [C12]): углы 28 dp, размытие **34 dp**, плитки сетки 64 dp
  (элемент 98 dp). PRESS [P1]: в 17 QPR1 меню обновлено и получило размытие фона.

---

## 8. Сводка для мокапа

### 8.1 Изинги
| Имя | Кривая |
|---|---|
| emphasized (EMPHASIZED, fast_out_extra_slow_in) | `M0,0 C0.05,0 0.133333,0.06 0.166666,0.4 C0.208333,0.82 0.25,1 1,1` (два куска; в CSS — через `linear()` с выборкой, приближённо `cubic-bezier(0.2,0,0,1)` — UNCERTAIN) |
| emphasized decelerate | (0,05, 0,7, 0,1, 1) |
| emphasized accelerate | (0,3, 0, 0,8, 0,15) |
| standard | (0,2, 0, 0, 1) |
| standard decelerate | (0, 0, 0, 1) |
| BACK_GESTURE | (0,1, 0,1, 0, 1) |
| AGGRESSIVE_EASE | (0,2, 0, 0, 1) |
| app_open_x | `M0,0 C0.1217,0.0462 0.15,0.4686 0.1667,0.66 C0.1834,0.8878 0.1667,1 1,1` |
| home reveal scale | `M0,0 C0.045,0.0356 0.0975,0.2055 0.15,0.3952 C0.235,0.6855 0.235,1 1,1` |
Источники: [C5, C20, C19, C22, C49].

### 8.2 Пружины Material 3 (androidx, VERIFIED-CODE [C51])
| Схема | spatial default | spatial fast | spatial slow | effects default / fast / slow |
|---|---|---|---|---|
| **Expressive** | **k=380, ζ=0,8** | k=800, ζ=0,6 | k=200, ζ=0,8 | 1600 / 3800 / 800, все ζ=1 |
| Standard | k=700, ζ=0,9 | k=1400, ζ=0,9 | k=300, ζ=0,9 | 1600 / 3800 / 800, ζ=1 |
Константы `SpringForce`: HIGH 10000, MEDIUM 1500, LOW 200, VERY_LOW 50; демпфирование
NO_BOUNCY 1, LOW_BOUNCY 0,75, MEDIUM 0,5, HIGH 0,2.

Масса 1, ω₀ = √k, ωd = ω₀√(1−ζ²); `x(t) = 1 − e^(−ζω₀t)·(cos ωd·t + (ζω₀/ωd)·sin ωd·t)`.
Оценки (не замеры): время установления ≈ 4/(ζω₀), перелёт = e^(−πζ/√(1−ζ²)):
- 380/0,8 → ≈ 260 мс, перелёт 1,5 %;
- 800/0,6 → ≈ 235 мс, перелёт 9,5 %;
- 200/0,8 → ≈ 355 мс, перелёт 1,5 %;
- 1500/1 (без перелёта) → до 99 % ≈ 170 мс;
- 200/0,75 → ≈ 380 мс, перелёт 2,8 %;
- 550/0,6 → ≈ 285 мс, перелёт 9,5 %;
- 850/0,65 → ≈ 210 мс, перелёт 6,8 %;
- 450/0,965, 400/0,95, 500/0,99 → ≈ 180–210 мс, перелёта почти нет.

### 8.3 Главные числа одним списком
- Переходы между Activity: 450 мс, ±96 dp, проявление 83 мс.
- Запуск приложения: 500 мс; иконка гаснет 25–75 мс; дом 1 → 0,97 за 350 мс; размытие 30 dp.
- Свайп домой: пружины X 450/0,965, Y 400/0,95, масштаб 500/0,99; дом 0,85 → 1 за 1000 мс,
  прозрачность за 200 мс; бросок ≥ 500 dp/с; пауза 300 мс.
- Назад домой: окно до 0,75, углы до 40 dp, поле 10 dp, скрим 0,8/0,2, размытие 30 dp, скрим гаснет за 233 мс.
- Назад между Activity: 0,85, поле 8 dp, сдвиг 96 dp, 450 мс; закрываемое гаснет за 90 мс.
- Назад между задачами: 0,8, промежуток 14 dp, 500 мс.
- Стрелка «назад»: срабатывает на 16 dp, круг 48 dp, 14 dp от края, линия 4 dp.
- Плитки QS: 72 dp, зазор 8 dp, 4 колонки; углы 24 dp (вкл.) и 50 dp (выкл.); отклик +8 dp, иконка ×1,1 за 334 мс.
- Уведомления: углы 28 dp, внутренние 4 dp, зазор 2 dp, поля 16 dp; смахивание 0,5× до 72 dp, затем 800/0,95.
- Часы на блокировке: 150 dp и 86 dp; смена 133 мс + 167 мс; сдвиг блока 467/967 мс.
- Разблокировка: окно 0,95 → 1, +5 % высоты, опора на 0,66·H; 300 мс; иконки 633 мс.
- Полоска жестов 108×4 dp, 10 dp от низа; строка состояния Pixel 5 ≈ 49,5 dp; вырез — центр (29,5; 28) dp, радиус 14,5 dp.

---

## Источники

### Код AOSP (VERIFIED-CODE)
Префиксы:
- FB = `https://android.googlesource.com/platform/frameworks/base/+/refs/heads/android17-release/`
- L3 = `https://android.googlesource.com/platform/packages/apps/Launcher3/+/refs/heads/android17-release/`
- LIBS = `https://android.googlesource.com/platform/frameworks/libs/systemui/+/refs/heads/android17-release/`

- C1 FB `libs/WindowManager/Shell/src/com/android/wm/shell/back/CrossActivityBackAnimation.kt`
- C2 FB `libs/WindowManager/Shell/src/com/android/wm/shell/back/DefaultCrossActivityBackAnimation.kt`
- C3 FB `libs/WindowManager/Shell/src/com/android/wm/shell/back/CrossTaskBackAnimation.java`
- C4 FB `libs/WindowManager/Shell/res/values/dimen.xml`
- C5 FB `libs/WindowManager/Shell/shared/src/com/android/wm/shell/shared/animation/Interpolators.java`; FB `core/java/android/view/animation/BackGestureInterpolator.java`
- C7 FB `core/java/android/window/BackTouchTracker.java`
- C8 FB `core/java/android/window/BackProgressAnimator.java`
- C9 FB `packages/SystemUI/src/com/android/systemui/navigationbar/gestural/EdgeBackGestureHandler.java`
- C10 FB `packages/SystemUI/src/com/android/systemui/navigationbar/gestural/EdgePanelParams.kt`
- C11 FB `packages/SystemUI/src/com/android/systemui/navigationbar/gestural/BackPanelController.kt`
- C12 FB `packages/SystemUI/res/values/dimens.xml`
- C13 FB `core/res/res/values/dimens.xml`
- C14 FB `packages/overlays/NavigationBarModeGesturalOverlay/res/values/dimens.xml` и `…/config.xml`
- C15 L3 `quickstep/src/com/android/quickstep/LauncherBackAnimationController.java`
- C16 L3 `res/values/config.xml`
- C17 L3 `quickstep/res/values/config.xml`
- C18 L3 `quickstep/src/com/android/launcher3/QuickstepTransitionManager.java`
- C19 L3 `quickstep/res/interpolator/app_open_x.xml`
- C20 LIBS `animationlib/src/com/android/app/animation/Interpolators.java`, LIBS `animationlib/res/interpolator/emphasized_interpolator.xml`
- C21 L3 `quickstep/src/com/android/quickstep/util/RectFSpringAnim.java`
- C22 L3 `quickstep/src/com/android/quickstep/util/ScalingWorkspaceRevealAnim.kt`
- C23 L3 `quickstep/src/com/android/quickstep/AbsSwipeUpHandler.java`
- C24 L3 `quickstep/src/com/android/quickstep/util/MotionPauseDetector.java`
- C25 L3 `quickstep/res/values/dimens.xml`
- C26 L3 `res/values/dimens.xml` (`workspace_content_scale 0.97`)
- C27 L3 `src/com/android/launcher3/shapes/ShapesProvider.kt`
- C28 FB `packages/SystemUI/src/com/android/systemui/keyguard/KeyguardUnlockAnimationController.kt`
- C29 FB `packages/SystemUI/compose/features/src/com/android/systemui/scene/ui/composable/transitions/` (`FromLockscreenToGoneTransition.kt`, `ToSingleShadeTransition.kt`, `FromShadeToQuickSettingsTransition.kt`, `ToNotificationsShadeTransition.kt`, `ToQuickSettingsShadeTransition.kt`)
- C30 FB `packages/SystemUI/res/values/shade_dimens.xml`
- C31 FB `packages/SystemUI/src/com/android/systemui/qs/panels/ui/compose/infinitegrid/CommonTile.kt`
- C32 FB `packages/SystemUI/src/com/android/systemui/qs/panels/ui/compose/infinitegrid/Tile.kt`
- C33 FB `packages/SystemUI/src/com/android/systemui/qs/panels/ui/viewmodel/BounceableTileViewModel.kt`
- C34 FB `packages/SystemUI/compose/core/src/com/android/compose/animation/Bounceable.kt`
- C35 FB `packages/SystemUI/res/values/config.xml`
- C36 FB `packages/SystemUI/src/com/android/systemui/qs/composefragment/QSFragmentCompose.kt`, `…/composefragment/ui/FromQuickQuickSettingsToQuickSettings.kt`
- C37 FB `packages/SystemUI/compose/features/src/com/android/systemui/qs/footer/ui/compose/FooterActions.kt`
- C38 FB `packages/SystemUI/src/com/android/systemui/brightness/ui/compose/BrightnessSlider.kt`
- C39 FB `packages/SystemUI/src/com/android/systemui/statusbar/notification/stack/MagneticNotificationRowManagerImpl.kt` (+ `MagneticNotificationRowManager.kt`)
- C40 LIBS `mechanics/src/com/android/mechanics/behavior/VerticalExpandContainerSpec.kt`
- C41 FB `core/res/res/values/config.xml` (`config_statusBarIcons`); сравнение с `…/+/refs/heads/android16-release/…` и `…/android16-qpr1-release/…`
- C42 FB `packages/SystemUI/res/values/styles.xml`
- C43 FB `packages/SystemUI/customization/clocks/common/res/values/dimens.xml`
- C44 FB `packages/SystemUI/src/com/android/systemui/keyguard/ui/view/layout/sections/transitions/ClockSizeTransition.kt`
- C45 FB `packages/SystemUI/src/com/android/systemui/keyguard/domain/interactor/FromLockscreenTransitionInteractor.kt` (+ `FromAodTransitionInteractor.kt`, `FromGoneTransitionInteractor.kt`)
- C46 FB `packages/SettingsLib/SettingsTheme/res/drawable-v36/settingslib_round_background*.xml`, `…/values-v36/dimens.xml`, `…/values/dimens_expressive.xml`
- C47 FB `packages/SettingsLib/CollapsingToolbarBaseActivity/res/values-v36/styles_expressive.xml`, `…/values-v31/dimens.xml`
- C48 FB `packages/SettingsLib/SettingsTransition/src/com/android/settingslib/transition/SettingsTransitionHelper.java`
- C49 FB `core/res/res/anim/activity_open_enter.xml` (+ `_open_exit`, `_close_enter`, `_close_exit`), FB `core/res/res/interpolator/fast_out_extra_slow_in.xml`
- C50 `https://android.googlesource.com/platform/packages/apps/Settings/+/refs/heads/android17-release/AndroidManifest.xml`
- C51 `https://android.googlesource.com/platform/frameworks/support/+/refs/heads/androidx-main/compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/ExpressiveMotionTokens.kt` (+ `StandardMotionTokens.kt`)
- C52 `https://android.googlesource.com/device/google/redfin/+/refs/heads/main/redfin/overlay/frameworks/base/core/res/res/values/config.xml`, `https://android.googlesource.com/device/google/redfin/+/refs/heads/main/device-redfin.mk`
- C53 FB `packages/SystemUI/src/com/android/systemui/qs/panels/ui/viewmodel/QSColumnsViewModel.kt`, `…/domain/interactor/QuickQuickSettingsRowInteractor.kt`
- C54 L3 `quickstep/src/com/android/quickstep/LauncherSwipeHandlerV2.java`
- C55 L3 `quickstep/src/com/android/quickstep/SwipeUpAnimationLogic.java`

### Официальные (VERIFIED-DOC)
- O1 https://developer.android.com/about/versions/17/qpr1/release-notes
- O2 https://developer.android.com/about/versions/17/qpr2/release-notes
- O3 https://developer.android.com/about/versions/17/qpr3
- O4 https://source.android.com/docs/whatsnew/site-updates
- O5 https://developer.android.com/design/ui/mobile/guides/patterns/predictive-back
- O6 https://developer.android.com/guide/navigation/custom-back/predictive-back-gesture
- O7 https://blog.google/products/android/material-3-expressive-android-wearos-launch/ (13.05.2025)
- O8 https://support.google.com/pixelphone/answer/16520562
- O9 https://blog.google/products-and-platforms/devices/pixel/september-2026-pixel-drop/ (поиск)
- O10 https://blog.google/products-and-platforms/platforms/android/android-17-features/ (поиск)
- O11 https://developer.android.com/develop/ui/compose/notifications (поиск)

### Пресса (PRESS)
- P1 https://9to5google.com/2026/09/15/android-17-qpr1-pixel/
- P2 https://9to5google.com/2025/09/03/android-16-qpr1-pixel/
- P3 https://www.androidauthority.com/android-16-qpr1-features-3594123/
- P4 https://www.androidauthority.com/android-16-qpr2-features-3621204/
- P5 https://www.androidauthority.com/android-17-features-3561251/
- P6 https://9to5google.com/2025/05/21/android-16-qpr1-beta-1-hands-on/
- P7 https://9to5google.com/2026/05/19/android-17-qpr1-beta-3-has-a-bouncy-new-animation-for-quick-settings-video/ (поиск)
- P8 https://www.androidauthority.com/aosp-source-code-schedule-3630018/ (поиск)
- P9 https://9to5google.com/2025/05/20/google-pixel-at-a-glance-android-16-shrinks/
- P10 https://www.androidauthority.com/android-17-quick-settings-notifications-shade-split-wifi-mobile-data-toggles-leak-3632251 (поиск)
- P11 https://androidauthority.com/android-17-stable-rollout-features-3675016 (поиск)
- P12 https://9to5google.com/2026/03/03/android-16-march-update-pixel/ (поиск)
- P13 https://www.androidauthority.com/android-16-new-battery-icon-3534047 (поиск)
- P14 https://9to5google.com/2025/05/20/android-16-qpr1-beta-notification-updates-lock-screen/ (поиск)
