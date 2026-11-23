#include "naomi_hwc.h"
#include "naomi_display.h"

#include <QtCore/QDateTime>
#include <QtCore/QLoggingCategory>
#include <QtCore/QThread>
#include <QtCore/QTimer>
#include <QtCore/qplugin.h>
#include <QtEglFSDeviceIntegration/private/qeglfswindow_p.h>
#include <QtGui/QGuiApplication>
#include <QtGui/QWindow>

#include <cmath>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <dlfcn.h>
#include <unistd.h>
#include <utility>

#include <hardware/gralloc.h>
#include <hybris/gralloc/gralloc.h>
#include <hybris/hwc2/hwc2_compatibility_layer.h>
#include <hybris/hwcomposerwindow/hwcomposer.h>

// загрузчик Android-библиотек libhybris (libhybris-common); в hybris/common/binding.h объявлены без
// extern "C", а сами функции — сишные
extern "C" void *android_dlopen(const char *filename, int flag);
extern "C" void *android_dlsym(void *handle, const char *symbol);

Q_LOGGING_CATEGORY(lcNaomiHwc, "naomi.hwc")

NaomiHwcIntegration *NaomiHwcIntegration::s_instance = nullptr;

namespace {

// Режимы панели Pixel 5 (redfin), как их отдаёт composer: id 0 — 60 Гц, id 1 — 90 Гц.
// Проверено device/hwcspike.py; docs/DISPLAY.md.
hwc2_config_t configForHz(int hz) { return hz >= 75 ? 1 : 0; }
int otherHz(int hz) { return hz >= 75 ? 60 : 90; }

// Сколько кадров ждать после смены режима, прежде чем мерить, и сколько интервалов мерить.
constexpr int kSettleFrames = 12;
constexpr int kKickFrames = 6;
constexpr int kMeasureIntervals = 8;

// V2_4::IComposerClient::VsyncPeriodChangeConstraints и V2_4::VsyncPeriodChangeTimeline
struct VsyncConstraints { int64_t desiredTimeNanos; bool seamlessRequired; };
struct VsyncTimeline { int64_t newVsyncAppliedTimeNanos; bool refreshRequired; int64_t refreshTimeNanos; };
using SetActiveConfigFn = int32_t (*)(void *display, uint32_t config, const VsyncConstraints *,
                                      VsyncTimeline *);

const char kSetConfigSymbol[] =
    "_ZN7android4HWC24impl7Display30setActiveConfigWithConstraintsEjRKNS_8hardware8graphics8"
    "composer4V2_415IComposerClient28VsyncPeriodChangeConstraintsEPNS6_25VsyncPeriodChangeTimelineE";

void frameSnapshot(NaomiFrameSnapshot *out)
{
    if (auto *self = NaomiHwcIntegration::instance())
        *out = self->snapshot();
    else
        *out = {};
}

void displaySizes(NaomiDisplaySizes *out)
{
    if (auto *self = NaomiHwcIntegration::instance())
        *out = self->sizes();
    else
        *out = {};
}

bool requestRefresh(int hz)
{
    auto *self = NaomiHwcIntegration::instance();
    return self && self->requestRefresh(hz);
}

bool setDisplayMode(int mode)
{
    auto *self = NaomiHwcIntegration::instance();
    return self && self->setDisplayMode(mode);
}

const char *modeName(int mode)
{
    return mode == NaomiDisplayOn ? "включён" : mode == NaomiDisplayDoze ? "в малом питании" : "выключен";
}

// Окно Qt на экране: кадр по выводу прошлого (NaomiHwcIntegration::scheduleUpdate).
class NaomiHwcWindow : public QEglFSWindow
{
public:
    using QEglFSWindow::QEglFSWindow;
    void requestUpdate() override
    {
        if (auto *self = NaomiHwcIntegration::instance())
            self->scheduleUpdate(window());
        else
            QEglFSWindow::requestUpdate();
    }
};

} // namespace

struct NaomiHwcIntegration::Listener : HWC2EventListener
{
    NaomiHwcIntegration *self;
};

NaomiHwcIntegration::NaomiHwcIntegration()
{
    s_instance = this;
}

NaomiHwcIntegration::~NaomiHwcIntegration()
{
    s_instance = nullptr;
    delete m_listener;
}

