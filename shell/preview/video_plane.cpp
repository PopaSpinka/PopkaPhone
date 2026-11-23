#include "video_plane.h"

#include "preview_video.h"

#include <QLoggingCategory>
#include <QSGTexture>
#include <QSGTextureProvider>
#include <rhi/qrhi.h>

#include <memory>

Q_DECLARE_LOGGING_CATEGORY(lcPreviewVideo)

// Текстура одной плоскости: кадр сменился — в commitTextureOperations (поток отрисовки стола, перед кадром)
// загрузка прямо из буфера кадра; размер сменился — новая текстура. Кадра нет — 2 × 2 чёрного.
class PlaneTexture : public QSGTexture
{
public:
    PlaneTexture(const QPointer<PreviewVideo> &video, int plane)
        : m_video(video)
        , m_plane(plane)
    {
        setFiltering(QSGTexture::Linear);
        setHorizontalWrapMode(QSGTexture::ClampToEdge);
        setVerticalWrapMode(QSGTexture::ClampToEdge);
    }
    ~PlaneTexture() override
    {
        if (m_texture)
            m_texture->deleteLater();   // отпустит сам RHI в конце кадра — контекст GL здесь не текущий
    }
    void setVideo(const QPointer<PreviewVideo> &video) { m_video = video; }

    qint64 comparisonKey() const override { return qint64(reinterpret_cast<quintptr>(this)); }
    QRhiTexture *rhiTexture() const override { return m_texture; }
    QSize textureSize() const override { return m_size; }
    bool hasAlphaChannel() const override { return false; }
    bool hasMipmaps() const override { return false; }

    void commitTextureOperations(QRhi *rhi, QRhiResourceUpdateBatch *updates) override
    {
        std::shared_ptr<const VideoFrame> frame = m_video ? m_video->frame() : nullptr;
        const QSize size = !frame ? QSize(2, 2)
                         : m_plane == 0 ? QSize(frame->width, frame->height)
                                        : QSize((frame->width + 1) / 2, (frame->height + 1) / 2);
        const QRhiTexture::Format format = m_plane == 0 ? QRhiTexture::R8 : QRhiTexture::RG8;
        if (!m_texture || m_size != size) {
            if (m_texture)
                m_texture->deleteLater();
            m_texture = rhi->newTexture(format, size);
            if (!m_texture->create())
                qCWarning(lcPreviewVideo, "видео: текстура %dx%d не создана", size.width(), size.height());
            m_size = size;
            m_serial = 0;
            m_blank = false;
        }
        if (!frame) {
            if (!m_blank) {   // чёрный: Y = 0, U = V = ½
                const QByteArray black(m_plane == 0 ? 4 : 8, m_plane == 0 ? char(0) : char(128));
                updates->uploadTexture(m_texture, QRhiTextureUploadEntry(0, 0, QRhiTextureSubresourceUploadDescription(black)));
                m_blank = true;
            }
            m_hold.reset();
            return;
        }
        if (frame->serial == m_serial)
            return;
        const int stride = m_plane == 0 ? frame->strideY : frame->strideUV;
        const unsigned char *data = m_plane == 0 ? frame->y : frame->uv;
        // данные — без копии: кадр держим до следующей загрузки (загрузка — в этом же кадре стола)
        QRhiTextureSubresourceUploadDescription desc(
            QByteArray::fromRawData(reinterpret_cast<const char *>(data), stride * size.height()));
        desc.setDataStride(quint32(stride));
        desc.setSourceSize(size);
        updates->uploadTexture(m_texture, QRhiTextureUploadEntry(0, 0, desc));
        m_serial = frame->serial;
        m_hold = std::move(frame);
        m_blank = false;
    }

private:
    QPointer<PreviewVideo> m_video;
    int m_plane;
    QRhiTexture *m_texture = nullptr;
    QSize m_size;
    quint64 m_serial = 0;
    bool m_blank = false;
    std::shared_ptr<const VideoFrame> m_hold;
};

class PlaneProvider : public QSGTextureProvider
{
public:
    PlaneProvider(const QPointer<PreviewVideo> &video, int plane)
        : m_texture(video, plane)
    {
    }
    QSGTexture *texture() const override { return const_cast<PlaneTexture *>(&m_texture); }
    void changed() { emit textureChanged(); }
    void setVideo(const QPointer<PreviewVideo> &video) { m_texture.setVideo(video); }

private:
    PlaneTexture m_texture;
};

// ItemHasContents: новый кадр — update() этого элемента, и стол рисует кадр. Одного textureChanged мало: ShaderEffect
// по нему лишь помечает материал, а кадр не просит — видео шло 8–9 к/с, попутно с другими переменами сцены
// (время на полосе окна), при 30 кадрах декодера (замер 18 ноября 2026)
VideoPlane::VideoPlane(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

VideoPlane::~VideoPlane()
{
    delete m_provider;
}

void VideoPlane::setVideo(PreviewVideo *video)
{
    if (m_video == video)
        return;
    if (m_video)
        disconnect(m_video, nullptr, this, nullptr);
    m_video = video;
    if (m_provider)
        m_provider->setVideo(m_video);
    if (m_video)
        connect(m_video, &PreviewVideo::frameChanged, this, [this] {
            if (m_provider)
                m_provider->changed();
            update();
        });
    emit videoChanged();
}

void VideoPlane::setPlane(int plane)
{
    if (m_plane == plane || m_provider)
        return;   // плоскость — до первой текстуры
    m_plane = plane;
    emit planeChanged();
}

QSGTextureProvider *VideoPlane::textureProvider() const
{
    if (!m_provider)
        m_provider = new PlaneProvider(m_video, m_plane);
    return m_provider;
}
