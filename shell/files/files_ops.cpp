#include "files_ops.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include <sys/stat.h>

#include <cerrno>
#include <cstring>
#include <functional>

namespace {

constexpr int kHistory = 50;
constexpr qint64 kUndoMs = 8000;

qint64 nowMs() { return QDateTime::currentMSecsSinceEpoch(); }

QString infoPath(const QString &trashName)
{
    return FilesOps::trashDir() + "/info/" + trashName + ".trashinfo";
}

// размер для хода: файлы — их размер, папки — всё внутри
qint64 treeSize(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.isDir() || fi.isSymLink())
        return fi.size();
    qint64 total = 0;
    QDirIterator it(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (!it.fileInfo().isDir())
            total += it.fileInfo().size();
    }
    return total;
}

bool removeTree(const QString &path)
{
    const QFileInfo fi(path);
    if (fi.isDir() && !fi.isSymLink())
        return QDir(path).removeRecursively();
    return QFile::remove(path);
}

bool sameDevice(const QString &a, const QString &bDir)
{
    struct stat sa{}, sb{};
    return ::stat(QFile::encodeName(a).constData(), &sa) == 0 && ::stat(QFile::encodeName(bDir).constData(), &sb) == 0
           && sa.st_dev == sb.st_dev;
}

} // namespace

QString FilesOps::trashDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Trash";
}

QString FilesOps::uniqueChild(const QString &dir, const QString &name)
{
    QString path = dir + "/" + name;
    if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink())
        return path;
    // «имя (2).ext»: у папок и файлов без расширения — «имя (2)»
    const QFileInfo fi(name);
    QString base = name, ext;
    const int dot = name.lastIndexOf('.');
    if (dot > 0 && !QFileInfo(dir + "/" + name).isDir()) {
        base = name.left(dot);
        ext = name.mid(dot);
    }
    for (int n = 2;; ++n) {
        path = dir + "/" + base + " (" + QString::number(n) + ")" + ext;
        if (!QFileInfo::exists(path) && !QFileInfo(path).isSymLink())
            return path;
    }
}

QString FilesOps::sizeText(qint64 bytes)
{
    if (bytes < 1000)
        return QString::number(bytes) + " Б";
    static const char *units[] = {"КБ", "МБ", "ГБ", "ТБ"};
    double value = bytes;
    int unit = -1;
    while (value >= 1000 && unit < 3) {
        value /= 1000;
        ++unit;
    }
    QString text = QString::number(value, 'f', value < 10 ? 1 : 0) + " " + QString::fromUtf8(units[unit]);
    return text.replace('.', ',');
}

// В корзину по стандарту freedesktop: сначала info (свободное имя — «имя», «имя.2»…), потом сам файл в files/
QString FilesOps::trashFile(const QString &path, QString *error)
{
    const QString trash = trashDir();
    QDir().mkpath(trash + "/files");
    QDir().mkpath(trash + "/info");
    const QString base = QFileInfo(path).fileName();
    QString name = base;
    for (int n = 2; QFileInfo::exists(trash + "/files/" + name) || QFileInfo::exists(infoPath(name)); ++n)
        name = base + "." + QString::number(n);
    QFile info(infoPath(name));
    if (!info.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        *error = "нет места в корзине";
        return {};
    }
    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(QFileInfo(path).absoluteFilePath(), "/"));
    info.write("[Trash Info]\nPath=" + encoded.toUtf8() + "\nDeletionDate="
               + QDateTime::currentDateTime().toString("yyyy-MM-ddTHH:mm:ss").toUtf8() + "\n");
    info.close();
    const QString target = trash + "/files/" + name;
    if (::rename(QFile::encodeName(path).constData(), QFile::encodeName(target).constData()) != 0) {
        QFile::remove(infoPath(name));
        *error = errno == EXDEV ? "файл на другом диске — корзины там нет" : QString::fromLocal8Bit(strerror(errno));
        return {};
    }
    return name;
}

