// naomi-game — «приставка» рабочего стола (docs/GAMES.md): запускает одну игру в ядре эмулятора (libretro —
// ~/naomi/cores, device/cores.sh) и показывает её окном Wayland оболочки. Своя, с нуля; без Qt — libc, libwayland-client
// и libpulse. Ни меню, ни настроек эмулятора: окно — только картинка игры; полосу окна рисует стол (GameWindow.qml).
//
//   naomi-game <система> <ядро.so> <файл игры> <папка сохранений>
//
// Картинка — буфер wl_shm: кадр игры, увеличенный в целое число раз без сглаживания (до 1,3 млн пикселей; NES
// 256 × 240 и PS1 320 × 240 — в 4 раза), до размера окна его дотягивает стол — пиксели игры остаются чёткими. Размер окна стол сообщает настройкой
// xdg (нужна только высота — выбрать кратность).
// Темп — по расписанию, ровно раз в период ядра (NES — 60,1 раза в с), и в такт стола: он рисует кадр, когда игра его
// положила, но не чаще своего предела (60 раз в с), и сообщает, что показал (wl_surface.frame) — следующий кадр игры
// ждёт этого, но не дольше половины периода сверх расписания (см. цикл в main). Так каждый кадр игры попадает в свой
// кадр стола, а звук не теряет времени.
// Звук под темп подстраивается сам: в очереди PulseAudio (100 мс) держим половину — её мало, отсчётов делаем чуть
// больше, много — чуть меньше (до ±0,5 %, на слух не заметно): часы звуковой карты и телефона расходятся, а щелчков
// нет. Ждать места в очереди нельзя: кадры шли неровно — стол показывал 54–56 из 60 в секунду (замер 21 ноября 2026).
// Клавиши — wl_keyboard, по месту клавиши (коды evdev), раскладка не важна: стрелки — крестовина, C — Select,
// V — Start, R — кнопка Reset приставки; NES: X — A, Z — B; PS1 (система «ps1»): X — крест, Z — квадрат,
// S — треугольник, A — круг, Q и W — L1 и R1, E и D — L2 и R2.
// Геймпады телефона (Bluetooth, USB) — evdev напрямую, все сразу, вместе с клавиатурой; подключённый посреди игры
// подхватывается (inotify на /dev/input). Кнопки — по месту, как у геймпада libretro: нижняя — B, правая — A,
// левая — Y, верхняя — X (NES: A, B и турбо-A, турбо-B; PS1: крест, круг, квадрат, треугольник); крестовина (и
// левый стик у NES) — крестовина; у PS1 геймпад — DualShock: стики — стики, Home — кнопка ANALOG.
// Сигналы: SIGUSR1 — пауза (окно свернули), SIGUSR2 — дальше, SIGTERM — выйти. Сохранение игры (память картриджа с
// «батарейкой», карта памяти PS1) — в <папка сохранений>/<игра>.srm: раз в 10 с, если сменилось, на паузе и при выходе.
#include "libretro.h"
#include "xdg-shell-client-protocol.h"

#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sched.h>
#include <pulse/pulseaudio.h>
#include <signal.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>

