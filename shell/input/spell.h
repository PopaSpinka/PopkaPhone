// Исправление опечаток клавиатуры (docs/KEYBOARD.md) — своё, локальное, без сети.
//
// Клавиатура знает точку каждого касания. Поэтому слово ищется не по буквам, а по касаниям:
//   - касание → вероятности клавиш вокруг (нормальное распределение по расстоянию до центров, σ — доля
//     шага клавиш), так промах на соседнюю клавишу почти бесплатен, на дальнюю — дорог;
//   - словарь — дерево слов с частотами (tools/naomi dict: частоты из субтитров OpenSubtitles, FrequencyWords,
//     CC BY-SA 4.0), лежит на телефоне в ~/.local/share/naomi/dict/<язык>.ndict и читается через mmap;
//   - поиск — лучом по дереву: на каждое касание — совпадение (по касанию), лишняя буква, пропущенная
//     буква, перестановка соседних; не больше двух правок. Итог слова = касания + правки + частота слова.
//   - свои слова (что ты набрал и оставил, что вернул после исправления) — ~/.local/share/naomi/dict/
//     user-<язык>.txt, с ними считается так же. Пишут их оболочка и программы — при записи файл сливается.
//
// Словарь (формат): «NDIC», версия, число узлов, число букв, ln мин/макс частоты, ln суммы (u32×3, f32×3),
// буквы (UTF-32), узлы по 8 байт в порядке обхода в ширину: первый ребёнок u32, детей u8, буква u8,
// частота слова u8 (0 — не слово), лучшая частота в поддереве u8. Частота — ln числа встреч, сжатый в 1…255.
#pragma once

#include <QFile>
#include <QHash>
#include <QObject>
#include <QPointF>
#include <QSet>
#include <QTimer>
#include <QVariantList>

#include <memory>
#include <vector>

class SpellDict;

class Spell : public QObject
{
    Q_OBJECT
public:
    explicit Spell(QObject *parent = nullptr);
    ~Spell() override;

    // клавиши текущей страницы: [{ch, x, y}] — центры букв; σ — разброс касаний по осям, в точках
    Q_INVOKABLE void setKeys(const QVariantList &keys, qreal sigmaX, qreal sigmaY);
    // касания слова: [{x, y, ch, exact}] — ch: что ввела клавиатура (буква клавиши под пальцем);
    // exact — буква не с этой страницы букв (ё долгим нажатием, дефис со знаков): сравнивать как есть.
    // Итог: {literal, known, best, correct, confidence, us}; correct — заменить на best пробелом
    Q_INVOKABLE QVariantMap decode(const QString &lang, const QVariantList &taps);
    Q_INVOKABLE bool known(const QString &lang, const QString &word);
    Q_INVOKABLE void learn(const QString &lang, const QString &word, int weight = 1);   // своё слово

private:
    struct Tap { QPointF pos; QChar ch; bool exact; };
    SpellDict *dict(const QString &lang);
    QHash<QString, int> &user(const QString &lang);
    void saveUser();
    float personalLogP(int count) const;

    QHash<QString, SpellDict *> m_dicts;
    QHash<QString, QHash<QString, int>> m_user;
    QHash<QChar, QPointF> m_keys;
    qreal m_sx = 16, m_sy = 20;
    QTimer m_save;
    QSet<QString> m_dirty;
};
