// Проект студии NaomiPhone на телефоне (studio/, docs/SHELL.md «Проект студии»): файл
// ~/.config/naomi/project.json (NAOMI_PROJECT — другой путь), его кладёт кнопка Load студии. В QML — naomiProject:
// data — проект как есть (экраны, слои, программы; null — файла нет или он не читается: оболочка показывает свои
// экраны), loaded — проект есть. Файл меняется — data меняется сама (Load и Live без перезапуска оболочки).
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariant>

class ProjectFile : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant data READ data NOTIFY dataChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY dataChanged)
    Q_PROPERTY(QString path READ path CONSTANT)

public:
    explicit ProjectFile(QObject *parent = nullptr);
    QVariant data() const { return m_data; }
    bool loaded() const { return m_data.isValid(); }
    QString path() const { return m_path; }

signals:
    void dataChanged();

private:
    void watch();
    void read();

    QString m_path;
    QFileSystemWatcher m_watcher;
    QTimer m_settle;        // файл изменился — прочесть через 16 мс (не чаще: перемена за переменой — одно чтение)
    QByteArray m_raw;       // прочитанное в прошлый раз: тот же файл — тот же проект, QML не трогаем
    QVariant m_data;
    QString m_who;          // чей проект в журнале: id в студии (+ live)
};
