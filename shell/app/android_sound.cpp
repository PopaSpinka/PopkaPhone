#include "android_sound.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <cstring>

Q_LOGGING_CATEGORY(lcAndroidSound, "naomi.android")

namespace {
const char kRoom[] = "naomi.android";   // «приёмная»: пустой выход на паузе, тут рождается поток Android
constexpr qint64 kCorkedMs = 1500;      // поток Android на паузе дольше — закрыть
const char kMark[] = "\n__naomi_done__ ";   // помощник пишет после каждой команды: метка и код выхода

QByteArray quoted(const QString &arg)   // слово для sh
{
    return '\'' + arg.toUtf8().replace("'", "'\\''") + '\'';
}

// поток Android: клиент PulseAudio из контейнера Waydroid (модуль ALSA пишет имя узла и программы)
bool isAndroid(const QJsonObject &input)
{
    const QJsonObject p = input.value("properties").toObject();
    return p.value("application.process.host").toString() == QLatin1String("waydroid")
           || p.value("application.name").toString() == QLatin1String("Waydroid");
}
}

AndroidSound::AndroidSound(QObject *parent)
    : QObject(parent)
{
    m_check.setSingleShot(true);
    m_check.setInterval(10);
    connect(&m_check, &QTimer::timeout, this, &AndroidSound::check);
    m_retry.setSingleShot(true);
    m_retry.setInterval(2000);
    connect(&m_retry, &QTimer::timeout, this, &AndroidSound::subscribe);

    m_events.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_events, &QProcess::readyRead, this, [this] {
        // «Event 'new' on sink-input #576»; выходы — тоже: «приёмную» могли выгрузить (PulseAudio перезапустился)
        static const QRegularExpression re("^Event '(new|change)' on (sink-input|sink) #");
        while (m_events.canReadLine())
            if (re.match(QString::fromUtf8(m_events.readLine())).hasMatch())
                m_check.start();
    });
    connect(&m_events, &QProcess::finished, this, [this] {
        if (m_on)
            m_retry.start();
    });

    m_timeout.setSingleShot(true);
    m_timeout.setInterval(5000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { m_sh.kill(); });
    connect(&m_sh, &QProcess::readyReadStandardOutput, this, [this] {
        m_out += m_sh.readAllStandardOutput();
        // конец команды — строка «метка код»
        const int mark = m_out.lastIndexOf(kMark);
        if (m_busy && mark >= 0 && m_out.endsWith('\n')) {
            const int code = m_out.mid(mark + int(strlen(kMark))).trimmed().toInt();
            m_out.truncate(mark);
            finish(code);
        }
    });
    connect(&m_sh, &QProcess::finished, this, [this] {
        m_leaving = false;
        if (m_busy)
            finish(-1);
        else
            next();
    });
}

void AndroidSound::start()
{
    if (m_on)
        return;
    m_on = true;
    subscribe();
}

void AndroidSound::stop()
{
    if (!m_on)
        return;
    m_on = false;
    m_retry.stop();
    m_check.stop();
    m_events.kill();
    m_corked.clear();
    pactl({"-f", "json", "list", "sinks"}, [this](const QByteArray &list) {
        for (const QJsonValue &v : QJsonDocument::fromJson(list).array())
            if (v.toObject().value("name").toString() == QLatin1String(kRoom))
                pactl({"unload-module", QString::number(v.toObject().value("owner_module").toInt())});
    });
}

void AndroidSound::subscribe()
{
    if (!m_on)
        return;
    m_events.start("pactl", {"subscribe"});
    check();
}

void AndroidSound::pactl(const QStringList &args, std::function<void(const QByteArray &)> done, const QString &program)
{
    QByteArray line = quoted(program);
    for (const QString &arg : args)
        line += ' ' + quoted(arg);
    m_jobs.append({line, std::move(done)});
    next();
}

void AndroidSound::next()
{
    if (m_busy)
        return;
    if (m_leaving)   // помощник уходит: новые команды — новому, когда этот кончится (finished)
        return;
    if (m_jobs.isEmpty()) {
        if (!m_on && m_sh.state() != QProcess::NotRunning) {
            m_leaving = true;
            m_sh.write("exit\n");   // Android выключен, «приёмная» выгружена — помощник не нужен
        }
        return;
    }
    if (m_sh.state() == QProcess::NotRunning)
        m_sh.start("sh", QStringList());
    m_busy = true;
    m_out.clear();
    m_sh.readAllStandardError();
    m_sh.write(m_jobs.first().line + "; printf '\\n__naomi_done__ %d\\n' $?\n");
    m_timeout.start();
}

void AndroidSound::finish(int code)
{
    m_timeout.stop();
    m_busy = false;
    const Job job = m_jobs.takeFirst();
    if (code != 0)
        qCWarning(lcAndroidSound, "звук Android: %s — %s %s", job.line.constData(),
                  code < 0 ? "помощник не ответил" : qPrintable(QStringLiteral("код %1").arg(code)),
                  m_sh.readAllStandardError().trimmed().constData());
    else if (job.done)
        job.done(m_out);
    next();
}

void AndroidSound::check()
{
    if (!m_on)
        return;
    pactl({"-f", "json", "list", "sinks"}, [this](const QByteArray &sinks) {
        int room = -1;
        for (const QJsonValue &v : QJsonDocument::fromJson(sinks).array())
            if (v.toObject().value("name").toString() == QLatin1String(kRoom))
                room = v.toObject().value("index").toInt();
        if (room < 0) {
            if (!m_loading) {
                m_loading = true;
                pactl({"load-module", "module-null-sink", QStringLiteral("sink_name=%1").arg(kRoom), "rate=48000",
                       "channels=2", "format=s16le", "sink_properties=device.description=Android"},
                      [this](const QByteArray &) {
                          // на паузе (пользователем — сама с неё не сходит): поток ждёт переезда, не играя
                          pactl({"suspend-sink", kRoom, "1"}, [this](const QByteArray &) {
                              m_loading = false;
                              qCInfo(lcAndroidSound, "звук Android: «приёмная» %s создана (на паузе)", kRoom);
                              check();
                          });
                      });
            }
            return;
        }
        pactl({"-f", "json", "list", "sink-inputs"}, [this, room](const QByteArray &inputs) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            QHash<int, qint64> corked;
            bool recheck = false;
            for (const QJsonValue &v : QJsonDocument::fromJson(inputs).array()) {
                const QJsonObject in = v.toObject();
                if (!isAndroid(in))
                    continue;
                const int id = in.value("index").toInt();
                if (in.value("sink").toInt() == room) {   // ещё здесь (и после команды — она в пути): снова
                    qCInfo(lcAndroidSound, "звук Android: поток #%d — из «приёмной» на выход стола", id);
                    pactl({"move-sink-input", QString::number(id), "@DEFAULT_SINK@"});
                    continue;
                }
                if (!in.value("corked").toBool())
                    continue;
                const qint64 since = m_corked.value(id, now);
                if (now - since >= kCorkedMs) {
                    qCWarning(lcAndroidSound, "звук Android: поток #%d стоит на паузе %.1f с — закрываю (Android "
                              "откроет новый)", id, (now - since) / 1000.0);
                    pactl({"kill-sink-input", QString::number(id)}, {}, QStringLiteral("pacmd"));
                    continue;
                }
                corked.insert(id, since);
                recheck = true;
            }
            m_corked = corked;
            if (recheck)
                QTimer::singleShot(kCorkedMs + 100, this, [this] { check(); });
        });
    });
}