bool FilesOps::restoreFile(const QString &trashName, QString *restoredTo, QString *error)
{
    // «Path=…» в [Trash Info] — путь с %-кодами
    QString original;
    QFile f(infoPath(trashName));
    if (f.open(QIODevice::ReadOnly)) {
        for (const QByteArray &line : f.readAll().split('\n'))
            if (line.startsWith("Path="))
                original = QUrl::fromPercentEncoding(line.mid(5));
    }
    if (original.isEmpty()) {
        *error = "не знаю, откуда этот файл";
        return false;
    }
    const QFileInfo to(original);
    QDir().mkpath(to.absolutePath());
    const QString target = uniqueChild(to.absolutePath(), to.fileName());
    if (::rename(QFile::encodeName(trashDir() + "/files/" + trashName).constData(),
                 QFile::encodeName(target).constData()) != 0) {
        *error = QString::fromLocal8Bit(strerror(errno));
        return false;
    }
    QFile::remove(infoPath(trashName));
    *restoredTo = target;
    return true;
}

FilesOps::FilesOps(QObject *parent)
    : QObject(parent)
{
    m_thread = std::thread([this] { loop(); });
}

FilesOps::~FilesOps()
{
    {
        std::lock_guard l(m_lock);
        m_quit = true;
        m_cancel = true;
    }
    m_wake.notify_all();
    m_thread.join();
}

void FilesOps::run(Kind kind, const QStringList &sources, const QString &dest)
{
    if (sources.isEmpty() && kind != EmptyTrash)
        return;
    {
        std::lock_guard l(m_lock);
        m_queue.push_back({kind, sources, dest, {}});
    }
    m_wake.notify_all();
}

QString FilesOps::status() const
{
    std::lock_guard l(m_lock);
    return m_status;
}

bool FilesOps::busy() const
{
    std::lock_guard l(m_lock);
    return m_busy || !m_queue.empty();
}

void FilesOps::cancel()
{
    std::lock_guard l(m_lock);
    m_cancel = true;
}

bool FilesOps::canUndo() const
{
    std::lock_guard l(m_lock);
    return !m_history.empty() && nowMs() < m_undoUntil;
}

QString FilesOps::undoWhat() const
{
    std::lock_guard l(m_lock);
    return m_history.empty() ? QString() : m_history.back().what;
}

bool FilesOps::undo()
{
    Action a;
    {
        std::lock_guard l(m_lock);
        if (m_history.empty())
            return false;
        a = std::move(m_history.back());
        m_history.pop_back();
        m_undoUntil = 0;
        m_queue.push_back({Undo, {}, {}, std::move(a)});
    }
    m_wake.notify_all();
    emit changed();
    return true;
}

void FilesOps::rememberRename(const QString &from, const QString &to)
{
    // назвали только что созданную папку — то же действие: «создана» уже под новым именем
    {
        std::lock_guard l(m_lock);
        if (!m_history.empty() && m_history.back().steps.size() == 1 && m_history.back().steps[0].type == Step::Created
            && m_history.back().steps[0].a == from) {
            m_history.back().steps[0].a = to;
            return;
        }
    }
    pushHistory({"ПЕРЕИМЕНОВАНИЕ", {{Step::Moved, from, to}}});
}

void FilesOps::rememberCreated(const QString &path, const QString &what)
{
    pushHistory({what, {{Step::Created, path, {}}}});
}

void FilesOps::pushHistory(Action a)
{
    {
        std::lock_guard l(m_lock);
        m_history.push_back(std::move(a));
        if (m_history.size() > kHistory)
            m_history.erase(m_history.begin());
    }
    emit changed();
}

void FilesOps::setStatus(const QString &s)
{
    {
        std::lock_guard l(m_lock);
        m_status = s;
    }
    QMetaObject::invokeMethod(this, [this] { emit changed(); }, Qt::QueuedConnection);
}

