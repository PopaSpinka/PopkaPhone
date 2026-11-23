// НаомиФон — плоскость кадра видео как текстура для шейдера стола («Просмотр», docs/DESKTOP.md): кадр NV12
// (PreviewVideo) — две текстуры видеочипа, яркость (R8, plane 0) и цвет (RG8 — U и V через один, plane 1, вдвое
// меньше); цвет из них считает ShaderEffect (qml/desktop/preview/shaders/nv12.frag). Процессор кадр не
// пересчитывает и не копирует: данные уходят в текстуру прямо из буфера декодера, с его шагом строк. В QML —
// Naomi.Preview/VideoPlane: невидимый элемент, источник текстуры для ShaderEffect.
#pragma once

#include <QPointer>
#include <QQuickItem>

class PreviewVideo;
class PlaneProvider;

class VideoPlane : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(PreviewVideo *video READ video WRITE setVideo NOTIFY videoChanged)
    Q_PROPERTY(int plane READ plane WRITE setPlane NOTIFY planeChanged)   // 0 — яркость, 1 — цвет
public:
    explicit VideoPlane(QQuickItem *parent = nullptr);
    ~VideoPlane() override;

    PreviewVideo *video() const { return m_video; }
    void setVideo(PreviewVideo *video);
    int plane() const { return m_plane; }
    void setPlane(int plane);

    bool isTextureProvider() const override { return true; }
    QSGTextureProvider *textureProvider() const override;

signals:
    void videoChanged();
    void planeChanged();

private:
    QPointer<PreviewVideo> m_video;
    int m_plane = 0;
    mutable PlaneProvider *m_provider = nullptr;
};
