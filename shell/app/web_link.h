// НаомиФон — связь стола с «Браузером» (docs/WEB.md). Как у bcdesk с Naomi Web в НаомиОС (NaomiOS src/web.h):
// браузер — отдельный процесс без своих полос, полосу вкладок и режим панели рисует стол. Сокет
// $XDG_RUNTIME_DIR/naomi-web.sock, строки JSON с \n, тот же язык сообщений:
//   браузер → стол: hello, tabs {active, tabs[id, pinned, loading, title, url, icon]}, bookmarks {items[id, title,
//     url, icon]}, downloads {items[id, name, received, total, state, path]}, fullscreen {on}, contextmenu {tab},
//     field {text?} — строку адреса в фокус (Ctrl+L; Ctrl+T — с пустым текстом), cursor {shape} — форма курсора над
//     страницей (Qt::CursorShape: текст, ссылка, «занято»; стол ставит её над страницей — naomiWeb.cursorShape);
//   стол → браузер: new-tab, activate, close, navigate, pin, reload, back, forward, duplicate, restore-closed,
//     cycle, select, dismiss-download, quit, bookmark, bookmark-rename, open, focus (send из QML); стол сам (setDesk):
//     pause — зритель ушёл: видео и звук на паузу; freeze {on} — страницы заморозить (зрителя нет 2 минуты или
//     трансляцию выключили) или оттаять (зритель вернулся); браузер отвечает frozen, когда страницы заморожены, —
//     тогда стол замораживает его процессы ядром (freezeDesk → AppLauncher).
// Значки (icon) — пути к PNG в профиле браузера, а не картинки: браузер и стол на одной машине.
// Браузер — тот, кто прислал hello последним. В QML — naomiWeb.
// Снаружи — ссылки других программ (xdg-open у Claude, docs/CLAUDE.md): сессионная шина, служба org.naomi.Web,
// объект /web, метод Open(адрес) — адрес http или https стол открывает в браузере новой вкладкой (openRequested →
// Desktop.qml; браузера нет — запускает его).
#pragma once

#include <QLocalSocket>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>

class QLocalServer;

class WebLink : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Web")
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QVariantList tabs READ tabs NOTIFY tabsChanged)
    Q_PROPERTY(int active READ active NOTIFY tabsChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(QVariantList downloads READ downloads NOTIFY downloadsChanged)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(int cursorShape READ cursorShape NOTIFY cursorShapeChanged)
public:
    explicit WebLink(QObject *parent = nullptr);

    bool connected() const { return m_browser; }
    QVariantList tabs() const { return m_tabs; }
    int active() const { return m_active; }
    QVariantList bookmarks() const { return m_bookmarks; }
    QVariantList downloads() const { return m_downloads; }
    bool fullscreen() const { return m_fullscreen; }
    int cursorShape() const { return m_cursorShape; }

    Q_INVOKABLE void send(const QVariantMap &message);
    // Выключатель трансляции (StreamService::enabled) и есть ли зритель (connected). Стол без зрителя не виден никому
    // (docs/WEB.md, «Без зрителя»): зритель ушёл сам — видео и звук сразу на паузу (как с наушниками), через
    // NAOMI_WEB_FREEZE_S секунд (120) — заморозка; трансляцию выключили — заморозка сразу. Заморозка: страницы
    // Frozen, потом все процессы браузера — ядром (freezeDesk). Зритель вернулся — оттаивают сразу, ролик на паузе.
    void setDesk(bool enabled, bool viewer);

public slots:
    Q_SCRIPTABLE void Open(const QString &url);   // org.naomi.Web.Open: адрес http(s) — новой вкладкой (openRequested)

signals:
    void openRequested(const QString &url);
    void freezeDesk(bool on);   // программы стола — заморозить ядром (страницы уже заморожены) или оттаять
    void connectedChanged();
    void tabsChanged();
    void bookmarksChanged();
    void downloadsChanged();
    void fullscreenChanged();
    void cursorShapeChanged();
    void contextMenu(int tab);
    void fieldRequested(const QVariant &text);   // строка адреса: текст или undefined — адрес открытой вкладки

private:
    void onConnection();
    void read(QLocalSocket *socket);
    void handle(QLocalSocket *socket, const QVariantMap &m);
    void reset();
    void freeze(bool on);
    void pause();

    QLocalServer *m_server;
    QPointer<QLocalSocket> m_browser;
    QVariantList m_tabs, m_bookmarks, m_downloads;
    int m_active = -1;
    bool m_fullscreen = false;
    int m_cursorShape = Qt::ArrowCursor;
    bool m_enabled = true;
    bool m_viewer = false;
    bool m_frozen = false;
    QTimer m_freezeLater;
    QTimer m_freezeReply;   // браузер не ответил frozen — заморозить ядром всё равно
};
