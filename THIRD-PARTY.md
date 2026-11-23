# Чужие компоненты

Свой код проекта — под лицензией MIT ([LICENSE](LICENSE)). Всё чужое берётся как есть, с указанием источника и
версии. В репозитории чужих программ, шрифтов, игр и образов **нет** — только пять заголовочных файлов; остальное
скрипты скачивают с сайтов авторов на ваш компьютер или телефон, и у каждого остаётся его лицензия.

Это не юридическая консультация: лицензии переписаны со страниц проектов, перед своим распространением проверьте их
сами.

## Лежит в репозитории

| Что | Откуда | Лицензия |
|---|---|---|
| `shell/apps/game/third_party/libretro.h` | [libretro/Mesen](https://github.com/libretro/Mesen), описание интерфейса ядер libretro | MIT (текст в начале файла) |
| `shell/apps/web/third_party/QtWebEngineCore/*.h` (три заголовка API) | [Qt WebEngine 6.10.2](https://github.com/qt/qtwebengine/tree/v6.10.2/src/core/api) | LGPL-3.0 / GPL-2.0 / GPL-3.0 (строка SPDX в каждом файле) |

Числа движения и геометрии в макете `design/mockup/` взяты из исходников Android (AOSP, Apache 2.0) и спецификаций
Material 3 (Apache 2.0 / CC BY 4.0) — разбор в [docs/ui-research/licenses.md](docs/ui-research/licenses.md).

## Скачивается при установке

| Что | Откуда | Лицензия | Кто скачивает |
|---|---|---|---|
| Заводской Android 13 для Pixel 5 | [developers.google.com/android/images](https://developers.google.com/android/images) | условия Google; раздавать дальше нельзя — только с сайта Google | `tools/images` |
| Ubuntu Touch 24.04 для Pixel 5 (порт) | [dadrickpistachio/google-redfin](https://gitlab.com/dadrickpistachio/google-redfin), UBports | свободные лицензии частей системы (GPL и другие); драйверы — из заводского Android | `tools/images` |
| Qt 6.10.2, GCC, CMake и пакеты сборки | репозитории Ubuntu и UBports | LGPL-3.0 / GPL (Qt), GPL и др. | `tools/naomi devroot` (apt на телефоне) |
| Шрифты Google Sans, Google Sans Flex | [google/fonts](https://github.com/google/fonts) | SIL OFL 1.1 | `tools/naomi fonts` |
| Значки Material Symbols Rounded | [google/material-design-icons](https://github.com/google/material-design-icons) | Apache 2.0 | `tools/naomi fonts` |
| Частоты слов для клавиатуры (FrequencyWords, по OpenSubtitles 2018) | [hermitdave/FrequencyWords](https://github.com/hermitdave/FrequencyWords) | CC BY-SA 4.0 (данные), MIT (код) | `tools/naomi dict` |
| Списки EasyList и EasyPrivacy | [easylist.to](https://easylist.to) | GPL-3.0 или CC BY-SA 3.0 | `tools/naomi web-filters` |
| Raspberry Pi OS Lite | [raspberrypi.com](https://www.raspberrypi.com/software/operating-systems/) | свободные лицензии частей системы | вручную, пишет `tools/pi-card` |
| Electron | npm | MIT | `npm install` в `mirror/` |

## Скачивается по желанию

| Что | Откуда | Лицензия | Кто скачивает |
|---|---|---|---|
| Waydroid и его образы Android (LineageOS, приложения Google) | [waydro.id](https://waydro.id) | GPL-3.0 (Waydroid), Apache 2.0 и др. (LineageOS); приложения Google — условия Google | `tools/naomi android init` |
| Эмулятор NES Mesen (ядро libretro) | [libretro/Mesen](https://github.com/libretro/Mesen) | GPL-3.0 | `tools/naomi cores` |
| Эмулятор PlayStation PCSX-ReARMed (ядро libretro) | [libretro/pcsx_rearmed](https://github.com/libretro/pcsx_rearmed) | GPL-2.0 | `tools/naomi cores` |
| Wine 10.0 (сборка Kron4ek) | [Kron4ek/Wine-Builds](https://github.com/Kron4ek/Wine-Builds) | LGPL-2.1 | `tools/naomi wine` |
| Box64 | [ptitSeb/box64](https://github.com/ptitSeb/box64) | MIT | `tools/naomi wine` |
| DXVK 2.4.1 | [doitsujin/dxvk](https://github.com/doitsujin/dxvk) | zlib | `tools/naomi wine` |
| Mesa (драйвер Vulkan Turnip) | [mesa3d.org](https://archive.mesa3d.org) | MIT | `tools/naomi wine` |
| Библиотека `d3dx9_30.dll` из пакета DirectX (июнь 2010) | download.microsoft.com | условия Microsoft; раздавать дальше нельзя | `wine.sh d3dx9` на телефоне |
| Claude Desktop для Linux | репозиторий Anthropic | условия Anthropic | `tools/naomi claude install` |
| Погода (слой экрана) | [open-meteo.com](https://open-meteo.com) | данные CC BY 4.0 | сама оболочка, по сети |

## Игры

Игр, образов игр и BIOS в проекте нет. Программа игр открывает файлы, которые вы сами положили в `~/Games` на
телефоне; игры Windows ставятся из вашего установщика. Берите только то, на что у вас есть права.

## Названия

Google, Pixel, Android, Google Play, YouTube — товарные знаки Google; Apple, Mac, macOS, iPhone, Magic Keyboard,
Magic Trackpad — Apple; Windows, DirectX — Microsoft; Raspberry Pi — Raspberry Pi Ltd; Ubuntu — Canonical; Claude —
Anthropic; названия игр — их издателей. Они упоминаются только чтобы сказать, с чем проект работает. Проект с этими
компаниями не связан и ими не одобрен.