void NaomiHwcIntegration::platformInit()
{
    setenv("EGL_PLATFORM", "hwcomposer", 0);

    m_device = hwc2_compat_device_new(false);
    if (!m_device)
        qFatal("naomi.hwc: нет устройства HWC2 — экраном уже владеет другой композитор?");

    m_listener = new Listener;
    m_listener->on_vsync_received = [](HWC2EventListener *, int32_t, hwc2_display_t, int64_t) {};
    m_listener->on_hotplug_received = [](HWC2EventListener *l, int32_t, hwc2_display_t display,
                                         bool connected, bool) {
        hwc2_compat_device_on_hotplug(static_cast<Listener *>(l)->self->m_device, display, connected);
    };
    m_listener->on_refresh_received = [](HWC2EventListener *, int32_t, hwc2_display_t) {};
    m_listener->self = this;
    hwc2_compat_device_register_callback(m_device, m_listener, 0);

    for (int i = 0; i < 50 && !m_display; ++i) {
        m_display = hwc2_compat_device_get_display_by_id(m_device, 0);
        if (!m_display)
            QThread::msleep(100);
    }
    if (!m_display)
        qFatal("naomi.hwc: дисплей 0 не появился");

    hwc2_compat_display_set_power_mode(m_display, HWC2_POWER_MODE_ON);

    void *lib = android_dlopen("libhwc2_compat_layer.so", RTLD_NOW);
    m_setConfigSym = lib ? android_dlsym(lib, kSetConfigSymbol) : nullptr;
    if (!m_setConfigSym)
        qCWarning(lcNaomiHwc, "нет setActiveConfigWithConstraints — частота останется как есть");

    readActiveConfig();
    m_panel = m_size;
    // Частоту включает поток отрисовки, когда кадры уже идут (driveRefresh): режим, заданный до
    // первого кадра, composer запоминает, а панель остаётся на старом. Qt сразу знает целевую.
    const QByteArray hz = qgetenv("NAOMI_REFRESH");
    const int initialHz = hz.isEmpty() ? 90 : hz.toInt();
    m_refreshHz = initialHz;
    requestRefresh(initialHz);

    // Кадр меньше панели (NAOMI_RENDER — ширина, например 720): Qt рисует 720 × 1560, а на панель его растягивает
    // дисплей (MDP) — слой DEVICE с обрезкой по кадру и местом во всю панель. Видеочипу работы меньше, растяжение
    // бесплатно (docs/DISPLAY.md, «Кадр меньше панели»). Плотность — в той же доле: экран в dp тот же.
    if (const int w = qgetenv("NAOMI_RENDER").toInt(); w >= 360 && w < m_panel.width()) {
        const int h = qRound(double(m_panel.height()) * w / m_panel.width() / 2) * 2;
        m_size = QSize(w, h);
        m_scaled = true;
    }
    const QByteArray d = qgetenv("NAOMI_DENSITY");
    m_density = (d.isEmpty() ? 2.75 : d.toDouble()) * m_size.width() / m_panel.width();

    m_layer = hwc2_compat_display_create_layer(m_display);
    hwc2_compat_layer_set_composition_type(m_layer, m_scaled ? HWC2_COMPOSITION_DEVICE : HWC2_COMPOSITION_CLIENT);
    hwc2_compat_layer_set_blend_mode(m_layer, HWC2_BLEND_MODE_NONE);
    hwc2_compat_layer_set_source_crop(m_layer, 0, 0, m_size.width(), m_size.height());
    hwc2_compat_layer_set_display_frame(m_layer, 0, 0, m_panel.width(), m_panel.height());
    hwc2_compat_layer_set_visible_region(m_layer, 0, 0, m_panel.width(), m_panel.height());

    qCInfo(lcNaomiHwc, "панель %dx%d, кадр %dx%d%s, %.1f мм × %.1f мм, плотность %.4g, нужно %d Гц",
           m_panel.width(), m_panel.height(), m_size.width(), m_size.height(),
           m_scaled ? " (растягивает дисплей)" : "", m_physicalMm.width(), m_physicalMm.height(), m_density,
           initialHz);
}

