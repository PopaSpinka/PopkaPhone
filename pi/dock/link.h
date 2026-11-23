// НаомиФон, приставка на Pi — связь с телефоном (docs/PI.md, «Приёмник»): `ssh -W 127.0.0.1:8478` своим ключом Pi
// (~/.ssh/naomi_stream: на телефоне им можно только открыть поток трансляции), протокол — stream_protocol.h.
// Картинка — напрямую (docs/STREAM.md, «Картинка напрямую»): по SSH просим DirectAsk, телефон отвечает адресом и
// одноразовым пропуском (Direct) — openDirect подключается по TCP и говорит его; дальше кадры идут по прямому, а
// ввод, такты и пинги для сверки — по SSH. Прямой оборвался — кадры снова по SSH, и сразу просим заново.
// Путей у приёмника два — кабель и Wi-Fi, каждый — свой Link (main.cpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <sys/types.h>
#include <vector>

class Link
{
public:
    ~Link() { close(); }
    // Запустить ssh к host и сказать Join: билет сеанса (у приёмника один на все пути: кабель и Wi-Fi — один
    // зритель), use — этот путь сразу основной. Настройки (Viewer, Want, DeskSize, Mode) шлёт main — ввод с номером,
    // по всем путям сразу (docs/PI.md, «Кабель и Wi-Fi без шва»). quiet — тихий путь (трансляция выключена на
    // телефоне, Switch 0): SSH проверяет связь раз в kParkedPingS, а не раз в 2 с — Wi-Fi телефона не будится зря.
    bool open(const std::string &host, const std::string &keyDir, uint64_t ticket, bool use, bool quiet = false);
    void close();
    bool isOpen() const { return m_out >= 0; }
    bool quiet() const { return m_quiet; }
    int fd() const { return m_out; }                 // читать, когда есть данные
    int inFd() const { return m_in; }                // писать, когда wantsWrite
    int64_t lastDataUs() const { return m_lastData; }
    int64_t openedUs() const { return m_opened; }
    const std::string &host() const { return m_host; }

    using Handler = std::function<void(uint8_t type, const uint8_t *body, size_t n)>;
    bool read(const Handler &on);                    // всё, что пришло; false — связь кончилась
    // Отправка — без ожидания: что не влезло — в очередь, дошлётся по POLLOUT (flush). Путь умер (кабель выдернули)
    // — очередь растёт; больше 4 МБ — broken(), путь закрыть: ждать нельзя, приёмник один поток на всё
    void send(uint8_t type, const void *body = nullptr, size_t n = 0);    // как есть (Ping, Use, DirectAsk…)
    void input(uint32_t seq, uint8_t type, const void *body = nullptr, size_t n = 0);   // ввод — в Input с номером
    bool wantsWrite() const { return m_sshOut.size() > m_sshOff; }
    bool directWantsWrite() const { return m_directOut.size() > m_directOff; }
    void flush();                                    // дослать очереди (POLLOUT)
    bool broken() const { return m_broken; }
    // картинка напрямую: подключиться к addr:port (адрес — байты IPv4 как в сети), сказать пропуск (DirectJoin)
    bool openDirect(const uint8_t addr[4], uint16_t port, const uint8_t pass[16]);
    void closeDirect();
    int directFd() const { return m_direct; }        // −1 — нет
    int64_t directDataUs() const { return m_directData; }
    bool readDirect(const Handler &on);              // false — прямой кончился
    void sendDirect(uint8_t type, const void *body = nullptr, size_t n = 0);

private:
    static bool drain(int fd, std::vector<uint8_t> &buf, int64_t &last, const Handler &on);
    static void frame(std::vector<uint8_t> &out, uint8_t type, const void *body, size_t n);
    static bool push(int fd, std::vector<uint8_t> &out, size_t &off);   // false — fd умер
    pid_t m_pid = -1;
    std::string m_host;
    int m_direct = -1;
    std::vector<uint8_t> m_directBuf, m_directOut;
    size_t m_directOff = 0;
    int64_t m_directData = 0;
    int m_in = -1, m_out = -1;
    std::vector<uint8_t> m_buf, m_sshOut;
    size_t m_sshOff = 0;
    bool m_broken = false;
    bool m_quiet = false;
    int64_t m_lastData = 0, m_opened = 0;
};

int64_t monoUs();   // CLOCK_MONOTONIC, мкс — так же меряет телефон (метки кадров)
