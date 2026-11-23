#include "web_store.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>

Q_LOGGING_CATEGORY(lcStore, "naomi.web.store")

namespace {

constexpr int kHistoryMax = 5000;   // страниц истории за эти дни — с запасом (HISTORY_MAX в newtab.js НаомиОС)

QJsonDocument readJson(const QString &path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()) : QJsonDocument();
}

void writeJson(const QString &path, const QJsonDocument &doc)
{
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(doc.toJson(QJsonDocument::Indented));
        f.commit();
    }
}

// сайт для «Часто»: хост без www (у адресов без хоста — весь адрес). Строкой, без QUrl: «Часто» перебирает всю
// историю за 30 дней (2,7 тыс. адресов) на каждый показ новой вкладки, и QUrl на малом ядре держал его ~100 мс
// (docs/WEB.md, «Новая вкладка — сразу»). Международные имена (xn--…) — через QUrl: он показывает их буквами
QString shortSite(const QString &url)
{
    const qsizetype scheme = url.indexOf(QLatin1String("://"));
    if (scheme > 0) {
        qsizetype end = scheme + 3;
        while (end < url.size() && url[end] != u'/' && url[end] != u'?' && url[end] != u'#')
            ++end;
        QStringView host = QStringView(url).mid(scheme + 3, end - scheme - 3);
        if (const qsizetype at = host.lastIndexOf(u'@'); at >= 0)
            host = host.mid(at + 1);
        if (const qsizetype colon = host.lastIndexOf(u':'); colon >= 0 && !host.startsWith(u'['))
            host = host.left(colon);
        if (host.contains(QLatin1String("xn--"), Qt::CaseInsensitive) || host.startsWith(u'['))
            host = QStringView();   // ниже — QUrl
        else if (!host.isEmpty()) {
            if (host.startsWith(QLatin1String("www."), Qt::CaseInsensitive))
                host = host.mid(4);
            return host.toString().toLower();
        }
    }
    QString host = QUrl(url).host();
    if (host.startsWith("www."))
        host = host.mid(4);
    return host.isEmpty() ? url : host;
}

// не в «Часто»: служебные адреса (как в newtab.js НаомиОС)
bool hiddenUrl(const QString &url)
{
    for (const char *s : {"chrome:", "chrome-extension:", "about:", "javascript:", "data:", "devtools:", "file:"})
        if (url.startsWith(QLatin1String(s), Qt::CaseInsensitive))
            return true;
    return false;
}

// главная сайта — её открывает строка «Часто» (у адресов не http(s) — сам адрес)
QString home(const QString &url)
{
    const QUrl u(url);
    if (u.scheme() != "http" && u.scheme() != "https")
        return url;
    return u.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment).toString() + "/";
}

} // namespace

WebStore::WebStore(const QString &dir, QQmlEngine *engine, QObject *parent)
    : QObject(parent)
    , m_dir(dir)
    , m_engine(engine)
{
    QDir().mkpath(m_dir + "/icons");
    m_db = QSqlDatabase::addDatabase("QSQLITE", "naomi-web-history");
    m_db.setDatabaseName(m_dir + "/history.sqlite");
    if (!m_db.open())
        qCWarning(lcStore) << "история:" << m_db.lastError().text();
    QSqlQuery(m_db).exec("create table if not exists urls(url text primary key, title text, visit_count integer, "
                         "last_visit integer)");
    loadBookmarks();
}

WebStore::~WebStore()
{
    m_db.close();
}

// ---------- «Избранное» ----------

void WebStore::loadBookmarks()
{
    m_bookmarks = readJson(m_dir + "/bookmarks.json").array().toVariantList();
}

void WebStore::writeBookmarks()
{
    writeJson(m_dir + "/bookmarks.json", QJsonDocument(QJsonArray::fromVariantList(m_bookmarks)));
    emit bookmarksChanged();
}

