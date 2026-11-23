# PopkaPhone

**Linux-телефон, который стал компьютером.** Своя оболочка для Google Pixel 5 (`redfin`) поверх Ubuntu Touch 24.04:
свой драйвер экрана, свой Wayland-композитор, своя клавиатура — и рабочий стол с окнами, который телефон показывает
на мониторе, MacBook, планшете и телевизоре сразу. Считает всё сам телефон, экраны только показывают картинку.

Написано с нуля вместе с Claude (Claude Code). В коде и документах проект называется «НаомиФон» / `naomi`.

Видео: [обзор на 25 минут](https://www.youtube.com/watch?v=1gPjkVrjHgA) · Telegram: https://t.me/+Ekb7_o51208wNWE6

> **Проект закрыт и выложен как есть.** Это снимок рабочего состояния на 22 ноября 2026: без истории, без гарантий и
> без поддержки. Берите, что пригодится, — лицензия MIT. Установка **стирает телефон**; это тестовый стенд, а не
> телефон на каждый день.

*English: a from-scratch shell for the Google Pixel 5 on top of Ubuntu Touch 24.04 — its own display driver (HWC2),
Wayland compositor and keyboard, plus a windowed desktop the phone streams to a monitor (via a Raspberry Pi 5), a Mac,
an Android tablet and an Android TV at once. Written together with Claude Code. Code comments and docs are in Russian.
The project is frozen and published as is under the MIT license; installing it wipes the phone. Start with
[docs/INSTALL.md](docs/INSTALL.md).*

## Что умеет

- **Оболочка телефона** на Qt Quick при 90 Гц: главный экран, шторка, «Настройки» с настоящими данными, AOD, своя
  клавиатура RU/EN с исправлением опечаток ([SHELL](docs/SHELL.md), [DISPLAY](docs/DISPLAY.md),
  [KEYBOARD](docs/KEYBOARD.md)).
- **Рабочий стол** 16:9: панель, меню, окна, обои, «Файлы» с просмотром по пробелу, браузер (Qt WebEngine) с
  вкладками на панели, «Терминал», монитор системы, окна «Интернет», Bluetooth, звук, батарея, «Закрыть
  принудительно», скриншоты ([DESKTOP](docs/DESKTOP.md), [FILES](docs/FILES.md), [WEB](docs/WEB.md),
  [TERMINAL](docs/TERMINAL.md)).
- **Экраны:** телефон сжимает стол аппаратным видеокодером и отдаёт его приставке на Raspberry Pi 5 (монитор по
  HDMI, клавиатура, мышь, трекпад), окну на Mac, планшету Android и телевизору Android TV — любому набору сразу; с
  кабеля на Wi-Fi и обратно — без разрыва ([STREAM](docs/STREAM.md), [PI](docs/PI.md), [SCREENS](docs/SCREENS.md),
  [TABLET](docs/TABLET.md), [TV](docs/TV.md)).
- **Android-приложения** в окнах стола через Waydroid ([ANDROID](docs/ANDROID.md)).
- **Игры:** Денди и PlayStation (ядра libretro), программы и игры Windows через Wine + Box64 + DXVK + Turnip, геймпад
  Bluetooth ([GAMES](docs/GAMES.md), [WINDOWS](docs/WINDOWS.md)). Самих игр в проекте нет.
- **Claude Desktop для Linux** окном стола ([CLAUDE](docs/CLAUDE.md)); платы ESP32 из приставки — на телефоне по
  USB/IP ([BOARDS](docs/BOARDS.md)).

Что не вошло в открытую версию: чат и блокнот автора (работали только с его домашним сервером), его редакторы
экранов. В документах они ещё упоминаются — это рабочие записи.

## С чего начать

**[docs/INSTALL.md](docs/INSTALL.md)** — установка с нуля по шагам: открыть загрузчик, `tools/images`,
`tools/flash.sh`, `tools/naomi access`, `tools/naomi setup`, потом приставка и остальные экраны.

Нужны: Pixel 5 с открываемым загрузчиком; компьютер с `adb`, `fastboot`, `python3` (проверено на macOS); для стола на
мониторе — Raspberry Pi 5, карта microSD и монитор.

Готовых сборок нет — всё собирается из исходников командами из инструкции: оболочка — на самом телефоне, приёмник —
на Pi, приложения Mac и Android — на компьютере.

## Под капотом

| | |
|---|---|
| Телефон | Google Pixel 5 (`redfin`): Snapdragon 765G, 8 ГБ, OLED 90 Гц |
| Основа | Ubuntu Touch 24.04 на Halium 13 (UBports, порт [dadrickpistachio/google-redfin](https://gitlab.com/dadrickpistachio/google-redfin)) — оттуда ядро и драйверы |
| Оболочка | Qt Quick 6.10, драйвер экрана прямо в HWC2, Wayland-композитор, служба железа `naomid` |
| Стол на экранах | аппаратный кодер телефона (H.264 / HEVC) → приёмник `naomi-dock` на Pi (KMS), окно Electron на Mac, приложение Android без библиотек |

## Что где

| Папка | Что |
|---|---|
| `shell/` | Оболочка: `display/` — драйвер экрана для Qt, `app/` — оболочка-композитор и её QML (в `qml/desktop/` — рабочий стол), `ui/` — общие QML-модули, `apps/` — браузер и программа игр, `input/` — клавиатура, `stream/` — трансляция, `files/`, `term/`, `preview/`, `naomid/` — служба железа |
| `device/` | Что запускается на телефоне: установка служб, среда сборки, кабель USB, Android, эмуляторы, Wine, замеры и тесты |
| `pi/` | Приставка на Raspberry Pi 5: приёмник `naomi-dock`, первый запуск, проброс плат |
| `mirror/` | Окно с экраном телефона на Mac (Electron) |
| `tablet/` | Приложение для планшета и телевизора Android (Java, без Gradle) |
| `tools/` | Команды с компьютера: `images`, `flash.sh`, `naomi`, `pi`, `pi-card`, `mirror`, `tablet`, `tv`, `boards`, замеры (`deskbench`, `xray`, `pi-smooth`, `kbtaps`) |
| `design/mockup/` | Веб-макет в духе Android 17 — справочник по движению |
| `docs/` | Документы: как устроено, замеры, что проверено и что нет |

## Документы

Рабочие записи проекта — как есть, с замерами на самом телефоне, датами и тем, что не получилось.

| Документ | О чём |
|---|---|
| [INSTALL](docs/INSTALL.md) | Установка с нуля и возврат на Android |
| [DEVELOPMENT](docs/DEVELOPMENT.md) | Среда сборки на телефоне, кабель: ADB и сеть по USB |
| [PHONE](docs/PHONE.md), [HWTEST](docs/HWTEST.md) | Что есть в телефоне и что из железа работает |
| [DISPLAY](docs/DISPLAY.md) | Дисплей без Lomiri: путь кадра через HWC2, 90 Гц |
| [SHELL](docs/SHELL.md), [UI](docs/UI.md), [KEYBOARD](docs/KEYBOARD.md) | Оболочка телефона, интерфейс, клавиатура |
| [NAOMID](docs/NAOMID.md), [POWER](docs/POWER.md) | Служба железа; батарея и сон |
| [DESKTOP](docs/DESKTOP.md), [FILES](docs/FILES.md), [WEB](docs/WEB.md), [TERMINAL](docs/TERMINAL.md) | Рабочий стол и его программы |
| [STREAM](docs/STREAM.md), [PI](docs/PI.md), [SCREENS](docs/SCREENS.md), [TABLET](docs/TABLET.md), [TV](docs/TV.md) | Трансляция, приставка, несколько экранов, планшет, телевизор |
| [ANDROID](docs/ANDROID.md), [ANDROID-AUDIT](docs/ANDROID-AUDIT.md) | Android-приложения на столе |
| [GAMES](docs/GAMES.md), [WINDOWS](docs/WINDOWS.md) | Игры приставок и Windows |
| [CLAUDE](docs/CLAUDE.md), [BOARDS](docs/BOARDS.md) | Claude Desktop; платы ESP32 |
| [AUDIT](docs/AUDIT.md), [XRAY](docs/XRAY.md) | Разбор скорости сверху вниз; трасса ядра |
| [NOTHING](docs/NOTHING.md) | Разведка перед переносом на Nothing Phone 1 |
| [ROADMAP](docs/ROADMAP.md), [CHANGELOG](CHANGELOG.md) | План и история |

## Лицензия

Свой код — [MIT](LICENSE): делайте с ним что хотите. Чужие компоненты и их лицензии — [THIRD-PARTY.md](THIRD-PARTY.md).
Игр, образов систем, шрифтов и чужих программ в репозитории нет — скрипты скачивают их с сайтов авторов.
