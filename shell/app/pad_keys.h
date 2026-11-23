// НаомиФон — кнопки геймпада клавишами (docs/GAMES.md, «Кнопки геймпада»). В QML — naomiPadKeys.
//   Раскладка — у каждой игры своя: «как есть» (геймпад читает сама игра: Денди и PS1 — naomi-game) или «клавишами»
//   (кнопка геймпада — клавиша клавиатуры; так у игр Windows). Хранится в ~/.config/naomi/desktop/pads.json:
//   {путь игры: {mode: "keys" | "native", keys: {кнопка: клавиша}}}; кнопки — a b x y (по месту: A справа, B снизу,
//   X сверху, Y слева, как у 8BitDo и Switch), l1 r1 l2 r2, du dd dl dr (крестовина), ls_u ls_d ls_l ls_r l3 и
//   rs_u … r3 (стики по направлениям и нажатием), minus plus star home, p1 p2 (лепестки); клавиши — Esc, Q, 1, Space,
//   Up… (keyCode). Нет записи — по умолчанию: Windows — «клавишами», остальные — «как есть».
//   Переводит стол: окно игры впереди (setTarget — его поверхность) и раскладка «клавишами» — оболочка забирает
//   геймпады себе (EVIOCGRAB: игра их больше не слышит, двойных нажатий нет) и шлёт клавиши окну игры клавиатурой
//   Wayland, как клавиатура зрителя. Правка раскладки — сразу. Окно игры ушло назад — геймпад свободен, зажатые
//   клавиши отпущены. listening — окно «Кнопки геймпада» впереди: геймпад тоже наш, но не играет, а называет
//   нажатую кнопку (pressed) — её выбирают на рисунке.
//   Стик: отклонён больше чем на половину — направление нажато, вернулся меньше чем на треть — отпущено.
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QVariantMap>

class GameLibrary;
class QSocketNotifier;
class QWaylandQuickItem;

class PadKeys : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool listening READ listening WRITE setListening NOTIFY listeningChanged)
    Q_PROPERTY(int revision READ revision NOTIFY layoutsChanged)   // раскладки сменились — для привязок QML
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)       // переводит сейчас: окно игры впереди, «клавишами»
public:
    explicit PadKeys(GameLibrary *games, QObject *parent = nullptr);
    ~PadKeys() override;

    // раскладка игры: {mode, keys}; system — её система (win, nes, ps1): у какой «клавишами» по умолчанию
    Q_INVOKABLE QVariantMap layout(const QString &game, const QString &system) const;
    Q_INVOKABLE void setLayout(const QString &game, const QVariantMap &layout);
    // окно игры впереди: item — его поверхность (WebSurface), game и system — какая игра; null — игры впереди нет
    Q_INVOKABLE void setTarget(QObject *item, const QString &game, const QString &system);

    bool listening() const { return m_listening; }
    void setListening(bool on);
    int revision() const { return m_revision; }
    bool active() const { return m_mode == Mode::Keys; }

signals:
    void pressed(const QString &control);   // listening: нажали кнопку (или стик, крестовину — направление)
    void listeningChanged();
    void layoutsChanged();
    void activeChanged();

private:
    enum class Mode { Off, Keys, Listen };
    struct Pad {
        QString path, name;
        QSocketNotifier *notifier = nullptr;
        QHash<int, int> absMin, absMax;   // ось → её пределы
        QHash<int, int> dir;              // ось → куда отклонена: −1, 0, +1
    };
    void update();                        // что делать сейчас (m_mode), открыть или закрыть геймпады
    void openPads();
    void closePads();
    void read(int fd);
    void button(const QString &control, bool down);   // кнопка геймпада (или направление стика, крестовины)
    void axis(Pad &pad, int code, int value);
    void releaseAll();                    // отпустить всё зажатое (клавиши — окну игры)
    void sendKey(int code, bool down);
    void load();
    void save() const;

    GameLibrary *m_games;
    QString m_file;                       // ~/.config/naomi/desktop/pads.json
    QJsonObject m_layouts;
    int m_revision = 0;
    QPointer<QWaylandQuickItem> m_target;
    QString m_game, m_system;
    QHash<QString, int> m_keys;           // раскладка окна впереди: кнопка → код клавиши (evdev)
    bool m_listening = false;
    Mode m_mode = Mode::Off;
    QHash<int, Pad> m_pads;               // fd → геймпад (открыты, пока Keys или Listen)
    QHash<QString, int> m_down;           // кнопка → сколькими геймпадами нажата
    QHash<int, int> m_keyDown;            // клавиша → сколькими кнопками нажата
    QSet<int> m_unknown;                  // незнакомые коды кнопок — в журнал по разу (лепестки P1, P2?)
};
