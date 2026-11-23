// НаомиФон — перетаскивание окна Android столом (docs/ANDROID.md, «Перетаскивание и мерцание»). В QML — AndroidMove
// (Naomi.Desk).
//   Android двигал окно сам — за свою полосу: на каждый шаг мыши новое место задачи и перекладка приложения, а Waydroid
//   при смене расклада экрана заново отдаёт оболочке буферы всех окон — 13–46 кадров в секунду, окно прыгало назад
//   (запись потока приставки, 18 ноября 2026). К тому же Unity-игра брала нажатие в полосу Android себе: перетаскивание
//   нажимало в игру. Поэтому окно тащит только стол: полоса окна — своя (AndroidWindow.qml, press/drag/release),
//   Android во время перетаскивания ничего не получает, окно сдвигает стол — элемент поверхности целиком, как окна
//   «Файлов», в такт стола. Отпустили — задачу у Android переносит команда (moveTask → naomiAndroid.moveTask → naomid:
//   `cmd activity task resize`), а стол, пока Android не довёл, держит картинку, где отпустили: каждый его шаг (слои с
//   прежним размером сдвинулись все на одно и то же) тут же вычитает из своего сдвига. Окно развёрнуто на весь экран
//   Android — не тащится. Рамка окна — задача Android, её границы сообщает он сам (naomid Tasks, setTasks;
//   docs/ANDROID.md, «Рамка окна — задача Android»): по слоям Waydroid окно не угадать — у «Настроек» шире 1200
//   пикселей это два окна в одной задаче.
#pragma once

#include <QHash>
#include <QList>
#include <QVariantList>
#include <QPointF>
#include <QRect>
#include <QPointer>
#include <QQuickItem>
#include <QTimer>

class AndroidMove : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem *surface READ surface WRITE setSurface NOTIFY surfaceChanged)   // элемент окна Waydroid
    Q_PROPERTY(QPointF offset READ offset NOTIFY offsetChanged)                       // сдвиг столом, точки стола
    Q_PROPERTY(bool moving READ moving NOTIFY movingChanged)                          // стол сейчас тащит окно
    // окно Android (задача: без полей тени), точки стола от этого элемента, со сдвигом стола; нет слоёв — пустой
    Q_PROPERTY(QRectF window READ window NOTIFY windowChanged)
    // приложение (пакет): по нему помнится место до «развернуть» — Waydroid, разворачивая, бывает, пересоздаёт окно
    Q_PROPERTY(QString pkg MEMBER m_pkg)
    Q_PROPERTY(bool full READ full NOTIFY fullChanged)              // развёрнуто или на весь экран — не тащится
    Q_PROPERTY(bool panel READ panel NOTIFY panelChanged)           // на весь экран (как браузер в режиме панели)
    // скрытая полоса экрана Android сверху, пиксели: под панелью стола, туда уходит полоса Android окна на весь экран
    Q_PROPERTY(int hiddenPx MEMBER m_hiddenPx)
    Q_PROPERTY(bool resizing READ resizing NOTIFY resizingChanged)  // стол тянет край окна (или ждёт Android)
    // Android открыл окно больше видимой области — стол ставит его в неё и до тех пор окно не показывает (fitAtOpen)
    Q_PROPERTY(bool fitting READ fitting NOTIFY fittingChanged)
    // задачи Android (naomiAndroid.tasks): рамка окна — своя задача; слои сменились сами — wantTasks
    Q_PROPERTY(QVariantList tasks READ tasks WRITE setTasks)
public:
    explicit AndroidMove(QQuickItem *parent = nullptr);

    QQuickItem *surface() const { return m_surface; }
    void setSurface(QQuickItem *surface);
    QPointF offset() const { return m_offset; }
    bool moving() const { return m_moving; }
    QRectF window() const { return m_window; }
    bool full() const { return m_full; }
    bool panel() const { return m_panel; }
    bool resizing() const { return !m_preview.isEmpty(); }
    bool fitting() const { return m_fitting; }

    // нажали в полосу окна, мышь идёт, отпустили (точки этого элемента)
    Q_INVOKABLE void press(QPointF at);
    Q_INVOKABLE void drag(QPointF at);
    Q_INVOKABLE void release();
    // двойной щелчок по полосе: развернуть во всю видимую часть экрана Android (задача — окно Android такого размера)
    // или вернуть туда, где окно было до того
    Q_INVOKABLE void toggleMaximized();
    Q_INVOKABLE void togglePanel();   // на весь экран и обратно (кнопка на полосе окна и на панели стола)
    Q_INVOKABLE void zoomNow();       // то, что просили (zoomAsked), — делать: рамка — на новое место, задача — Android
    // тянут край окна: новая рамка (точки этого элемента) — стол показывает её сразу, Android — когда отпустили
    // (commitResize: задачу — в эту рамку; до тех пор, как Android её не примет, рамка — стола)
    Q_INVOKABLE void setPreview(QRectF rect);
    QVariantList tasks() const { return m_tasks; }
    void setTasks(const QVariantList &tasks);
    Q_INVOKABLE void commitResize();

