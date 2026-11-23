// НаомиФон — композитор оболочки (корень Main.qml): WaylandCompositor Qt и один буфер обмена на оболочку и программы
// (docs/DESKTOP.md, «Копировать и вставить»). У оболочки свой буфер (QClipboard: её поля, «Терминал», заметки), у
// программ Wayland (браузер, игры) — выбор Wayland (wl_data_device); сами они не встречаются. Мост: программа
// положила в буфер — композитор вычитывает всё (retainedSelection) и кладёт оболочке; оболочка положила — композитор
// забирает выбор у программы и предлагает своё (overrideSelection) окну в фокусе клавиатуры — и заново при каждой
// смене фокуса, пока буфер за оболочкой: сам Qt даёт новому окну только выбор программ.
#pragma once

#include <QQmlListProperty>
#include <QWaylandQuickCompositor>

class ShellCompositor : public QWaylandQuickCompositor
{
    Q_OBJECT
    // дети в QML (WaylandOutput, XdgShell…) — как у WaylandCompositor Qt
    Q_PROPERTY(QQmlListProperty<QObject> data READ data DESIGNABLE false)
    Q_CLASSINFO("DefaultProperty", "data")
public:
    explicit ShellCompositor(QObject *parent = nullptr);
    QQmlListProperty<QObject> data() { return QQmlListProperty<QObject>(this, &m_objects); }
    void create() override;

protected:
    void retainedSelectionReceived(QMimeData *mime) override;

private:
    void shellClipboardChanged();
    void offerShellClipboard();
    bool focusHasDataDevice() const;   // нет фокуса клавиатуры или у программы в фокусе есть wl_data_device

    QList<QObject *> m_objects;
    bool m_shellOwns = false;    // в буфере — «копировать» оболочки (новее, чем у программ)
    bool m_fromClient = false;   // буфер оболочки кладём мы — выбор программы: программам обратно не предлагать
    bool m_offering = false;     // предлагаем программам своё: Qt зовёт retainedSelectionReceived и с ним
};
