#pragma once

#include <QMap>
#include <QString>
#include <QVariant>

namespace drift {

// Per-clip libavfilter effect with named parameters.
struct Effect
{
    QString name;
    QMap<QString, QVariant> parameters;

    QString filterGraphString() const;
};

} // namespace drift
