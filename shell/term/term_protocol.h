// НаомиФон — язык стола и службы «Терминала» naomi-term (docs/TERMINAL.md). Сокет $XDG_RUNTIME_DIR/naomi-term.sock,
// кадры: тип (1 байт), номер вкладки (4 байта, младший первым), длина (4 байта, так же) и столько байт содержимого.
//   стол → служба: H «версия номер номер…» — привет и какие вкладки у стола есть (остальные служба закрывает);
//     O «столбцы строки восстановить папка» — открыть вкладку: живую — прислать её текст, нет — запустить оболочку
//     в папке (восстановить = 1 — сначала текст, сохранённый на диске); I — нажатия; R «столбцы строки»; C — закрыть
//     вкладку (процессы прочь, текст с диска прочь); Q — закрыть терминал: текст сохранить, процессы прочь;
//   служба → стол: H «версия»; Y — текст вкладки, что служба помнит (после O: на запросы в нём стол не отвечает — их
//     задавали прошлые программы); D — новый вывод; P «папка\nпрограмма» — где оболочка и что запущено в ней
//     (пусто — ждёт команды); X «код» — оболочка вкладки завершилась.
#pragma once

#include <cstdint>

namespace termproto {

constexpr uint32_t kVersion = 1;
constexpr uint32_t kHeader = 9;
constexpr uint32_t kMaxPayload = 1u << 20;

enum Type : char {
    Hello = 'H',
    Open = 'O',
    Input = 'I',
    Resize = 'R',
    Close = 'C',
    Quit = 'Q',
    Replay = 'Y',
    Data = 'D',
    Place = 'P',
    Exit = 'X',
};

inline void putU32(char *p, uint32_t v)
{
    p[0] = char(v & 0xff);
    p[1] = char((v >> 8) & 0xff);
    p[2] = char((v >> 16) & 0xff);
    p[3] = char((v >> 24) & 0xff);
}

inline uint32_t getU32(const char *p)
{
    const auto *u = reinterpret_cast<const unsigned char *>(p);
    return uint32_t(u[0]) | uint32_t(u[1]) << 8 | uint32_t(u[2]) << 16 | uint32_t(u[3]) << 24;
}

}   // namespace termproto
