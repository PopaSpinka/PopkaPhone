// «Браузер» — своё хранилище в профиле (~/.local/share/naomi/web, docs/WEB.md). У Naomi Web НаомиОС это делал
// Chromium с расширением (chrome.bookmarks, chrome.history, кэш значков, --restore-last-session); у Qt WebEngine
// этого нет, поэтому здесь:
//   bookmarks.json  — «Избранное»: [{title, url}] по порядку списка под звездой;
//   history.sqlite  — адреса с числом заходов и последним заходом (urls: url, title, visit_count, last_visit мс) —
//                     для «Часто» на новой вкладке;
//   icons/<сайт>.png — значки сайтов: стол рисует их на полосе, новая вкладка — в плитках;
//   session.json    — открытые вкладки, чтобы вернуть их при следующем запуске.
#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QUrl>
#include <QVariantList>

class QQmlEngine;

class WebStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
public:
    WebStore(const QString &dir, QQmlEngine *engine, QObject *parent = nullptr);
    ~WebStore() override;

    // [{id, title, url, icon}] — id — номер в списке (bookmark-rename от стола), icon — iconUrl или ""
    QVariantList bookmarks() const;
    Q_INVOKABLE bool isBookmarked(const QString &url) const;
    // звёздочка в строке адреса: on — добавить в конец (если такого адреса нет), иначе убрать все с этим адресом
    Q_INVOKABLE void setBookmark(const QString &url, const QString &title, bool on);
    Q_INVOKABLE void renameBookmark(int id, const QString &title);
    // корзина в списке «Избранного»: убрать закладку id, только если там всё ещё url (список мог сдвинуться)
    Q_INVOKABLE void removeBookmark(int id, const QString &url);

    Q_INVOKABLE void visit(const QString &url, const QString &title);   // загрузилась страница
    Q_INVOKABLE void retitle(const QString &url, const QString &title);
    // «Часто»: сайты за days дней, заходы — сумма по их страницам, кроме сайтов «Избранного»; первые max
    // [{site, url (главная сайта), visits, icon}] — как frequent() в newtab.js НаомиОС
    Q_INVOKABLE QVariantList frequent(int days, int max) const;
    // корзина у строки «Часто»: забыть сайт — стереть из истории все его адреса (site — как в frequent())
    Q_INVOKABLE void forgetSite(const QString &site);

    // значок сайта страницы: file:///…/icons/<сайт>.png?v=<время файла> (новый файл — новый адрес, кэш картинок
    // стола и страницы его не держит) или ""
    Q_INVOKABLE QString iconUrl(const QString &pageUrl) const;
    // значок вкладки (WebEngineView.icon, image://favicon/…) → icons/<сайт>.png; готово — iconSaved
    Q_INVOKABLE void saveIcon(const QString &pageUrl, const QUrl &icon);

    Q_INVOKABLE QVariantMap loadSession() const;
    Q_INVOKABLE void saveSession(const QVariantMap &session);

signals:
    void bookmarksChanged();
    void iconSaved(const QString &site, const QString &url);

private:
    void loadBookmarks();
    void writeBookmarks();

    QString m_dir;
    QQmlEngine *m_engine;
    QVariantList m_bookmarks;   // [{title, url}]
    QSqlDatabase m_db;
    QHash<QString, QUrl> m_iconAsked;   // сайт → значок, который уже взяли или берём
};
