#include "spell.h"

#include <QDir>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(lcSpell, "naomi.spell", QtInfoMsg)

namespace {

// Цены (натуральные логарифмы вероятностей): попасть точно в клавишу ≈ −0,1, в соседнюю ≈ −3
const float kIns = -5.0f;       // лишняя буква: касание, которому в слове ничего не соответствует
const float kDel = -5.5f;       // пропущенная буква
const float kTrans = -4.5f;     // две соседние буквы переставлены
const float kFloor = -9.0f;     // любая буква вместо задуманной — не дороже (и это правка)
const float kExactMiss = -12.0f; // буква не со страницы букв (ё, дефис) — только как есть
const float kAlt = -1.0f;       // ё на месте е, ъ на месте ь (их ставят долгим нажатием)
const float kUnknown = 0.0f;    // незнакомое слово — как самое редкое в словаре (подобрано пробой, KEYBOARD.md)
// Слово из словаря (KEYBOARD.md, «Исправление словарных слов»): касание буквы — у границы клавиш, если чужая
// буква не дешевле своей больше чем на kNearEdge (≈ 20 пикселей экрана от границы), и лучшее слово вероятнее
// набранного хотя бы в e^kWordLead (20) раз. Подобрано пробой с разбросом пальца пользователя (KEYBOARD.md)
const float kNearEdge = 1.0f;
const float kWordLead = 3.0f;
const int kBeam = 64;           // ширина луча: 64 и 256 дают одно и то же (проба), 64 — втрое быстрее
const int kMaxEdits = 2;

QString dictDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/naomi/dict";
}

} // namespace

class SpellDict
{
public:
    struct Node {
        quint32 first;
        quint8 kids, letter, freq, best;
    };
    static_assert(sizeof(Node) == 8);

    bool open(const QString &path)
    {
        file.setFileName(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() < 28)
            return false;
        const uchar *d = file.map(0, file.size());
        if (!d || memcmp(d, "NDIC", 4) != 0)
            return false;
        quint32 head[3];
        float ln[3];
        memcpy(head, d + 4, 12);
        memcpy(ln, d + 16, 12);
        if (head[0] != 1 || 28 + 4 * qint64(head[2]) + 8 * qint64(head[1]) > file.size())
            return false;
        for (quint32 i = 0; i < head[2]; ++i) {
            char32_t c;
            memcpy(&c, d + 28 + 4 * i, 4);
            alphabet.append(QChar(char16_t(c)));
            index.insert(QChar(char16_t(c)), int(i));
        }
        nodes = reinterpret_cast<const Node *>(d + 28 + 4 * head[2]);
        count = head[1];
        lnMin = ln[0];
        lnMax = ln[1];
        lnTotal = ln[2];
        return true;
    }
    float logP(quint8 q) const { return lnMin + (q - 1) / 254.0f * (lnMax - lnMin) - lnTotal; }
    float rarest() const { return lnMin - lnTotal; }
    int child(int node, int letter) const
    {
        const Node &n = nodes[node];
        for (quint32 k = n.first; k < n.first + n.kids; ++k)
            if (nodes[k].letter == letter)
                return int(k);
        return -1;
    }
    int find(const QString &w) const   // узел слова или −1
    {
        int n = 0;
        for (QChar c : w) {
            const int l = index.value(c, -1);
            if (l < 0 || (n = child(n, l)) < 0)
                return -1;
        }
        return n;
    }

    QFile file;
    const Node *nodes = nullptr;
    quint32 count = 0;
    QString alphabet;
    QHash<QChar, int> index;
    float lnMin = 0, lnMax = 0, lnTotal = 0;
};

Spell::Spell(QObject *parent)
    : QObject(parent)
{
    m_save.setSingleShot(true);
    m_save.setInterval(2000);
    connect(&m_save, &QTimer::timeout, this, &Spell::saveUser);
}

Spell::~Spell()
{
    saveUser();
    qDeleteAll(m_dicts);
}