void NaomiHwcIntegration::readActiveConfig()
{
    HWC2DisplayConfig *c = hwc2_compat_display_get_active_config(m_display);
    if (!c)
        qFatal("naomi.hwc: нет активного режима дисплея");
    m_size = QSize(c->width, c->height);
    // dpi из composer; если пустой — паспорт Pixel 5: 432 точки на дюйм
    const qreal dpiX = c->dpiX > 1 ? c->dpiX : 432, dpiY = c->dpiY > 1 ? c->dpiY : 432;
    m_physicalMm = QSizeF(c->width / dpiX * 25.4, c->height / dpiY * 25.4);
    free(c);
}

bool NaomiHwcIntegration::requestRefresh(int hz)
{
    if (!m_setConfigSym || (hz != 60 && hz != 90))
        return false;
    m_requestHz = hz;
    return true;
}

// Поток отрисовки. Внутренний setActiveConfigWithConstraints (в C API libhybris его нет).
bool NaomiHwcIntegration::applyConfig(int hz)
{
    void *hwcDisplay = *reinterpret_cast<void **>(m_display);  // hwc2_compat_display { HWC2::Display *self; }
    VsyncConstraints constraints{0, false};
    VsyncTimeline timeline{};
    const int32_t err = reinterpret_cast<SetActiveConfigFn>(m_setConfigSym)(
        hwcDisplay, configForHz(hz), &constraints, &timeline);
    if (err != 0)
        qCWarning(lcNaomiHwc, "режим %d Гц: composer отказал (ошибка %d)", hz, err);
    return err == 0;
}

// Поток отрисовки, после каждого present(). Частоту подтверждают реальные интервалы вывода, а не
// ответ composer: он может считать режим включённым, когда панель на старом (бывает, если режим
// задали до первого кадра). Тогда «толчок»: на несколько кадров другой режим и снова нужный.
void NaomiHwcIntegration::driveRefresh()
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const int64_t nowNs = int64_t(ts.tv_sec) * 1000000000 + ts.tv_nsec;
    if (m_switch == Switch::Idle && m_recheckAt && nowNs >= m_recheckAt) {   // проверка заново: режим и замер
        m_recheckAt = 0;
        m_requestHz = m_targetHz;
    }
    if (const int hz = m_requestHz.exchange(0)) {
        if (hz != m_targetHz)   // новая частота (не повтор проверки) — повторов снова три
            m_rechecks = 0;
        m_targetHz = hz;
        m_refreshHz = hz;
        m_timer.setPeriodNs(int64_t(1e9 / hz));
        m_switch = applyConfig(hz) ? Switch::Settling : Switch::Idle;
        m_framesSinceSwitch = 0;
        return;
    }
    if (m_switch == Switch::Idle)
        return;
    ++m_framesSinceSwitch;
    if (m_switch == Switch::Kicking) {
        if (m_framesSinceSwitch >= kKickFrames) {
            applyConfig(m_targetHz);
            m_switch = Switch::Resettling;
            m_framesSinceSwitch = 0;
        }
        return;
    }
    if (m_framesSinceSwitch < kSettleFrames)
        return;

    const double measuredMs = m_timer.recentMedianMs(kMeasureIntervals);
    if (measuredMs <= 0)
        return;  // метки ещё не накопились (fence приходят с задержкой)
    // Интервалы длиннее 25 мс — не частота панели (её режимы — 90 и 60 Гц, 11,1 и 16,7 мс), а паузы в рисовании:
    // экран ничем не занят или оболочка только стартует. По ним не судить — ждать сплошных кадров (любая анимация);
    // иначе после перезагрузки выходило «не включилась: 33 мс» при панели на 90 Гц
    if (measuredMs > 25)
        return;
    const double wantedMs = 1000.0 / m_targetHz;
    if (std::fabs(measuredMs - wantedMs) < 1.0) {
        qCInfo(lcNaomiHwc, "частота %d Гц: подтверждена, кадр %.2f мс%s", m_targetHz, measuredMs,
               m_switch == Switch::Resettling ? " (после толчка)" : "");
        m_switch = Switch::Idle;
        m_rechecks = 0;
    } else if (m_switch == Switch::Settling) {
        qCInfo(lcNaomiHwc, "частота %d Гц: панель даёт %.2f мс — толчок через %d Гц", m_targetHz,
               measuredMs, otherHz(m_targetHz));
        applyConfig(otherHz(m_targetHz));
        m_switch = Switch::Kicking;
        m_framesSinceSwitch = 0;
    } else {
        // Не включилась — или замер соврал: сразу после старта оболочка сама рисует реже панели (кадр 33 мс при
        // панели на 90 Гц — после перезагрузки телефона, 16 ноября 2026). Ещё до трёх проверок через 2 с
        qCWarning(lcNaomiHwc, "частота %d Гц не включилась: кадр %.2f мс%s", m_targetHz, measuredMs,
                  m_rechecks < 3 ? " — проверю снова через 2 с" : "");
        m_refreshHz = 1000.0 / measuredMs;
        m_timer.setPeriodNs(int64_t(measuredMs * 1e6));
        m_switch = Switch::Idle;
        if (m_rechecks < 3) {
            ++m_rechecks;
            m_recheckAt = nowNs + 2000000000LL;
        }
    }
}