namespace {

// ---------- ядро ----------
struct Core {
    void (*init)();
    void (*deinit)();
    unsigned (*api_version)();
    void (*get_system_info)(retro_system_info *);
    void (*get_system_av_info)(retro_system_av_info *);
    void (*set_environment)(retro_environment_t);
    void (*set_video_refresh)(retro_video_refresh_t);
    void (*set_audio_sample)(retro_audio_sample_t);
    void (*set_audio_sample_batch)(retro_audio_sample_batch_t);
    void (*set_input_poll)(retro_input_poll_t);
    void (*set_input_state)(retro_input_state_t);
    void (*set_controller_port_device)(unsigned, unsigned);
    bool (*load_game)(const retro_game_info *);
    void (*unload_game)();
    void (*run)();
    void (*reset)();
    void *(*get_memory_data)(unsigned);
    size_t (*get_memory_size)(unsigned);
} core;

[[noreturn]] void die(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fputs("naomi-game: ", stderr);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
    exit(1);
}

void loadCore(const char *path)
{
    void *lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!lib)
        die("ядро не загрузилось: %s", dlerror());
#define SYM(name) \
    if (!(core.name = reinterpret_cast<decltype(core.name)>(dlsym(lib, "retro_" #name)))) \
        die("в ядре нет retro_" #name)
    SYM(init); SYM(deinit); SYM(api_version); SYM(get_system_info); SYM(get_system_av_info); SYM(set_environment);
    SYM(set_video_refresh); SYM(set_audio_sample); SYM(set_audio_sample_batch); SYM(set_input_poll);
    SYM(set_input_state); SYM(set_controller_port_device); SYM(load_game); SYM(unload_game); SYM(run); SYM(reset);
    SYM(get_memory_data); SYM(get_memory_size);
#undef SYM
    if (core.api_version() != RETRO_API_VERSION)
        die("ядро другой версии libretro: %u", core.api_version());
}

// ---------- состояние ----------
std::string saveDir, savePath;
retro_pixel_format pixelFormat = RETRO_PIXEL_FORMAT_0RGB1555;

wl_display *display;
wl_compositor *compositor;
wl_shm *shm;
xdg_wm_base *wmBase;
wl_seat *seat;
wl_keyboard *keyboard;
wl_surface *surface;
xdg_surface *xdgSurface;
xdg_toplevel *toplevel;
uint32_t compositorVersion = 1;
bool configured = false;   // стол ответил на создание окна — можно класть кадры
int targetHeight = 0;      // высота картинки в окне, пиксели кадра стола (0 — ещё не сказали)
bool quit = false;
bool paused = false;
bool resetAsked = false;
bool playstation = false;     // система — PS1: у неё свои кнопки
int64_t framePeriod = 16666667;   // период кадра ядра, нс (ядро может сменить: игра другого региона)
wl_callback *frameAsked = nullptr;   // стол ещё не показал последний положенный кадр (ждём ответа wl_surface.frame)
uint16_t keyPad = 0;       // кнопки, нажатые клавишами, — биты RETRO_DEVICE_ID_JOYPAD_* (геймпады — gamepads)

struct Buffer {
    wl_buffer *wl = nullptr;
    uint32_t *pixels = nullptr;
    size_t bytes = 0;
    int width = 0, height = 0;
    bool busy = false;   // у стола: ждём wl_buffer.release
};
Buffer buffers[3];

pa_threaded_mainloop *audioLoop = nullptr;
pa_context *audioContext = nullptr;
pa_stream *audioStream = nullptr;   // nullptr — звука нет, игра идёт без него
size_t audioQueue = 0;              // очередь PulseAudio, байты
double audioAt = 0;                 // пересчёт: где в куске следующий отсчёт (−1 — последний отсчёт прошлого куска)
int16_t audioLast[2] = {0, 0};
std::vector<int16_t> audioOut;
std::vector<int16_t> singles;       // звук, что ядро отдаёт по одному отсчёту, — до конца кадра

int64_t now()
{
    timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return int64_t(t.tv_sec) * 1000000000 + t.tv_nsec;
}

// ---------- сохранение памяти картриджа ----------
std::vector<uint8_t> savedRam;   // что уже лежит на диске

void loadRam()
{
    const size_t size = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
    void *ram = core.get_memory_data(RETRO_MEMORY_SAVE_RAM);
    if (!size || !ram)
        return;
    if (FILE *f = fopen(savePath.c_str(), "rb")) {
        std::vector<uint8_t> data(size);
        if (fread(data.data(), 1, size, f) == size) {
            memcpy(ram, data.data(), size);
            savedRam = data;
        }
        fclose(f);
    }
    if (savedRam.empty())
        savedRam.assign(static_cast<uint8_t *>(ram), static_cast<uint8_t *>(ram) + size);   // новая — не писать зря
}

void saveRam()
{
    const size_t size = core.get_memory_size(RETRO_MEMORY_SAVE_RAM);
    const auto *ram = static_cast<const uint8_t *>(core.get_memory_data(RETRO_MEMORY_SAVE_RAM));
    if (!size || !ram || (savedRam.size() == size && !memcmp(savedRam.data(), ram, size)))
        return;
    const std::string tmp = savePath + ".tmp";
    FILE *f = fopen(tmp.c_str(), "wb");
    if (!f) {
        fprintf(stderr, "naomi-game: сохранение не записать (%s): %s\n", tmp.c_str(), strerror(errno));
        return;
    }
    const bool ok = fwrite(ram, 1, size, f) == size;
    if (fclose(f) == 0 && ok && rename(tmp.c_str(), savePath.c_str()) == 0)
        savedRam.assign(ram, ram + size);
}

// ---------- такт стола ----------
void frameDone(void *, wl_callback *callback, uint32_t)
{
    wl_callback_destroy(callback);
    frameAsked = nullptr;
}
const wl_callback_listener frameListener = {frameDone};

// ---------- картинка ----------
void bufferRelease(void *data, wl_buffer *)
{
    static_cast<Buffer *>(data)->busy = false;
}
const wl_buffer_listener bufferListener = {bufferRelease};

void dropBuffer(Buffer &b)
{
    if (b.wl)
        wl_buffer_destroy(b.wl);
    if (b.pixels)
        munmap(b.pixels, b.bytes);
    b = Buffer();
}

bool makeBuffer(Buffer &b, int width, int height)
{
    const size_t bytes = size_t(width) * height * 4;
    const int fd = memfd_create("naomi-game", MFD_CLOEXEC);
    if (fd < 0 || ftruncate(fd, off_t(bytes)) < 0) {
        if (fd >= 0)
            close(fd);
        return false;
    }
    void *map = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return false;
    }
    wl_shm_pool *pool = wl_shm_create_pool(shm, fd, int32_t(bytes));
    b.wl = wl_shm_pool_create_buffer(pool, 0, width, height, width * 4, WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    wl_buffer_add_listener(b.wl, &bufferListener, &b);
    b.pixels = static_cast<uint32_t *>(map);
    b.bytes = bytes;
    b.width = width;
    b.height = height;
    return true;
}

Buffer *freeBuffer(int width, int height)
{
    for (Buffer &b : buffers) {
        if (b.busy)
            continue;
        if (b.width != width || b.height != height) {
            dropBuffer(b);
            if (!makeBuffer(b, width, height))
                return nullptr;
        }
        return &b;
    }
    return nullptr;   // стол держит все три — этот кадр мимо
}

// во сколько раз увеличить кадр игры: до высоты окна и выше (стол уменьшит — пиксели останутся чёткими), но не больше
// 1,3 млн пикселей (NES 256 × 240 и PS1 320 × 240 — ×4, PS1 640 × 480 — ×2) и не больше 4
int upscale(int width, int height)
{
    int k = targetHeight > 0 ? (targetHeight + height - 1) / height : 3;
    k = k < 2 ? 2 : k > 4 ? 4 : k;
    while (k > 1 && int64_t(width) * k * height * k > 1300000)
        --k;
    return k;
}

void videoRefresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (!data || !configured || !width || !height)
        return;   // кадр не менялся
    // разведка: NAOMI_GAME_DUMP=файл — каждый 60-й кадр ядра как есть (заголовок: ширина, высота, шаг строки, формат)
    static const char *const dump = getenv("NAOMI_GAME_DUMP");
    static unsigned frames = 0;
    if (dump && ++frames % 60 == 0) {
        if (FILE *f = fopen(dump, "wb")) {
            const uint32_t head[4] = {width, height, uint32_t(pitch), uint32_t(pixelFormat)};
            fwrite(head, sizeof head, 1, f);
            fwrite(data, pitch, height, f);
            fclose(f);
        }
    }
    const int k = upscale(int(width), int(height));
    Buffer *b = freeBuffer(int(width) * k, int(height) * k);
    if (!b)
        return;
    const size_t rowBytes = size_t(b->width) * 4;
    for (unsigned y = 0; y < height; ++y) {
        const auto *line = static_cast<const uint8_t *>(data) + y * pitch;
        uint32_t *out = b->pixels + size_t(y) * k * b->width, *first = out;
        if (pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888) {
            const auto *in = reinterpret_cast<const uint32_t *>(line);
            for (unsigned x = 0; x < width; ++x)
                for (int i = 0; i < k; ++i)
                    *out++ = in[x] | 0xff000000;   // старший байт — «непрозрачно»: стол читает его и у XRGB
        } else {
            const auto *in = reinterpret_cast<const uint16_t *>(line);
            const bool rgb565 = pixelFormat == RETRO_PIXEL_FORMAT_RGB565;
            for (unsigned x = 0; x < width; ++x) {
                const uint32_t c = in[x];
                const uint32_t r = rgb565 ? c >> 11 & 31 : c >> 10 & 31, g = rgb565 ? c >> 5 & 63 : (c >> 5 & 31) << 1,
                               bl = c & 31;
                const uint32_t pixel = 0xff000000 | (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (bl << 3 | bl >> 2);
                for (int i = 0; i < k; ++i)
                    *out++ = pixel;
            }
        }
        for (int i = 1; i < k; ++i)
            memcpy(first + size_t(i) * b->width, first, rowBytes);
    }
    if (!frameAsked) {
        frameAsked = wl_surface_frame(surface);
        wl_callback_add_listener(frameAsked, &frameListener, nullptr);
    }
    wl_surface_attach(surface, b->wl, 0, 0);
    if (compositorVersion >= 4)
        wl_surface_damage_buffer(surface, 0, 0, b->width, b->height);
    else
        wl_surface_damage(surface, 0, 0, b->width, b->height);
    wl_surface_commit(surface);
    b->busy = true;
}

// ---------- звук ----------
void closeAudio()
{
    if (!audioLoop)
        return;
    pa_threaded_mainloop_stop(audioLoop);
    if (audioStream)
        pa_stream_unref(audioStream);
    if (audioContext) {
        pa_context_disconnect(audioContext);
        pa_context_unref(audioContext);
    }
    pa_threaded_mainloop_free(audioLoop);
    audioLoop = nullptr;
    audioContext = nullptr;
    audioStream = nullptr;
}

void audioChanged(pa_context *, void *)
{
    pa_threaded_mainloop_signal(audioLoop, 0);
}
std::atomic<int> audioDry{0};   // сколько раз очередь звука опустела (заикание) — в журнал раз в 10 с
void audioUnderflow(pa_stream *, void *)
{
    ++audioDry;
}
void audioStreamChanged(pa_stream *, void *)
{
    pa_threaded_mainloop_signal(audioLoop, 0);
}

void openAudio(double rate, const char *name)
{
    audioLoop = pa_threaded_mainloop_new();
    audioContext = pa_context_new(pa_threaded_mainloop_get_api(audioLoop), "Игры");
    pa_context_set_state_callback(audioContext, audioChanged, nullptr);
    pa_threaded_mainloop_lock(audioLoop);
    bool ok = pa_threaded_mainloop_start(audioLoop) == 0
              && pa_context_connect(audioContext, nullptr, PA_CONTEXT_NOFLAGS, nullptr) == 0;
    while (ok) {
        const pa_context_state_t state = pa_context_get_state(audioContext);
        if (state == PA_CONTEXT_READY)
            break;
        if (!PA_CONTEXT_IS_GOOD(state))
            ok = false;
        else
            pa_threaded_mainloop_wait(audioLoop);
    }
    if (ok) {
        pa_sample_spec spec;
        spec.format = PA_SAMPLE_S16LE;
        spec.rate = uint32_t(rate + 0.5);
        spec.channels = 2;
        // Очередь — 100 мс; играть PulseAudio начинает, когда в ней половина (prebuf), и так же после каждого
        // опустошения (игра стояла — грузила диск, стол замораживали): ждёт половину и играет дальше. С prebuf 0 он,
        // оставшись без звука, уходил вперёд, а наша запись ложилась в уже сыгранное — поток есть, а тишина: так
        // молчала PS1 после первой же загрузки (21 ноября 2026), Денди без остановок играла
        pa_buffer_attr attr;
        attr.maxlength = uint32_t(-1);
        attr.tlength = uint32_t(spec.rate * 4 * 100 / 1000);
        attr.prebuf = attr.tlength / 2;
        attr.minreq = uint32_t(-1);
        attr.fragsize = uint32_t(-1);
        audioStream = pa_stream_new(audioContext, name, &spec, nullptr);
        pa_stream_set_state_callback(audioStream, audioStreamChanged, nullptr);
        pa_stream_set_underflow_callback(audioStream, audioUnderflow, nullptr);
        ok = audioStream
             && pa_stream_connect_playback(audioStream, nullptr, &attr, PA_STREAM_NOFLAGS, nullptr, nullptr) == 0;
        while (ok) {
            const pa_stream_state_t state = pa_stream_get_state(audioStream);
            if (state == PA_STREAM_READY)
                break;
            if (!PA_STREAM_IS_GOOD(state))
                ok = false;
            else
                pa_threaded_mainloop_wait(audioLoop);
        }
        if (ok)
            audioQueue = pa_stream_get_buffer_attr(audioStream)->tlength;
    }
    pa_threaded_mainloop_unlock(audioLoop);
    if (!ok || !audioQueue) {
        fprintf(stderr, "naomi-game: звука нет (%s)\n", pa_strerror(pa_context_errno(audioContext)));
        closeAudio();
    }
}

// кусок звука ядра (frames пар отсчётов) — в очередь, растянув или сжав до ±0,5 % по тому, сколько в ней сейчас
void writeAudio(const int16_t *data, size_t frames)
{
    if (!audioStream || !frames)
        return;
    pa_threaded_mainloop_lock(audioLoop);
    size_t room = pa_stream_get_state(audioStream) == PA_STREAM_READY ? pa_stream_writable_size(audioStream) : 0;
    if (room == size_t(-1))
        room = 0;
    const double half = audioQueue / 2.0;
    double error = (double(audioQueue - (room < audioQueue ? room : audioQueue)) - half) / half;   // −1 пусто … +1 полно
    error = error < -1 ? -1 : error > 1 ? 1 : error;
    const double step = 1.0 + 0.005 * error;   // шаг по куску на отсчёт на выходе: полно — шагаем шире, выходит меньше
    audioOut.clear();
    const double end = double(frames) - 1;
    while (audioAt < end) {
        const long i = long(audioAt < 0 ? -1 : audioAt);   // левый из двух соседних отсчётов (−1 — из прошлого куска)
        const double f = audioAt - double(i);
        for (int c = 0; c < 2; ++c) {
            const double a = i < 0 ? audioLast[c] : data[i * 2 + c], b = data[(i + 1) * 2 + c];
            audioOut.push_back(int16_t(a + (b - a) * f));
        }
        audioAt += step;
    }
    audioAt -= double(frames);
    audioLast[0] = data[(frames - 1) * 2];
    audioLast[1] = data[(frames - 1) * 2 + 1];
    size_t bytes = audioOut.size() * 2;
    if (bytes > room)
        bytes = room & ~size_t(3);   // места нет — хвост мимо (не ждать)
    int failed = 0;
    if (bytes && pa_stream_write(audioStream, audioOut.data(), bytes, nullptr, 0, PA_SEEK_RELATIVE) < 0)
        failed = pa_context_errno(audioContext);
    pa_threaded_mainloop_unlock(audioLoop);
    // разведка (NAOMI_GAME_LOG): раз в ~10 с — сколько звука пришло от ядра и ушло в очередь
    static const bool trace = getenv("NAOMI_GAME_LOG") != nullptr;
    static size_t calls = 0, got = 0, sent = 0;
    static int peak = 0;
    got += frames;
    sent += bytes / 4;
    for (size_t i = 0; i < frames * 2; i += 16)
        peak = abs(data[i]) > peak ? abs(data[i]) : peak;
    if (trace && ++calls % 600 == 0) {
        fprintf(stderr, "naomi-game: звук: кусков %zu, от ядра %zu отсчётов, в очередь %zu, пик %d, место %zu из %zu%s%s\n",
                calls, got, sent, peak, room, audioQueue, failed ? ", ошибка: " : "", failed ? pa_strerror(failed) : "");
        got = sent = 0;
        peak = 0;
    }
}

void pauseAudio(bool on)   // пауза: очередь — прочь, поток стоит
{
    if (!audioStream)
        return;
    pa_threaded_mainloop_lock(audioLoop);
    if (pa_operation *o = pa_stream_cork(audioStream, on, nullptr, nullptr))
        pa_operation_unref(o);
    if (on)
        if (pa_operation *o = pa_stream_flush(audioStream, nullptr, nullptr))
            pa_operation_unref(o);
    pa_threaded_mainloop_unlock(audioLoop);
}

void audioSample(int16_t left, int16_t right)
{
    singles.push_back(left);
    singles.push_back(right);
}

size_t audioBatch(const int16_t *data, size_t frames)
{
    writeAudio(data, frames);
    return frames;
}

// ---------- геймпады ----------
// Геймпад — устройство evdev с кнопкой BTN_SOUTH (у Linux она же BTN_GAMEPAD) и крестовиной или стиком (scanGamepads).
// Читаем без захвата: браузер стола
// (Gamepad API) видит его дальше. Оси — к −32767…32767 по их пределам (EVIOCGABS).
enum { AxisX, AxisY, AxisRX, AxisRY, AxisHatX, AxisHatY, AxisCount };
const int axisCode[AxisCount] = {ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_HAT0X, ABS_HAT0Y};
struct Gamepad {
    int fd = -1;
    std::string path;
    uint16_t buttons = 0;          // биты RETRO_DEVICE_ID_JOYPAD_* от кнопок
    bool home = false;             // Home (BTN_MODE) нажата
    input_absinfo info[AxisCount] = {};
    int axis[AxisCount] = {};
};
std::vector<Gamepad> gamepads;
int inputWatch = -1;   // inotify на /dev/input: геймпад подключили — найти

// кнопка evdev → кнопка геймпада libretro по месту (−1 — не наша)
int retroButton(int code)
{
    switch (code) {
    case BTN_SOUTH: return RETRO_DEVICE_ID_JOYPAD_B;
    case BTN_EAST: return RETRO_DEVICE_ID_JOYPAD_A;
    case BTN_WEST: return RETRO_DEVICE_ID_JOYPAD_Y;
    case BTN_NORTH: return RETRO_DEVICE_ID_JOYPAD_X;
    case BTN_TL: return RETRO_DEVICE_ID_JOYPAD_L;
    case BTN_TR: return RETRO_DEVICE_ID_JOYPAD_R;
    case BTN_TL2: return RETRO_DEVICE_ID_JOYPAD_L2;
    case BTN_TR2: return RETRO_DEVICE_ID_JOYPAD_R2;
    case BTN_SELECT: return RETRO_DEVICE_ID_JOYPAD_SELECT;
    case BTN_START: return RETRO_DEVICE_ID_JOYPAD_START;
    case BTN_THUMBL: return RETRO_DEVICE_ID_JOYPAD_L3;
    case BTN_THUMBR: return RETRO_DEVICE_ID_JOYPAD_R3;
    case BTN_DPAD_UP: return RETRO_DEVICE_ID_JOYPAD_UP;
    case BTN_DPAD_DOWN: return RETRO_DEVICE_ID_JOYPAD_DOWN;
    case BTN_DPAD_LEFT: return RETRO_DEVICE_ID_JOYPAD_LEFT;
    case BTN_DPAD_RIGHT: return RETRO_DEVICE_ID_JOYPAD_RIGHT;
    default: return -1;
    }
}

int axisValue(const input_absinfo &a, int value)
{
    if (a.maximum <= a.minimum)
        return 0;
    const long v = (2L * value - a.minimum - a.maximum) * 32767 / (a.maximum - a.minimum);
    return v < -32767 ? -32767 : v > 32767 ? 32767 : int(v);
}

bool hasBit(const unsigned long *bits, int bit)
{
    return bits[bit / (8 * sizeof(long))] >> (bit % (8 * sizeof(long))) & 1;
}

// всё состояние геймпада заново (подключили; ядро потеряло события — SYN_DROPPED)
void syncGamepad(Gamepad &g)
{
    unsigned long keys[KEY_MAX / (8 * sizeof(long)) + 1] = {};
    g.buttons = 0;
    g.home = false;
    if (ioctl(g.fd, EVIOCGKEY(sizeof keys), keys) >= 0) {
        for (int code = BTN_SOUTH; code <= BTN_DPAD_RIGHT; ++code) {
            if (!hasBit(keys, code))
                continue;
            const int button = retroButton(code);
            if (button >= 0)
                g.buttons |= uint16_t(1u << button);
            g.home |= code == BTN_MODE;
        }
    }
    for (int i = 0; i < AxisCount; ++i) {
        if (ioctl(g.fd, EVIOCGABS(axisCode[i]), &g.info[i]) < 0)
            g.info[i] = input_absinfo();
        g.axis[i] = axisValue(g.info[i], g.info[i].value);
    }
}

// найти новые геймпады в /dev/input (при запуске и по inotify: только что созданное устройство открыть ещё нельзя —
// права ему udev ставит следом, это второе событие)
void scanGamepads()
{
    DIR *dir = opendir("/dev/input");
    if (!dir)
        return;
    while (dirent *e = readdir(dir)) {
        if (strncmp(e->d_name, "event", 5))
            continue;
        const std::string path = std::string("/dev/input/") + e->d_name;
        bool known = false;
        for (const Gamepad &g : gamepads)
            known |= g.path == path;
        if (known)
            continue;
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0)
            continue;
        // кнопки геймпада и крестовина или стик: у датчика отпечатка Pixel 5 («uinput-fpc», жесты) BTN_SOUTH тоже есть,
        // но крестовины и стиков нет — стрелки клавиатуры и ABS_Z
        unsigned long keys[KEY_MAX / (8 * sizeof(long)) + 1] = {}, axes[ABS_MAX / (8 * sizeof(long)) + 1] = {};
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof axes), axes);
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof keys), keys) < 0 || !hasBit(keys, BTN_SOUTH)
            || !(hasBit(keys, BTN_DPAD_UP) || hasBit(axes, ABS_X) || hasBit(axes, ABS_HAT0X))) {
            close(fd);
            continue;
        }
        Gamepad g;
        g.fd = fd;
        g.path = path;
        syncGamepad(g);
        char name[128] = "?";
        ioctl(fd, EVIOCGNAME(sizeof name), name);
        fprintf(stderr, "naomi-game: геймпад «%s» (%s)\n", name, path.c_str());
        gamepads.push_back(g);
    }
    closedir(dir);
}