signals:
    void surfaceChanged();
    void offsetChanged();
    void movingChanged();
    void windowChanged();
    void fullChanged();
    void panelChanged();
    void wantTasks();   // перечитать задачи Android (naomiAndroid.refreshTasks)
    void resizingChanged();
    void fittingChanged();
    // отпустили: задачу Android — из from (пиксели экрана Android) левым верхом в to (там окно видно на столе)
    void moveTask(QRect from, QPoint to);
    void resizeTask(QRect from, QRect to);   // развернуть / вернуть: задачу — из from в to (пиксели экрана Android)
    // развернуть, вернуть, на весь экран — просят: окно снимает свою картинку, пока рамка и обрезка слоёв прежние,
    // и зовёт zoomNow() (AndroidWindow.qml, «с движением»)
    void zoomAsked();

protected:
    void itemChange(ItemChange change, const ItemChangeData &data) override;

private:
    void watchLayers();       // слои — дети элемента поверхности (подповерхности Waydroid)
    static bool hasPicture(const QQuickItem *layer);   // у слоя есть картинка (не пустой буфер Waydroid)
    QList<QRectF> content() const;   // слои с картинкой — целиком (AndroidSurface::fullRect)
    void check();             // слои сменились: во весь экран ли, перенёс ли Android окно
    void place();             // поставить элемент поверхности: сдвиг или 0 (развёрнуто)
    void frameWindow();       // рамка окна Android (window) и картинка, положенная под полосу Android
    qreal px() const;         // пикселей Android в единице элемента поверхности
    bool sameSize(QSizeF a, QSizeF b) const;   // рамка не сменила размер (с допуском на округления)
    QPointF keep(QPointF offset) const;   // сдвиг, при котором полосу окна можно взять (не за краем стола)

    QPointer<QQuickItem> m_surface;
    QRectF m_task;                     // задача у Android без полей тени (единицы элемента поверхности)
    QString m_pkg;
    QVariantList m_tasks;
    bool m_known = false;              // Android сказал, где задача (setTasks)
    QRectF m_taskPx;                   // задача у Android, пиксели экрана: его ответ, между ответами — вслед за слоями
    QRectF m_pendingTask;              // ответ Android, ждущий, пока слои успокоятся (свой перенос или размер идёт)
    QString m_pendingMode;
    QTimer m_askTasks, m_applyTask;
    QTimer m_sameTasks;                // спросили задачи, а список не сменился — приложить прежний
    void applyTask();
    QRect m_broughtFrom;               // откуда уже просили перенести в экран (bringIntoView)
    void setFull(bool full);
    void setPanel(bool panel);
    QRect screenRect() const;   // весь экран Android, пиксели
    QRect seenRect() const;     // видимая его часть — без скрытой полосы сверху
    QRect normalRect() const;
    // задачу — в to (развернуть, на весь экран, вернуть); remember — запомнить нынешнее место как «куда вернуть»
    void resizeTo(const QRect &to, const char *what, bool remember = true);
    // Окно, открытое Android больше видимой области (было: «Настройки» 2400 × 1355 с y = 69 при экране 1360 — низ за
    // краем), — в неё: размер не больше области, место — внутри. По слоям это видно раньше ответа Android — окно до
    // конца перестановки не показывается (fitting), чтобы не мигнуть большим
    QRect fitted(const QRectF &task) const;   // пиксели экрана Android; пусто — окно и так помещается
    void setFitting(bool fitting);
    void askZoom(const QRect &to, const char *what);
    QRect m_zoomTo;                    // куда просили (до zoomNow)
    const char *m_zoomWhat = "";
    bool m_fitting = false;
    bool m_fitDone = false;            // решено: помещается или переставлено
    QTimer m_fitEnd;                   // Android не переставил за 1,5 с — показать как есть
    bool m_panel = false;
    int m_hiddenPx = 0;
    QPointF layersMoved(const QHash<const QQuickItem *, QRectF> &now) const;
    void compensate(QPointF d);
    QHash<const QQuickItem *, QRectF> m_layers;   // слои с картинкой в прошлую проверку (целиком)
    void bringIntoView();
    QRectF m_preview;                  // рамка, в которую тянут край (единицы элемента поверхности); пусто — нет
    bool m_previewSent = false;        // отпустили: Android переносит задачу в m_preview
    QTimer m_previewEnd;               // не принял за 4 с — рамка снова его
    void endPreview();
    static QHash<QString, QRect> s_normal;   // где окно приложения было до «развернуть» (пиксели экрана Android)
    QRect androidRect(const QRectF &r) const;   // единицы элемента поверхности → пиксели экрана Android
    QPointF m_pressAt;                 // мышь при нажатии (точки этого элемента)
    QPointF m_offset, m_pressOffset;
    bool m_moving = false;
    bool m_full = false;               // развёрнуто или на весь экран — без сдвига
    QRectF m_window;
    QTimer m_check;                    // пачка смен слоёв — одной проверкой (и перед кадром стола)
    QMetaObject::Connection m_sync;
    QTimer m_settle;                   // отпустили — ждём переноса Android
};
