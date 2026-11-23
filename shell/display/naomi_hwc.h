// НаомиФон — драйвер экрана для Qt (eglfs device integration): Qt Quick рисует прямо в HWC2
// через libhybris, без Mir и Lomiri. Включает режим панели (90 Гц по умолчанию) и считает реально
// выведенные кадры. Подробности — docs/DISPLAY.md.
#pragma once

#include "naomi_display.h"
#include "present_timer.h"

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtEglFSDeviceIntegration/private/qeglfsdeviceintegration_p.h>

#include <atomic>
#include <mutex>

class QTimer;
class QWindow;

struct ANativeWindow;
struct ANativeWindowBuffer;
struct hwc2_compat_device;
struct hwc2_compat_display;
struct hwc2_compat_layer;

class NaomiHwcIntegration : public QEglFSDeviceIntegration
{
public:
    NaomiHwcIntegration();
    ~NaomiHwcIntegration() override;

    void platformInit() override;
    QSize screenSize() const override { return m_size; }
    QSizeF physicalScreenSize() const override { return m_physicalMm; }
    QDpi logicalDpi() const override;
    QDpi logicalBaseDpi() const override { return {160, 160}; }
    qreal refreshRate() const override { return m_refreshHz; }
    int screenDepth() const override { return 32; }
    QImage::Format screenFormat() const override { return QImage::Format_RGBA8888; }
    QSurfaceFormat surfaceFormatFor(const QSurfaceFormat &input) const override;
    EGLNativeWindowType createNativeWindow(QPlatformWindow *window, const QSize &size,
                                           const QSurfaceFormat &format) override;
    void destroyNativeWindow(EGLNativeWindowType window) override;
    QFunctionPointer platformFunction(const QByteArray &function) const override;
    QEglFSWindow *createWindow(QWindow *window) const override;
    void presentBuffer(QPlatformSurface *surface) override;

    // Новый кадр Qt Quick — только когда прошлый выведен (поток интерфейса, из requestUpdate окна).
    void scheduleUpdate(QWindow *window);

    // Заявка на частоту панели (из любого потока); выполняет поток отрисовки в present().
    bool requestRefresh(int hz);
    // Режим панели — NaomiDisplayMode (из потока интерфейса); вызовы composer — под m_hwcMutex вместе с present().
    bool setDisplayMode(int mode);
    NaomiFrameSnapshot snapshot() const { return m_timer.snapshot(); }
    NaomiDisplaySizes sizes() const
    {
        return {m_size.width(), m_size.height(), m_panel.width(), m_panel.height()};
    }
    static NaomiHwcIntegration *instance() { return s_instance; }

private:
    struct Listener;
    // смена частоты: ждём и меряем → (не та) толчок другим режимом → снова нужный, ждём и меряем
    enum class Switch { Idle, Settling, Kicking, Resettling };

    static void presentCallback(void *self, ANativeWindow *window, ANativeWindowBuffer *buffer);
    void present(ANativeWindowBuffer *buffer);
    void frameSwapped();   // поток интерфейса: обмен буферов кадра кончился
    void flushUpdates();   // поток интерфейса: отдать окнам отложенные кадры
    void readActiveConfig();
    bool applyConfig(int hz);
    void driveRefresh();

    static NaomiHwcIntegration *s_instance;
    Listener *m_listener = nullptr;
    hwc2_compat_device *m_device = nullptr;
    hwc2_compat_display *m_display = nullptr;
    hwc2_compat_layer *m_layer = nullptr;
    ANativeWindow *m_window = nullptr;
    ANativeWindowBuffer *m_shown = nullptr;   // буфер прошлого кадра: свободен, когда панель покажет следующий
    void *m_setConfigSym = nullptr;   // HWC2::impl::Display::setActiveConfigWithConstraints
    QSize m_size;                     // кадр Qt (NAOMI_RENDER — меньше панели)
    QSize m_panel;                    // панель
    bool m_scaled = false;            // кадр меньше панели: слой DEVICE, растягивает дисплей (MDP)
    ANativeWindowBuffer *m_blank = nullptr;   // чёрный кадр во всю панель — клиентский кадр при отказе (blankFrame)
    ANativeWindowBuffer *blankFrame();
    bool m_refusing = false;              // composer сейчас не берёт слой с растяжением (под m_hwcMutex)
    int m_refusedSec = 0;                 // отказов за секунду
    int64_t m_refusedFrom = 0;
    QSizeF m_physicalMm;
    qreal m_density = 2.75;
    PresentTimer m_timer;

    std::mutex m_hwcMutex;                // present() (поток отрисовки) и setDisplayMode() (интерфейс)
    int m_mode = NaomiDisplayOn;          // под m_hwcMutex
    std::atomic<double> m_refreshHz{60};  // читает поток GUI, пишет поток отрисовки
    std::atomic<int> m_requestHz{0};      // заявка, ещё не взятая потоком отрисовки
    // ниже — только поток интерфейса
    QObject m_gui;                        // живёт в потоке интерфейса: туда поток отрисовки шлёт «вывел»
    QTimer *m_watchdog = nullptr;         // отданный кадр так и не дошёл до вывода
    QList<QPointer<QWindow>> m_waiting;   // окна, ждущие своего кадра
    bool m_inFlight = false;              // кадр отдан Qt Quick, его обмен буферов ещё не кончился
    bool m_flushPosted = false;
    // ниже — только поток отрисовки
    uint64_t m_presented = 0;
    Switch m_switch = Switch::Idle;
    int m_targetHz = 0;
    int m_framesSinceSwitch = 0;
    int m_rechecks = 0;                   // «не включилась» — проверить заново (замер сразу после старта врёт)
    int64_t m_recheckAt = 0;              // когда, нс CLOCK_MONOTONIC
};
