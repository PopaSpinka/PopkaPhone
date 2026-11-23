// НаомиФон, приставка на Pi — экран через KMS (atomic), без рабочего стола Linux: кадр декодера (DRM PRIME, NV12
// «колонками» SAND) — прямо основным слоем дисплея, растянут на монитор в своих пропорциях; курсор — своим слоем
// поверх. Кадр показывается в обновление монитора, каждый в своё; пришли пачкой после провала связи — догоняет, но
// не больше LAG позади (как окно Mac, docs/STREAM.md, «Плавность по Wi-Fi»). Ещё два своих слоя — для экрана без
// дока (lock.h): покров (обои поверх кадра) и часы; курсор — выше всех. Монитор можно усыпить (сигнала нет) и сменить
// ему частоту на ходу (разрешение — всегда родное).
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <xf86drmMode.h>

typedef struct _drmModeAtomicReq drmModeAtomicReq;

struct AVFrame;

struct FrameInfo {
    int64_t capture = 0;   // часы телефона, мкс
    int64_t encoded = 0;   // часы телефона
    int64_t recv = 0;      // часы Pi (CLOCK_MONOTONIC)
    int64_t decoded = 0;   // часы Pi
    int64_t target = 0;    // обновление монитора, к которому кадр заказан тактом (0 — не по такту)
    bool key = false;      // опорный (тяжёлый, идёт дольше — в замер пути кадра не берётся)
    uint8_t away = 0;      // метка Away (экран без дока): фаза + 1, 0 — нет
    int bytes = 0;         // сжатый кадр, байт (трасса: большие первые кадры анимаций идут дольше на каждом участке)
};

struct Rect { int x = 0, y = 0, w = 0, h = 0; };

class Display
{
public:
    ~Display() { close(); }
    bool open(double hz);   // монитор в родном разрешении, частота — ближайшая к hz (0 — самая высокая)
    void close();
    int fd() const { return m_fd; }
    int timerFd() const { return m_timer; }              // готов — пора отдать экрану свежее (onTimer)
    int width() const { return m_w; }
    int height() const { return m_h; }
    double refreshHz() const { return m_hz; }
    std::vector<double> rates() const;                   // частоты монитора в родном разрешении
    bool setRate(double hz);                             // другая из них на ходу; false — не сейчас, повторить
    std::string name() const { return m_name; }          // выход: HDMI-A-1
    std::string monitor() const { return m_monitor; }    // имя монитора из EDID (пусто — не назвался)
    Rect video() const { return m_video; }   // где кадр на экране (последний показанный размер)

    void show(AVFrame *frame, const FrameInfo &info);   // кадр — дисплею (сам освободит)
    void newStream();                                    // новый поток (Hello): буферы прошлого декодера — прочь, как сменится кадр
    void cursorChanged();                                // курсор сдвинулся: положение спросить у cursorAt
    // картинка курсора (64 × 64 ARGB, умноженная альфа, cursor.h) и её горячая точка — с ближайшей отдачи экрану
    void setCursor(const uint32_t *argb, int hotX, int hotY);
    // где курсор к обновлению монитора vblankUs (досчёт по скорости мыши, motion.h); пока cursorMoving — курсор
    // отдаётся экрану каждое обновление, даже без новых сдвигов
    std::function<void(int64_t vblankUs, int &x, int &y)> cursorAt;
    std::function<bool()> cursorMoving;
    int64_t nextVblank(int64_t after);                   // ближайшее обновление монитора позже after
    int64_t periodUs() const { return m_periodUs; }
    int64_t marginUs() const { return m_marginUs; }
    void handleEvents();                                 // fd() готов: кадр встал на экран
    void onTimer();

