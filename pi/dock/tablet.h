// НаомиФон, приставка на Pi — планшет — экран стола (docs/TABLET.md, docs/SCREENS.md): приложение на планшете Android
// подключается к приставке по домашней сети, приставка отдаёт ему те же кадры стола, что получает от телефона (без
// нового сжатия), и свой курсор; касания планшета приставка превращает в мышь стола. Клавиатура, мышь и трекпад Pi
// работают как были. Планшет — свой экран в «Экранах» телефона (id «tablet», несёт его приставка): стол на него не
// выведен — у него заставка без дока (обои и часы, как у монитора приставки), двойное касание — «стол сюда».
// Телевизор (docs/TV.md) — то же приложение и тот же протокол: экран «tv» на своём порту (kTvPort) и «дежурный»
// (kTvAgentPort) — служба приложения на ТВ, что на связи всегда, пока телевизор не спит: ей приставка говорит Launch —
// «включи экран и открой приложение» (и поверх HDMI-входа). Каждый порт — свой Tablet: подключение на нём одно.
//
// Связь — TLS 1.3 (OpenSSL Pi, ssl_min.h) на порту kPort, сертификат Pi и ключ планшета — /var/lib/naomi-dock/tablet
// (cert.pem, key.pem, token — их делает tools/tablet pair; ключ телевизора — /var/lib/naomi-dock/tv/token, tools/tv
// pair, сертификат — тот же): планшет сверяет отпечаток сертификата, а первым словом говорит ключ (Auth) — без него
// за 3 с подключение закрыто. Планшет один: новый, сказавший ключ, сменяет прошлого.
// Сообщение — как у телефона (stream_protocol.h): u32 длина (тип + тело), u8 тип, тело; числа — little-endian,
// координаты — доли кадра (0…1).
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tablet {

constexpr int kPort = 8490;          // планшет
constexpr int kTvPort = 8491;        // телевизор: экран (приложение на экране ТВ)
constexpr int kTvAgentPort = 8492;   // телевизор: дежурный (служба приложения — будит экран и открывает приложение)

enum Type : uint8_t {
    // приставка → планшет
    Hello = 1,        // JSON: desk (1 — стол идёт, 0 — нет: на планшет не выведен, выключен, телефон пропал — заставка),
                      // width, height (кадр), codec
    Video = 2,        // u64 захват (мкс, часы телефона), i64 он же в часах приставки (0 — часы телефона ещё не сверены),
                      // u8 флаги (1 — опорный), HEVC Annex B — кадр, как пришёл от телефона
    Pong = 3,         // u64 время планшета из Ping, i64 часы приставки (мкс, CLOCK_MONOTONIC) — планшет сверяет с ними
                      // свои и считает путь кадра от телефона до своего экрана
    Cursor = 4,       // u8 виден, f32 x, f32 y — курсор приставки (мышь, трекпад); касание планшета его прячет
    CursorImage = 5,  // u16 сторона n, u16 hotX, u16 hotY, u32[n·n] ARGB (без умножения альфы) — форма курсора
                      // («Мягкая», cursor.h) под размер картинки на планшете (View)
    Splash = 6,       // JSON — заставка без дока: clock (1 — часы есть), clockX, clockY (левый верх часов, 1/10000 ширины
                      // и высоты экрана), width (ширина стола, точки: размер часов — 150 точек стола) — как Screen у
                      // телефона; новому планшету — сразу, потом — когда сменились
    Wallpaper = 7,    // u8[16] хэш, JPEG обоев стола (пусто — обоев нет: градиент) — как у телефона (stream::Wallpaper);
                      // когда хэш планшета (Screen) не тот и когда обои сменили
    Light = 8,        // u8 авто (1 — яркость экрана планшет ведёт сам, по своему датчику света), u8 уровень, 1…100 % —
                      // яркость экрана со стола («Экраны»); новому планшету — сразу, потом — когда сменили
    Leave = 9,        // стол с этого экрана сняли («нет» в «Экранах», стол выключили или он переехал): планшет и так
                      // на заставке (Hello desk 0), приложение на телевизоре — уходит с экрана (что было до него —
                      // HDMI-вход, другое приложение — возвращается; будили телевизор мы — он засыпает снова)
    Launch = 20,      // дежурному ТВ: включи экран и открой приложение (в «Экранах» телевизор поставили на «стол»)
    // планшет → приставка
    Auth = 10,        // u8[32] ключ (token) — первое слово; дальше — имя планшета (UTF-8)
    Ping = 11,        // u64 время планшета, мкс — раз в 0,5 с; молчит 3 с — подключение закрыто
    Pointer = 12,     // u8 кнопки (1 — левая, 2 — правая), f32 x, f32 y, u64 время касания (мкс, часы планшета)
    Scroll = 13,      // f32 x, f32 y (где пальцы), f32 dx, f32 dy (ход пальцев с прошлого, доли кадра; вниз и вправо — плюс),
                      // u8 пальцы на экране (0 — сняли: дальше инерция), u64 время (мкс, часы планшета)
    KeyFrame = 14,    // декодер планшета сбился — нужен опорный кадр
    View = 15,        // u16 ширина, u16 высота — картинка стола на экране планшета, пикселей (размер курсора)
    Wake = 16,        // двойное касание заставки — «стол сюда» (приставка — телефону: Wake «tablet»)
    Screen = 17,      // u16 ширина, u16 высота экрана планшета, пикселей; u8[16] хэш обоев заставки, что у него есть
                      // (нули — нет) — сразу после Auth
};

} // namespace tablet

