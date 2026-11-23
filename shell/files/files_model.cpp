#include "files_model.h"

#include "files_ops.h"
#include "files_service.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QSet>
#include <QUrl>

#include <algorithm>

namespace {

// Что это за файл (колонка ТИП) — по типу содержимого, по-русски (kind_name НаомиОС)
QString kindName(const QMimeType &mime)
{
    static const struct { const char *prefix, *name; } kinds[] = {
        {"image/", "Картинка"}, {"video/", "Видео"}, {"audio/", "Музыка"}, {"font/", "Шрифт"},
        {"application/pdf", "Документ"}, {"application/msword", "Документ"},
        {"application/vnd.oasis.opendocument.text", "Документ"},
        {"application/vnd.openxmlformats-officedocument.wordprocessingml", "Документ"},
        {"application/vnd.oasis.opendocument.spreadsheet", "Таблица"}, {"application/vnd.ms-excel", "Таблица"},
        {"application/vnd.openxmlformats-officedocument.spreadsheetml", "Таблица"}, {"text/csv", "Таблица"},
        {"application/vnd.oasis.opendocument.presentation", "Презентация"},
        {"application/vnd.ms-powerpoint", "Презентация"},
        {"application/vnd.openxmlformats-officedocument.presentationml", "Презентация"},
        {"application/zip", "Архив"}, {"application/x-tar", "Архив"}, {"application/x-compressed-tar", "Архив"},
        {"application/gzip", "Архив"}, {"application/x-7z-compressed", "Архив"}, {"application/vnd.rar", "Архив"},
        {"application/x-rar", "Архив"}, {"application/x-xz", "Архив"}, {"application/x-bzip", "Архив"},
        {"application/zstd", "Архив"}, {"application/x-zstd", "Архив"}, {"application/x-lzma", "Архив"},
        {"application/x-cd-image", "Образ диска"}, {"application/x-iso9660", "Образ диска"},
        {"application/x-raw-disk-image", "Образ диска"},
        {"application/x-executable", "Программа"}, {"application/x-pie-executable", "Программа"},
        {"application/vnd.appimage", "Программа"}, {"application/x-sharedlib", "Библиотека"},
        {"application/x-desktop", "Ярлык"}, {"application/x-shellscript", "Скрипт"},
        {"application/x-font", "Шрифт"}, {"application/json", "Данные"}, {"application/xml", "Данные"},
        {"application/x-bittorrent", "Торрент"},
    };
    const QString name = mime.name();
    for (const auto &k : kinds)
        if (name.startsWith(QLatin1String(k.prefix)))
            return QString::fromUtf8(k.name);
    return mime.inherits("text/plain") ? QStringLiteral("Текст") : QStringLiteral("Файл");
}

// «ZIP», «PNG»; нет расширения (или длиннее 5) — «ФАЙЛ»
QString extText(const QString &name)
{
    const int dot = name.lastIndexOf('.');
    if (dot > 0 && dot < name.size() - 1 && name.size() - dot - 1 <= 5)
        return name.mid(dot + 1).toUpper();
    return QStringLiteral("ФАЙЛ");
}

// «Сегодня, 14:05», «Вчера, 9:30», «3 окт, 18:12», прошлые годы — «3 окт 2025»
QString dateText(const QDateTime &when)
{
    if (!when.isValid())
        return QStringLiteral("—");
    static const char *months[] = {"янв", "фев", "мар", "апр", "мая", "июн", "июл", "авг", "сен", "окт", "ноя", "дек"};
    const QDateTime t = when.toLocalTime();
    const QDate today = QDate::currentDate();
    const QString hm = QString::number(t.time().hour()) + ":" + QString::asprintf("%02d", t.time().minute());
    if (t.date() == today)
        return "Сегодня, " + hm;
    if (t.date() == today.addDays(-1))
        return "Вчера, " + hm;
    const QString dm = QString::number(t.date().day()) + " " + QString::fromUtf8(months[t.date().month() - 1]);
    if (t.date().year() == today.year())
        return dm + ", " + hm;
    return dm + " " + QString::number(t.date().year());
}

// имена — как в НаомиОС (glibc, en_US): латиница раньше кириллицы, Ё рядом с Е, числа — числами
QCollator &collator()
{
    static QCollator c = [] {
        QCollator x(QLocale(QLocale::English, QLocale::UnitedStates));
        x.setNumericMode(true);
        x.setCaseSensitivity(Qt::CaseInsensitive);
        return x;
    }();
    return c;
}

} // namespace

FilesModel::FilesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_reload.setSingleShot(true);
    m_reload.setInterval(150);
    connect(&m_reload, &QTimer::timeout, this, &FilesModel::reload);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_reload, qOverload<>(&QTimer::start));
    if (FilesService *s = FilesService::instance()) {
        // избранное поменялось — звёздочки в плитках и строках, а открытое «Избранное» — заново
        connect(s, &FilesService::starredChanged, this, [this] {
            if (starredPlace())
                reload();
            else if (!m_items.isEmpty())
                emit dataChanged(index(0), index(int(m_items.size()) - 1), {StarredRole});
        });
    }
}

