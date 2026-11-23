// «Браузер» ↔ стол оболочки (docs/WEB.md): сокет $XDG_RUNTIME_DIR/naomi-web.sock, строки JSON — язык Naomi Web
// НаомиОС (как bridge.js с bcdesk). Браузер шлёт своё (вкладки, закладки, загрузки…) — send из QML; команды стола
// приходят сигналом command. Обрыв — снова через 3 с; при связи — hello, и QML шлёт всё заново (linked).
//
// Форма курсора: страница (Chromium) меняет её над текстом, ссылками, при «занято» — Qt ставит её окну программы
// (QEvent::CursorChange); стол рисует курсор сам (зритель — приставка) — ему cursor {shape: Qt::CursorShape}.
//
// Клавиши браузера (как webframe_key у bcdesk): зритель печатает в окно через модуль ввода «naomi» — клавиши
// приходят событиями прямо элементу страницы, мимо сочетаний Qt. Поэтому их ловит фильтр событий программы:
// Ctrl+T, Ctrl+W, Ctrl+Tab, Ctrl+1…9, Ctrl+L… — сигнал shortcut с именем действия; страница их не получает.
#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantMap>

class QLocalSocket;

class WebBridge : public QObject
{
    Q_OBJECT
    // видео страницы на весь экран: Esc — выйти из него (Qt WebEngine сам не выходит)
    Q_PROPERTY(bool fullscreen MEMBER m_fullscreen)
public:
    explicit WebBridge(QObject *parent = nullptr);
    Q_INVOKABLE void send(const QVariantMap &message);

signals:
    void linked();                            // связь есть, hello ушёл — пора прислать всё
    void command(const QVariantMap &message);
    // new-tab, restore-closed, close, next, previous, select (index: 0…7, −1 — последняя), field, reload,
    // back, forward, zoom-in, zoom-out, zoom-reset, exit-fullscreen
    void shortcut(const QString &action, int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void connectToShell();
    void read();

    QLocalSocket *m_socket;
    QTimer m_retry;
    bool m_fullscreen = false;
    int m_cursor = -1;   // последняя форма курсора, что ушла столу (Qt::CursorShape)
};
