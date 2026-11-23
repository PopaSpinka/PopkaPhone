#include "dev_screenshot.h"

#include <QElapsedTimer>
#include <QImage>
#include <QLoggingCategory>
#include <QQuickWindow>
#include <QSaveFile>

#include <thread>

Q_LOGGING_CATEGORY(lcShot, "naomi.shot")

// сжатие PNG — своим потоком: кадр стола 2400 × 1350 жмётся ~2 с, поток интерфейса (и стол у зрителя) столько не ждёт
void saveImage(const QImage &image, const char *path)
{
    if (image.isNull())
        return;
    std::thread([image, path] {
        QElapsedTimer t;
        t.start();
        QSaveFile file(QString::fromUtf8(path));
        const bool ok = file.open(QIODevice::WriteOnly) && image.save(&file, "PNG") && file.commit();
        qCInfo(lcShot, "снимок %s: %dx%d за %lld мс", ok ? path : "не сохранён", image.width(), image.height(),
               t.elapsed());
    }).detach();
}

void takeScreenshot(QQuickWindow *window)
{
    saveImage(window->grabWindow(), "/tmp/naomi-shot.png");
}