QHash<int, QByteArray> FilesModel::roleNames() const
{
    return {{NameRole, "name"}, {PathRole, "path"}, {IsDirRole, "isDir"}, {ExtRole, "ext"}, {TypeRole, "type"},
            {DateRole, "date"}, {DateTextRole, "dateText"}, {SizeRole, "size"}, {SizeTextRole, "sizeText"},
            {StarredRole, "starred"}, {ImageRole, "image"}, {HiddenRole, "hidden"}};
}

QVariant FilesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &it = m_items[index.row()];
    switch (role) {
    case NameRole: return it.name;
    case PathRole: return it.path;
    case IsDirRole: return it.dir;
    case ExtRole: return it.ext;
    case TypeRole: return it.type;
    case DateRole: return it.date;
    case DateTextRole: return it.dateText;
    case SizeRole: return it.size;
    case SizeTextRole: return it.dir ? QStringLiteral("—") : FilesOps::sizeText(it.size);
    case StarredRole: return FilesService::instance() && FilesService::instance()->isStarred(it.path);
    case ImageRole: return it.image;
    case HiddenRole: return it.hidden;
    }
    return {};
}

void FilesModel::setLocation(const QString &l)
{
    if (l == m_location)
        return;
    m_location = l;
    emit locationChanged();
    watch();
    apply(load(), true);
}

void FilesModel::setShowHidden(bool on)
{
    if (on == m_showHidden)
        return;
    m_showHidden = on;
    emit showHiddenChanged();
    apply(load(), true);
}

void FilesModel::setSortKey(const QString &k)
{
    if (k == m_sortKey)
        return;
    m_sortKey = k;
    emit sortChanged();
    apply(load(), true);
}

void FilesModel::setDescending(bool d)
{
    if (d == m_descending)
        return;
    m_descending = d;
    emit sortChanged();
    apply(load(), true);
}

void FilesModel::watch()
{
    if (!m_watcher.directories().isEmpty())
        m_watcher.removePaths(m_watcher.directories());
    const QString dir = trash() ? FilesOps::trashDir() + "/files" : starredPlace() ? QString() : m_location;
    if (!dir.isEmpty() && QFileInfo(dir).isDir())
        m_watcher.addPath(dir);
}

FilesModel::Item FilesModel::itemFor(const QString &path, const QDateTime &deleted) const
{
    static QMimeDatabase db;
    const QFileInfo fi(path);
    Item it;
    it.name = fi.fileName();
    it.path = fi.absoluteFilePath();
    it.dir = fi.isDir();
    it.hidden = it.name.startsWith('.');
    it.size = it.dir ? 0 : fi.size();
    // создан — время рождения; его нет или оно до 2000 года — изменён. Папки дома телефон создал при первой
    // загрузке, когда часы ещё не были выставлены: ядро помнит их рождение как 16 ноября 1970 года
    const QDateTime born = fi.birthTime();
    it.date = deleted.isValid() ? deleted : born.isValid() && born.date().year() >= 2000 ? born : fi.lastModified();
    it.dateText = dateText(it.date);
    if (it.dir) {
        it.ext = QString();
        it.type = QStringLiteral("Папка");
    } else {
        const QMimeType mime = db.mimeTypeForFile(fi, QMimeDatabase::MatchExtension);
        it.ext = extText(it.name);
        const QString kind = kindName(mime);
        it.type = it.ext == QLatin1String("ФАЙЛ") ? kind : kind + " · " + it.ext;
        static const QSet<QString> pictures = {"image/jpeg", "image/png", "image/gif", "image/webp", "image/bmp"};
        it.image = pictures.contains(mime.name());
    }
    return it;
}

QList<FilesModel::Item> FilesModel::load() const
{
    QList<Item> items;
    auto *self = const_cast<FilesModel *>(this);
    self->m_readable = true;
    if (starredPlace()) {
        if (FilesService *s = FilesService::instance())
            for (const QString &p : s->starred())
                if (QFileInfo::exists(p))
                    items.append(itemFor(p));
    } else if (trash()) {
        const QString dir = FilesOps::trashDir();
        const QStringList names = QDir(dir + "/files").entryList(QDir::AllEntries | QDir::Hidden | QDir::System
                                                                 | QDir::NoDotAndDotDot);
        for (const QString &n : names) {
            QDateTime deleted;
            QFile info(dir + "/info/" + n + ".trashinfo");
            if (info.open(QIODevice::ReadOnly))
                for (const QByteArray &line : info.readAll().split('\n'))
                    if (line.startsWith("DeletionDate="))
                        deleted = QDateTime::fromString(QString::fromUtf8(line.mid(13)).trimmed(), Qt::ISODate);
            Item it = itemFor(dir + "/files/" + n, deleted);
            // имя в корзине бывает «имя.2» — показываем исходное
            if (info.isOpen()) {
                info.seek(0);
                for (const QByteArray &line : info.readAll().split('\n'))
                    if (line.startsWith("Path="))
                        it.name = QFileInfo(QUrl::fromPercentEncoding(line.mid(5))).fileName();
            }
            items.append(it);
        }
    } else if (!m_location.isEmpty()) {
        QDir dir(m_location);
        self->m_readable = dir.isReadable();
        const QStringList names = dir.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
        for (const QString &n : names) {
            if (!m_showHidden && n.startsWith('.'))
                continue;
            items.append(itemFor(dir.absoluteFilePath(n)));
        }
    }
    sort(items);
    return items;
}

