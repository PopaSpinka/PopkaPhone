#include "dbus_plain.h"

#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDBusVariant>

QVariant plain(const QVariant &v)
{
    if (v.metaType() == QMetaType::fromType<QDBusVariant>())
        return plain(v.value<QDBusVariant>().variant());
    if (v.metaType() == QMetaType::fromType<QDBusObjectPath>())
        return v.value<QDBusObjectPath>().path();
    if (v.metaType() == QMetaType::fromType<QList<QDBusObjectPath>>()) {
        QVariantList paths;
        for (const QDBusObjectPath &p : v.value<QList<QDBusObjectPath>>())
            paths << p.path();
        return paths;
    }
    if (v.metaType() != QMetaType::fromType<QDBusArgument>())
        return v;
    const QDBusArgument arg = v.value<QDBusArgument>();
    switch (arg.currentType()) {
    case QDBusArgument::MapType: {
        QVariantMap map;
        arg.beginMap();
        while (!arg.atEnd()) {
            arg.beginMapEntry();
            const QVariant key = plain(arg.asVariant());
            const QVariant value = plain(arg.asVariant());
            arg.endMapEntry();
            map.insert(key.toString(), value);
        }
        arg.endMap();
        return map;
    }
    case QDBusArgument::ArrayType: {
        if (arg.currentSignature() == "ay") {   // байты (SSID и т. п.) — одной строкой байтов
            QByteArray bytes;
            arg >> bytes;
            return bytes;
        }
        QVariantList list;
        arg.beginArray();
        while (!arg.atEnd())
            list << plain(arg.asVariant());
        arg.endArray();
        return list;
    }
    case QDBusArgument::StructureType: {
        QVariantList fields;
        arg.beginStructure();
        while (!arg.atEnd())
            fields << plain(arg.asVariant());
        arg.endStructure();
        return fields;
    }
    default:
        return plain(arg.asVariant());
    }
}

QVariantMap plainMap(const QVariant &v)
{
    QVariantMap map = plain(v).toMap();
    for (auto it = map.begin(); it != map.end(); ++it)
        *it = plain(*it);
    return map;
}