// события геймпада; false — отключили
bool readGamepad(Gamepad &g)
{
    input_event events[64];
    bool dropped = false;
    for (;;) {
        const ssize_t n = read(g.fd, events, sizeof events);
        if (n < 0)
            return errno == EAGAIN || errno == EINTR;
        if (n == 0)
            return false;
        for (size_t i = 0; i < size_t(n) / sizeof(input_event); ++i) {
            const input_event &e = events[i];
            if (e.type == EV_KEY) {
                const int button = retroButton(e.code);
                if (button >= 0)
                    g.buttons = e.value ? uint16_t(g.buttons | 1u << button) : uint16_t(g.buttons & ~(1u << button));
                else if (e.code == BTN_MODE)
                    g.home = e.value != 0;
            } else if (e.type == EV_ABS) {
                for (int a = 0; a < AxisCount; ++a)
                    if (e.code == axisCode[a])
                        g.axis[a] = axisValue(g.info[a], e.value);
            } else if (e.type == EV_SYN && e.code == SYN_DROPPED) {
                dropped = true;
            }
        }
        if (dropped) {
            syncGamepad(g);
            dropped = false;
        }
    }
}

// кнопки всех геймпадов вместе: крестовина — и кнопками, и «шляпкой» (ABS_HAT0), у NES — и левым стиком (за половину
// хода); у PS1 Home — кнопка ANALOG DualShock: ядро переключает режим сочетанием L1 + R1 + Select (настройка
// pcsx_rearmed_analog_combo) — его и отдаём, пока Home нажата
uint16_t gamepadButtons()
{
    uint16_t bits = 0;
    for (const Gamepad &g : gamepads) {
        if (playstation && g.home)
            return 1u << RETRO_DEVICE_ID_JOYPAD_L | 1u << RETRO_DEVICE_ID_JOYPAD_R | 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
        bits |= g.buttons;
        int x = g.axis[AxisHatX], y = g.axis[AxisHatY];
        if (!playstation) {
            x = x ? x : g.axis[AxisX];
            y = y ? y : g.axis[AxisY];
        }
        if (x < -16384)
            bits |= 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
        if (x > 16384)
            bits |= 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
        if (y < -16384)
            bits |= 1u << RETRO_DEVICE_ID_JOYPAD_UP;
        if (y > 16384)
            bits |= 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
    }
    return bits;
}