// Папки первыми, потом по колонке, при равенстве — по имени (как люди считают: 2 раньше 10)
void FilesModel::sort(QList<Item> &items) const
{
    const QString key = m_sortKey;
    const bool desc = m_descending;
    std::stable_sort(items.begin(), items.end(), [&](const Item &a, const Item &b) {
        if (a.dir != b.dir)
            return a.dir;
        int c = 0;
        if (key == QLatin1String("type"))
            c = collator().compare(a.type, b.type);
        else if (key == QLatin1String("date"))
            c = a.date < b.date ? -1 : a.date > b.date ? 1 : 0;
        else if (key == QLatin1String("size") && !a.dir)
            c = a.size < b.size ? -1 : a.size > b.size ? 1 : 0;
        if (c == 0)
            c = collator().compare(a.name, b.name);
        return desc ? c > 0 : c < 0;
    });
}

// Новый список — по месту: ушедшие строки убираются, новые вставляются, у оставшихся — свежие сведения.
// Порядок тот же (те же правила), поэтому прежние — подпоследовательность новых. reset — сменили место или порядок:
// строки — на месте (dataChanged), убираются или добавляются только лишние: элементы строк видов стола получают новые
// файлы, а не создаются заново. Было — сброс модели: каждая папка заново ~15 строк по ~12 элементов, 30–48 мс в потоке
// интерфейса (прогон стола 19 ноября 2026, выборки стека; «убрать все и вставить» Qt тоже создавал заново;
// docs/FILES.md, «Растягивание и папки»)
void FilesModel::apply(QList<Item> items, bool reset)
{
    const int before = int(m_items.size());
    if (reset) {
        const int n = int(items.size());
        if (n < before) {
            beginRemoveRows({}, n, before - 1);
            m_items.resize(n);
            endRemoveRows();
        }
        const int common = std::min(n, before);
        for (int i = 0; i < common; ++i)
            m_items[i] = items[i];
        if (common)
            emit dataChanged(index(0), index(common - 1));
        if (n > common) {
            beginInsertRows({}, common, n - 1);
            m_items.append(items.mid(common));
            endInsertRows();
        }
    } else {
        QSet<QString> fresh;
        for (const Item &it : items)
            fresh.insert(it.path);
        for (int i = int(m_items.size()) - 1; i >= 0; --i)
            if (!fresh.contains(m_items[i].path)) {
                beginRemoveRows({}, i, i);
                m_items.removeAt(i);
                endRemoveRows();
            }
        for (int i = 0; i < items.size(); ++i) {
            if (i < m_items.size() && m_items[i].path == items[i].path) {
                const Item &o = m_items[i], &n = items[i];
                if (o.size != n.size || o.date != n.date || o.name != n.name) {
                    m_items[i] = n;
                    emit dataChanged(index(i), index(i));
                }
                continue;
            }
            beginInsertRows({}, i, i);
            m_items.insert(i, items[i]);
            endInsertRows();
        }
    }
    if (int(m_items.size()) != before || reset)
        emit countChanged();
}

void FilesModel::reload()
{
    watch();   // папку могли удалить и создать заново — следим снова
    apply(load(), false);
}

QString FilesModel::pathAt(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items[row].path : QString();
}

bool FilesModel::isDirAt(int row) const
{
    return row >= 0 && row < m_items.size() && m_items[row].dir;
}

int FilesModel::indexOf(const QString &path) const
{
    for (int i = 0; i < m_items.size(); ++i)
        if (m_items[i].path == path)
            return i;
    return -1;
}

QVariantMap FilesModel::get(int row) const
{
    if (row < 0 || row >= m_items.size())
        return {};
    const Item &it = m_items[row];
    return {{"name", it.name}, {"path", it.path}, {"isDir", it.dir}, {"size", it.size}, {"image", it.image},
            {"ext", it.ext}, {"type", it.type}, {"dateText", it.dateText},
            {"sizeText", it.dir ? QStringLiteral("—") : FilesOps::sizeText(it.size)}};
}

int FilesModel::find(const QString &prefix, int from) const
{
    const int n = int(m_items.size());
    for (int k = 0; k < n; ++k) {
        const int i = (from + k) % n;
        if (m_items[i].name.startsWith(prefix, Qt::CaseInsensitive))
            return i;
    }
    return -1;
}
