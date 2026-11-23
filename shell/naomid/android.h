// naomid — задачи Android (Waydroid, docs/NAOMID.md «Android»; docs/ANDROID.md «Перетаскивание»): перенести окно
// приложения Android туда, где его оставил стол. Стол тащит окно сам и Android во время перетаскивания ничего не шлёт:
// нажатие в полосу Android Unity-игра брала себе — перетаскивание нажимало в игру. Отпустили — задачу переносит сам
// Android по команде `cmd activity task resize`, а её дают только от root в контейнере. Служба держит одну оболочку
// Android (`waydroid shell`: запуск — 1,5 с, команда в ней — ~70 мс) и подаёт в неё команды по одной; Android
// выключили — оболочка уходит с ним, следующая просьба открывает новую. Тем же — «развернуть» и «вернуть» (ResizeTask):
// задача остаётся окном Android (freeform) размером во весь его экран, а не уходит «во весь экран» Android (F11 у
// Waydroid): так её полоса и картинка лежат, как у обычного окна, а F11 доставался окну в фокусе у Android — не всегда
// тому, по чьей полосе щёлкнули. F11 — только чтобы вернуть окном задачу, уже бывшую «во весь экран» (и в фокусе).
// Sleep — усыпить и разбудить Android целиком (нет видимых окон на столе — нечего и рисовать).
// ForceStop и Memory — «закрыть принудительно» меню N стола: закрыть приложение насовсем и сколько у него памяти.
// Задачи с границами (Tasks) — для стола: где окно приложения на самом деле (угадывать по слоям Waydroid нельзя:
// у «Настроек» в два столбца — два окна в одной задаче, у растягиваемого — поверхность во весь экран).
#pragma once

#include "bus_object.h"

#include <QHash>
#include <QList>
#include <QProcess>
#include <QStringList>
#include <QTimer>

#include <functional>

class Android : public BusObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.naomi.Phone.Android")

    // Задачи Android — правда о том, где окна приложений: JSON-массив [{id, pkg, x, y, w, h, mode, visible}] (пиксели
    // экрана Android; mode — freeform | fullscreen). Обновляется по RefreshTasks и после каждого MoveTask/ResizeTask
    Q_PROPERTY(QString Tasks READ tasks)

public:
    explicit Android(QObject *parent = nullptr);
    QString tasks() const { return prop("Tasks").toString(); }

public slots:
    // перечитать задачи Android (cmd activity stack list); просьбы, пришедшие, пока читает, — одним следующим чтением
    Q_SCRIPTABLE void RefreshTasks();
    // Задачу пакета pkg, которая лежит у Android прямоугольником x, y, width × height (пиксели экрана Android), —
    // левым верхом в toX, toY; размер — её собственный. Задач у пакета несколько — та, что ближе к прямоугольнику
    Q_SCRIPTABLE void MoveTask(const QString &pkg, int x, int y, int width, int height, int toX, int toY);
    // то же, но в прямоугольник toX, toY, toWidth × toHeight: развернуть, на весь экран и вернуть (двойной щелчок и
    // кнопка на полосе окна на столе, «в окно» на панели). Задачу «во весь экран» Android (не окно) — вернуть окном клавишей F11, если
    // это приложение у Android в фокусе
    Q_SCRIPTABLE void ResizeTask(const QString &pkg, int x, int y, int width, int height, int toX, int toY,
                                 int toWidth, int toHeight);

    // Android спит или не спит — как телефон с погашенным и включённым экраном (клавиши SLEEP и WAKEUP): уснул —
    // приложения на паузе (игра стоит, музыка в фоне играет), кадров и работы нет; проснулся — всё как было. Оболочка
    // зовёт, когда на столе не осталось видимых окон Android (docs/ANDROID.md, «Сон»)
    Q_SCRIPTABLE void Sleep(bool on);

    // Закрыть приложение насовсем — «закрыть принудительно» стола (docs/DESKTOP.md): `am force-stop` — Android сам
    // убивает все процессы пакета и убирает его задачи (окна на столе уходят с ними). Через 3 с процессы пакета ещё
    // живы (оболочка Android не ответила, Android встал) — им SIGKILL отсюда, от root
    Q_SCRIPTABLE void ForceStop(const QString &pkg);
    // Память приложений — тому же списку: {пакет: КБ} — сумма PSS процессов контейнера, чьё имя — пакет или
    // «пакет:процесс» (их память оболочке не прочесть: процессы Android — чужих пользователей)
    Q_SCRIPTABLE QVariantMap Memory(const QStringList &pkgs) const;

private:
    // процессы контейнера Android по пакетам: имя процесса (cmdline) — пакет или «пакет:процесс»
    static QHash<QString, QList<qint64>> appPids(const QStringList &pkgs);
    // задачу пакета, ближайшую к x, y, width × height, — в toX, toY; размер toWidth × toHeight (0 — её собственный)
    void place(const QString &pkg, int x, int y, int width, int height, int toX, int toY, int toWidth, int toHeight);
    // команды оболочке Android по одной: done — строки вывода (пусто — не вышло)
    void run(const QString &script, std::function<void(const QStringList &)> done);
    void next();
    void read();
    void drop(const char *why);   // оболочка не отвечает или ушла — закрыть, текущей просьбе — пусто
    bool m_reading = false, m_readAgain = false;

    struct Job {
        QString script;
        std::function<void(const QStringList &)> done;
    };
    QProcess *m_sh = nullptr;
    QList<Job> m_jobs;
    bool m_busy = false;
    QByteArray m_buf;
    QStringList m_out;
    QTimer m_timeout;
};