// ---------- кнопки ----------
void inputPoll() {}

int16_t inputState(unsigned port, unsigned device, unsigned index, unsigned id)
{
    if (port != 0)
        return 0;
    device &= RETRO_DEVICE_MASK;
    if (device == RETRO_DEVICE_ANALOG) {   // стик: из геймпада, где он отклонён сильнее
        if (index > RETRO_DEVICE_INDEX_ANALOG_RIGHT || id > RETRO_DEVICE_ID_ANALOG_Y)
            return 0;
        const int a = index == RETRO_DEVICE_INDEX_ANALOG_LEFT ? AxisX + int(id) : AxisRX + int(id);
        int value = 0;
        for (const Gamepad &g : gamepads)
            value = abs(g.axis[a]) > abs(value) ? g.axis[a] : value;
        return int16_t(value);
    }
    if (device != RETRO_DEVICE_JOYPAD)
        return 0;
    const uint16_t all = gamepadButtons() | keyPad;
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
        return int16_t(all);
    return id < 16 ? all >> id & 1 : 0;
}

void keymap(void *, wl_keyboard *, uint32_t, int32_t fd, uint32_t)
{
    close(fd);   // раскладка не нужна: кнопки — по месту клавиши
}
void keyEnter(void *, wl_keyboard *, uint32_t, wl_surface *, wl_array *) {}
void keyLeave(void *, wl_keyboard *, uint32_t, wl_surface *)
{
    keyPad = 0;   // фокус ушёл — отпусканий не будет
}
// клавиша → кнопка геймпада libretro (−1 — нет такой). У PS1 в libretro: B — крест, A — круг, Y — квадрат,
// X — треугольник, L / R — L1 / R1
int buttonFor(uint32_t key)
{
    switch (key) {
    case KEY_UP: return RETRO_DEVICE_ID_JOYPAD_UP;
    case KEY_DOWN: return RETRO_DEVICE_ID_JOYPAD_DOWN;
    case KEY_LEFT: return RETRO_DEVICE_ID_JOYPAD_LEFT;
    case KEY_RIGHT: return RETRO_DEVICE_ID_JOYPAD_RIGHT;
    case KEY_C: return RETRO_DEVICE_ID_JOYPAD_SELECT;
    case KEY_V: return RETRO_DEVICE_ID_JOYPAD_START;
    default: break;
    }
    if (playstation) {
        switch (key) {
        case KEY_X: return RETRO_DEVICE_ID_JOYPAD_B;    // крест
        case KEY_Z: return RETRO_DEVICE_ID_JOYPAD_Y;    // квадрат
        case KEY_S: return RETRO_DEVICE_ID_JOYPAD_X;    // треугольник
        case KEY_A: return RETRO_DEVICE_ID_JOYPAD_A;    // круг
        case KEY_Q: return RETRO_DEVICE_ID_JOYPAD_L;    // L1
        case KEY_W: return RETRO_DEVICE_ID_JOYPAD_R;    // R1
        case KEY_E: return RETRO_DEVICE_ID_JOYPAD_L2;
        case KEY_D: return RETRO_DEVICE_ID_JOYPAD_R2;
        default: return -1;
        }
    }
    switch (key) {
    case KEY_X: return RETRO_DEVICE_ID_JOYPAD_A;
    case KEY_Z: return RETRO_DEVICE_ID_JOYPAD_B;
    default: return -1;
    }
}