    // Свои слои поверх кадра (экран без дока, lock.h): их вид к обновлению монитора vblankUs спрашивается у layersAt в
    // каждой отдаче экрану, там же — прозрачность курсора; пока layersBusy — отдавать экрану каждое обновление, даже без
    // кадров и мыши. fb 0 или alpha 0 — слоя нет.
    struct Layer { uint32_t fb = 0; int sx = 0, sy = 0, sw = 0, sh = 0; Rect to; double alpha = 1; bool yuv = false; };
    enum { Cover = 0, Clock = 1, kLayers = 2 };
    std::function<void(int64_t vblankUs, Layer *layers, double &cursorAlpha)> layersAt;
    std::function<bool()> layersBusy;
    void layersChanged();
    uint32_t dumbFb(int w, int h, uint32_t format, uint32_t **pixels, uint32_t *pitch);
    uint32_t blackFb() const { return m_blackFb; }   // чёрный 320×180 (растягивается)
    // буфер слоя для кадра декодера вне потока (обои — свой декодер, lock.h); убрать — dropFb с теми же хэндлами
    uint32_t frameFb(AVFrame *f, uint32_t handles[4]);
    void dropFb(uint32_t fb, const uint32_t handles[4]);
    Rect fit(int sw, int sh) const;   // кадр sw×sh на экране: в своих пропорциях, по центру
    // Сон монитора: сигнала HDMI нет (CRTC ACTIVE=0) — монитор засыпает сам. false — не вышло сейчас (прошлое ещё не
    // встало на экран): повторить позже
    bool setAsleep(bool asleep);
    bool asleep() const { return m_asleep; }
    std::function<void(const FrameInfo &, int64_t shownUs)> onShown;
    std::function<void(const FrameInfo &)> onDropped;    // выброшен при догоне, на экран не попал (метку Away
                                                         // забирает следующий)
    std::function<void(const FrameInfo &, int64_t vblankUs)> onCommit;   // кадр отдан экрану: встанет в vblankUs
    int dropped = 0;                                     // выброшено при догоне (счёт — у вызывающего)
    // счёт обновлений экрана (у вызывающего — за секунду): отдано кадром, отдано только курсором, встало на экран,
    // «занят» (прошлое ещё не встало), сумма времени от отдачи до экрана
    int commitsFrame = 0, commitsCursor = 0, flips = 0, busy = 0, late = 0;   // late — отдали, а встало позже
    int framesLate = 0;   // кадр встал позже своего обновления (target): опоздал от телефона
    int timerLate = 0;    // будильник отдачи сработал позже чем на 1 мс (главный цикл был занят)
    int64_t timerLateMaxUs = 0;
    int64_t flipWaitUs = 0;

private:
    struct Fb { uint32_t id = 0; int gen = 0; uint32_t handles[4] = {}; };
    struct Pending { AVFrame *frame; FrameInfo info; int gen; };
    uint32_t prop(uint32_t obj, uint32_t type, const char *name, uint64_t *enumValue = nullptr,
                  const char *enumName = nullptr);
    uint32_t fbFor(AVFrame *f);
    uint32_t addFb(AVFrame *f, uint32_t handles[4]);
    void addLayers(drmModeAtomicReq *req);
    void dropStale();
    void dropFront();
    void schedule();
    bool pickReady();   // в это обновление есть кадр: лишние старшие — прочь
    void commitNext();
    void commitCursor();
    void onFlip(int64_t us);
    static void flipHandler(int, unsigned, unsigned, unsigned, unsigned, void *);

    int m_fd = -1;
    uint32_t m_conn = 0, m_crtc = 0, m_videoPlane = 0, m_cursorPlane = 0, m_modeBlob = 0;
    uint32_t m_layerPlane[kLayers] = {};
    bool m_layerOn[kLayers] = {}, m_layersDirty = false, m_asleep = false;
    uint32_t m_crtcActive = 0;
    size_t pickMode(double hz) const;
    int m_w = 0, m_h = 0;
    double m_hz = 60;
    std::vector<drmModeModeInfo> m_modes;   // режимы родного разрешения
    std::string m_name, m_monitor;
    uint32_t m_blackFb = 0;
    // курсор: три буфера — рисовать в тот, что не на экране и не отдан (cursor.h); горячая точка — у мыши
    static constexpr int kCursor = 64;
    uint32_t m_cursorFbs[3] = {}, *m_cursorPx[3] = {}, m_cursorPitch = 0;
    int m_cursorFront = 0, m_cursorShown = 0, m_cursorCommitted = 0, m_cursorNext = -1;
    int m_hotX = 0, m_hotY = 0, m_nextHotX = 0, m_nextHotY = 0;
    int m_cx = 0, m_cy = 0, m_backlog = 0;
    void placeCursor(drmModeAtomicReq *req);
    bool m_cursorDirty = false, m_inFlight = false;
    int64_t m_committedUs = 0;
    // Отдавать экрану — не сразу, а за m_marginUs до обновления монитора: тогда встаёт самое свежее (кадр и курсор),
    // а не то, что было в момент прошлого обновления (иначе кадр, пришедший после, ждал лишнее обновление целиком)
    // Запас — 2 мс (NAOMI_DOCK_MARGIN_US): было 4. Отдача доходит до экрана и за 1 мс (от отдачи до экрана 0,6 мс,
    // опозданий не прибавилось), а каждая мс запаса — мс возраста кадра и курсора: 4 → 2 мс — от кадра до экрана
    // 34,6–35,2 → 31,3–33,6 мс, 95 % — 44 → 35–40 (кадр на грани успевает к своему обновлению); замер 19 ноября 2026,
    // tools/deskbench, по 2–3 прогона (docs/PI.md, «Срок отдачи и кабель»)
    int m_timer = -1;
    bool m_armed = false;
    int64_t m_lastVblankUs = 0, m_periodUs = 13333, m_marginUs = 2000, m_expectedUs = 0;
    Rect m_video;
    struct PlaneProps { uint32_t fb, crtc, srcX, srcY, srcW, srcH, crtcX, crtcY, crtcW, crtcH, encoding, range, alpha, zpos; };
    PlaneProps m_vp{}, m_cp{}, m_lp[kLayers]{};
    uint64_t m_bt709 = 0, m_limited = 0;
    std::map<uint32_t, Fb> m_fbs;   // по GEM-хэндлу первого буфера кадра: буферы декодера ходят по кругу
    int m_gen = 0;                  // поток (декодер): буферы прошлого — удалить, когда его кадр сойдёт с экрана
    AVFrame *m_screen = nullptr;    // на экране
    int m_screenGen = 0;
    Pending m_flipping{nullptr, {}, 0};   // отдан дисплею, встанет в следующее обновление
    std::deque<Pending> m_pending;
};
