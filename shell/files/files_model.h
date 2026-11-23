// НаомиФон — «Файлы» рабочего стола: содержимое места — папки, «Избранного» или корзины (docs/FILES.md). Перенос
// модели files-grid.c НаомиОС: папки первыми, потом по колонке (имя — как люди: 2.jpg раньше 10.jpg; тип; дата; размер),
// при равенстве — по имени; ТИП — «Картинка · PNG», «Архив · ZIP», «Папка»; дата — когда создан (нет — изменён), в
// корзине — когда удалён: «Сегодня, 14:05», «Вчера, 9:30», «3 окт, 18:12», «3 окт 2025»; скрытые — по выбору.
// Папка меняется — список обновляется сам, по месту (плитки не пересоздаются, прокрутка не сбрасывается).
#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QTimer>

class FilesModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString location READ location WRITE setLocation NOTIFY locationChanged)   // путь, starred:, trash:
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(QString sortKey READ sortKey WRITE setSortKey NOTIFY sortChanged)          // name, type, date, size
    Q_PROPERTY(bool descending READ descending WRITE setDescending NOTIFY sortChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool trash READ trash NOTIFY locationChanged)
    Q_PROPERTY(bool starredPlace READ starredPlace NOTIFY locationChanged)
    Q_PROPERTY(bool readable READ readable NOTIFY countChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1, PathRole, IsDirRole, ExtRole, TypeRole, DateRole, DateTextRole, SizeRole,
        SizeTextRole, StarredRole, ImageRole, HiddenRole,
    };

    explicit FilesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex & = {}) const override { return int(m_items.size()); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString location() const { return m_location; }
    void setLocation(const QString &l);
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool on);
    QString sortKey() const { return m_sortKey; }
    void setSortKey(const QString &k);
    bool descending() const { return m_descending; }
    void setDescending(bool d);
    int count() const { return int(m_items.size()); }
    bool trash() const { return m_location == QLatin1String("trash:"); }
    bool starredPlace() const { return m_location == QLatin1String("starred:"); }
    bool readable() const { return m_readable; }

    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirAt(int row) const;
    Q_INVOKABLE int indexOf(const QString &path) const;
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE void reload();
    // начало имени (набор с клавиатуры) — первый подходящий после from
    Q_INVOKABLE int find(const QString &prefix, int from) const;

signals:
    void locationChanged();
    void showHiddenChanged();
    void sortChanged();
    void countChanged();

private:
    struct Item {
        QString name, path, ext, type, dateText;
        QDateTime date;
        qint64 size = 0;
        bool dir = false, image = false, hidden = false;
    };
    QList<Item> load() const;
    Item itemFor(const QString &path, const QDateTime &deleted = {}) const;
    void sort(QList<Item> &items) const;
    void apply(QList<Item> items, bool reset);
    void watch();

    QString m_location;
    bool m_showHidden = false;
    QString m_sortKey = QStringLiteral("name");
    bool m_descending = false;
    bool m_readable = true;
    QList<Item> m_items;
    QFileSystemWatcher m_watcher;
    QTimer m_reload;   // папка шумит (копирование) — обновлять не чаще 150 мс
};