QDpi NaomiHwcIntegration::logicalDpi() const
{
    // логический пиксель Qt = 1 dp Android: 160 × плотность (Pixel 5 — 2,75, как xxhdpi)
    return {160 * m_density, 160 * m_density};
}

QSurfaceFormat NaomiHwcIntegration::surfaceFormatFor(const QSurfaceFormat &input) const
{
    QSurfaceFormat f = input;
    // Qt 6 в Ubuntu собран под настольный OpenGL, а драйвер Adreno (Android EGL) умеет только
    // OpenGL ES — без этого EGL не найдёт ни одной конфигурации. Qt Quick (RHI) работает и с ES.
    f.setRenderableType(QSurfaceFormat::OpenGLES);
    f.setVersion(3, 0);
    f.setRedBufferSize(8);
    f.setGreenBufferSize(8);
    f.setBlueBufferSize(8);
    f.setAlphaBufferSize(8);
    return f;
}

EGLNativeWindowType NaomiHwcIntegration::createNativeWindow(QPlatformWindow *, const QSize &,
                                                            const QSurfaceFormat &)
{
    m_window = HWCNativeWindowCreate(m_size.width(), m_size.height(), HAL_PIXEL_FORMAT_RGBA_8888,
                                     &NaomiHwcIntegration::presentCallback, this);
    if (m_scaled && !m_blank) {
        std::lock_guard<std::mutex> lock(m_hwcMutex);
        m_blank = blankFrame();
    }
    return (EGLNativeWindowType)m_window;
}

// Чёрный кадр размером с панель — клиентский кадр, когда composer не берёт слой с растяжением (present). Буфер — из
// второго окна libhybris размером с панель: его только берём (dequeue) и не возвращаем, окно живёт до выхода (как и
// главное — HWCNativeWindowDestroy падает). Заливка нулями — через gralloc; не вышло — память ION и так нулевая.
ANativeWindowBuffer *NaomiHwcIntegration::blankFrame()
{
    ANativeWindow *win = HWCNativeWindowCreate(m_panel.width(), m_panel.height(), HAL_PIXEL_FORMAT_RGBA_8888,
                                               [](void *, ANativeWindow *, ANativeWindowBuffer *) {}, nullptr);
    if (!win) {
        qCWarning(lcNaomiHwc, "чёрного кадра нет: окно %dx%d не создалось", m_panel.width(), m_panel.height());
        return nullptr;
    }
    native_window_set_buffer_count(win, 1);
    native_window_set_usage(win, GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_SW_WRITE_RARELY);
    ANativeWindowBuffer *b = nullptr;
    int fence = -1;
    if (win->dequeueBuffer(win, &b, &fence) != 0 || !b) {
        qCWarning(lcNaomiHwc, "чёрного кадра нет: буфер %dx%d не выдан", m_panel.width(), m_panel.height());
        return nullptr;
    }
    if (fence >= 0)
        close(fence);   // новый буфер: его никто не рисует
    void *pixels = nullptr;
    const bool filled = hybris_gralloc_lock(b->handle, GRALLOC_USAGE_SW_WRITE_RARELY, 0, 0, b->width, b->height,
                                            &pixels) == 0 && pixels;
    if (filled) {
        memset(pixels, 0, size_t(b->stride) * size_t(b->height) * 4);
        hybris_gralloc_unlock(b->handle);
    }
    qCInfo(lcNaomiHwc, "чёрный кадр %dx%d на случай отказа composer%s", b->width, b->height,
           filled ? "" : " (не залит: gralloc не дал записать)");
    return b;
}

