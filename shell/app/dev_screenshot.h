// НаомиФон — снимок экрана для разработчика: PNG окна оболочки в /tmp/naomi-shot.png и последнего кадра рабочего
// стола в /tmp/naomi-desk.png. Запрос — сигнал SIGUSR2 (UnixSignals); с Mac — tools/naomi shot [desk].
#pragma once

class QImage;
class QQuickWindow;

void takeScreenshot(QQuickWindow *window);
// PNG — целиком или никак (запись во временный и переименование): tools/naomi shot ждёт файл
void saveImage(const QImage &image, const char *path);
