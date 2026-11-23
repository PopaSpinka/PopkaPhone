// НаомиФон — «Просмотр» рабочего стола (docs/DESKTOP.md, «Просмотр»): перенос naomi-preview НаомиОС
// (~/Desktop/NaomiOS/src/preview.c). Окно рисует стол (qml/desktop/preview/PreviewWindow.qml), отсюда — в QML
// naomiPreview: что за файл (info: фото — размер в пикселях с поворотом по EXIF, видео, карточка остального), что
// из папки можно смотреть (folder — по имени, как листает НаомиОС, когда открыли не из «Файлов»; previewable —
// «Файлам», чтобы листание пропускало остальное) и видео (video — PreviewVideo, preview_video.h).
#pragma once

#include "preview_video.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>

class PreviewService : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PreviewVideo *video READ video CONSTANT)
public:
    explicit PreviewService(QObject *parent = nullptr);
    PreviewVideo *video() const { return m_video; }

    // kind: image (Qt его читает), video, card (папка, документ, фото незнакомого формата), missing (файла нет);
    // name, size (байты), sizeText («3,9 МБ»), format («JPEG»), width/height (пиксели — уже повёрнутые по EXIF),
    // animated (GIF из нескольких кадров), type и hint — для карточки («ПАПКА», «просмотр показывает фото и видео»)
    Q_INVOKABLE QVariantMap info(const QString &path) const;
    // фото и видео папки этого файла по имени (цифры — по величине), без скрытых
    Q_INVOKABLE QStringList folder(const QString &path) const;
    // фото или видео (по имени файла — быстро; листание «Файлов» пропускает остальное)
    Q_INVOKABLE bool previewable(const QString &path) const;
    Q_INVOKABLE QString sizeText(qint64 bytes) const;

private:
    PreviewVideo *m_video;
};