// HWCNativeWindowDestroy в libhybris делает `delete` через указатель на сишную ANativeWindow, а не на
// настоящий класс окна — адрес не тот, процесс падает при выходе («free(): invalid pointer», SIGABRT,
// systemd считает это падением). Окно одно на всё время работы оболочки: при выходе его освобождает
// система, поэтому не удаляем.
void NaomiHwcIntegration::destroyNativeWindow(EGLNativeWindowType)
{
    m_window = nullptr;
}

void NaomiHwcIntegration::presentCallback(void *self, ANativeWindow *, ANativeWindowBuffer *buffer)
{
    static_cast<NaomiHwcIntegration *>(self)->present(buffer);
}

// Поток отрисовки, после eglSwapBuffers: кадр отдан, и следующий буфер уже получен — после present() окно
// libhybris ещё ждёт, пока панель отпустит буфер (до vsync), и поток отрисовки до того занят.
void NaomiHwcIntegration::presentBuffer(QPlatformSurface *)
{
    QMetaObject::invokeMethod(&m_gui, [this] { frameSwapped(); }, Qt::QueuedConnection);
}

QEglFSWindow *NaomiHwcIntegration::createWindow(QWindow *window) const
{
    return new NaomiHwcWindow(window);
}

// Когда начинать кадр (docs/SHELL.md, «Касания» и «Окна программ»). Поток отрисовки Qt Quick занят кадром до
// конца eglSwapBuffers, а тот ждёт vsync (≈ 10 мс из 11). Попросит поток интерфейса новый кадр раньше — он
// встаёт в синхронизацию и стоит до vsync. Стоя, он не читает Wayland: кадр программы и её «рисуй дальше»
// (frame callback) проходили только через кадр — пока палец на экране, программы рисовали 45 кадров в
// секунду. Поэтому кадров в пути не больше одного: новый Qt Quick получает, когда обмен буферов прошлого
// кончился (как vsync у Android и Wayland), а если в пути ничего — сразу. Каждый отданный кадр Qt 6 доводит
// до обмена буферов (кадров «без изменений» не бывает); сторож — на случай, если окно не готово рисовать.
void NaomiHwcIntegration::scheduleUpdate(QWindow *window)
{
    if (!m_waiting.contains(window))
        m_waiting.append(window);
    if (m_inFlight || m_flushPosted)
        return;
    m_flushPosted = true;   // не изнутри requestUpdate — из цикла событий
    QMetaObject::invokeMethod(&m_gui, [this] {
        m_flushPosted = false;
        flushUpdates();
    }, Qt::QueuedConnection);
}

void NaomiHwcIntegration::frameSwapped()
{
    m_inFlight = false;
    if (m_watchdog)
        m_watchdog->stop();
    flushUpdates();
}

void NaomiHwcIntegration::flushUpdates()
{
    if (m_inFlight || m_waiting.isEmpty())
        return;
    if (!m_watchdog) {
        m_watchdog = new QTimer(&m_gui);
        m_watchdog->setSingleShot(true);
        QObject::connect(m_watchdog, &QTimer::timeout, &m_gui, [this] {
            qCWarning(lcNaomiHwc, "кадр не дошёл до вывода за 100 мс — следующий без ожидания");
            frameSwapped();
        });
    }
    const QList<QPointer<QWindow>> waiting = std::exchange(m_waiting, {});
    for (const QPointer<QWindow> &w : waiting) {
        if (!w || !w->handle() || !w->handle()->hasPendingUpdateRequest())
            continue;
        m_inFlight = true;          // до deliver: внутри него Qt Quick уже просит следующий кадр
        m_watchdog->start(100);
        w->handle()->deliverUpdateRequest();
    }
}

