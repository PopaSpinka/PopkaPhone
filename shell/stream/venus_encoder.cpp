#include "venus_encoder.h"

#include <linux/dma-buf.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr const char *kDevice = "/dev/video/venus_enc";
constexpr uint32_t kOut = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;    // вход кодера: сырые кадры
constexpr uint32_t kCap = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;   // выход: сжатое
// msm_vidc: номер dma-buf и смещение — в planes[].reserved[0] и [1]
constexpr int kReservedFd = 0, kReservedOffset = 1;
// своя настройка Qualcomm (V4L2_CID_MPEG_MSM_VIDC_BASE + 0x35): «скорость работы», к/с × 65536 — по ней драйвер
// выбирает частоты кодера: чем она выше, тем быстрее сжатие; предел — VenusEncoder::maxOperatingRate
constexpr uint32_t kCidOperatingRate = 0x00992035;
// своя настройка Qualcomm (V4L2_CID_MPEG_MSM_VIDC_BASE + 0x38): режим малой задержки, 0 / 1. Имён настройки драйвер
// не называет — найдена перебором: из 19 настроек с двумя-тремя значениями время сжатия меняет только она
constexpr uint32_t kCidLowLatency = 0x00992038;
// своя настройка Qualcomm (V4L2_CID_MPEG_VIDC_VENC_BITRATE_SAVINGS, BASE + 131): «экономия битов» — кодер сам решает,
// где перемену «не видно», и такие блоки не пишет: копирует из прошлого кадра. По умолчанию включена (3: и 8, и 10
// бит). На столе это текст движущегося окна — буквы двоятся и крошатся, вокруг цветные точки (docs/STREAM.md,
// «Экономия битов»); 0 — выключена
constexpr uint32_t kCidBitrateSavings = 0x00992083;

int xioctl(int fd, unsigned long req, void *arg)
{
    int r;
    do
        r = ioctl(fd, req, arg);
    while (r < 0 && errno == EINTR);
    return r;
}

int alignUp(int v, int a) { return (v + a - 1) / a * a; }

// кодер пишет мимо кэша процессора: перед чтением — сброс кэша по dma-buf
void cpuRead(int fd, bool begin)
{
    dma_buf_sync s{__u64(DMA_BUF_SYNC_READ | (begin ? DMA_BUF_SYNC_START : DMA_BUF_SYNC_END))};
    ioctl(fd, DMA_BUF_IOCTL_SYNC, &s);
}

} // namespace

VenusEncoder::~VenusEncoder()
{
    close();
}

bool VenusEncoder::setControl(uint32_t id, int32_t value, const char *name)
{
    v4l2_control c{id, value};
    if (xioctl(m_fd, VIDIOC_S_CTRL, &c) < 0) {
        fprintf(stderr, "venus: настройка «%s» = %d: %s\n", name, value, strerror(errno));
        return false;
    }
    return true;
}