SpellDict *Spell::dict(const QString &lang)
{
    if (!m_dicts.contains(lang)) {
        auto *d = new SpellDict;
        const QString path = dictDir() + "/" + lang + ".ndict";
        if (!d->open(path)) {
            qCWarning(lcSpell, "нет словаря %s (tools/naomi dict)", qPrintable(path));
            delete d;
            d = nullptr;
        } else {
            qCInfo(lcSpell, "словарь %s: %u узлов", qPrintable(lang), d->count);
        }
        m_dicts.insert(lang, d);
    }
    return m_dicts.value(lang);
}

QHash<QString, int> &Spell::user(const QString &lang)
{
    if (!m_user.contains(lang)) {
        QHash<QString, int> words;
        QFile f(dictDir() + "/user-" + lang + ".txt");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&f);
            while (!in.atEnd()) {
                const QStringList p = in.readLine().split(' ');
                if (p.size() == 2)
                    words.insert(p[0], p[1].toInt());
            }
        }
        m_user.insert(lang, words);
    }
    return m_user[lang];
}

// свои слова пишут оболочка и каждая программа (у каждой своя клавиатура): перед записью — слить с тем, что
// уже в файле (берётся большее число)
void Spell::saveUser()
{
    for (const QString &lang : std::as_const(m_dirty)) {
        QDir().mkpath(dictDir());
        const QString path = dictDir() + "/user-" + lang + ".txt";
        QHash<QString, int> &words = m_user[lang];
        QFile old(path);
        if (old.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&old);
            while (!in.atEnd()) {
                const QStringList p = in.readLine().split(' ');
                if (p.size() == 2)
                    words[p[0]] = std::max(words.value(p[0]), p[1].toInt());
            }
        }
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            continue;
        QTextStream out(&f);
        for (auto it = words.begin(); it != words.end(); ++it)
            out << it.key() << ' ' << it.value() << '\n';
        out.flush();
        f.commit();
    }
    m_dirty.clear();
}

float Spell::personalLogP(int count) const
{
    return -11.0f + std::min(3.0f, std::log(float(count)));   // как довольно частое слово, и чаще — выше
}

void Spell::setKeys(const QVariantList &keys, qreal sigmaX, qreal sigmaY)
{
    m_keys.clear();
    for (const QVariant &v : keys) {
        const QVariantMap k = v.toMap();
        const QString ch = k.value("ch").toString().toLower();
        if (ch.size() == 1)
            m_keys.insert(ch[0], QPointF(k.value("x").toReal(), k.value("y").toReal()));
    }
    m_sx = sigmaX;
    m_sy = sigmaY;
}

bool Spell::known(const QString &lang, const QString &word)
{
    const QString w = word.toLower();
    if (SpellDict *d = dict(lang)) {
        const int n = d->find(w);
        if (n >= 0 && d->nodes[n].freq > 0)
            return true;
    }
    return user(lang).value(w) >= 2;
}

void Spell::learn(const QString &lang, const QString &word, int weight)
{
    const QString w = word.toLower();
    if (w.isEmpty() || w.size() > 24)
        return;
    if (SpellDict *d = dict(lang)) {
        const int n = d->find(w);
        if (n >= 0 && d->nodes[n].freq > 0)
            return;   // и так в словаре
        for (QChar c : w)
            if (!d->index.contains(c))
                return;
    }
    user(lang)[w] += weight;
    m_dirty.insert(lang);
    m_save.start();
}

