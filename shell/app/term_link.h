// НаомиФон — «Терминал» рабочего стола (docs/TERMINAL.md): вкладки и связь со службой naomi-term. В QML — naomiTerm.
//   Вкладки — список стола: имя (своё, если переименовали), закреплена ли, папка и что в ней запущено (от службы);
//   закреплённые — всегда первыми. Список, открытая вкладка и счётчик номеров — в ~/.config/naomi/desktop/term.json:
//   переживают перезапуск оболочки и телефона. Процессы вкладок держит служба (shell/term): оболочка запускает её
//   отдельной программой пользователя, и перезапуск оболочки её не трогает.
//   Экран вкладки (TermSession) — разбор её вывода (term_screen.h); рисует TermView.
#pragma once

#include "term_screen.h"

#include <QList>
#include <QObject>
#include <QTimer>
#include <QVariantList>

#include <memory>

class QLocalSocket;
class TermLink;

class TermSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int tabId READ tabId CONSTANT)
public:
    TermSession(int id, TermLink *link, int cols, int rows);

    int tabId() const { return m_id; }
    term::Screen &screen() { return *m_screen; }
    void reset(int cols, int rows);                  // экран заново: служба пришлёт весь текст вкладки
    void feed(const QByteArray &data, bool replay);  // replay — текст прошлых программ: на запросы в нём не отвечать
    void input(const QByteArray &bytes);

    // где вкладку оставили прокрученной (TermView): номер верхней строки от начала истории за всё время, −1 — внизу
    double anchor = -1;

signals:
    void changed();

private:
    int m_id;
    TermLink *m_link;
    std::unique_ptr<term::Screen> m_screen;
    bool m_replaying = false;
};

class TermLink : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(int active READ active NOTIFY activeChanged)
    Q_PROPERTY(QObject *activeSession READ activeSession NOTIFY activeSessionChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
public:
    explicit TermLink(QObject *parent = nullptr);
    ~TermLink() override;

    QVariantList tabs() const;
    int active() const { return m_active; }
    QObject *activeSession() const;
    bool running() const { return m_running; }

    Q_INVOKABLE void start();   // терминал открыли: служба (запустить, если нет), вкладки
    Q_INVOKABLE void quit();    // закрыли: текст вкладок — на диск, оболочки — прочь; список вкладок помнится
    // pid службы naomi-term на том конце сокета (SO_PEERCRED); нет связи — -1. «Закрыть принудительно» (force_quit.h)
    qint64 servicePid() const;
    Q_INVOKABLE int newTab(int after = -1, const QString &cwd = QString());
    Q_INVOKABLE void closeTab(int id);
    Q_INVOKABLE void activate(int id);
    Q_INVOKABLE void pin(int id, bool on);
    Q_INVOKABLE void rename(int id, const QString &name);
    Q_INVOKABLE void duplicate(int id);
    Q_INVOKABLE void cycle(int step);
    Q_INVOKABLE void select(int index);   // Ctrl+1…8; −1 (Ctrl+9) — последняя
    Q_INVOKABLE void setSize(int cols, int rows);

    void send(char type, int id, const QByteArray &payload);

signals:
    void tabsChanged();
    void activeChanged();
    void activeSessionChanged();
    void runningChanged();
    void finished();   // вкладок не осталось — терминал закрылся

private:
    struct Tab {
        int id = 0;
        QString name;
        bool pinned = false;
        QString cwd = QStringLiteral("~");
        QString proc;
        TermSession *session = nullptr;
        bool opened = false;
    };

    int indexOf(int id) const;
    Tab *tab(int id);
    void setActive(int id);
    void removeTab(int index);
    void load();
    void saveLater();
    void save();
    void connectService();
    void launchService();
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void processInput();
    void handle(char type, int id, const QByteArray &payload);
    void openSessions();
    void openSession(Tab &t);

    QList<Tab> m_tabs;
    int m_active = -1;
    int m_next = 1;
    bool m_running = false;
    int m_cols = 0, m_rows = 0;
    QLocalSocket *m_socket = nullptr;
    bool m_connected = false;
    QByteArray m_in;
    QTimer m_save;
    QTimer m_retry;
    QTimer m_more;
    int m_attempts = 0;
};
