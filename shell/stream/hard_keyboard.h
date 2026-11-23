// НаомиФон — настоящая клавиатура, подключённая к самому телефону (Bluetooth; docs/DESKTOP.md, «Клавиатура
// Bluetooth»). Оболочка забирает её себе (evdev, EVIOCGRAB — экран телефона клавиш мимо нас не получит) и переводит
// клавиши тем же кодом, что приставка свои (key_layout.h): буквы — текстом по раскладке EN / RU, сочетания и служебные
// — клавишами Qt. Дальше — путь клавиш зрителя (StreamInput::key): стол смотрят — столу, нет — экрану телефона.
// Новые устройства — по событию каталога /dev/input, без опроса: телефон в покое не будим.
#pragma once

#include "key_layout.h"

#include <QHash>
#include <QObject>

class QSocketNotifier;

class HardKeyboard : public QObject
{
    Q_OBJECT
public:
    explicit HardKeyboard(QObject *parent = nullptr);
    ~HardKeyboard() override;

    bool present() const { return !m_devices.isEmpty(); }
    bool russian() const { return m_keys.russian(); }
    void setRussian(bool ru) { m_keys.setRussian(ru); }   // со стороны (зритель, панель стола); layoutToggled не зовёт

signals:
    void key(bool down, int qtKey, int modifiers, const QString &text);
    void layoutToggled(bool russian);   // сменили сочетанием на этой клавиатуре
    void presentChanged();

private:
    void scan();
    void read(int fd);
    void drop(int fd);
    void setCapsLed(bool on);

    int appleKey(int code, int value);

    // apple — клавиатура Apple, которую ядро ведёт как обычную (hid-generic: модель ему не знакома): верхний ряд
    // приходит F1–F12, а fn — кодом KEY_BRIGHTNESSDOWN; переводим сами (appleKey)
    struct Device { QString path, name; QSocketNotifier *notifier; bool apple; };
    bool m_fn = false;              // fn клавиатуры Apple держат
    QHash<int, int> m_appleDown;    // нажатая клавиша → чем её отдали: отпустить то же, даже если fn уже отпустили
    QHash<int, Device> m_devices;   // fd → клавиатура
    QHash<QString, int> m_denied;   // узел → сколько раз не дали открыть
    KeyLayout m_keys;
};