QVariantList WebStore::bookmarks() const
{
    QVariantList out;
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        QVariantMap b = m_bookmarks[i].toMap();
        b["id"] = i;
        b["icon"] = iconUrl(b.value("url").toString());
        out << b;
    }
    return out;
}

bool WebStore::isBookmarked(const QString &url) const
{
    for (const QVariant &b : m_bookmarks)
        if (b.toMap().value("url").toString() == url)
            return true;
    return false;
}

void WebStore::setBookmark(const QString &url, const QString &title, bool on)
{
    if (url.isEmpty())
        return;
    if (on) {
        if (isBookmarked(url))
            return;
        m_bookmarks << QVariantMap{{"title", title.isEmpty() ? url : title}, {"url", url}};
    } else {
        const qsizetype before = m_bookmarks.size();
        m_bookmarks.removeIf([&](const QVariant &b) { return b.toMap().value("url").toString() == url; });
        if (m_bookmarks.size() == before)
            return;
    }
    writeBookmarks();
}

void WebStore::renameBookmark(int id, const QString &title)
{
    if (id < 0 || id >= m_bookmarks.size() || title.trimmed().isEmpty())
        return;
    QVariantMap b = m_bookmarks[id].toMap();
    if (b.value("title").toString() == title)
        return;
    b["title"] = title;
    m_bookmarks[id] = b;
    writeBookmarks();
}

void WebStore::removeBookmark(int id, const QString &url)
{
    if (id < 0 || id >= m_bookmarks.size() || m_bookmarks[id].toMap().value("url").toString() != url)
        return;
    m_bookmarks.removeAt(id);
    writeBookmarks();
}

// ---------- история ----------

void WebStore::visit(const QString &url, const QString &title)
{
    const QString scheme = QUrl(url).scheme();
    if (scheme != "http" && scheme != "https")
        return;   // своя новая вкладка, about:, data: — не история
    QSqlQuery q(m_db);
    q.prepare("insert into urls values(?, ?, 1, ?) on conflict(url) do update set visit_count = visit_count + 1, "
              "last_visit = excluded.last_visit, title = coalesce(nullif(excluded.title, ''), title)");
    q.addBindValue(url);
    q.addBindValue(title);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!q.exec())
        qCWarning(lcStore) << "история:" << q.lastError().text();
}

void WebStore::retitle(const QString &url, const QString &title)
{
    if (title.isEmpty())
        return;
    QSqlQuery q(m_db);
    q.prepare("update urls set title = ? where url = ?");
    q.addBindValue(title);
    q.addBindValue(url);
    q.exec();
}

QVariantList WebStore::frequent(int days, int max) const
{
    QElapsedTimer took;
    took.start();
    QSet<QString> taken;
    for (const QVariant &b : m_bookmarks)
        taken.insert(shortSite(b.toMap().value("url").toString()));
    struct Site { QString site, top; int visits = 0, topVisits = -1; };
    QHash<QString, Site> sites;
    QSqlQuery q(m_db);
    q.prepare("select url, visit_count from urls where last_visit >= ? order by last_visit desc limit ?");
    q.addBindValue(QDateTime::currentMSecsSinceEpoch() - qint64(days) * 24 * 3600 * 1000);
    q.addBindValue(kHistoryMax);
    q.exec();
    while (q.next()) {
        const QString url = q.value(0).toString();
        const int count = qMax(1, q.value(1).toInt());
        const QString key = shortSite(url);
        if (hiddenUrl(url) || taken.contains(key))
            continue;
        Site &s = sites[key];
        s.site = key;
        s.visits += count;
        if (count > s.topVisits) {
            s.topVisits = count;
            s.top = url;
        }
    }
    QList<Site> list = sites.values();
    std::stable_sort(list.begin(), list.end(), [](const Site &a, const Site &b) { return a.visits > b.visits; });
    QVariantList out;
    for (const Site &s : list.mid(0, max))
        out << QVariantMap{{"site", s.site}, {"url", home(s.top)}, {"visits", s.visits}, {"icon", iconUrl(s.top)}};
    if (took.elapsed() > 20)   // главный поток браузера: дольше — заметно на «+»
        qCInfo(lcStore, "«Часто» считалось %lld мс", qlonglong(took.elapsed()));
    return out;
}

