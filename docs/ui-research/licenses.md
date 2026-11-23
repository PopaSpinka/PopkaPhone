# НаомиФон: что можно законно взять из Material 3 Expressive / Android

Проверено 2026-11-10: веб-страницы и метаданные репозиториев прочитаны напрямую, файлы не скачивались.
Это не юридическая консультация. То, что не подтверждено первоисточником, помечено **[не проверено]**.

---

## 0. Итоговая таблица

| Что | Лицензия | Можно ли в открытой оболочке | Условия |
|---|---|---|---|
| Тексты, картинки и спецификации m3.material.io (токены, пружины, формы, цвета) | Apache 2.0 **или** CC BY 4.0 (на выбор) | Да | Указать источник. Товарные знаки и логотипы Google лицензией не покрываются |
| material-components-android (MDC-Android) | Apache 2.0 | Да, в том числе переносить на C++/QML | Требования Apache §4 (раздел 2.4). NOTICE в корне нет |
| androidx: Compose Material3, graphics-shapes (морфинг фигур) | Apache 2.0, «Copyright … The Android Open Source Project» | Да, переносить на C++/QML можно | Apache §4: сохранить заголовок с копирайтом, отметить изменения, приложить текст лицензии. NOTICE в корне нет (есть только LICENSE.txt) |
| material-web | Apache 2.0 | Да | Как выше. Проект «в режиме поддержки» (ждёт новых сопровождающих) |
| Material Color Utilities (HCT, динамический цвет) | Apache 2.0 | Да. Есть C++ | Apache §4. В C++ нет спецификаций цвета 2025/2026: их переносить из Java, Kotlin или TypeScript |
| Material Symbols (вариативный шрифт и SVG) | Apache 2.0 | Да | Указание авторства «желательно, но не обязательно»; положить текст лицензии. На значках с марками (Android и т. п.) права на товарный знак не передаются |
| Google Sans Flex | SIL OFL 1.1, без Reserved Font Name | Да, можно вшить в образ | Приложить OFL.txt; не продавать шрифт отдельно; не называть продукт «Google…». **Кириллицы нет** |
| Google Sans | SIL OFL 1.1, без RFN | Да | Как выше. **Кириллица есть**. Насыщенность только 400–700 |
| Google Sans Code | SIL OFL 1.1, без RFN | Да | Кириллицы нет |
| Roboto Flex | SIL OFL 1.1, без RFN | Да | Кириллица есть, оси wght 100–1000, wdth, opsz, GRAD и др. |
| Roboto, Roboto Mono | SIL OFL 1.1 (в каталоге Google Fonts) | Да | Кириллица есть |
| Исходники AOSP: SystemUI, WM Shell, Launcher3 | Apache 2.0 (MODULE_LICENSE_APACHE2 + NOTICE) | Да: брать идеи, числа и алгоритмы, переносить код | Apache §4. Код опубликован по `android17-release` (июнь 2026) включительно |
| Pixel Launcher (NexusLauncher), SystemUIGoogle, At a Glance, обои, циферблаты и звуки Pixel | Проприетарные (образы Pixel: «не разбирать, не изменять, не распространять») | **Нет** | Только делать своё «по мотивам», не копируя ресурсы |
| Звуки AOSP (`frameworks/base/data/sounds`) | Apache 2.0 по метаданным сборки (`Android-Apache-2.0`) | Да **[проверено частично]** | Приложить лицензию. Это старые звуки Android, не звуки Pixel |
| Робот Android | CC BY 3.0 | Да, с обязательной строкой авторства | Строка из раздела 7. Логотип и словесный знак Android — нет |
| Названия Android, Google, Pixel, «G», значки приложений Google | Товарные знаки Google | В названии продукта и логотипах — **нет**. Упомянуть в тексте — да | «inspired by Material Design» / «в стиле Material 3» — допустимо, без логотипов и без намёка на одобрение Google |

---

## 1. Сайт с правилами Material Design (m3.material.io)

**Что сказано.** На странице https://m3.material.io/get-started, в блоке «What's Material?», дословно:

> «All Material 3 content, guidelines, imagery, and resources are available under the Apache 2.0 or CC BY 4.0 license.»