void keyPress(void *, wl_keyboard *, uint32_t, uint32_t, uint32_t key, uint32_t state)
{
    const bool down = state == WL_KEYBOARD_KEY_STATE_PRESSED;
    if (key == KEY_R) {
        if (down)
            resetAsked = true;
        return;
    }
    const int button = buttonFor(key);
    if (button < 0)
        return;
    if (down)
        keyPad |= uint16_t(1u << button);
    else
        keyPad &= uint16_t(~(1u << button));
}
void keyModifiers(void *, wl_keyboard *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}
void keyRepeat(void *, wl_keyboard *, int32_t, int32_t) {}
const wl_keyboard_listener keyboardListener = {keymap, keyEnter, keyLeave, keyPress, keyModifiers, keyRepeat};

void seatCapabilities(void *, wl_seat *s, uint32_t capabilities)
{
    if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && !keyboard) {
        keyboard = wl_seat_get_keyboard(s);
        wl_keyboard_add_listener(keyboard, &keyboardListener, nullptr);
    }
}
void seatName(void *, wl_seat *, const char *) {}
const wl_seat_listener seatListener = {seatCapabilities, seatName};

// ---------- окно ----------
void wmPing(void *, xdg_wm_base *base, uint32_t serial)
{
    xdg_wm_base_pong(base, serial);
}
const xdg_wm_base_listener wmListener = {wmPing};

