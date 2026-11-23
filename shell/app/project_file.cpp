#include "project_file.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>

ProjectFile::ProjectFile(QObject *parent) : QObject(parent)
{
    m_path = qEnvironmentVariableIsSet("NAOMI_PROJECT")
        ? qEnvironmentVariable("NAOMI_PROJECT")
        : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/naomi/project.json";
    m_settle.setSingleShot(true);
    m_settle.setInterval(16);
    connect(&m_settle, &QTimer::timeout, this, &ProjectFile::read);
    // Load и Live пишут новый файл рядом и переименовывают его на место: следим за папкой (у файла меняется inode).
    // Читаем не чаще раза в 16 мс, но и не ждём тишины: Live при перетаскивании пишет без перерыва — с ожиданием
    // тишины слой стоял бы, пока палец не остановится
    const auto soon = [this] { if (!m_settle.isActive()) m_settle.start(); };
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, soon);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, soon);
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    read();
}

void ProjectFile::watch()
{
    const QString dir = QFileInfo(m_path).absolutePath();
    if (!m_watcher.directories().contains(dir))
        m_watcher.addPath(dir);
    if (QFile::exists(m_path) && !m_watcher.files().contains(m_path))
        m_watcher.addPath(m_path);
}

void ProjectFile::read()
{
    watch();
    QFile f(m_path);
    QByteArray raw;
    if (f.open(QIODevice::ReadOnly))
        raw = f.readAll();
    if (raw == m_raw)
        return;
    m_raw = raw;
    QVariant next;
    if (!raw.isEmpty()) {
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            qWarning("naomi: проект студии %s не читается: %s — свои экраны", qPrintable(m_path), qPrintable(err.errorString()));
        } else {
            next = doc.toVariant();
            // кто его положил: проект студии и как (Load, Live) — в журнал при смене, не на каждую правку Live
            const QJsonObject studio = doc["studio"].toObject();
            const QString who = studio["pid"].toString() + (studio["live"].toBool() ? " live" : "");
            if (who != m_who)
                qInfo("naomi: проект студии «%s» (%s, правка %d)", qPrintable(doc["name"].toString()),
                      qPrintable(who), studio["rev"].toInt());
            m_who = who;
        }
    } else if (m_data.isValid()) {
        qInfo("naomi: проекта студии нет — свои экраны");
        m_who.clear();
    }
    m_data = next;
    emit dataChanged();
}