void FilesOps::loop()
{
    for (;;) {
        Job job;
        {
            std::unique_lock l(m_lock);
            m_wake.wait(l, [this] { return m_quit || !m_queue.empty(); });
            if (m_quit)
                return;
            job = std::move(m_queue.front());
            m_queue.pop_front();
            m_busy = true;
            m_cancel = false;
        }
        execute(job);
        {
            std::lock_guard l(m_lock);
            m_busy = false;
        }
        QMetaObject::invokeMethod(this, [this] { emit changed(); }, Qt::QueuedConnection);
    }
}

// Поток операций. Каждый шаг — в историю (Action), чтобы Ctrl+Z мог вернуть.
void FilesOps::execute(Job &job)
{
    auto cancelled = [this] {
        std::lock_guard l(m_lock);
        return m_cancel;
    };
    const int total = int(job.sources.size());
    Action done;
    QString failed;

    if (job.kind == Undo) {
        const auto &steps = job.undo.steps;
        for (int i = int(steps.size()) - 1, n = 1; i >= 0 && !cancelled(); --i, ++n) {
            setStatus(QString("ОТМЕНЯЮ %1 ИЗ %2").arg(n).arg(steps.size()));
            const Step &s = steps[size_t(i)];
            QString err;
            if (s.type == Step::Created) {
                if (trashFile(s.a, &err).isEmpty())
                    failed = s.a;
            } else if (s.type == Step::Moved) {
                const QFileInfo from(s.a);
                const QString back = uniqueChild(from.absolutePath(), from.fileName());
                if (::rename(QFile::encodeName(s.b).constData(), QFile::encodeName(back).constData()) != 0)
                    failed = s.b;
            } else {
                QString to;
                if (!restoreFile(s.b, &to, &err))
                    failed = s.a;
            }
        }
        setStatus(failed.isEmpty() ? QString() : "НЕ ВЫШЛО: " + QFileInfo(failed).fileName().toUpper());
        return;
    }

    if (job.kind == EmptyTrash) {
        const QString trash = trashDir();
        const QStringList names = QDir(trash + "/files").entryList(QDir::AllEntries | QDir::Hidden | QDir::System
                                                                   | QDir::NoDotAndDotDot);
        int i = 0;
        for (const QString &name : names) {
            if (cancelled())
                break;
            setStatus(QString("ОЧИЩАЮ КОРЗИНУ %1 ИЗ %2").arg(++i).arg(names.size()));
            removeTree(trash + "/files/" + name);
            QFile::remove(infoPath(name));
        }
        setStatus({});
        return;
    }

    // копирование с ходом по байтам
    qint64 bytesTotal = 0, bytesDone = 0;
    if (job.kind == Copy || job.kind == Drop)
        for (const QString &p : job.sources)
            bytesTotal += treeSize(p);
    QString verb;
    int index = 0;
    std::function<bool(const QString &, const QString &)> copyTree = [&](const QString &from, const QString &to) {
        const QFileInfo fi(from);
        if (fi.isSymLink())
            return QFile::link(fi.symLinkTarget(), to);
        if (fi.isDir()) {
            if (!QDir().mkpath(to))
                return false;
            const QStringList kids = QDir(from).entryList(QDir::AllEntries | QDir::Hidden | QDir::System
                                                          | QDir::NoDotAndDotDot);
            for (const QString &k : kids)
                if (cancelled() || !copyTree(from + "/" + k, to + "/" + k))
                    return false;
            return true;
        }
        QFile in(from), out(to);
        if (!in.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            return false;
        QByteArray buf(1 << 20, Qt::Uninitialized);
        for (;;) {
            if (cancelled()) {
                out.close();
                QFile::remove(to);
                return false;
            }
            const qint64 n = in.read(buf.data(), buf.size());
            if (n < 0)
                return false;
            if (n == 0)
                break;
            if (out.write(buf.constData(), n) != n)
                return false;
            bytesDone += n;
            const int percent = bytesTotal > 0 ? int(bytesDone * 100 / bytesTotal) : 100;
            setStatus(QString("%1 %2 ИЗ %3 · %4%").arg(verb).arg(index).arg(total).arg(percent));
        }
        out.setPermissions(in.permissions());
        return true;
    };

    for (const QString &src : job.sources) {
        if (cancelled())
            break;
        ++index;
        const QFileInfo fi(src);
        QString err;
        switch (job.kind) {
        case Copy:
        case Move:
        case Drop: {
            const bool move = job.kind == Move || (job.kind == Drop && sameDevice(src, job.dest));
            if (QFileInfo(job.dest).absoluteFilePath().startsWith(fi.absoluteFilePath() + "/")
                || (move && fi.absolutePath() == QFileInfo(job.dest).absoluteFilePath()))
                continue;   // в саму себя или туда же, где лежит, — нечего делать
            const QString target = uniqueChild(job.dest, fi.fileName());
            if (move) {
                verb = "ПЕРЕМЕЩАЮ";
                setStatus(QString("ПЕРЕМЕЩАЮ %1 ИЗ %2").arg(index).arg(total));
                if (::rename(QFile::encodeName(src).constData(), QFile::encodeName(target).constData()) == 0) {
                    done.steps.push_back({Step::Moved, src, target});
                } else if (errno == EXDEV && copyTree(src, target) && removeTree(src)) {
                    done.steps.push_back({Step::Moved, src, target});
                } else {
                    failed = src;
                }
            } else {
                verb = "КОПИРУЮ";
                setStatus(QString("КОПИРУЮ %1 ИЗ %2").arg(index).arg(total));
                if (copyTree(src, target))
                    done.steps.push_back({Step::Created, target, {}});
                else if (!cancelled())
                    failed = src;
            }
            break;
        }
        case Trash: {
            setStatus(QString("В КОРЗИНУ %1 ИЗ %2").arg(index).arg(total));
            const QString name = trashFile(src, &err);
            if (name.isEmpty())
                failed = src;
            else
                done.steps.push_back({Step::Trashed, src, name});
            break;
        }
        case Delete: {
            setStatus(QString("УДАЛЯЮ %1 ИЗ %2").arg(index).arg(total));
            const bool inTrash = fi.absolutePath() == trashDir() + "/files";
            if (!removeTree(src))
                failed = src;
            else if (inTrash)
                QFile::remove(infoPath(fi.fileName()));
            break;
        }
        case Restore: {
            setStatus(QString("ВОССТАНАВЛИВАЮ %1 ИЗ %2").arg(index).arg(total));
            QString to;
            if (!restoreFile(fi.fileName(), &to, &err))
                failed = src;
            else
                done.steps.push_back({Step::Moved, src, to});   // отмена — снова в корзину тем же путём
            break;
        }
        default:
            break;
        }
    }
    setStatus(failed.isEmpty() ? QString() : "НЕ ВЫШЛО: " + QFileInfo(failed).fileName().toUpper());
    if (done.steps.empty())
        return;
    bool moved = false;
    for (const Step &s : done.steps)
        moved |= s.type == Step::Moved;
    switch (job.kind) {
    case Copy: done.what = "КОПИРОВАНИЕ"; break;
    case Move: done.what = "ПЕРЕМЕЩЕНИЕ"; break;
    case Drop: done.what = moved ? "ПЕРЕМЕЩЕНИЕ" : "КОПИРОВАНИЕ"; break;
    case Trash: done.what = "В КОРЗИНУ"; break;
    case Restore: done.what = "ВОССТАНОВЛЕНИЕ"; break;
    default: done.what = "ДЕЙСТВИЕ"; break;
    }
    // восстановленное отменяется возвратом в корзину: шаг «перемещён из корзины» → отмена кладёт обратно в корзину
    if (job.kind == Restore)
        for (Step &s : done.steps)
            s = {Step::Created, s.b, {}};
    {
        std::lock_guard l(m_lock);
        m_history.push_back(std::move(done));
        if (m_history.size() > kHistory)
            m_history.erase(m_history.begin());
        m_undoUntil = job.kind == Trash ? nowMs() + kUndoMs : 0;
    }
}