// Малое питание: частоту в нём не трогаем (она — панели); после него, как и после выключения, — заново.
bool NaomiHwcIntegration::setDisplayMode(int mode)
{
    std::lock_guard<std::mutex> lock(m_hwcMutex);
    if (mode == m_mode)
        return true;
    const hwc2_power_mode_t hwcMode = mode == NaomiDisplayOn ? HWC2_POWER_MODE_ON
                                    : mode == NaomiDisplayDoze ? HWC2_POWER_MODE_DOZE : HWC2_POWER_MODE_OFF;
    const hwc2_error_t err = hwc2_compat_display_set_power_mode(m_display, hwcMode);
    if (err != HWC2_ERROR_NONE) {
        qCWarning(lcNaomiHwc, "дисплей %s: ошибка %d", modeName(mode), err);
        return false;
    }
    m_mode = mode;
    qCInfo(lcNaomiHwc, "дисплей %s", modeName(mode));
    if (mode == NaomiDisplayOn)  // панель может вернуться на 60 Гц — заново, с проверкой замером
        requestRefresh(m_targetHz ? m_targetHz : int(m_refreshHz.load()));
    return true;
}

// Вызывается из eglSwapBuffers (поток отрисовки Qt Quick): отдать буфер панели.
void NaomiHwcIntegration::present(ANativeWindowBuffer *buffer)
{
    std::lock_guard<std::mutex> lock(m_hwcMutex);
    const int acquire = HWCNativeBufferGetFence(buffer);  // готовность отрисовки (GPU)
    if (m_mode == NaomiDisplayOff) {  // дисплей выключен: кадр не выводим, буфер сразу свободен; панель не держит ни один
        if (acquire >= 0)
            close(acquire);
        HWCNativeBufferSetFence(buffer, -1);
        m_shown = nullptr;
        return;
    }
    // Кадр меньше панели — слой DEVICE: буфер и его готовность — слою (до validate: composer выбирает по нему). Вид
    // слоя — заново каждый кадр: сразу после перезагрузки телефона composer на первый кадр просит «составлю сам»
    // (CLIENT), и accept_changes делал слой клиентским навсегда — экран оставался чёрным
    if (m_scaled) {
        hwc2_compat_layer_set_composition_type(m_layer, HWC2_COMPOSITION_DEVICE);
        hwc2_compat_layer_set_buffer(m_layer, 0, buffer, acquire);
    }
    uint32_t types = 0, requests = 0;
    hwc2_error_t err = hwc2_compat_display_validate(m_display, &types, &requests);
    if (err != HWC2_ERROR_NONE && err != HWC2_ERROR_HAS_CHANGES) {
        qCWarning(lcNaomiHwc, "validate: ошибка %d", err);
        if (acquire >= 0 && !m_scaled)
            close(acquire);
        HWCNativeBufferSetFence(buffer, -1);
        return;
    }
    // Отказ (хочет составлять сам): после перезагрузки телефона composer так отвечает, пока не покажет хоть один
    // кадр, составленный им самим (замер: кадры без показа — 570 отказов в секунду, экран чёрный). Показываем ему
    // чёрный кадр во всю панель клиентским кадром (blankFrame), а кадр Qt рисуем снова — он уже слоем DEVICE. Свой
    // кадр клиентским отдавать нельзя: composer (SDM Qualcomm) подгоняет размер кадра дисплея под клиентский буфер —
    // после буфера 720 × 1560 смеситель дисплея становился 720 × 1560, место слоя (вся панель) обрезалось по нему, и
    // на экране был левый верхний угол картинки в 1,5 раза крупнее — до конца работы composer (16 ноября 2026, журнал
    // драйвера: смеситель 2 × 360 × 1560, из кадра — 480 × 1040; docs/DISPLAY.md, «Кадр меньше панели»)
    const bool refused = m_scaled && types;
    if (refused) {
        ++m_refusedSec;
        if (!std::exchange(m_refusing, true))
            qCWarning(lcNaomiHwc, "composer не берёт слой с растяжением (хочет составлять сам) — чёрный кадр во всю "
                                  "панель, кадр — снова");
    }
    if (const int64_t now = QDateTime::currentMSecsSinceEpoch(); now - m_refusedFrom >= 1000) {
        if (m_refusedSec)
            qCWarning(lcNaomiHwc, "composer отказал в растяжении: %d кадров за секунду", m_refusedSec);
        m_refusedSec = 0;
        m_refusedFrom = now;
    }
    if (refused && !m_blank) {   // чёрного кадра нет (в журнале — «чёрного кадра нет») — экран останется чёрным
        HWCNativeBufferSetFence(buffer, -1);
        return;
    }
    if (types || requests)
        hwc2_compat_display_accept_changes(m_display);
    if (!m_scaled)
        hwc2_compat_display_set_client_target(m_display, 0, buffer, acquire, HAL_DATASPACE_UNKNOWN);
    else if (refused)
        hwc2_compat_display_set_client_target(m_display, 0, m_blank, -1, HAL_DATASPACE_UNKNOWN);
    if (m_scaled && !refused && std::exchange(m_refusing, false))
        qCInfo(lcNaomiHwc, "composer снова берёт слой с растяжением");
    int32_t presentFence = -1;
    err = hwc2_compat_display_present(m_display, &presentFence);
    if (err != HWC2_ERROR_NONE)
        qCWarning(lcNaomiHwc, "present: ошибка %d", err);
    if (refused) {
        // кадр Qt не показан — его буфер свободен сразу; прошлый показанный — когда панель покажет чёрный
        HWCNativeBufferSetFence(buffer, -1);
        if (m_shown && m_shown != buffer)
            HWCNativeBufferSetFence(m_shown, presentFence >= 0 ? dup(presentFence) : -1);
        m_shown = nullptr;
        if (presentFence >= 0)
            close(presentFence);
        QMetaObject::invokeMethod(qApp, [] {   // QQuickWindow::update — полный кадр, даже если сцена та же
            for (QWindow *w : QGuiApplication::topLevelWindows())
                QMetaObject::invokeMethod(w, "update");
        }, Qt::QueuedConnection);
        return;
    }

    // Когда буфер свободен. Клиентский слой (client target): getReleaseFences отдаёт метки только слоям, которые
    // составляет сам дисплей, — для него всегда −1 (замер). С этой −1 окно libhybris (два буфера по кругу) отдавало
    // буфер под новый кадр сразу — видеочип рисовал в буфер, который панель ещё показывала: при прокрутке —
    // горизонтальные полосы и белые точки у краёв (docs/DISPLAY.md, «Буферы»). Как в Android (FramebufferSurface):
    // буфер прошлого кадра панель отпускает, когда показала этот, — его метка — present fence этого кадра (копия:
    // сам fence забирает таймер). У этого буфера метка появится со следующим кадром; готовность (acquire) ушла в
    // composer вместе с ним. Слой DEVICE (кадр меньше панели) получает свою метку — она и есть «прошлый буфер
    // отпущен» (по HWC2 метка present относится к буферу, который этот кадр сменил); нет её — та же present fence.
    int32_t release = -1;
    if (m_scaled) {
        hwc2_compat_out_fences_t *fences = nullptr;
        if (hwc2_compat_display_get_release_fences(m_display, &fences) == HWC2_ERROR_NONE && fences) {
            release = hwc2_compat_out_fences_get_fence(fences, m_layer);
            hwc2_compat_out_fences_destroy(fences);
        }
    }
    if (m_shown && m_shown != buffer)
        HWCNativeBufferSetFence(m_shown, release >= 0 ? release : presentFence >= 0 ? dup(presentFence) : -1);
    else if (release >= 0)
        close(release);
    HWCNativeBufferSetFence(buffer, -1);
    m_shown = buffer;
    if (presentFence >= 0)
        m_timer.add(presentFence);
    if (++m_presented >= 2 && m_mode == NaomiDisplayOn)   // в малом питании частота — панели
        driveRefresh();
}

QFunctionPointer NaomiHwcIntegration::platformFunction(const QByteArray &function) const
{
    if (function == kNaomiFrameSnapshot)
        return QFunctionPointer(&frameSnapshot);
    if (function == kNaomiDisplaySizes)
        return QFunctionPointer(&displaySizes);
    if (function == kNaomiSetRefresh)
        return QFunctionPointer(&::requestRefresh);
    if (function == kNaomiSetDisplayMode)
        return QFunctionPointer(&::setDisplayMode);
    return nullptr;
}

class NaomiHwcPlugin : public QEglFSDeviceIntegrationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QEglFSDeviceIntegrationFactoryInterface_iid FILE "eglfs_naomi.json")
public:
    QEglFSDeviceIntegration *create() override { return new NaomiHwcIntegration; }
};

#include "naomi_hwc.moc"
