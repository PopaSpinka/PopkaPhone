// НаомиФон — колесо над окном Android: жестом прокрутки (docs/ANDROID.md, «Колесо»).
//   Колесо мыши Android понимает только целыми щелчками (Waydroid делает щелчок из каждых 10 единиц оси), и на щелчок
//   список прыгает сразу на 64 dp, без движения: плавная прокрутка стола (приставка шлёт сдвиг каждый кадр)
//   превращалась в прыжки — 21 кадр в секунду, шаги по 200 мс (замер 20 ноября 2026). Палец Android ведёт плавно:
//   оболочка прикладывает его там, где курсор, и ведёт на сдвиг колеса кадр в кадр — список едет ровно на столько же.
//   - Порог касания. Первые 8 dp (12 пикселей при плотности 240) Android считает дрожанием пальца и из сдвига
//     вычитает: первый шаг длиннее на порог — список проходит ровно сдвиг колеса, а нажатия по строке не выходит
//     (палец ушёл дальше порога).
//   - Перехват. Пальцу нельзя за край слоя и экрана Android: у края он отпускает и в том же кадре берётся снова у
//     места, где колесо началось, — список в движении новое касание подхватывает сразу, без порога.
//   - Конец. Прокрутка кончилась (ScrollEnd от зрителя или 140 мс тишины) — палец отпускает; сдвиг к этому мигу уже
//     затух (приставка сглаживает), броска нет.
//   - Фокус. Касание Android считает нажатием в задачу и отдаёт ей свой фокус ввода — а на столе активным могло
//     быть другое окно Android (колесо окно не поднимает), и клавиши ушли бы прокрученному. Палец отпущен —
//     released(слой): стол возвращает фокус Android своему активному окну (AndroidWindow.qml).
//   Цена: колесо в игре — тоже жест (в Subway Surfers — шаг в сторону или прыжок); приложение, которое колесом
//   меняет масштаб (карты), теперь сдвигается.
#pragma once

#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QRectF>
#include <QTimer>

class QWaylandCompositor;
class QWaylandQuickItem;

class AndroidScroll : public QObject
{
    Q_OBJECT
public:
    explicit AndroidScroll(QObject *parent = nullptr);
    void setCompositor(QWaylandCompositor *compositor) { m_compositor = compositor; }
    // сдвиг колеса d в точке at слоя (единицы элемента слоя; вниз и вправо — плюс: туда едет содержимое);
    // end — прокрутка кончилась
    void scroll(QWaylandQuickItem *layer, QPointF at, QPointF d, bool end);

signals:
    void released(QWaylandQuickItem *layer);   // жест кончился: палец был на этом слое

private:
    bool press(QPointF dir);
    void release();
    void send(Qt::TouchPointState state, QPointF at);
    QRectF reach() const;     // где пальцу можно быть (единицы элемента слоя)
    qreal perUnit() const;    // пикселей Android в единице элемента слоя

    QWaylandCompositor *m_compositor = nullptr;
    QPointer<QWaylandQuickItem> m_layer;
    QPointF m_grip;           // где колесо началось
    QPointF m_at;             // где палец сейчас
    bool m_down = false;
    bool m_first = false;     // первый шаг после касания стоящего списка — с порогом
    QTimer m_end;
};