bool VenusEncoder::open(const Settings &s)
{
    m_settings = s;
    m_fd = ::open(kDevice, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0) {
        fprintf(stderr, "venus: %s: %s\n", kDevice, strerror(errno));
        return false;
    }
    // сначала кодек (выход): без него драйвер не знает размеров
    v4l2_format f{};
    f.type = kCap;
    f.fmt.pix_mp.width = s.width;
    f.fmt.pix_mp.height = s.height;
    f.fmt.pix_mp.pixelformat = s.hevc ? V4L2_PIX_FMT_HEVC : V4L2_PIX_FMT_H264;
    f.fmt.pix_mp.num_planes = 1;
    if (xioctl(m_fd, VIDIOC_S_FMT, &f) < 0) {
        fprintf(stderr, "venus: формат выхода: %s\n", strerror(errno));
        return false;
    }
    m_packetSize = f.fmt.pix_mp.plane_fmt[0].sizeimage;

    f = {};
    f.type = kOut;
    f.fmt.pix_mp.width = s.width;
    f.fmt.pix_mp.height = s.height;
    f.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_NV12;
    f.fmt.pix_mp.num_planes = 1;
    if (xioctl(m_fd, VIDIOC_S_FMT, &f) < 0) {
        fprintf(stderr, "venus: формат входа: %s\n", strerror(errno));
        return false;
    }
    m_layout = nv12Layout(s.width, s.height);
    if (int(f.fmt.pix_mp.plane_fmt[0].bytesperline) != m_layout.stride
        || f.fmt.pix_mp.plane_fmt[0].sizeimage != m_layout.size) {
        fprintf(stderr, "venus: раскладка NV12 не та, что ждали: строка %u, буфер %u\n",
                f.fmt.pix_mp.plane_fmt[0].bytesperline, f.fmt.pix_mp.plane_fmt[0].sizeimage);
        return false;
    }

    // B-кадров нет (по умолчанию 0): декодеру не нужно переставлять кадры — нет задержки на перестановку.
    // Профиль Constrained Baseline: и декодер Mac (VideoToolbox в Chromium) не копит кадры «на всякий случай».
    bool ok = true;
    if (!s.hevc) {
        ok &= setControl(V4L2_CID_MPEG_VIDEO_H264_PROFILE, V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE,
                         "профиль");
        ok &= setControl(V4L2_CID_MPEG_VIDEO_H264_LEVEL, V4L2_MPEG_VIDEO_H264_LEVEL_5_1, "уровень");
    }
    ok &= setControl(V4L2_CID_MPEG_VIDEO_BITRATE_MODE, V4L2_MPEG_VIDEO_BITRATE_MODE_VBR, "режим битрейта");
    ok &= setControl(V4L2_CID_MPEG_VIDEO_BITRATE, s.bitrate, "битрейт");
    // Потолок квантования. У Qualcomm он один на H.264 и HEVC (номер — HEVC_MAX_QP) и упакован: QP опорного,
    // P- и B-кадров — по байту. Без него кодер на резкой смене всего кадра (окно появилось, анимация) уходил в QP
    // до 51 — кадр распадался на блоки; с ним кадр просто крупнее (замер 13 ноября 2026: docs/STREAM.md)
    if (s.maxQp > 0)
        ok &= setControl(V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP, s.maxQp | s.maxQp << 8 | s.maxQp << 16, "потолок QP");
    // опорный кадр — только по просьбе (новый зритель): связь надёжная (TCP), кадры не теряются
    ok &= setControl(V4L2_CID_MPEG_VIDEO_GOP_SIZE, 268435455, "группа кадров");
    setControl(kCidBitrateSavings, 0, "экономия битов");
    if (s.lowLatency)
        setControl(kCidLowLatency, 1, "малая задержка");
    for (auto [id, value] : s.extra)
        setControl(id, value, "добавочная");
    setControl(kCidOperatingRate, s.operatingRate == 0 ? INT32_MAX : (s.operatingRate < 0 ? s.fps : s.operatingRate) << 16,
               "скорость работы");
    return ok;
}

bool VenusEncoder::queueOutput(int index)
{
    v4l2_plane plane{};
    plane.m.userptr = static_cast<unsigned long>(m_outputs[index].fd);
    plane.length = uint32_t(m_outputs[index].size);
    plane.reserved[kReservedFd] = uint32_t(m_outputs[index].fd);
    plane.reserved[kReservedOffset] = 0;
    v4l2_buffer b{};
    b.type = kCap;
    b.memory = V4L2_MEMORY_USERPTR;
    b.index = uint32_t(index);
    b.m.planes = &plane;
    b.length = 1;
    if (xioctl(m_fd, VIDIOC_QBUF, &b) < 0) {
        fprintf(stderr, "venus: выходной буфер %d: %s\n", index, strerror(errno));
        return false;
    }
    return true;
}

bool VenusEncoder::start(const std::vector<Buffer> &inputs, const std::vector<Buffer> &outputs)
{
    m_inputs = inputs;
    m_outputs = outputs;
    for (auto [type, count] : {std::pair{kOut, inputs.size()}, std::pair{kCap, outputs.size()}}) {
        v4l2_requestbuffers r{};
        r.count = uint32_t(count);
        r.type = type;
        r.memory = V4L2_MEMORY_USERPTR;
        if (xioctl(m_fd, VIDIOC_REQBUFS, &r) < 0 || r.count < count) {
            fprintf(stderr, "venus: буферы (%s): %s, дали %u из %zu\n", type == kOut ? "вход" : "выход",
                    strerror(errno), r.count, count);
            return false;
        }
    }
    for (size_t i = 0; i < m_outputs.size(); ++i)
        if (!queueOutput(int(i)))
            return false;
    for (uint32_t type : {kOut, kCap}) {
        uint32_t t = type;
        if (xioctl(m_fd, VIDIOC_STREAMON, &t) < 0) {
            fprintf(stderr, "venus: запуск (%s): %s\n", type == kOut ? "вход" : "выход", strerror(errno));
            return false;
        }
    }
    m_streaming = true;
    return true;
}

