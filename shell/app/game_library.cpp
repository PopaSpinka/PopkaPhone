#include "game_library.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QStandardPaths>

#include <linux/input.h>

#include <cerrno>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(lcGames, "naomi.games")

namespace {
// система: папка в ~/Games, подпись в списке, ядро эмулятора (у Windows его нет), расширения файлов игр
struct System {
    const char *id, *dir, *label, *core;
    QStringList extensions;
};
const QList<System> &systems()
{
    static const QList<System> list = {
        {"nes", "NES", "NES", "mesen", {QStringLiteral("nes"), QStringLiteral("unf"), QStringLiteral("unif")}},
        {"ps1", "PS1", "PS1", "pcsx_rearmed",
         {QStringLiteral("cue"), QStringLiteral("chd"), QStringLiteral("pbp"), QStringLiteral("m3u"), QStringLiteral("iso"),
          QStringLiteral("img"), QStringLiteral("bin")}},
        {"win", "Windows", "WIN", "", {QStringLiteral("win")}},
    };
    return list;
}
}

GameLibrary::GameLibrary(QObject *parent)
    : QObject(parent)
    , m_root(QDir::homePath() + QStringLiteral("/Games"))
    , m_coresDir(QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../../cores")))
{
    m_rescan.setSingleShot(true);
    m_rescan.setInterval(300);
    connect(&m_rescan, &QTimer::timeout, this, &GameLibrary::scan);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_rescan, qOverload<>(&QTimer::start));
    scan();
    m_input.addPath(QStringLiteral("/dev/input"));
    connect(&m_input, &QFileSystemWatcher::directoryChanged, this, &GameLibrary::scanPads);
    scanPads();
}

// Геймпад — устройство evdev с кнопкой BTN_SOUTH и крестовиной или стиком, как у naomi-game (scanGamepads): у датчика
// отпечатка Pixel 5 («uinput-fpc», жесты) BTN_SOUTH тоже есть, но крестовины и стиков нет. Открываем только посмотреть.
void GameLibrary::scanPads()
{
    const QStringList nodes = QDir(QStringLiteral("/dev/input")).entryList({QStringLiteral("event*")}, QDir::System);
    bool changed = false;
    for (auto it = m_pads.begin(); it != m_pads.end();) {
        if (nodes.contains(it.key().section(QLatin1Char('/'), -1))) {
            ++it;
            continue;
        }
        qCInfo(lcGames, "геймпад отключён: %s (%s)", qPrintable(it.value()), qPrintable(it.key()));
        it = m_pads.erase(it);
        changed = true;
    }
    for (auto it = m_notPads.begin(); it != m_notPads.end();)   // узел пропал — его номер может взять другое устройство
        it = nodes.contains(it->section(QLatin1Char('/'), -1)) ? std::next(it) : m_notPads.erase(it);
    for (auto it = m_denied.begin(); it != m_denied.end();)
        it = nodes.contains(it.key().section(QLatin1Char('/'), -1)) ? std::next(it) : m_denied.erase(it);
    for (const QString &n : nodes) {
        const QString path = QStringLiteral("/dev/input/") + n;
        if (m_pads.contains(path) || m_notPads.contains(path))
            continue;
        // узел только что появился — права udev могли ещё не встать, а смена прав каталог не будит: при отказе — ещё
        // раз через полсекунды, до шести раз на узел (как у клавиатуры, hard_keyboard.cpp)
        const int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            if (errno == EACCES && ++m_denied[path] <= 6)
                QTimer::singleShot(500, this, &GameLibrary::scanPads);
            continue;
        }
        m_denied.remove(path);
        constexpr int kBits = 8 * sizeof(long);
        unsigned long keys[KEY_MAX / kBits + 1] = {}, axes[ABS_MAX / kBits + 1] = {};
        const auto has = [](const unsigned long *bits, int bit) { return bits[bit / kBits] >> (bit % kBits) & 1; };
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof axes), axes);
        const bool pad = ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys) >= 0 && has(keys, BTN_SOUTH)
                         && (has(keys, BTN_DPAD_UP) || has(axes, ABS_X) || has(axes, ABS_HAT0X));
        char name[128] = "?";
        if (pad)
            ioctl(fd, EVIOCGNAME(sizeof name), name);
        ::close(fd);
        if (!pad) {
            m_notPads.insert(path);
            continue;
        }
        m_pads.insert(path, QString::fromUtf8(name));
        qCInfo(lcGames, "геймпад: %s (%s)", name, qPrintable(path));
        changed = true;
    }
    if (changed)
        emit gamepadsChanged();
}