QVariantMap Spell::decode(const QString &lang, const QVariantList &tapList)
{
    QElapsedTimer timer;
    timer.start();
    std::vector<Tap> taps;
    QString literal;
    for (const QVariant &v : tapList) {
        const QVariantMap t = v.toMap();
        const QString ch = t.value("ch").toString();
        if (ch.size() != 1)
            continue;
        taps.push_back({QPointF(t.value("x").toReal(), t.value("y").toReal()), ch[0].toLower(),
                        t.value("exact").toBool()});
        literal += ch;
    }
    const QString lower = literal.toLower();
    QVariantMap out{{"literal", literal}, {"known", false}, {"best", QString()}, {"correct", false}, {"us", 0}};
    SpellDict *d = dict(lang);
    if (!d || taps.empty())
        return out;
    const bool isKnown = known(lang, lower);
    out["known"] = isKnown;

    // касание → ln вероятности каждой буквы
    const int n = int(taps.size()), A = int(d->alphabet.size());
    std::vector<float> lp(size_t(n) * A, kFloor);
    for (int i = 0; i < n; ++i) {
        float *row = &lp[size_t(i) * A];
        const Tap &t = taps[size_t(i)];
        if (t.exact || !m_keys.contains(t.ch)) {
            for (int a = 0; a < A; ++a)
                row[a] = d->alphabet[a] == t.ch ? 0.0f : kExactMiss;
            continue;
        }
        float norm = 0;
        for (int a = 0; a < A; ++a) {
            const auto it = m_keys.constFind(d->alphabet[a]);
            if (it == m_keys.constEnd())
                continue;
            const qreal dx = (t.pos.x() - it->x()) / m_sx, dy = (t.pos.y() - it->y()) / m_sy;
            row[a] = float(-(dx * dx + dy * dy) / 2);
            norm += std::exp(row[a]);
        }
        const float ln = std::log(norm);
        for (int a = 0; a < A; ++a)
            if (m_keys.contains(d->alphabet[a]))
                row[a] = std::max(kFloor, row[a] - ln);
        auto alt = [&](QChar to, QChar from) {
            const int t2 = d->index.value(to, -1), f2 = d->index.value(from, -1);
            if (t2 >= 0 && f2 >= 0 && !m_keys.contains(to))
                row[t2] = std::max(kFloor, row[f2] + kAlt);
        };
        alt(u'ё', u'е');
        alt(u'ъ', u'ь');
    }

    // поиск лучом: слои по числу съеденных касаний, в слое — лучшее состояние на узел дерева
    // состояние: узел, путь (буквы), цена, правки; k — цена с лучшим словом впереди (по ней отсев)
    struct St { int node, path; float s, k; int edits; };
    std::vector<std::pair<int, quint8>> arena{{-1, 0}};   // путь: (родитель, буква)
    arena.reserve(1 << 16);
    auto extend = [&](int path, quint8 letter) {
        arena.push_back({path, letter});
        return int(arena.size()) - 1;
    };
    auto st = [&](int node, int path, float s, int edits) {
        return St{node, path, s, s + d->logP(d->nodes[node].best), edits};
    };
    // отсев: на узел — лучшее состояние (хеш-таблица с поколениями — без очистки), затем лучшие kBeam
    const int kTable = 1 << 15;
    std::vector<quint32> stamp(kTable, 0);
    std::vector<int> slot(kTable, 0);
    quint32 epoch = 0;
    std::vector<St> kept;
    auto prune = [&](std::vector<St> &v) {
        ++epoch;
        kept.clear();
        for (const St &x : v) {
            quint32 h = (quint32(x.node) * 2654435761u) >> 17;
            while (true) {
                if (stamp[h] != epoch) {
                    stamp[h] = epoch;
                    slot[h] = int(kept.size());
                    kept.push_back(x);
                    break;
                }
                St &y = kept[size_t(slot[h])];
                if (y.node == x.node) {
                    if (x.s > y.s)
                        y = x;
                    break;
                }
                h = (h + 1) & (kTable - 1);
            }
        }
        if (int(kept.size()) > kBeam) {
            std::nth_element(kept.begin(), kept.begin() + kBeam, kept.end(), [](const St &a, const St &b) { return a.k > b.k; });
            kept.resize(kBeam);
        }
        v.swap(kept);
    };
    std::vector<std::vector<St>> layer(size_t(n) + 1);
    layer[0].push_back(st(0, 0, 0.0f, 0));
    for (int i = 0; i <= n; ++i) {
        std::vector<St> &cur = layer[size_t(i)];
        prune(cur);
        // пропущенные буквы — не съедая касания (до двух правок)
        std::vector<St> from = cur;
        for (int round = 0; round < kMaxEdits && !from.empty(); ++round) {
            std::vector<St> add;
            for (const St &x : from) {
                if (x.edits >= kMaxEdits)
                    continue;
                const SpellDict::Node &nd = d->nodes[x.node];
                for (quint32 k = nd.first; k < nd.first + nd.kids; ++k)
                    add.push_back(st(int(k), extend(x.path, d->nodes[k].letter), x.s + kDel, x.edits + 1));
            }
            prune(add);
            cur.insert(cur.end(), add.begin(), add.end());
            from.swap(add);
        }
        prune(cur);
        if (i == n)
            break;
        const float *row = &lp[size_t(i) * A];
        std::vector<St> &next = layer[size_t(i) + 1];
        for (const St &x : cur) {
            const SpellDict::Node &nd = d->nodes[x.node];
            for (quint32 k = nd.first; k < nd.first + nd.kids; ++k) {
                const float p = row[d->nodes[k].letter];
                const int e = x.edits + (p <= kFloor + 0.01f ? 1 : 0);
                if (e <= kMaxEdits)
                    next.push_back(st(int(k), extend(x.path, d->nodes[k].letter), x.s + p, e));
            }
            if (x.edits >= kMaxEdits)
                continue;
            next.push_back(St{x.node, x.path, x.s + kIns, x.k + kIns, x.edits + 1});   // лишнее касание
            if (i + 1 < n) {   // перестановка: касания i, i+1 — буквы b, a, а в слове a, b
                const float *row2 = &lp[size_t(i + 1) * A];
                for (quint32 k = nd.first; k < nd.first + nd.kids; ++k) {
                    const float pa = row2[d->nodes[k].letter];
                    if (pa < -4.0f)
                        continue;
                    const SpellDict::Node &c = d->nodes[k];
                    for (quint32 g = c.first; g < c.first + c.kids; ++g) {
                        const float pb = row[d->nodes[g].letter];
                        if (pb < -4.0f)
                            continue;
                        layer[size_t(i) + 2].push_back(st(int(g), extend(extend(x.path, c.letter), d->nodes[g].letter),
                                                          x.s + pa + pb + kTrans, x.edits + 1));
                    }
                }
            }
        }
    }

    // кандидаты: слова, до которых дошёл луч
    auto wordOf = [&](int path) {
        QString w;
        for (int p = path; p > 0; p = arena[size_t(p)].first)
            w.prepend(d->alphabet[arena[size_t(p)].second]);
        return w;
    };
    QHash<QString, float> score;
    auto offer = [&](const QString &w, float s) {
        auto it = score.find(w);
        if (it == score.end() || s > *it)
            score.insert(w, s);
    };
    for (const St &x : layer[size_t(n)])
        if (d->nodes[x.node].freq > 0)
            offer(wordOf(x.path), x.s + d->logP(d->nodes[x.node].freq));
    // свои слова, которых нет в словаре: та же цена правок, выравниванием
    const QHash<QString, int> &mine = user(lang);
    for (auto it = mine.begin(); it != mine.end(); ++it) {
        const QString &w = it.key();
        const int m = int(w.size());
        if (m == 0 || m > n + kMaxEdits || m + kMaxEdits < n)
            continue;
        std::vector<int> letters(static_cast<size_t>(m));
        bool ok = true;
        for (int j = 0; j < m && ok; ++j)
            ok = (letters[size_t(j)] = d->index.value(w[j], -1)) >= 0;
        if (!ok)
            continue;
        std::vector<float> dp(size_t(n + 1) * (m + 1), -1e9f);
        auto at = [&](int i, int j) -> float & { return dp[size_t(i) * (m + 1) + j]; };
        at(0, 0) = 0;
        for (int i = 0; i <= n; ++i)
            for (int j = 0; j <= m; ++j) {
                float &v = at(i, j);
                if (i > 0 && j > 0)
                    v = std::max(v, at(i - 1, j - 1) + lp[size_t(i - 1) * A + letters[size_t(j - 1)]]);
                if (i > 0)
                    v = std::max(v, at(i - 1, j) + kIns);
                if (j > 0)
                    v = std::max(v, at(i, j - 1) + kDel);
                if (i > 1 && j > 1)
                    v = std::max(v, at(i - 2, j - 2) + lp[size_t(i - 1) * A + letters[size_t(j - 2)]]
                                        + lp[size_t(i - 2) * A + letters[size_t(j - 1)]] + kTrans);
            }
        offer(w, at(n, m) + personalLogP(it.value()));
    }

    QList<QPair<float, QString>> ranked;
    for (auto it = score.begin(); it != score.end(); ++it)
        ranked.append({it.value(), it.key()});
    std::sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first > b.first; });

    // как написано: всё в верхнем регистре — так и оставить, первая заглавная — заглавная
    auto cased = [&](QString w) {
        if (literal.size() > 1 && literal == literal.toUpper() && literal != lower)
            return w.toUpper();
        if (!literal.isEmpty() && literal[0].isUpper() && !w.isEmpty())
            w[0] = w[0].toUpper();
        return w;
    };
    // заменить пробелом — только незнакомое слово и только если лучший вариант уверенно впереди
    const QString best = ranked.isEmpty() ? QString() : ranked.first().second;
    const float bestS = ranked.isEmpty() ? 0.0f : ranked.first().first;
    if (!isKnown && !best.isEmpty() && best != lower) {
        float typed = d->rarest() + kUnknown;   // написанное как есть — незнакомое слово
        for (int i = 0; i < n; ++i) {
            const int a = d->index.value(taps[size_t(i)].ch, -1);
            typed += a >= 0 ? lp[size_t(i) * A + a] : 0.0f;
        }
        float rivals = std::exp(typed - bestS);
        int counted = 0;
        for (const auto &[s, w] : ranked) {
            if (w == best)
                continue;
            rivals += std::exp(s - bestS);
            if (++counted == 3)
                break;
        }
        const float confidence = 1.0f / (1.0f + rivals);
        out["confidence"] = confidence;
        if (confidence >= 0.5f) {
            out["best"] = cased(best);
            out["correct"] = true;
        }
    } else if (isKnown && !best.isEmpty() && best != lower && best.size() == lower.size() && !mine.contains(lower)) {
        // Слово из словаря — осторожно (замер набора пользователя: «деда» вместо «дела», палец на 6 пикселей за
        // границей «л»): лучшее отличается одной буквой, у этой буквы своя клавиша (не ё и не ъ), касание — у
        // самой границы, и лучшее в разы вероятнее набранного. Свои слова не трогаем; «стереть» сразу после
        // исправления, как всегда, вернёт набранное и запомнит его
        int at = -1, diffs = 0;
        for (int i = 0; i < lower.size(); ++i)
            if (best[i] != lower[i]) {
                at = i;
                ++diffs;
            }
        const int was = at >= 0 ? d->index.value(lower[at], -1) : -1;
        const int now = at >= 0 ? d->index.value(best[at], -1) : -1;
        if (diffs == 1 && was >= 0 && now >= 0 && !taps[size_t(at)].exact && m_keys.contains(best[at])) {
            const float near = lp[size_t(at) * A + now] - lp[size_t(at) * A + was];
            const float lead = bestS - score.value(lower, bestS);
            out["confidence"] = 1.0f / (1.0f + std::exp(-lead));
            if (near >= -kNearEdge && lead >= kWordLead) {
                out["best"] = cased(best);
                out["correct"] = true;
            }
        }
    }
    out["us"] = int(timer.nsecsElapsed() / 1000);
    qCDebug(lcSpell, "«%s» → %s за %lld мкс", qPrintable(literal),
            qPrintable(out["correct"].toBool() ? out["best"].toString() : QStringLiteral("—")),
            timer.nsecsElapsed() / 1000);
    return out;
}
