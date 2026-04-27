#pragma once

#include "Time.h"

#include <QMap>

namespace drift {

enum class Interpolation { Linear, Hold };

template<typename T>
class KeyframeTrack
{
public:
    bool isEmpty() const { return m_values.isEmpty(); }

    void setKeyframe(TimeUs time, const T &value) { m_values.insert(time, value); }

    void removeKeyframe(TimeUs time) { m_values.remove(time); }

    const QMap<TimeUs, T> &keyframes() const { return m_values; }

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

        const double t = static_cast<double>(time - prev.key())
                         / static_cast<double>(it.key() - prev.key());
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
