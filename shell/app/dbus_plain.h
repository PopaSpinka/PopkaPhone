// НаомиФон — ответы D-Bus в простые типы Qt (для naomid и для оболочки): варианты раскрыты, пути —
// строки, массивы — QVariantList, словари — QVariantMap, байты — QByteArray. Такое можно отдать в QML
// или сравнить.
#pragma once

#include <QVariant>

QVariant plain(const QVariant &v);
QVariantMap plainMap(const QVariant &v);   // ответ GetAll, словарь сигнала PropertiesChanged
