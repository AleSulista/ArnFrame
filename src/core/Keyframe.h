#pragma once

#include "Time.h"

#include <QMap>
#include <QString>
#include <QtMath>

namespace drift {

enum class Interpolation { Linear, Hold, Ease };

inline QString interpolationToString(Interpolation mode)
{
    switch (mode) {
    case Interpolation::Hold:
        return QStringLiteral("hold");
    case Interpolation::Ease:
        return QStringLiteral("ease");
    case Interpolation::Linear:
    default:
        return QStringLiteral("linear");
    }
}

inline Interpolation interpolationFromString(const QString &mode)
{
    if (mode == QLatin1String("hold"))
        return Interpolation::Hold;
    if (mode == QLatin1String("ease"))
        return Interpolation::Ease;
    return Interpolation::Linear;
}

template<typename T>
class KeyframeTrack
{
public:
    bool isEmpty() const { return m_values.isEmpty(); }

    void setKeyframe(TimeUs time, const T &value) { m_values.insert(time, value); }

    void removeKeyframe(TimeUs time) { m_values.remove(time); }

    const QMap<TimeUs, T> &keyframes() const { return m_values; }

    // Returns the key time within tolerance, or -1 if none.
    TimeUs nearestKeyframe(TimeUs time, TimeUs tolerance) const
    {
        TimeUs best = -1;
        TimeUs bestDist = tolerance + 1;
        for (auto it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
            const TimeUs d = qAbs(it.key() - time);
            if (d < bestDist) {
                bestDist = d;
                best = it.key();
            }
        }
        return bestDist <= tolerance ? best : TimeUs{-1};
    }

    T evaluateAt(TimeUs time) const
    {
        if (m_values.isEmpty())
            return T{};

        auto it = m_values.lowerBound(time);
        if (it == m_values.end())
            return std::prev(it).value();

        if (it.key() == time || it == m_values.begin())
            return it.value();

        const auto prev = std::prev(it);
        if (m_interpolation == Interpolation::Hold)
            return prev.value();

        double t = static_cast<double>(time - prev.key())
                   / static_cast<double>(it.key() - prev.key());
        if (m_interpolation == Interpolation::Ease)
            t = t * t * (3.0 - 2.0 * t); // smoothstep
        return lerp(prev.value(), it.value(), t);
    }

    void setInterpolation(Interpolation mode) { m_interpolation = mode; }
    Interpolation interpolation() const { return m_interpolation; }

private:
    static double lerp(double a, double b, double t) { return a + (b - a) * t; }

    static float lerp(float a, float b, double t)
    {
        return static_cast<float>(a + (b - a) * static_cast<float>(t));
    }

    QMap<TimeUs, T> m_values;
    Interpolation m_interpolation = Interpolation::Linear;
};

} // namespace drift
