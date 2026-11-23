// НаомиФон, приставка на Pi — курсор стола, набор «Мягкая» (выбор пользователя 17 ноября 2026 из 10 наборов макета,
// docs/DESKTOP.md, «Курсор»): тёплый белый со скруглёнными углами, тёмная обводка, мягкая тень. Форму называет
// телефон (сообщение Cursor, stream_protocol.h); рисует приставка — cairo, в квадрат 64 × 64 для слоя курсора (ARGB
// с умноженной альфой), в натуральную величину монитора: слой курсора всегда 1:1. Формы — те же пути SVG, что в
// макете (поле 32 × 32); «занято» — крутилка «Морфинг» (круг → скруглённый квадрат → лист, оборот за 1,8 с).
#pragma once

#include <cstdint>
#include <vector>

class CursorArt
{
public:
    static constexpr int kSize = 64;   // сторона картинки (слой курсора дисплея)
    // monitorH — строк монитора: размер — как у прежней стрелки (19 точек при 1080 строках, больше — пропорционально)
    explicit CursorArt(int monitorH);
    struct Image {
        std::vector<uint32_t> px = std::vector<uint32_t>(kSize * kSize, 0);   // ARGB8888, умноженная альфа
        int hotX = 0, hotY = 0;                                                 // горячая точка в картинке
    };
    Image render(int shape, double seconds = 0) const;   // stream::CursorShape; seconds — время крутилки «занято»

private:
    double m_unit;   // пикселей монитора в единице рисунка
};