void GameLibrary::scan()
{
    // следим за ~/Games и папками систем, какие есть (папку системы могут создать и позже — увидим по ~/Games)
    QStringList watch = {QDir::homePath()};
    if (QFileInfo(m_root).isDir())
        watch << m_root;
    static const QRegularExpression tags(QStringLiteral("\\s*[\\(\\[][^\\)\\]]*[\\)\\]]"));
    QVariantList games;
    for (const System &s : systems()) {
        const QDir top(m_root + "/" + QLatin1String(s.dir));
        if (!top.exists())
            continue;
        // сама папка системы и папки в ней (игра PS1 — часто папкой: образ и его описание)
        QStringList dirs = {top.path()};
        const QStringList inner = top.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable);
        for (const QString &name : inner)
            dirs << top.path() + "/" + name;
        for (const QString &path : std::as_const(dirs)) {
            watch << path;
            const QFileInfoList files = QDir(path).entryInfoList(QDir::Files | QDir::Readable);
            for (const QFileInfo &file : files) {
                const QString suffix = file.suffix().toLower();
                if (file.fileName().startsWith('.') || !s.extensions.contains(suffix))
                    continue;
                // образ диска с описанием (.bin при .cue) — одна игра: её запускает описание
                if (suffix == QLatin1String("bin")
                    && QFileInfo::exists(file.path() + "/" + file.completeBaseName() + QStringLiteral(".cue")))
                    continue;
                QString name = file.completeBaseName();
                name.remove(tags);
                name = name.trimmed();
                games.append(QVariantMap{{QStringLiteral("path"), file.absoluteFilePath()},
                                         {QStringLiteral("name"), name.isEmpty() ? file.completeBaseName() : name},
                                         {QStringLiteral("system"), QLatin1String(s.id)},
                                         {QStringLiteral("label"), QLatin1String(s.label)}});
            }
        }
    }
    const QStringList watched = m_watcher.directories();
    for (const QString &path : watched)
        if (!watch.contains(path))
            m_watcher.removePath(path);
    for (const QString &path : std::as_const(watch))
        if (!watched.contains(path))
            m_watcher.addPath(path);

    QCollator collator(QLocale(QStringLiteral("ru_RU")));
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(games.begin(), games.end(), [&collator](const QVariant &a, const QVariant &b) {
        return collator.compare(a.toMap().value(QStringLiteral("name")).toString(),
                                b.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
    if (games == m_games)
        return;
    m_games = games;
    qCInfo(lcGames, "игр: %d (%s)", int(m_games.size()), qPrintable(m_root));
    emit gamesChanged();
}

QStringList GameLibrary::command(const QString &path) const
{
    for (const QVariant &g : m_games) {
        const QVariantMap game = g.toMap();
        if (game.value(QStringLiteral("path")).toString() != path)
            continue;
        const QString id = game.value(QStringLiteral("system")).toString();
        for (const System &s : systems()) {
            if (id != QLatin1String(s.id))
                continue;
            if (!*s.core)
                return {id, path};
            const QString core = m_coresDir + "/" + QLatin1String(s.core) + QStringLiteral("_libretro.so");
            if (!QFileInfo::exists(core)) {
                qCWarning(lcGames, "ядра нет: %s (tools/naomi cores)", qPrintable(core));
                return {};
            }
            const QString saves = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                                  + QStringLiteral("/naomi/games/") + id;
            QDir().mkpath(saves);
            return {id, core, path, saves};
        }
    }
    return {};
}
