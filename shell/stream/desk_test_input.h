// НаомиФон — проверочная мышь стола (docs/STREAM.md, «Проверочная мышь»): разработчик по SSH шлёт столу мышь и
// клавиши тем же путём, что зритель (StreamInput), — когда приставки нет, а стол смотрит окно Mac (с проверочной
// мышью приставки, pi/tools/vinput.py, — только через приставку). Сессионная шина пользователя телефона:
// org.naomi.DeskTest /input, Pointer(кнопки, x, y) — доли стола 0…1, кнопки как у зрителя (1 — левая, 16 — Shift);
// Key(нажата, клавиша Qt, модификаторы, текст). Стол никто не смотрит — StreamInput мышь не берёт. Команда —
// tools/naomi mouse.
// Там же — «Экраны» (docs/SCREENS.md) без пальца и мыши: Screens() — JSON списка экранов и выключателя,
// ScreenPosition(id, 0 / 1 / 2) — заставка, стол, главный; Desk(вкл) — выключатель стола; Wake(id) — как жест на
// экране; ScreenLight(id, сам, уровень) — яркость экрана (планшета). Команда — tools/naomi screens.
#pragma once

#include <QObject>

class StreamInput;
class StreamService;

class DeskTestInput : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.DeskTest")
public:
    explicit DeskTestInput(StreamInput *input, StreamService *service);

public slots:
    Q_SCRIPTABLE void Pointer(int buttons, double x, double y);
    Q_SCRIPTABLE void Key(bool down, int qtKey, int modifiers, const QString &text);
    Q_SCRIPTABLE QString Screens();
    Q_SCRIPTABLE void ScreenPosition(const QString &id, int position);
    Q_SCRIPTABLE void Desk(bool on);
    Q_SCRIPTABLE void Wake(const QString &id);
    Q_SCRIPTABLE void ScreenLight(const QString &id, bool automatic, int level);

private:
    StreamInput *m_input;
    StreamService *m_service;
};
