// НаомиФон — ввод зрителя трансляции (docs/STREAM.md, «Управление»): касания, прокрутка, клавиши — прямо в окно
// оболочки, как от своих устройств (QWindowSystemInterface). Дальше — обычный путь: фильтр питания (будит
// экран, сбрасывает гашение), жесты оболочки, окна программ (касания — wl_touch, прокрутка — wl_pointer,
// клавиши — объекту с фокусом). Мышь — палец; прокрутка трекпадом — колесо с точным сдвигом.
#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QTimer>

class QPointingDevice;
class QQuickWindow;
class StreamScreen;

class QMouseEvent;
class QWaylandSeat;
class QWaylandQuickItem;

class StreamInput : public QObject
{
    Q_OBJECT
public:
    explicit StreamInput(QObject *parent = nullptr);
    void setWindow(QQuickWindow *window);
    // зритель смотрит рабочий стол (docs/DESKTOP.md): мышь, прокрутка и клавиши — ему, телефон не трогается;
    // nullptr — снова телефону
    void setDesktop(StreamScreen *desktop);

    void touch(int phase, int id, double x, double y);
    void scroll(double x, double y, double dx, double dy, int phase);
    void key(bool down, int qtKey, int modifiers, const QString &text);
    void nav(int action);
    void pointer(int buttons, double x, double y);   // мышь стола: кнопки (1 — левая, 2 — правая, 4 — средняя)
    void releaseAll();   // зритель ушёл: отпустить пальцы
    // прицел скриншота области на столе: Esc — ему (screenshotKey 0), а не тому, что в фокусе
    void setShotSelecting(bool on) { m_shotSelecting = on; }

signals:
    void activity();     // клавиша зрителя: гашение экрана — заново (касания это делают сами, через окно)
    void volumeKey(int step);   // клавиша громкости зрителя на столе: +1, −1, 0 — «без звука» (не полю в фокусе)
    // скриншот стола (docs/DESKTOP.md, «Скриншоты»): 3 — Shift+Alt+3 (весь стол), 4 — Shift+Alt+4 (область),
    // 0 — Esc при прицеле; столу, а не тому, что в фокусе
    void screenshotKey(int key);
    // клавиши плеера (Qt::Key_MediaTogglePlayPause, Key_MediaNext, Key_MediaPrevious) — играющему, а не тому, что в
    // фокусе; смотрят ли стол — не важно
    void mediaKey(int qtKey);
    void brightnessKey(int step);   // яркость монитора зрителя, пока смотрят стол: +1 — ярче, −1 — темнее
    // клавиша блокировки: выключатель стола (решает служба); dim — «луна»: уходя, монитор приглушить
    void lockKey(bool dim);
    void pointerSent();         // мышь стола отдана окну: форма курсора под ней могла смениться

private:
    QPointF toNative(double x, double y) const;
    QPointF toDesktop(double x, double y) const;
    void sendToDesktop(QEvent *event);
    void sendTouch(int id, int state, QPointF pos);
    static bool acceptsText(QObject *focus);   // поле ввода (принимает метод ввода)
    // клавиша окну Android (docs/ANDROID.md): модификаторы — нажатиями их клавиш
    void keyToAndroid(QWaylandQuickItem *app, bool down, int qtKey, int modifiers, const QString &text);
    // клавиша окну игры (docs/GAMES.md): по месту клавиши, без модификаторов
    void keyToGame(QWaylandQuickItem *app, bool down, int qtKey, const QString &text);

    QPointer<QQuickWindow> m_window;
    StreamScreen *m_desktop = nullptr;
    bool m_shotSelecting = false;
    QList<int> m_shotKeys;        // нажаты сочетанием скриншота: их отпускание — тоже не программам
    QList<int> m_onceKeys;        // клавиши плеера и блокировки нажаты: держат — повтор клавиатуры, а действие одно
    QHash<int, int> m_cmdKeys;    // ⌘C, ⌘V… нажаты: клавиша → модификаторы, с которыми её отдали (Ctrl вместо ⌘)
    int m_buttons = 0;            // нажатые кнопки мыши стола
    void holdModifiers(Qt::KeyboardModifiers mods);
    void releaseModifiers();
    QList<uint> m_held;           // Ctrl, Shift, зажатые окну программы стола на время щелчка
    QPointer<QWaylandSeat> m_heldSeat;
    Qt::KeyboardModifiers m_androidMods;   // какие модификаторы окну Android посланы нажатыми
    QPointF m_pointerAt;          // где мышь стола, точки
    QElapsedTimer m_click;        // прошлое нажатие мыши стола: двойной щелчок
    void sendMouse(QMouseEvent &e);
    QPointF m_pressAt;            // где и когда нажата первая кнопка мыши стола (sendMouse)
    ulong m_pressTime = 0;
    QPointF m_clickAt;
    Qt::MouseButton m_clickButton = Qt::NoButton;
    QPointingDevice *m_touch = nullptr;
    QPointingDevice *m_mouse = nullptr;
    QHash<int, QPointF> m_down;   // пальцы на экране: id → место (пиксели экрана)
    QTimer m_scrollEnd;           // зритель не прислал конец прокрутки — сами
    bool m_scrolling = false;
    QPointF m_scrollAt;
    QTimer m_gestureTimer;        // «назад» / «домой» / шторка — палец ведётся по шагам
    QPointF m_gFrom, m_gTo;
    int m_gStep = 0, m_gSteps = 0;
};