bool VenusEncoder::encode(int input, int64_t timestampUs)
{
    v4l2_plane plane{};
    plane.m.userptr = static_cast<unsigned long>(m_inputs[input].fd);
    plane.length = uint32_t(m_inputs[input].size);
    plane.bytesused = uint32_t(m_layout.size);
    plane.reserved[kReservedFd] = uint32_t(m_inputs[input].fd);
    plane.reserved[kReservedOffset] = 0;
    v4l2_buffer b{};
    b.type = kOut;
    b.memory = V4L2_MEMORY_USERPTR;
    b.index = uint32_t(input);
    b.m.planes = &plane;
    b.length = 1;
    b.timestamp.tv_sec = timestampUs / 1000000;
    b.timestamp.tv_usec = timestampUs % 1000000;
    if (xioctl(m_fd, VIDIOC_QBUF, &b) < 0) {
        fprintf(stderr, "venus: кадр в кодер: %s\n", strerror(errno));
        return false;
    }
    return true;
}

bool VenusEncoder::setOperatingRate(int rate)
{
    return m_fd >= 0 && setControl(kCidOperatingRate, rate << 16, "скорость работы");
}

bool VenusEncoder::requestKeyFrame()
{
    return setControl(V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME, 1, "опорный кадр");
}

bool VenusEncoder::setBitrate(int bitsPerSecond)
{
    return setControl(V4L2_CID_MPEG_VIDEO_BITRATE, bitsPerSecond, "битрейт");
}

bool VenusEncoder::takePacket(Packet &out)
{
    v4l2_plane plane{};
    v4l2_buffer b{};
    b.type = kCap;
    b.memory = V4L2_MEMORY_USERPTR;
    b.m.planes = &plane;
    b.length = 1;
    if (xioctl(m_fd, VIDIOC_DQBUF, &b) < 0)
        return false;   // EAGAIN — ещё нет
    const Buffer &buf = m_outputs[b.index];
    const uint32_t offset = plane.data_offset, used = plane.bytesused;
    out.timestampUs = int64_t(b.timestamp.tv_sec) * 1000000 + b.timestamp.tv_usec;
    out.key = b.flags & V4L2_BUF_FLAG_KEYFRAME;
    if (buf.map && used && offset + used <= buf.size) {
        cpuRead(buf.fd, true);
        const auto *p = static_cast<const uint8_t *>(buf.map) + offset;
        out.data.assign(p, p + used);
        cpuRead(buf.fd, false);
    } else {
        out.data.clear();
    }
    queueOutput(int(b.index));
    return true;
}

int VenusEncoder::takeFreeInput()
{
    v4l2_plane plane{};
    v4l2_buffer b{};
    b.type = kOut;
    b.memory = V4L2_MEMORY_USERPTR;
    b.m.planes = &plane;
    b.length = 1;
    if (xioctl(m_fd, VIDIOC_DQBUF, &b) < 0)
        return -1;
    return int(b.index);
}

void VenusEncoder::close()
{
    if (m_fd < 0)
        return;
    if (m_streaming) {
        for (uint32_t type : {kOut, kCap}) {
            uint32_t t = type;
            xioctl(m_fd, VIDIOC_STREAMOFF, &t);
        }
        m_streaming = false;
    }
    ::close(m_fd);
    m_fd = -1;
}

VenusEncoder::Layout VenusEncoder::nv12Layout(int w, int h)
{
    Layout l;
    l.stride = alignUp(w, 512);
    l.yScan = alignUp(h, 512);
    l.uvScan = alignUp((h + 1) / 2, 256);
    l.size = size_t(l.stride) * (l.yScan + l.uvScan);
    return l;
}

bool VenusEncoder::hasKeyNal(const std::vector<uint8_t> &d, bool hevc)
{
    for (size_t i = 0; i + 3 < d.size(); ++i) {
        if (d[i] == 0 && d[i + 1] == 0 && d[i + 2] == 1) {
            const uint8_t h = d[i + 3];
            const int type = hevc ? (h >> 1) & 0x3f : h & 0x1f;
            if (hevc ? (type >= 16 && type <= 21) || type == 32 || type == 33 || type == 34
                     : type == 5 || type == 7 || type == 8)
                return true;
            i += 2;
        }
    }
    return false;
}
