# Заголовки Qt WebEngine

Пакета `-dev` Qt WebEngine в среде сборки нет (WEB.md), а перехватчику запросов (`web_filter.*`, блокировка
рекламы и слежки) нужны три заголовка API. Взяты как есть из исходников **Qt WebEngine 6.10.2** — той же версии,
что в системе телефона (`libqt6webenginecore6 6.10.2`): https://github.com/qt/qtwebengine/tree/v6.10.2/src/core/api,
16 ноября 2026. Лицензия — LGPL-3.0 / GPL-2.0 / GPL-3.0 (строка SPDX в каждом файле).

| Файл | SHA-256 |
|---|---|
| `QtWebEngineCore/qtwebenginecoreglobal.h` | `e32fc15b50ff9e24429cbab55b4a12a7e60cb407a94685b42427209fe80dd4e5` |
| `QtWebEngineCore/qwebengineurlrequestinfo.h` | `cecca164f4d63089945d332b2a74f96fb907f94f18acd63bde0489be6f1cca87` |
| `QtWebEngineCore/qwebengineurlrequestinterceptor.h` | `da67ded96d5f3380957b5e6a921c7bf00ed2c255814c0a44ceed22ed15574720` |

`QtWebEngineCore/qtwebenginecore-config.h` — не из Qt: там он генерируется сборкой; здесь пустой.
