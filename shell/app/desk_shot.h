// НаомиФон — скриншоты рабочего стола (docs/DESKTOP.md, «Скриншоты»): перенос скриншотов НаомиОС (bcdesk, раздел
// «Скриншоты»): Shift+Alt+3 — весь стол, Shift+Alt+4 — область. В QML — naomiShot; клавиши ловит ввод зрителя
// (StreamInput, naomiStream.screenshotKey), миниатюру и прицел рисует стол (qml/desktop/Shot*.qml); щелчок по
// миниатюре — снимок в «Просмотре» один к одному (preview/PreviewWindow.qml).
// Кадр — следующий кадр стола (DesktopScreen::captureNext): ровно то, что получает зритель, пиксель в пиксель, без
// курсора (его рисует зритель). PNG пишет свой поток: ~/Screenshots/«Скриншот 2026-11-18 в 07.53.56.png», сначала —
// во временный «.….png.part» с точкой в начале («Файлы» его не покажут), потом — переименованием: недописанный снимок
// никто не увидит. Там же — миниатюра (усреднением, без ряби). Готово — saved, миниатюра — image://deskshot/N;
// не вышло — failed (плашка «НЕ ПОЛУЧИЛСЯ»).
#pragma once

#include <QImage>
#include <QObject>
#include <QRectF>
#include <QSizeF>
#include <QTimer>

#include <thread>

class DesktopScreen;
class QQuickImageProvider;

class DeskShot : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)   // снимок берётся или пишется — следующий подождёт
public:
    explicit DeskShot(DesktopScreen *desk, QObject *parent = nullptr);
    ~DeskShot() override;
    // миниатюры для QML (image://deskshot/N): движок QML забирает его себе
    QQuickImageProvider *imageProvider();

    bool busy() const { return m_busy; }
    // снять стол: area — в точках стола (рамка прицела), пустой — весь; false — прошлый снимок ещё не готов
    Q_INVOKABLE bool take(const QRectF &area);

signals:
    // path — файл; thumb — миниатюра, size — её размер в точках стола; folderCreated — папку создал этот снимок
    void saved(const QString &path, const QString &thumb, const QSizeF &size, bool folderCreated);
    void failed(const QString &why);
    void busyChanged();

private:
    void captured(QImage image);
    void finish(bool ok, QImage thumb);
    void fail(const QString &why);
    void setBusy(bool on);

    DesktopScreen *m_desk;
    class Provider;
    Provider *m_provider = nullptr;
    bool m_busy = false;
    bool m_waiting = false;    // ждём кадр (captureNext)
    QTimer m_timeout;          // кадра так долго нет — не получился
    QString m_path;
    bool m_folderCreated = false;
    QSizeF m_thumbSize;        // миниатюра, точки стола
    QSize m_thumbPixels;       // она же в пикселях кадра
    std::thread m_writer;
    int m_serial = 0;
};
