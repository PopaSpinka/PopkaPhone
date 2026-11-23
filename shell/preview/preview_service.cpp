#include "preview_service.h"

#include "preview_video.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QImageIOHandler>
#include <QImageReader>
#include <QLocale>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {

// что Qt умеет читать (image/jpeg, image/png, image/gif, image/svg+xml…) — по типу из имени файла
const QSet<QByteArray> &readableImages()
{
    static const QSet<QByteArray> set = [] {
        QSet<QByteArray> s;
        for (const QByteArray &m : QImageReader::supportedMimeTypes())
            s.insert(m);
        return s;
    }();
    return set;
}

// image/jpeg → JPEG, image/svg+xml → SVG, video/x-matroska → MATROSKA (format_text у НаомиОС)
QString formatText(const QString &mime)
{
    const int slash = mime.indexOf('/');
    if (slash < 0)
        return {};
    QString sub = mime.mid(slash + 1);
    if (sub.startsWith(QLatin1String("x-")))
        sub = sub.mid(2);
    const int end = sub.indexOf(QRegularExpression(QStringLiteral("[+;]")));
    return (end < 0 ? sub : sub.left(end)).toUpper();
}

// для карточки: «ПАПКА», «ТЕКСТ», «ФАЙЛ .ZIP» (type_text у НаомиОС)
QString typeText(const QFileInfo &fi, const QString &mime)
{
    if (fi.isDir())
        return QStringLiteral("ПАПКА");
    static const struct { const char *prefix, *label; } kinds[] = {
        {"text/", "ТЕКСТ"}, {"video/", "ВИДЕО"}, {"audio/", "ЗВУК"}, {"image/", "КАРТИНКА"},
        {"application/pdf", "PDF"}, {"font/", "ШРИФТ"},
    };
    for (const auto &k : kinds)
        if (mime.startsWith(QLatin1String(k.prefix)))
            return QString::fromUtf8(k.label);
    const QString name = fi.fileName();
    const int dot = name.lastIndexOf('.');
    if (dot > 0 && dot < name.size() - 1)
        return QStringLiteral("ФАЙЛ ") + name.mid(dot).toUpper();
    return QStringLiteral("ФАЙЛ");
}

} // namespace

PreviewService::PreviewService(QObject *parent)
    : QObject(parent)
    , m_video(new PreviewVideo(this))
{
}

// «3,9 МБ» — по 1000, как в «Файлах» НаомиОС (size_text)
QString PreviewService::sizeText(qint64 bytes) const
{
    if (bytes < 1000)
        return QStringLiteral("%1 Б").arg(bytes);
    static const char *units[] = {"КБ", "МБ", "ГБ", "ТБ"};
    double value = double(bytes);
    int unit = -1;
    while (value >= 1000 && unit < 3) {
        value /= 1000;
        ++unit;
    }
    QString text = QString::number(value, 'f', value < 10 ? 1 : 0) + ' ' + QString::fromUtf8(units[unit]);
    return text.replace('.', ',');
}

QVariantMap PreviewService::info(const QString &path) const
{
    const QFileInfo fi(path);
    QVariantMap out{{"path", path}, {"name", fi.fileName()}};
    if (!fi.exists()) {
        out.insert("kind", "missing");
        out.insert("type", QStringLiteral("НЕ ОТКРЫЛОСЬ"));
        out.insert("hint", QStringLiteral("файла больше нет"));
        return out;
    }
    const qint64 size = fi.isFile() ? fi.size() : 0;
    out.insert("size", size);
    out.insert("sizeText", size > 0 ? sizeText(size) : QString());
    const QString mime = fi.isDir() ? QString() : QMimeDatabase().mimeTypeForFile(fi).name();
    out.insert("format", formatText(mime));
    if (fi.isFile() && readableImages().contains(mime.toLatin1())) {
        // размер — из заголовка (доли миллисекунды), повёрнутый по EXIF: окно — сразу нужной формы
        QImageReader reader(path);
        QSize px = reader.size();
        if (px.isValid()) {
            if (reader.transformation() & QImageIOHandler::TransformationRotate90)
                px.transpose();
            out.insert("kind", "image");
            out.insert("width", px.width());
            out.insert("height", px.height());
            out.insert("animated", reader.supportsAnimation() && reader.imageCount() != 1);
            return out;
        }
    }
    if (fi.isFile() && mime.startsWith(QLatin1String("video/"))) {
        out.insert("kind", "video");
        return out;
    }
    out.insert("kind", "card");
    out.insert("type", typeText(fi, mime));
    out.insert("hint", mime.startsWith(QLatin1String("image/")) ? QStringLiteral("этот формат фото просмотр пока не открывает")
                                                                : QStringLiteral("просмотр показывает фото и видео"));
    return out;
}

bool PreviewService::previewable(const QString &path) const
{
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension);
    return mime.name().startsWith(QLatin1String("video/")) || readableImages().contains(mime.name().toLatin1());
}

QStringList PreviewService::folder(const QString &path) const
{
    const QDir dir = QFileInfo(path).absoluteDir();
    QStringList names = dir.entryList(QDir::Files);
    names.erase(std::remove_if(names.begin(), names.end(), [&](const QString &n) {
                    return !previewable(dir.filePath(n));
                }), names.end());
    QCollator collator{QLocale(QLocale::English)};
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(names.begin(), names.end(), collator);
    QStringList out;
    for (const QString &n : names)
        out.append(dir.filePath(n));
    return out;
}
