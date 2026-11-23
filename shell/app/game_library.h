// НаомиФон — игры рабочего стола (docs/GAMES.md). В QML — naomiGames.
//   Игры — файлы в папках ~/Games/<система> и в папках внутри них: NES — Денди (.nes, .unf, .unif), PS1 — образы
//   дисков (.cue, .chd, .pbp, .m3u, .iso, .img; .bin — если рядом нет его .cue). За папками следит оболочка: положили
//   файл — игра в списке меню N («03 GAMES»), убрали — её нет. Имя — имя файла без расширения и пометок в скобках
//   («BattleCity (Japan).nes» → «BattleCity»). Windows — файлы .win: запускалка игры, сценарий sh (сама игра стоит
//   в префиксе Wine, docs/WINDOWS.md).
//   Запускает игру стол (Desktop.qml): программа naomi-game (shell/apps/game) с ядром эмулятора системы
//   (~/naomi/cores/<ядро>_libretro.so, device/cores.sh); command() — её аргументы. Игру Windows — sh с её файлом.
//   gamepads — геймпады, подключённые к телефону сейчас (тот же признак, что у naomi-game): по ним стол запускает и
//   закрывает игру, когда геймпад берут со станции и ставят (docs/GAMES.md, «Геймпад и станция»). Читает их сама игра.
#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

class GameLibrary : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)   // [{path, name, system, label}] — по имени
    Q_PROPERTY(QStringList gamepads READ gamepads NOTIFY gamepadsChanged)   // имена подключённых геймпадов
public:
    explicit GameLibrary(QObject *parent = nullptr);

    QVariantList games() const { return m_games; }
    QStringList gamepads() const { return m_pads.values(); }
    QStringList padNodes() const { return m_pads.keys(); }   // их узлы /dev/input/event* (pad_keys.h)
    // аргументы naomi-game для игры: система, ядро, файл, папка сохранений (создаёт её); ядро не собрано или система
    // не наша — пусто. Игра Windows — [«win», файл запускалки]
    Q_INVOKABLE QStringList command(const QString &path) const;

signals:
    void gamesChanged();
    void gamepadsChanged();

private:
    void scan();
    void scanPads();

    QString m_root;       // ~/Games
    QString m_coresDir;   // ~/naomi/cores
    QFileSystemWatcher m_watcher;
    QTimer m_rescan;      // файл копируют — папка меняется много раз подряд
    QVariantList m_games;

    QFileSystemWatcher m_input;     // /dev/input: устройство подключили, отключили
    QMap<QString, QString> m_pads;  // узел /dev/input/event* → имя геймпада
    QSet<QString> m_notPads;        // узлы, что уже смотрели, — не геймпады
    QHash<QString, int> m_denied;   // узел → сколько раз не дали открыть (права ему udev ставит следом)
};