void surfaceConfigure(void *, xdg_surface *s, uint32_t serial)
{
    xdg_surface_ack_configure(s, serial);
    configured = true;
}
const xdg_surface_listener surfaceListener = {surfaceConfigure};

void toplevelConfigure(void *, xdg_toplevel *, int32_t, int32_t height, wl_array *)
{
    if (height > 0)
        targetHeight = height;
}
void toplevelClose(void *, xdg_toplevel *)
{
    quit = true;
}
void toplevelBounds(void *, xdg_toplevel *, int32_t, int32_t) {}
void toplevelCapabilities(void *, xdg_toplevel *, wl_array *) {}
const xdg_toplevel_listener toplevelListener = {toplevelConfigure, toplevelClose, toplevelBounds, toplevelCapabilities};

void registryGlobal(void *, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    if (!strcmp(interface, wl_compositor_interface.name)) {
        compositorVersion = version < 4 ? version : 4;
        compositor = static_cast<wl_compositor *>(wl_registry_bind(registry, name, &wl_compositor_interface,
                                                                   compositorVersion));
    } else if (!strcmp(interface, wl_shm_interface.name)) {
        shm = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
    } else if (!strcmp(interface, xdg_wm_base_interface.name)) {
        wmBase = static_cast<xdg_wm_base *>(wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
        xdg_wm_base_add_listener(wmBase, &wmListener, nullptr);
    } else if (!strcmp(interface, wl_seat_interface.name) && !seat) {
        seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, version < 4 ? version : 4));
        wl_seat_add_listener(seat, &seatListener, nullptr);
    }
}
void registryRemove(void *, wl_registry *, uint32_t) {}
const wl_registry_listener registryListener = {registryGlobal, registryRemove};

// ---------- ядру от «приставки» ----------
void coreLog(retro_log_level level, const char *format, ...)
{
    static const bool all = getenv("NAOMI_GAME_LOG") != nullptr;   // всё, что говорит ядро, — в журнал (разведка)
    if (level < RETRO_LOG_WARN && !all)
        return;
    va_list args;
    va_start(args, format);
    fputs("naomi-game: ядро: ", stderr);
    vfprintf(stderr, format, args);
    va_end(args);
}

