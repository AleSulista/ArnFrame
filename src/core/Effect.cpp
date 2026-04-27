#include "Effect.h"

namespace drift {

QString Effect::filterGraphString() const
{
    if (name.isEmpty())
        return {};

    QStringList parts;
    for (auto it = parameters.constBegin(); it != parameters.constEnd(); ++it) {
        const QString value = it.value().toString();
        if (!value.isEmpty())
            parts.append(QStringLiteral("%1=%2").arg(it.key(), value));
    }

    if (parts.isEmpty())
        return name;

    return QStringLiteral("%1=%2").arg(name, parts.join(QLatin1Char(':')));
}

} // namespace drift
