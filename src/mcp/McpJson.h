#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace drift::mcp {

inline QJsonObject ok(QJsonObject extra = {})
{
    extra.insert(QStringLiteral("ok"), true);
    return extra;
}

inline QJsonObject err(const char *code, const QString &detail = {})
{
    QJsonObject o{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QString::fromUtf8(code)}};
    if (!detail.isEmpty())
        o.insert(QStringLiteral("detail"), detail);
    return o;
}

inline QJsonObject objectSchema(const QJsonObject &properties, const QStringList &required = {})
{
    QJsonObject s{{QStringLiteral("type"), QStringLiteral("object")},
                  {QStringLiteral("properties"), properties}};
    if (!required.isEmpty()) {
        QJsonArray req;
        for (const QString &key : required)
            req.append(key);
        s.insert(QStringLiteral("required"), req);
    }
    return s;
}

inline QJsonObject stringProp(const QString &description)
{
    return {{QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), description}};
}

inline QJsonObject numberProp(const QString &description)
{
    return {{QStringLiteral("type"), QStringLiteral("number")},
            {QStringLiteral("description"), description}};
}

inline QJsonObject integerProp(const QString &description)
{
    return {{QStringLiteral("type"), QStringLiteral("integer")},
            {QStringLiteral("description"), description}};
}

inline QJsonObject boolProp(const QString &description)
{
    return {{QStringLiteral("type"), QStringLiteral("boolean")},
            {QStringLiteral("description"), description}};
}

inline QJsonObject arrayProp(const QJsonObject &items, const QString &description)
{
    return {{QStringLiteral("type"), QStringLiteral("array")},
            {QStringLiteral("items"), items},
            {QStringLiteral("description"), description}};
}

inline QJsonObject toolDef(const QString &name, const QString &description, const QJsonObject &inputSchema)
{
    return {{QStringLiteral("name"), name},
            {QStringLiteral("description"), description},
            {QStringLiteral("inputSchema"), inputSchema}};
}

inline QJsonObject textResult(const QJsonObject &payload, bool isError = false)
{
    const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QJsonObject contentItem{{QStringLiteral("type"), QStringLiteral("text")},
                            {QStringLiteral("text"), QString::fromUtf8(json)}};
    return {{QStringLiteral("content"), QJsonArray{contentItem}},
            {QStringLiteral("isError"), isError || payload.value(QStringLiteral("ok")).toBool(true) == false}};
}

} // namespace drift::mcp