bool environment(unsigned command, void *data)
{
    switch (command) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        pixelFormat = *static_cast<const retro_pixel_format *>(data);
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char **>(data) = saveDir.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback *>(data)->log = coreLog;
        return true;
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool *>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {   // настройки ядра — его собственные по умолчанию, кроме немногих наших
        // PS1 (PCSX-ReARMed): первая карта памяти — через нас (файл .srm игры), второй нет; кнопка ANALOG DualShock —
        // сочетание L1 + R1 + Select (его отдаёт Home геймпада; без настройки у ядра сочетания нет вовсе)
        static const char *const ours[][2] = {{"pcsx_rearmed_memcard1", "libretro"}, {"pcsx_rearmed_memcard2", "none"},
                                              {"pcsx_rearmed_analog_combo", "l1+r1+select"}};
        auto *variable = static_cast<retro_variable *>(data);
        variable->value = nullptr;
        // разведка: NAOMI_GAME_VARS="ключ=значение,ключ=значение" — поверх всего
        static const std::string probe = getenv("NAOMI_GAME_VARS") ? getenv("NAOMI_GAME_VARS") : "";
        static std::string probed;
        if (variable->key && !probe.empty()) {
            const std::string key = std::string(variable->key) + "=";
            size_t at = 0;
            while ((at = probe.find(key, at)) != std::string::npos && at != 0 && probe[at - 1] != ',')
                ++at;
            if (at != std::string::npos) {
                const size_t from = at + key.size(), to = probe.find(',', from);
                probed = probe.substr(from, to == std::string::npos ? to : to - from);
                variable->value = probed.c_str();
                return true;
            }
        }
        for (const auto &o : ours) {
            if (variable->key && !strcmp(variable->key, o[0])) {
                variable->value = o[1];
                return true;
            }
        }
        return false;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool *>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
        const double fps = static_cast<const retro_system_av_info *>(data)->timing.fps;
        if (fps > 1)
            framePeriod = int64_t(1e9 / fps);
        return true;
    }
    case RETRO_ENVIRONMENT_SET_MESSAGE:   // сообщение игроку (PS1: «ANALOG ON») — показать негде, в журнал
        fprintf(stderr, "naomi-game: ядро: %s\n", static_cast<const retro_message *>(data)->msg);
        return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
        return true;
    default:
        return false;
    }
}

// ---------- цикл ----------
int signalFd = -1;

// события стола и сигналы; ждать не дольше timeout нс (меньше 0 — сколько угодно)
void pump(int64_t timeout)
{
    while (wl_display_prepare_read(display) != 0)
        wl_display_dispatch_pending(display);
    if (wl_display_flush(display) < 0 && errno != EAGAIN) {
        wl_display_cancel_read(display);
        quit = true;
        return;
    }
    // стол, сигналы, «подключили устройство», геймпады
    std::vector<pollfd> fds = {{wl_display_get_fd(display), POLLIN, 0}, {signalFd, POLLIN, 0}, {inputWatch, POLLIN, 0}};
    for (const Gamepad &g : gamepads)
        fds.push_back({g.fd, POLLIN, 0});
    timespec wait = {time_t(timeout / 1000000000), long(timeout % 1000000000)};
    ppoll(fds.data(), fds.size(), timeout < 0 ? nullptr : &wait, nullptr);
    for (size_t i = gamepads.size(); i-- > 0;) {
        if (fds[3 + i].revents && !readGamepad(gamepads[i])) {
            fprintf(stderr, "naomi-game: геймпад отключён (%s)\n", gamepads[i].path.c_str());
            close(gamepads[i].fd);
            gamepads.erase(gamepads.begin() + long(i));
        }
    }
    if (fds[2].revents & POLLIN) {
        char events[4096];
        while (read(inputWatch, events, sizeof events) > 0) {}
        scanGamepads();
    }
    if (fds[0].revents & POLLIN) {
        if (wl_display_read_events(display) < 0)
            quit = true;
    } else {
        wl_display_cancel_read(display);
        if (fds[0].revents & (POLLHUP | POLLERR))
            quit = true;
    }
    if (wl_display_dispatch_pending(display) < 0)
        quit = true;
    if (fds[1].revents & POLLIN) {
        signalfd_siginfo info;
        while (read(signalFd, &info, sizeof info) == sizeof info) {
            if (info.ssi_signo == SIGUSR1) {
                if (!paused) {
                    paused = true;
                    saveRam();
                    pauseAudio(true);
                }
            } else if (info.ssi_signo == SIGUSR2) {
                if (paused)
                    pauseAudio(false);
                paused = false;
            } else {
                quit = true;
            }
        }
    }
}

// Поток игры — на большие ядра процессора (у Pixel 5 — два A76 из восьми): на малых отдельный кадр эмулятора
// временами считался 16–35 мс вместо ~6 (ядро занято другим, частота низкая) — стол показывал сдвоенные кадры (замер
// 21 ноября 2026). Большие — те, чья мощность (cpu_capacity) больше половины наибольшей; все равны или узнать нельзя —
// как есть.
void useBigCores()
{
    int capacity[64], count = 0, best = 0;
    for (; count < 64; ++count) {
        char path[64];
        snprintf(path, sizeof path, "/sys/devices/system/cpu/cpu%d/cpu_capacity", count);
        FILE *f = fopen(path, "r");
        if (!f)
            break;
        if (fscanf(f, "%d", &capacity[count]) != 1)
            capacity[count] = 0;
        fclose(f);
        best = capacity[count] > best ? capacity[count] : best;
    }
    cpu_set_t big;
    CPU_ZERO(&big);
    int bigCount = 0;
    for (int i = 0; i < count; ++i) {
        if (capacity[i] * 2 > best) {
            CPU_SET(i, &big);
            ++bigCount;
        }
    }
    if (bigCount && bigCount < count && sched_setaffinity(0, sizeof big, &big) != 0)
        fprintf(stderr, "naomi-game: на большие ядра не перейти: %s\n", strerror(errno));
}

std::string baseName(const std::string &path)
{
    const size_t slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos || dot == 0 ? name : name.substr(0, dot);
}

}   // namespace

