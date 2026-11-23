# Что есть в телефоне

Опись чистого Ubuntu Touch 24.04 (порт redfin, Lomiri) от 10 ноября 2026: `tools/naomi probe`
(код — `device/probe.py`, полный JSON — `reports/`, не в git). Это **наличие** железа и служб;
работает ли каждое устройство по-настоящему, проверяют тесты этапа 2.

## Железо

| Что | Значение | Где в системе |
|---|---|---|
| Процессор | SM7250 (LITO): 6 × Cortex-A55 до 1,8 ГГц, 1 × A76 до 2,2 ГГц, 1 × A76 до 2,4 ГГц | `cpufreq/policy0`, `policy6`, `policy7`, регулятор `schedutil` |
| Простой ядра | C0 (60 мкс), C1 (901 мкс), C2 (915 мкс) | `cpu0/cpuidle` |
| Графика | Adreno 620, 275 / 400 / 500 / 625 МГц, регулятор `msm-adreno-tz` | `/sys/class/kgsl/kgsl-3d0` |
| Дисплей | 1080 × 2340, режимы **60 и 90 Гц** (command mode) | `/sys/class/drm/card0-DSI-1/modes` |
| Подсветка | 0…1023 | `/sys/class/backlight/panel0-backlight` |
| Тепло | 98 зон. Главные: `skin-therm` (корпус), `cpu-1-max-step`, `gpuss-max-step`, `battery`, `disp-therm` | `/sys/class/thermal` |
| Батарея | 3829 из 4169 мА·ч по паспорту (92%), 324 цикла; ток и напряжение в реальном времени | `/sys/class/power_supply/battery` |
| Сон | `deep` поддерживается (`/sys/power/mem_sleep`) | — |
| Ввод | `qpnp_pon` (питание), `gpio-keys` (громкость), `sec_touchscreen`, `uinput-fpc` (отпечаток) | `/proc/bus/input/devices` |
| Модем | `/ril_0` в oFono | `org.ofono` |
| Wi-Fi | `wlan0`, NetworkManager | — |
| Bluetooth | `hci0`, контроллер Pixel 5 | BlueZ + bluebinder |
| Звук | карта `sm7250-noextcodec`; в PulseAudio — выход и вход через `module-droid-card` | `pactl` |
| Датчики (sensorfw) | акселерометр, гироскоп, компас, магнитометр, освещённость, приближение, давление, ориентация, поворот | `com.nokia.SensorService` |
| USB | гаджет: ADB, MTP, RNDIS, mass storage; в ядре есть и NCM. У нас — ADB + NCM (`naomi-usb`, [DEVELOPMENT.md](DEVELOPMENT.md)) | `/sys/kernel/config/usb_gadget` |

Не температуры, хоть и лежат в `thermal`: `soc` (274 000), `pm7250b-*-lvl*`, зоны с −273°.

Вибромотор — `/sys/class/leds/vibrator` с триггером `transient`; вибрировать — через `hfd`, руками в sysfs не
писать. Служба вибрации Android `vendor.vibrator.redfin` 10 ноября 2026 падала каждые 5 с (статус 1); 19 ноября 2026 — работает
с загрузки под одним pid, в журнале Android о ней ничего ([AUDIT.md](AUDIT.md), этап 8). При `state` = 0 запись
`activate` = 1 держит мотор выключенным `duration` мс, а потом включает навсегда (`state` — состояние на время
`duration`): 12 ноября 2026 так он прожужжал 13 минут. Остановить — `brightness` = 0 (триггер снимается,
вернуть — `transient` в `trigger`).

## Что запущено со штатной оболочкой

- 652 процесса; свободно 5,7 из 7,5 ГиБ памяти.
- Больше всех памяти: `lomiri` — 413 МиБ, Android-служба камеры — 167 МиБ, клавиатура `maliit-server` —
  122 МиБ, `qcrild` (радио) — 51 МиБ, `lomiri-system-compositor` — 50 МиБ.
- 31 системная служба, 48 служб пользователя (почти все — Lomiri, индикаторы, telepathy, evolution).
- В Android-контейнере работает 104 службы: драйверы дисплея, камеры, звука, радио, IMS, датчиков,
  GPS, Wi-Fi, Bluetooth, NFC, вибрации, температуры, питания.
- Упали две службы: `dpkg-db-backup`, `logrotate` — им мешает корень только для чтения.
- Первая загрузка — 5 мин 55 с, из них 5 мин 36 с — `snapd.seeded.service`.
  Обычную загрузку замерим отдельно.

## Доступ разработчика

SSH по ключу через USB-кабель (`tools/naomi ssh`, сеть по кабелю — [DEVELOPMENT.md](DEVELOPMENT.md),
«Кабель»), `sudo` без пароля для `phablet` — настройка в [INSTALL.md](INSTALL.md), шаг 6.