Ссылки ведут на https://www.apache.org/licenses/LICENSE-2.0.html и https://creativecommons.org/licenses/by/4.0/.
Проверено в живом браузере 2026-11-10: страница рисуется скриптами, поэтому простой загрузчик видит только заголовок.

Внизу сайта стоят ссылки на Условия использования Google (https://policies.google.com/terms) и Политику конфиденциальности.
Старая формула developers.google.com — «текст под CC BY 4.0, примеры кода под Apache 2.0» — относится к сайтам Google for Developers
(https://developers.google.com/terms/site-policies), а не к m3.material.io. На m3.material.io правило проще: всё под Apache 2.0 **или** CC BY 4.0.

**Что это даёт.** Числа и таблицы из спецификаций можно переносить в код оболочки. Например, на странице
https://m3.material.io/styles/motion/overview/specs есть таблица перевода пружин в кривые Безье
(«Expressive fast spatial 0.42, 1.67, 0.21, 0.90, 350 мс» и т. д.).
Там же сказано, что система пружин «can be easily adapted to other platforms».

**Условия.**
- При CC BY 4.0 — указать автора, название и ссылку на источник, назвать лицензию и отметить, что изменено.
  При Apache 2.0 — приложить текст лицензии.
  Практично: в файле с токенами написать комментарий «Значения из Material Design 3 (m3.material.io), © Google, CC BY 4.0 / Apache 2.0»
  и упомянуть это в общем списке сторонних материалов.
- CC BY 4.0 (раздел 2(b)(2)) и Apache 2.0 (§6) **не дают прав на товарные знаки**: логотип Material, «G» и т. п. не берём.
- Картинки сайта могут содержать снимки экранов с продуктами Google. Лучше брать числа и описания, а картинки не вшивать **[осторожность]**.

## 2. Библиотеки компонентов (Apache 2.0)

| Репозиторий | Лицензия | NOTICE |
|---|---|---|
| https://github.com/material-components/material-components-android | Apache 2.0 (файл `LICENSE`) | нет |
| https://github.com/androidx/androidx — зеркало AOSP `platform/frameworks/support` | Apache 2.0 (`LICENSE.txt`) | нет в корне |
| https://github.com/material-components/material-web | Apache 2.0 | не видно. В README: «MWC is in maintenance mode pending new maintainers» |

### 2.1 Пружины (motion physics)
Файл `compose/material3/material3/src/commonMain/kotlin/androidx/compose/material3/tokens/ExpressiveMotionTokens.kt`
(шапка: «Copyright 2024 The Android Open Source Project», Apache 2.0; генератор токенов «VERSION: v0_14_0»):

| Токен | damping | stiffness |
|---|---|---|
| FastSpatial | 0.6 | 800 |
| DefaultSpatial | 0.8 | 380 |
| SlowSpatial | 0.8 | 200 |
| FastEffects | 1.0 | 3800 |
| DefaultEffects | 1.0 | 1600 |
| SlowEffects | 1.0 | 800 |

Названия схем — в `MotionSchemeKeyTokens.kt`. Для QML: `SpringAnimation` в Qt принимает `spring` и `damping`,
но их смысл в Qt не совпадает с stiffness и damping ratio в Compose. Нужен пересчёт или своя пружина
на C++ (ω = √stiffness, ζ = damping ratio) **[не проверено на телефоне]**.

### 2.2 Морфинг фигур — androidx.graphics.shapes
Каталог `graphics/graphics-shapes/src/commonMain/kotlin/androidx/graphics/shapes/`:
CornerRounding, Cubic, FeatureDetector, FeatureMapping, Features, FloatMapping, Morph, Point, PolygonMeasure,
RoundedPolygon, Shapes, SvgPathParser, Utils и др. Шапка `Morph.kt`: «Copyright 2022 The Android Open Source Project», Apache 2.0.
Набор фигур M3 Expressive (`MaterialShapes`) лежит в Compose Material3 и построен на этой библиотеке **[по памяти]**.

### 2.3 Можно ли переносить на C++/QML
**Да.** Перенос — это «производная работа» в смысле Apache 2.0, и лицензия её прямо разрешает (§2).
Числовые значения токенов, скорее всего, вообще не охраняются авторским правом, но они и так открыты.
Патентная лицензия (§3) даётся на «Work» и производные от него. Поэтому перенос кода надёжнее,
чем «угадывать» поведение заново **[юридический нюанс, не проверено]**.

### 2.4 Что обязательно при распространении (Apache 2.0, §4)
1. Приложить текст лицензии Apache 2.0, например `third_party/LICENSES/Apache-2.0.txt`.
2. В перенесённых файлах **сохранить** строки копирайта исходника
   («Copyright 2022 The Android Open Source Project», «Copyright 2023 Google LLC» и т. п.)
   и **заметно указать, что файл изменён**, например: «Перенесено на C++ для НаомиФона, 2026; изменения: …».
3. Если в исходном модуле есть файл NOTICE — включить его текст в свой NOTICE.
   У MDC-Android, androidx (корень) и MCU NOTICE нет. У AOSP SystemUI **есть** `packages/SystemUI/NOTICE`:
   «Copyright (c) 2005-2008, The Android Open Source Project» + Apache 2.0.
4. §6: лицензия не даёт права на названия и знаки Google.

**Совместимость с лицензией проекта.** Своего файла LICENSE в репозитории НаомиФона пока нет (проверено чтением, ничего не менялось).
Apache 2.0 совместима с GPLv3 и несовместима с GPLv2-only (https://www.apache.org/licenses/GPL-compatibility.html).
Значит, для оболочки подходят Apache-2.0, MIT/BSD (перенесённые файлы остаются под Apache) или GPLv3/LGPLv3 (как Qt). GPLv2-only не подходит.

## 3. Material Symbols / Material Icons
- Репозиторий: https://github.com/google/material-design-icons, файл `LICENSE` — Apache 2.0.
  README: «We have made these icons available for you to incorporate into your products under the Apache License Version 2.0.»
- Авторство: «We'd love attribution in your app's about screen, but it's not required.» Текст лицензии Apache всё равно нужно приложить (§4).
- Где взять (смотреть, не качать без спроса):
  - вариативные шрифты: `variablefont/MaterialSymbols{Outlined,Rounded,Sharp}[FILL,GRAD,opsz,wght].{ttf,woff2,codepoints}`;
  - SVG: `symbols/web/…` (есть также `symbols/android`, `symbols/ios`);
  - каталог с поиском: https://fonts.google.com/icons;
  - документация: https://developers.google.com/fonts/docs/material_symbols — Apache 2.0, оси FILL 0–1, wght 100–700, GRAD, opsz 20–48, допускается свой хостинг.
- Старые «Material Icons» больше не обновляются с 2022 года — брать Symbols.
- Знаки: README — «Google does not include 3rd-party logos among the Material Symbols or Material Icons due to legal reasons».
  В наборе есть значки, изображающие продукты Google (например, `android`). Apache §6 не передаёт права на эти знаки.
  Для обычных значков интерфейса (wifi, battery, settings) проблемы нет **[перечень «марочных» значков не проверялся]**.
- В QML: вариативный шрифт и оси через `font.variableAxes` — нужен Qt ≥ 6.7 (см. раздел 4.5).

## 4. Шрифты

### 4.1 Сводка (по `METADATA.pb` и `OFL.txt` в https://github.com/google/fonts)
| Шрифт | Лицензия / копирайт | RFN | Кириллица | Оси |
|---|---|---|---|---|
| **Google Sans Flex** (`ofl/googlesansflex`) | OFL 1.1; «Copyright 2015 The Google Sans Flex Authors (github.com/googlefonts/googlesans-flex)» | нет | **НЕТ** (подмножества: latin, latin-ext, vietnamese, symbols, math, menu, canadian-aboriginal, cherokee, nushu, syriac, tifinagh) | wght 1–1000, wdth 25–151, opsz 1–144 (на Google Fonts раздаётся 6–144), slnt −10–0, ROND 0–100. В upstream есть ещё GRAD 0–100 |
| **Google Sans** (`ofl/googlesans`) | OFL 1.1; «Copyright 2025 The Google Sans Project Authors (github.com/googlefonts/googlesans)» | нет | **ДА** (cyrillic, cyrillic-ext + ещё около 25 письменностей) | wght 400–700, opsz 17–18, GRAD −50–200; есть курсив |
| **Google Sans Code** (`ofl/googlesanscode`) | OFL 1.1; «Copyright 2025 The Google Sans Code Project Authors»; дизайнеры Google и Universal Thirst | нет | **НЕТ** | wght 300–800 |
| **Roboto Flex** (`ofl/robotoflex`) | OFL 1.1; «Copyright 2017 The Roboto Flex Project Authors» | нет | **ДА** | wght 100–1000, wdth 25–151, opsz 8–144, slnt, GRAD, XTRA, XOPQ, YOPQ и др. |
| **Roboto** (`ofl/roboto`) | OFL 1.1; «Copyright 2011 The Roboto Project Authors (github.com/googlefonts/roboto-classic)» | нет | **ДА** | wght 100–900, wdth 75–100 |
| **Roboto Mono** (`ofl/robotomono`) | OFL | — | **ДА** | wght 100–700 |

### 4.2 Когда Google открыл свои шрифты
- **Google Sans Flex**: открыт 18 ноября 2025 под OFL и выложен на Google Fonts
  (9to5Google, https://9to5google.com/2025/11/18/google-sans-flex-font-available/).
  Рассказ самого Google: https://design.google/library/google-sans-flex-font — «Google decided to make Google Sans and Google Sans Flex open-source».
  Исходники: https://github.com/googlefonts/googlesans-flex. README: «SIL Open Font License, without Reserved Font Names».
  Последняя версия в CHANGELOG — 5.000 от 2026-10-09.
  В `METADATA.pb` стоит `date_added: "2024-11-21"` — видимо, внутренняя дата заведения в каталоге, а не дата публикации **[не проверено]**.
- **Google Sans** (без Flex): открыт вместе с Flex. Репозиторий https://github.com/googlefonts/googlesans, выпуски v13.001 (2026-02-26), v13.002 (2026-04-15), v14.000 (2026-06-10).
  Точная дата появления на Google Fonts — между ноябрём 2025 и январём 2026 **[не проверено]**.
  Статья The Dieline «Google Makes Google Sans and Google Sans Flex Open Source» вышла 7 января 2026.
  `date_added: "2017-06-05"` в METADATA — тоже внутренняя дата.
  Координатор проверил, что fonts.googleapis.com отдаёт Google Sans с подмножествами cyrillic и cyrillic-ext.
- **Google Sans Code**: OFL, репозиторий https://github.com/googlefonts/googlesans-code (копирайт 2025).

### 4.3 Есть ли кириллица в исходниках Google Sans Flex? — Нет
Проверено по исходникам `sources/GoogleSansFlex.glyphspackage` в upstream (ветка main):
- в `order.plist` нет глифов `*-cy` и кодов U+0410–U+044F;
- файла `glyphs/Be-cy.glyph` нет (404), а `glyphs/A_.glyph` есть;
- CHANGELOG вплоть до 5.000 (2026-10-09) не упоминает кириллицу и греческий.
Вывод: кириллицы нет ни на Google Fonts, ни в исходниках. Отдельной «внутренней» сборки с кириллицей в открытом доступе нет.
В README_WORKSPACE.md описаны только сборки ширин и насыщенностей для Workspace.

### 4.4 Что разрешает OFL 1.1
Пункты из OFL.txt:
1. Шрифт нельзя продавать «by itself».
2. Шрифт можно «bundled, redistributed and/or sold with any software». В каждой копии должны быть копирайт и текст лицензии —
   отдельным файлом или в метаданных, «easily viewed by the user».
3. Изменённую версию нельзя называть зарезервированным именем (RFN). У всех перечисленных шрифтов RFN нет.
4. Имена авторов нельзя использовать для рекламы изменённой версии.
5. Шрифт, изменённый или нет, остаётся под OFL. На документы и интерфейс, набранные шрифтом, это не распространяется.

**Вшивать в образ ОС можно.** Положить `.ttf` + `OFL.txt` в `/usr/share/fonts/…`; на экране «О телефоне» перечислить шрифты.

**Дополнительно у Google Sans и Google Sans Flex** — файл `TRADEMARKS.md`/`TRADEMARKS.txt`:
«Google», «Google Sans», «Google Sans Flex» — знаки Google LLC. Их нельзя ставить в название продукта, компании, домена, соцсети;
нельзя намекать на связь с Google. Название шрифта можно использовать только применительно к самому шрифту.
Практическое правило: вшивать **неизменённые** файлы. Если делаем подмножество или статический срез — переименовать семейство
(например, «Naomi Sans»). Без RFN это разрешено.

### 4.5 Что выбрать для русского интерфейса
Оболочка на Qt 6 (`find_package(Qt6 6.5 …)` в `shell/CMakeLists.txt`). `QFont::setVariableAxis` и `QFont::setFeature` появились в **Qt 6.7**
(https://doc.qt.io/qt-6/qfont.html). Значит, для управления осями нужен Qt ≥ 6.7 на телефоне **[версия Qt на телефоне не проверялась]**.

- **Основной текст (русский): Google Sans.** Ближе всего к виду Pixel и M3 Expressive, OFL, кириллица.
  Ограничение: насыщенность только 400–700, без тонких и сверхжирных начертаний.
  Если нужны «экспрессивные» анимации насыщенности и ширины — запасной вариант **Roboto Flex**
  (базовый шрифт M3, кириллица, wght 100–1000, wdth 25–151).
- **Цифры часов на экране блокировки: Google Sans Flex.** Цифры и двоеточие входят в латинский набор.
  Ось ROND даёт «скруглённый» вид M3 Expressive, wght 1–1000 и wdth 25–151 — крупные «дышащие» часы.
  **Только для цифр**: дату («вторник, 10 ноября») набирать Google Sans. Иначе Qt подставит глифы из другого шрифта посреди строки.
  Чтобы цифры не «прыгали», нужны табличные цифры (`tnum`, через `font.features` в Qt ≥ 6.7).
  Есть ли `tnum` в Google Sans Flex — **[не проверено]**; если нет — ставить моноширинную раскладку вручную или брать Roboto Flex (`tnum` тоже **[не проверено]**).
- **DevHud / моноширинный:** Google Sans Code не умеет кириллицу. Для русских подписей — Roboto Mono (OFL, кириллица).
  Google Sans Code — только для цифр и латиницы.

## 5. Material Color Utilities (динамический цвет, HCT)
- https://github.com/material-foundation/material-color-utilities — Apache 2.0 (`LICENSE`, NOTICE нет).
- Языки (таблица в README, все ✅): **C++**, Dart (pub `material_color_utilities`), Java (используется в MDC-Android), Kotlin, Swift, TypeScript (npm `@material/material-color-utilities`).
- C++ (`cpp/`): blend, cam (HCT), contrast, dislike, dynamiccolor, palettes, quantize, score, scheme, temperature, utils.
  Варианты: Monochrome, Neutral, TonalSpot, Vibrant, Expressive, Fidelity, Content, Rainbow, FruitSalad. Шапка «Copyright 2023 Google LLC».
- **Важно:** в C++ `dynamiccolor/` **нет** `color_spec_2025` / `color_spec_2026`. В Java и TypeScript они есть: `ColorSpec2021/2025/2026`.
  Новую палитру Expressive (спецификация 2025 и новее) придётся переносить с Java или TypeScript в C++ — с соблюдением §4.
- Для оболочки на C++ это лучший кандидат: подключить C++ версию как стороннюю библиотеку (Apache 2.0) и дописать спецификацию 2025.

## 6. AOSP и закрытая часть Pixel

### 6.1 Лицензии
- https://source.android.com/docs/setup/about/licenses: «Apache License, Version 2.0 (Apache 2.0) is the preferred license for AOSP»; ядро — GPLv2.
- `frameworks/base/packages/SystemUI` (ветка `android17-release`): есть `MODULE_LICENSE_APACHE2` и `NOTICE` (Apache 2.0), есть каталог `compose/`.
- `packages/apps/Launcher3` (`android17-release`): есть; файлы под Apache 2.0 (заголовки). Отдельного NOTICE в корне нет.
- WindowManager Shell — `frameworks/base/libs/WindowManager/Shell`, та же лицензия Apache 2.0 **[каталог отдельно не открывался]**.
- Каркас часов экрана блокировки: `packages/SystemUI/customization/clocks/{common,sample}` — в AOSP только каркас и образец.
  Фирменные циферблаты Pixel поставляются отдельно **[проверено частично]**.

### 6.2 Что закрыто (Pixel и GMS)
- Страница образов Pixel https://developers.google.com/android/images: образы «for use only on your personal Nexus or Pixel devices»,
  их нельзя «disassembled, decompiled, reverse engineered, modified or redistributed». Действуют Условия Google.
- В этих образах лежат Pixel Launcher (`NexusLauncher`), SystemUIGoogle, At a Glance / Smartspace, обои Pixel, циферблаты, звуки Pixel,
  приложения Google. **Ничего оттуда не брать: ни картинки, ни звуки, ни декомпилированный код.**
  Пример ловушки: «Rootless Pixel Launcher» (F-Droid, `amirz.rootless.nexuslauncher`) — форк Launcher3 под Apache 2.0,
  но функции Pixel в нём воссозданы по декомпилированному коду. Брать оттуда тоже не стоит.
- Названия внутренних компонентов (NexusLauncher, SystemUIGoogle) известны по разбору прошивок, а не из официальных документов **[не проверено первоисточником]**.

### 6.3 Публикация исходников в 2025–2026
- Баннер на https://source.android.com: «Effective in 2026 … we will publish source code to AOSP in Q2 and Q4».
  Для сборки рекомендуют ветку `android-latest-release`. Сейчас она указывает на `android17-release`.
- Хронология по https://source.android.com/docs/setup/reference/build-numbers:
  - `android-16.0.0_r3` — BP3A, патч 2025-09 = Android 16 QPR1, **первая версия с M3 Expressive**.
    Код вышел с опозданием примерно на два месяца, в ноябре 2025 (ветка `android16-qpr1-release` есть; в SystemUI есть `compose/`).
  - `android-16.0.0_r4` — BP4A, 2025-12 = Android 16 QPR2.
  - `android-17.0.0_r1` — CP2A, патч 2026-06-05 = **Android 17**.
- **Ответ на вопрос:** на 10.11.2026 в AOSP открыт код SystemUI с редизайном M3 Expressive — Android 16 QPR1/QPR2 и Android 17.
  Ветки `android17-qpr1-release` нет (404): код сентябрьского QPR1 Android 17 ждём в Q4 2026 по новому графику **[срок не проверен]**.
  Что вошло в AOSP, а что осталось только в Pixel (часть M3E-функций может быть в закрытых плагинах), — **[поштучно не проверялось]**.

## 7. Товарные знаки
Источник: https://developer.android.com/distribute/marketing-tools/brand-guidelines и https://about.google/brand-resource-center/.
- «The "Android" name, the Android logo, … are property of Google LLC and not part of the assets available through the Android Open Source Project.»
- «Android» нельзя ставить в название продукта: «"Android", or anything confusingly similar … cannot be used in names of applications or … phones».
  Слово «Android» на устройстве и упаковке разрешено только совместимым с Android устройствам. Наша оболочка на Linux такой не является.
  НаомиФон нельзя называть «Android-телефоном» или «НаомиAndroid».
- Робот Android (зелёный): можно воспроизводить и менять при строке авторства:
  «The Android robot is reproduced or modified from work created and shared by Google and used according to terms described in the Creative Commons 3.0 Attribution License.»
  Нельзя регистрировать его как свой знак. **Словесный знак и шрифт Android — нельзя без письменного разрешения.**
- Google: упоминать в тексте в информационном смысле можно («You can refer to Google or our products in an informational context in plain text»).
  Нельзя использовать логотипы и элементы бренда, если это намекает на связь или одобрение. Нельзя ставить знаки Google в название продукта или домена.
  **Логотип «G», значки приложений Google (Play, Chrome, Gmail и т. п.) — не брать.**
- Список знаков Google (https://about.google/brand-resource-center/trademark-list/): «Android™ platform», «Pixel™ …», «Google Pixel™ …».
  «Material Design» и «Material You» в этом списке не найдены. Зарегистрированы ли они в USPTO — **[не проверено]**.
  Безопаснее считать их брендами Google: не ставить в название продукта и не брать логотип Material.
- **«Inspired by Material Design» / «интерфейс по мотивам Material 3» — допустимо** как описательное упоминание:
  обычным текстом, без логотипа, без слов «официальный», «одобрено Google» **[общая практика добросовестного упоминания; юридически не проверено]**.
- Pixel: не называть оболочку «Pixel UI», не брать обои, звуки, циферблаты и шрифт Pixel из прошивки.
  Работа на Pixel 5 — это просто «работает на Google Pixel 5» (описательно).
- Дизайн-патенты Google на элементы интерфейса могут существовать **[не проверено]**. Ещё одна причина не копировать Pixel пиксель в пиксель, а делать своё по правилам M3.

## 8. Свободные звуки и обои
| Источник | Лицензия | Комментарий |
|---|---|---|
| Kenney «UI Audio» (https://kenney.nl/assets/ui-audio) | CC0 | 50 щелчков и переключателей, без условий |
| Freesound (https://freesound.org) с фильтром CC0 / CC BY | по файлу | Проверять лицензию каждого звука |
| KDE Ocean Sound Theme (https://invent.kde.org/plasma/ocean-sound-theme) | в LICENSES: CC-BY-SA-4.0, CC0-1.0, BSD-2-Clause | Сами звуки, видимо, CC-BY-SA-4.0 **[пофайловая разметка не проверена]**. Share-alike: изменённые звуки — под той же лицензией |
| freedesktop sound theme (`sound-theme-freedesktop`, Debian copyright) | смесь CC-BY-3.0, CC-BY-SA-3.0, GPL-2/2+ | Лицензию каждого файла смотреть в CREDITS |
| ubuntu-touch-sounds (уже на телефоне в Ubuntu Touch) | CC-BY-SA 3.0, © Canonical (по OMG! Ubuntu, 2013) | **[первоисточник не проверен]** |
| Звуки AOSP (`frameworks/base/data/sounds`) | Apache 2.0 по `Android.bp` | Старые звуки Android, не Pixel. Знаки Google в них нет |
| Обои GNOME (`gnome-backgrounds`, COPYING) | CC BY-SA 3.0 US | Указать авторов |
| Снимки NASA (https://www.nasa.gov/nasa-brand-center/images-and-media/) | обычно без авторского права в США | Без логотипов NASA, без намёка на одобрение. Сторонние снимки помечены отдельно |
| Unsplash (https://unsplash.com/license) | своя лицензия, не CC | Бесплатно, но нельзя продавать без изменений и собирать сервис-конкурент. Для сборки ОС лучше CC0 или своё |
| Wikimedia Commons | по файлу (CC0 / CC BY / CC BY-SA) | Указывать авторов |
| **Своё** | — | Лучший вариант для фирменного стиля: обои по палитре HCT (генерация), звуки — синтез |

## 9. Что сделать в проекте (предложение; в репозитории ничего не менялось)
1. Выбрать лицензию НаомиФона: Apache-2.0 или GPLv3. Не GPLv2-only — с ней несовместим Apache-код.
2. Завести `third_party/` и файл `THIRD_PARTY_NOTICES` (или раздел в README): компонент, версия или коммит, URL, лицензия.
   Тексты Apache-2.0, OFL-1.1, CC-BY-4.0 положить в `LICENSES/`.
3. В каждом перенесённом файле: исходный копирайт + «перенесено и изменено …» + SPDX `Apache-2.0`.
4. В образ — неизменённые `.ttf` + `OFL.txt` + `TRADEMARKS.md`. Экран «О телефоне» → «Лицензии».
5. Скачивания шрифтов и значков — только спросив пользователя (по правилам проекта).

## 10. Не проверено / осталось открытым
- Регистрация «Material Design» / «Material You» как товарных знаков.
- Точная дата появления Google Sans на Google Fonts.
- Есть ли `tnum` в Google Sans Flex и Roboto Flex.
- Срок публикации Android 17 QPR1 в AOSP (ожидается Q4 2026).
- Какие функции M3E есть только в Pixel, а какие в AOSP SystemUI.
- Пофайловые лицензии KDE Ocean и ubuntu-touch-sounds.
- Версия Qt на телефоне (нужна ≥ 6.7 для осей вариативных шрифтов).
