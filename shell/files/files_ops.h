// НаомиФон — «Файлы» рабочего стола: операции с файлами — перенос files-ops.c НаомиОС (docs/FILES.md, «Операции»).
// Копировать, переместить, бросить (тот же диск — переместить, другой — скопировать), в корзину, навсегда,
// восстановить, очистить корзину — очередью в своём потоке, ход — строкой («КОПИРУЮ 3 ИЗ 10 · 45%»), «ОСТАНОВИТЬ».
// Имя занято — «имя (2)». Отмена (Ctrl+Z): история до 50 действий — каждое из шагов (появился файл — отмена в
// корзину; перемещён или переименован — обратно; ушёл в корзину — вернуть), отмена — шаги с конца, той же очередью.
// Корзина — по стандарту freedesktop: ~/.local/share/Trash/files и info/ИМЯ.trashinfo (Path, DeletionDate).
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

class FilesOps : public QObject
{
    Q_OBJECT
public:
    enum Kind { Copy, Move, Drop, Trash, Delete, Restore, EmptyTrash, Undo };

    explicit FilesOps(QObject *parent = nullptr);
    ~FilesOps() override;

    // sources — пути (у Restore и Delete в корзине — пути внутри Trash/files); dest — папка или пусто
    void run(Kind kind, const QStringList &sources, const QString &dest = {});
    QString status() const;   // что идёт («КОПИРУЮ 3 ИЗ 10 · 45%») или пусто
    bool busy() const;
    void cancel();
    bool canUndo() const;     // только что ушло в корзину — «ОТМЕНИТЬ» (8 с)
    QString undoWhat() const; // что отменит Ctrl+Z («ПЕРЕМЕЩЕНИЕ») — или пусто
    bool undo();
    // сделанное не через run — тоже в историю
    void rememberRename(const QString &from, const QString &to);
    void rememberCreated(const QString &path, const QString &what);

    static QString trashDir();                                   // ~/.local/share/Trash
    static QString uniqueChild(const QString &dir, const QString &name);   // «имя (2).ext»
    static QString sizeText(qint64 bytes);                       // «3,4 МБ»
    // в корзину: куда лёг (имя в Trash/files) — или пусто
    static QString trashFile(const QString &path, QString *error);
    static bool restoreFile(const QString &trashName, QString *restoredTo, QString *error);

signals:
    void changed();   // ход, конец операции, история

private:
    struct Step {
        enum Type { Created, Moved, Trashed } type;
        QString a, b;   // Created: путь; Moved: откуда → куда; Trashed: откуда → имя в корзине
    };
    struct Action { QString what; std::vector<Step> steps; };
    struct Job { Kind kind; QStringList sources; QString dest; Action undo; };

    void loop();
    void execute(Job &job);
    void setStatus(const QString &s);
    void pushHistory(Action a);

    mutable std::mutex m_lock;
    std::condition_variable m_wake;
    std::deque<Job> m_queue;
    std::vector<Action> m_history;
    QString m_status;
    bool m_busy = false;
    bool m_cancel = false;
    bool m_quit = false;
    qint64 m_undoUntil = 0;   // мс: до этого времени — «ОТМЕНИТЬ» в строке
    std::thread m_thread;
};