void WebStore::forgetSite(const QString &site)
{
    if (site.isEmpty())
        return;
    // instr — только отбор (имя сайта бывает и частью чужого адреса), решает shortSite. Международное имя shortSite
    // показывает буквами, а в адресе оно может быть xn--… — тогда смотрим всю историю
    const bool ascii = std::all_of(site.cbegin(), site.cend(), [](QChar c) { return c.unicode() < 0x80; });
    QSqlQuery q(m_db);
    q.prepare(ascii ? "select url from urls where instr(lower(url), ?) > 0" : "select url from urls where ? <> ''");
    q.addBindValue(site.toLower());
    q.exec();
    QStringList gone;
    while (q.next()) {
        const QString url = q.value(0).toString();
        if (shortSite(url) == site)
            gone << url;
    }
    m_db.transaction();
    QSqlQuery del(m_db);
    del.prepare("delete from urls where url = ?");
    for (const QString &url : std::as_const(gone)) {
        del.addBindValue(url);
        del.exec();
    }
    m_db.commit();
    qCInfo(lcStore, "«Часто»: забыт %s — адресов %lld", qPrintable(site), qlonglong(gone.size()));
}

// ---------- значки сайтов ----------

QString WebStore::iconUrl(const QString &pageUrl) const
{
    const QString host = QUrl(pageUrl).host();
    if (host.isEmpty())
        return QString();
    // значок сайта: свой файл, иначе — того же сайта с www или без
    const QString alt = host.startsWith("www.") ? host.mid(4) : "www." + host;
    for (const QString &h : {host, alt}) {
        const QFileInfo fi(m_dir + "/icons/" + h + ".png");
        if (fi.exists())
            return QUrl::fromLocalFile(fi.filePath()).toString() + "?v=" +
                   QString::number(fi.lastModified().toMSecsSinceEpoch());
    }
    return QString();
}

// Картинку значка даёт поставщик image://favicon движка (тот же, что у Image в QML) — в 32 точки.
void WebStore::saveIcon(const QString &pageUrl, const QUrl &icon)
{
    const QString host = QUrl(pageUrl).host();
    if (host.isEmpty() || icon.isEmpty() || m_iconAsked.value(host) == icon)
        return;
    auto *provider = dynamic_cast<QQuickAsyncImageProvider *>(m_engine->imageProvider("favicon"));
    if (!provider)
        return;
    m_iconAsked.insert(host, icon);
    // как QQuickPixmap: «image://favicon/<адрес>» → «<адрес>»
    const QString id = icon.toString(QUrl::RemoveScheme | QUrl::RemoveAuthority).mid(1);
    QQuickImageResponse *response = provider->requestImageResponse(id, QSize(32, 32));
    connect(response, &QQuickImageResponse::finished, this, [this, response, host] {
        response->deleteLater();
        if (!response->errorString().isEmpty())
            return;
        std::unique_ptr<QQuickTextureFactory> factory(response->textureFactory());
        const QImage image = factory ? factory->image() : QImage();
        if (image.isNull())
            return;
        const QString path = m_dir + "/icons/" + host + ".png";
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly) || !image.save(&f, "PNG") || !f.commit())
            return;
        emit iconSaved(host, iconUrl("https://" + host + "/"));
        emit bookmarksChanged();   // у закладок этого сайта значок новый
    }, Qt::QueuedConnection);
}

// ---------- сеанс ----------

QVariantMap WebStore::loadSession() const
{
    return readJson(m_dir + "/session.json").object().toVariantMap();
}

void WebStore::saveSession(const QVariantMap &session)
{
    writeJson(m_dir + "/session.json", QJsonDocument(QJsonObject::fromVariantMap(session)));
}