int main(int argc, char **argv)
{
    if (argc != 5)
        die("naomi-game <система> <ядро.so> <файл игры> <папка сохранений>");
    const std::string system = argv[1], game = argv[3];
    playstation = system == "ps1";
    saveDir = argv[4];
    savePath = saveDir + "/" + baseName(game) + ".srm";

    // сигналы — чтением из дескриптора в общем ожидании; закрыть раньше всех потоков (их заведёт PulseAudio)
    sigset_t signals;
    sigemptyset(&signals);
    for (int s : {SIGUSR1, SIGUSR2, SIGTERM, SIGINT})
        sigaddset(&signals, s);
    sigprocmask(SIG_BLOCK, &signals, nullptr);
    signalFd = signalfd(-1, &signals, SFD_NONBLOCK | SFD_CLOEXEC);

    loadCore(argv[2]);
    core.set_environment(environment);
    core.init();   // раньше остальных: Mesen в них уже обращается к тому, что создаёт в init
    core.set_video_refresh(videoRefresh);
    core.set_audio_sample(audioSample);
    core.set_audio_sample_batch(audioBatch);
    core.set_input_poll(inputPoll);
    core.set_input_state(inputState);

    retro_system_info info = {};
    core.get_system_info(&info);
    retro_game_info rom = {};
    rom.path = game.c_str();
    std::vector<uint8_t> image;
    if (!info.need_fullpath) {
        FILE *f = fopen(game.c_str(), "rb");
        if (!f)
            die("игру не открыть: %s", strerror(errno));
        uint8_t chunk[65536];
        for (size_t n; (n = fread(chunk, 1, sizeof chunk, f)) > 0;)
            image.insert(image.end(), chunk, chunk + n);
        fclose(f);
        rom.data = image.data();
        rom.size = image.size();
    }
    if (!core.load_game(&rom))
        die("ядро %s не приняло игру %s", info.library_name, game.c_str());
    // PS1 — DualShock: с цифровыми играми он цифровой, игры под DualShock ядро само переводит в режим ANALOG (стики)
    core.set_controller_port_device(0, playstation ? RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_ANALOG, 1) : RETRO_DEVICE_JOYPAD);
    retro_system_av_info av = {};
    core.get_system_av_info(&av);
    fprintf(stderr, "naomi-game: %s — %s %s, кадр %u × %u, %.3f кадра в с, звук %.0f Гц\n", system.c_str(),
            info.library_name, info.library_version, av.geometry.base_width, av.geometry.base_height, av.timing.fps,
            av.timing.sample_rate);
    loadRam();

    display = wl_display_connect(nullptr);
    if (!display)
        die("стола нет (WAYLAND_DISPLAY)");
    wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registryListener, nullptr);
    wl_display_roundtrip(display);
    if (!compositor || !shm || !wmBase)
        die("у стола нет wl_compositor, wl_shm или xdg_wm_base");
    surface = wl_compositor_create_surface(compositor);
    xdgSurface = xdg_wm_base_get_xdg_surface(wmBase, surface);
    xdg_surface_add_listener(xdgSurface, &surfaceListener, nullptr);
    toplevel = xdg_surface_get_toplevel(xdgSurface);
    xdg_toplevel_add_listener(toplevel, &toplevelListener, nullptr);
    xdg_toplevel_set_title(toplevel, baseName(game).c_str());
    xdg_toplevel_set_app_id(toplevel, ("naomi-game." + system).c_str());
    wl_surface_commit(surface);

    openAudio(av.timing.sample_rate, baseName(game).c_str());
    useBigCores();   // после звука: его поток — где был

    inputWatch = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inputWatch >= 0 && inotify_add_watch(inputWatch, "/dev/input", IN_CREATE | IN_ATTRIB) < 0) {
        close(inputWatch);
        inputWatch = -1;
    }
    scanGamepads();

    if (av.timing.fps > 1)
        framePeriod = int64_t(1e9 / av.timing.fps);
    int64_t saved = now();  // когда сверяли память картриджа с диском
    int64_t next = saved;   // когда следующий кадр по расписанию: ровно раз в период ядра
    int forced = 0;         // сколько кадров подряд сделано, не дождавшись показа прошлого
    int64_t freeUntil = 0;  // до этого времени показа кадров не ждём
    while (!quit) {
        if (paused || !configured) {
            pump(-1);
            next = now();
            continue;
        }
        const int64_t t = now();
        const int64_t period = framePeriod;
        if (t - saved > 10000000000) {   // раз в 10 с: сохранение игры (карта памяти PS1) — на диск, если сменилось
            saved = t;
            saveRam();
            if (const int dry = audioDry.exchange(0))
                fprintf(stderr, "naomi-game: звук: очередь пустела %d раз за 10 с\n", dry);
        }
        if (t < next) {
            pump(next - t);
            continue;
        }
        // Расписание — главное: по нему идёт звук, и время, потерянное на ожидании, подстройка звука (±0,5 %) не
        // вернёт — очередь пустеет, звук заикается (так было у PS1, пока кадр ждал стола сколько придётся: 21 ноября
        // 2026). Поэтому показа прошлого кадра ждём не дольше половины периода сверх расписания; не дождались — кадр
        // делается всё равно (стол покажет его вместе с прошлым). Три таких подряд — стол устойчиво медленнее игры
        // (60 против 60,1 у NES; экран телефона, который кадров не рисует): расписание — заново от этого мига, и 5 с
        // стола не ждём вовсе; полпериода звука раз в несколько секунд подстройка покрывает. Отстали больше трёх
        // периодов (грузился диск, стол замораживали) — тоже заново.
        const bool wait = frameAsked && t >= freeUntil;
        if (wait && t - next < period / 2) {
            pump(next + period / 2 - t);
            continue;
        }
        forced = wait ? forced + 1 : 0;
        if (forced >= 3) {
            freeUntil = t + 5000000000;
            next = t;
            forced = 0;
        } else if (t - next > 3 * period) {
            next = t;
        }
        next += period;
        pump(0);
        if (resetAsked) {
            resetAsked = false;
            core.reset();
        }
        core.run();
        if (!singles.empty()) {
            writeAudio(singles.data(), singles.size() / 2);
            singles.clear();
        }
    }

    saveRam();
    core.unload_game();
    core.deinit();
    closeAudio();
    for (Buffer &b : buffers)
        dropBuffer(b);
    wl_display_disconnect(display);
    return 0;
}
