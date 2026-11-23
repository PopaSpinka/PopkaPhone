#include "web_filter.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QUrl>

// Профиль страниц в QML — QQuickWebEngineProfile; его заголовка в среде сборки нет (как и всего пакета -dev,
// main.cpp), а нужна одна функция, экспортированная из libQt6WebEngineQuick.so.6 той же версии.
class QQuickWebEngineProfile : public QObject
{
public:
    void setUrlRequestInterceptor(QWebEngineUrlRequestInterceptor *interceptor);
};

namespace {

// типы запросов — как в EasyList (имена — те, что пишет tools/naomi web-filters)
enum : quint32 {
    Script = 1, Image = 2, Css = 4, Xhr = 8, Frame = 16, Media = 32, Font = 64, Object = 128, Ping = 256,
    WebSocket = 512, Other = 1024
};

quint32 typeBit(const QString &name)
{
    static const QHash<QString, quint32> bits{{"script", Script}, {"image", Image}, {"css", Css}, {"xhr", Xhr},
                                              {"frame", Frame}, {"media", Media}, {"font", Font},
                                              {"object", Object}, {"ping", Ping}, {"websocket", WebSocket},
                                              {"other", Other}};
    return bits.value(name, 0);
}

quint32 typeOf(QWebEngineUrlRequestInfo::ResourceType t)
{
    using R = QWebEngineUrlRequestInfo;
    switch (t) {
    case R::ResourceTypeSubFrame: return Frame;
    case R::ResourceTypeStylesheet: return Css;
    case R::ResourceTypeScript: return Script;
    case R::ResourceTypeImage:
    case R::ResourceTypeFavicon: return Image;
    case R::ResourceTypeFontResource: return Font;
    case R::ResourceTypeObject:
    case R::ResourceTypePluginResource: return Object;
    case R::ResourceTypeMedia: return Media;
    case R::ResourceTypeXhr: return Xhr;
    case R::ResourceTypePing:
    case R::ResourceTypeCspReport: return Ping;
    case R::ResourceTypeWebSocket: return WebSocket;
    default: return Other;
    }
}

// сайт адреса — «домен и зона» (amazon.com, bbc.co.uk): сторонний запрос — к другому сайту, чем страница
QString siteOf(const QUrl &url)
{
    const QString s = qWebEngineGetDomainAndRegistry(url);
    return s.isEmpty() ? url.host() : s;
}

} // namespace

WebFilter::WebFilter(const QString &path, QObject *parent)
    : QWebEngineUrlRequestInterceptor(parent)
{
    QElapsedTimer t;
    t.start();
    if (load(path))
        qInfo("web: реклама и слежка — %lld доменов, исключений %lld (%lld мс)", qlonglong(m_block.size()),
              qlonglong(m_allow.size() + m_sites.size()), qlonglong(t.elapsed()));
    else
        qInfo("web: реклама и слежка — списка нет (%s, tools/naomi web-filters), не блокируется ничего",
              qPrintable(path));
}

bool WebFilter::load(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        const QStringList w = line.split(' ', Qt::SkipEmptyParts);
        if (w.size() < 2 || line.startsWith('#'))
            continue;
        if (w[0] == "s") {
            m_sites.insert(w[1]);
            continue;
        }
        Rule r;
        for (int i = 2; i < w.size(); ++i) {
            if (w[i] == "3")
                r.thirdParty = true;
            else if (w[i].startsWith("t="))
                for (const QString &t : w[i].mid(2).split(','))
                    r.types |= typeBit(t);
            else if (w[i].startsWith("on="))
                r.on = w[i].mid(3).split(',');
        }
        (w[0] == "a" ? m_allow : m_block).insert(w[1], r);
    }
    return true;
}

const WebFilter::Rule *WebFilter::find(const QHash<QString, Rule> &rules, const QString &host) const
{
    for (qsizetype at = 0; at >= 0;) {
        const auto it = rules.constFind(host.mid(at));
        if (it != rules.constEnd())
            return &*it;
        at = host.indexOf('.', at);
        at = at < 0 ? -1 : at + 1;
    }
    return nullptr;
}

void WebFilter::interceptRequest(QWebEngineUrlRequestInfo &info)
{
    // страница, на которую идут, — всегда (сам пользователь набрал или щёлкнул); блокируется то, что она тянет
    if (info.resourceType() == QWebEngineUrlRequestInfo::ResourceTypeMainFrame || m_block.isEmpty())
        return;
    const QUrl url = info.requestUrl();
    const QString host = url.host().toLower();
    if (host.isEmpty() || !find(m_block, host))
        return;
    const QUrl page = info.firstPartyUrl();
    const QString pageHost = page.host().toLower();
    for (const QString &s : m_sites)   // сайт целиком без блокировки — их единицы
        if (pageHost == s || pageHost.endsWith('.' + s))
            return;
    const quint32 type = typeOf(info.resourceType());
    if (const Rule *a = find(m_allow, host)) {
        bool here = a->on.isEmpty();
        for (const QString &s : a->on)
            here |= pageHost == s || pageHost.endsWith('.' + s);
        if (here && (!a->types || (a->types & type)))
            return;
    }
    const Rule *b = find(m_block, host);
    if (b->types && !(b->types & type))
        return;
    if (b->thirdParty && (page.isEmpty() || siteOf(url) == siteOf(page)))
        return;
    info.block(true);
    ++m_blocked;
    // раз в минуту — сколько отсекли (журнал оболочки)
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_logAt > 60000) {
        qInfo("web: реклама и слежка — отсечено %llu запросов (за минуту %llu), последний: %s", m_blocked,
              m_blocked - m_logged, qPrintable(host));
        m_logAt = now;
        m_logged = m_blocked;
    }
}

void WebFilter::attach(QObject *profile)
{
    if (!profile || !profile->inherits("QQuickWebEngineProfile"))
        return;
    static_cast<QQuickWebEngineProfile *>(profile)->setUrlRequestInterceptor(this);
}