struct ssl_ctx_st;

class Tablet
{
public:
    ~Tablet() { stop(); }
    // слушать порт; ключ (token) — в dir, сертификат — в certDir (пусто — там же); нет их — порт слушается,
    // подключения закрываются, пока сопряжение (pair — какой командой, для журнала) их не сделает; label — кто это, для
    // журнала («планшет», «ТВ»)
    bool start(const std::string &dir, int port, const std::string &label = "планшет",
               const std::string &certDir = std::string(), const std::string &pair = "tools/tablet pair");
    void stop();

    // что пришло от планшета: сообщения Pointer, Scroll, View, Wake, Screen; тип 0 — планшет подключился (тело — его
    // имя), 255 — ушёл
    struct Event { uint8_t type; std::vector<uint8_t> body; int64_t recv; };
    int fd() const { return m_events; }   // eventfd: есть события
    std::vector<Event> take();
    bool connected() const { return m_connected; }

    // главному кругу приставки: всё без ожидания, без планшета — ничего
    void hello(const std::string &json);                              // помнится: новому планшету — сразу
    void splash(const std::string &json);                             // заставка (Splash) — помнится так же
    void light(bool automatic, int level);                            // яркость экрана (Light) — помнится так же
    void send(uint8_t type, const uint8_t *body, size_t n);           // служебное как есть (Wallpaper)
    // тело Video телефона (захват, сжат, флаги, данные); offset — часы телефона − часы Pi (сверены — known)
    void video(const uint8_t *body, size_t n, bool known, int64_t offset);
    void cursor(bool shown, float x, float y);                        // только последнее
    void cursorImage(const std::vector<uint32_t> &argb, int n, int hotX, int hotY);   // только последняя
    bool takeKeyWanted() { return m_keyWanted.exchange(false); }      // планшету нужен опорный кадр

    // цифры за секунду (главный круг пишет их в журнал)
    struct Stats { int frames = 0, dropped = 0; size_t bytes = 0; };
    Stats takeStats();

private:
    void run();
    void handshake(int fd);   // своим потоком: TLS и ключ, с ожиданием не дольше 3 с
    bool loadKeys();
    void post(uint8_t type, const uint8_t *body, size_t n);

    std::string m_dir, m_certDir, m_label, m_pair;
    int m_listen = -1, m_kick = -1, m_events = -1;
    std::thread m_thread;
    std::atomic<bool> m_quit{false}, m_connected{false}, m_keyWanted{false};
    std::atomic<int> m_handshakes{0};

    std::mutex m_mutex;   // всё ниже
    ssl_ctx_st *m_ctx = nullptr;
    long long m_certTime = 0;
    struct Ready { int fd; void *ssl; std::string name; };
    std::vector<Ready> m_ready;                   // прошли TLS и ключ — ждут потока планшета
    std::deque<std::vector<uint8_t>> m_ctrl;      // Hello, Pong, форма курсора — вперёд кадров
    std::deque<std::vector<uint8_t>> m_frames;    // Video
    size_t m_frameBytes = 0;
    bool m_waitKey = true;                        // кадры — только с опорного
    std::vector<uint8_t> m_hello, m_splash, m_light, m_image, m_cursorMsg;
    bool m_cursorDirty = false, m_imageDirty = false;
    std::deque<Event> m_in;
    Stats m_stats;
};
