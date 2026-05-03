#include "CompositorEffects.h"

#include "core/Time.h"

#include <QtMath>

#include <cmath>
#include <cstdint>
#include <cstring>

#include <QPainter>
#include <QColor>

namespace {

QRgb samplePixel(const QImage &image, int x, int y)
{
    x = qBound(0, x, image.width() - 1);
    y = qBound(0, y, image.height() - 1);
    return image.pixel(x, y);
}

uint32_t hashMix(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

uint32_t blockGlitchHash(int seed, drift::TimeUs timeUs, int blockCol, int blockRow)
{
    uint32_t hash = static_cast<uint32_t>(seed);
    hash ^= static_cast<uint32_t>(timeUs & 0xFFFFFFFFLL);
    hash ^= static_cast<uint32_t>((timeUs >> 32) & 0xFFFFFFFFLL);
    hash = hashMix(hash + static_cast<uint32_t>(blockCol) * 374761393u);
    hash = hashMix(hash + static_cast<uint32_t>(blockRow) * 668265263u);
    return hash;
}

uint32_t scanlineHash(drift::TimeUs animatedTimeUs, int row)
{
    uint32_t hash = static_cast<uint32_t>(animatedTimeUs & 0xFFFFFFFFLL);
    hash ^= static_cast<uint32_t>((animatedTimeUs >> 32) & 0xFFFFFFFFLL);
    return hashMix(hash + static_cast<uint32_t>(row) * 2246822519u);
}

uint32_t pixelHash(drift::TimeUs timeUs, int x, int y)
{
    uint32_t hash = static_cast<uint32_t>(timeUs & 0xFFFFFFFFLL);
    hash ^= static_cast<uint32_t>((timeUs >> 32) & 0xFFFFFFFFLL);
    hash = hashMix(hash + static_cast<uint32_t>(x) * 1597334677u);
    return hashMix(hash + static_cast<uint32_t>(y) * 3812015801u);
}

uchar clampByte(double value)
{
    return static_cast<uchar>(qBound(0, static_cast<int>(value + 0.5), 255));
}

double sampleLuminance(const QImage &image, int x, int y)
{
    const QRgb px = samplePixel(image, x, y);
    return 0.299 * static_cast<double>(qRed(px)) + 0.587 * static_cast<double>(qGreen(px))
           + 0.114 * static_cast<double>(qBlue(px));
}

QColor parseEffectColor(const QVariant &value)
{
    if (value.canConvert<QColor>()) {
        const QColor color = value.value<QColor>();
        if (color.isValid())
            return color;
    }

    const QString text = value.toString().trimmed();
    if (text.isEmpty())
        return QColor(0, 255, 255);

    QColor parsed(text);
    if (parsed.isValid())
        return parsed;

    if (text.compare(QStringLiteral("cyan"), Qt::CaseInsensitive) == 0)
        return QColor(0, 255, 255);

    return QColor(0, 255, 255);
}

QImage sobelEdgeMask(const QImage &src, double threshold)
{
    const int w = src.width();
    const int h = src.height();
    QImage mask(w, h, QImage::Format_RGBA8888);
    mask.fill(Qt::black);

    const double thresholdValue = qBound(0.0, threshold, 1.0) * 1020.0;

    for (int y = 1; y < h - 1; ++y) {
        for (int x = 1; x < w - 1; ++x) {
            const double p00 = sampleLuminance(src, x - 1, y - 1);
            const double p01 = sampleLuminance(src, x, y - 1);
            const double p02 = sampleLuminance(src, x + 1, y - 1);
            const double p10 = sampleLuminance(src, x - 1, y);
            const double p12 = sampleLuminance(src, x + 1, y);
            const double p20 = sampleLuminance(src, x - 1, y + 1);
            const double p21 = sampleLuminance(src, x, y + 1);
            const double p22 = sampleLuminance(src, x + 1, y + 1);

            const double gx = -p00 + p02 - 2.0 * p10 + 2.0 * p12 - p20 + p22;
            const double gy = -p00 - 2.0 * p01 - p02 + p20 + 2.0 * p21 + p22;
            const double magnitude = qSqrt(gx * gx + gy * gy);
            if (magnitude <= thresholdValue)
                continue;

            const uchar edge = clampByte((magnitude / 1020.0) * 255.0);
            mask.setPixel(x, y, qRgba(edge, edge, edge, 255));
        }
    }
    return mask;
}

QImage boxBlur(const QImage &input, int radius)
{
    if (input.isNull() || radius <= 0)
        return input;

    const int r = qBound(1, radius, 64);
    QImage temp = input.convertToFormat(QImage::Format_RGBA8888);
    for (int pass = 0; pass < 2; ++pass) {
        QImage out(temp.size(), QImage::Format_RGBA8888);
        const bool horizontal = pass == 0;
        for (int y = 0; y < temp.height(); ++y) {
            for (int x = 0; x < temp.width(); ++x) {
                int rSum = 0;
                int gSum = 0;
                int bSum = 0;
                int aSum = 0;
                int count = 0;
                for (int k = -r; k <= r; ++k) {
                    const int sx = horizontal ? qBound(0, x + k, temp.width() - 1) : x;
                    const int sy = horizontal ? y : qBound(0, y + k, temp.height() - 1);
                    const QRgb px = temp.pixel(sx, sy);
                    rSum += qRed(px);
                    gSum += qGreen(px);
                    bSum += qBlue(px);
                    aSum += qAlpha(px);
                    ++count;
                }
                out.setPixel(x, y, qRgba(rSum / count, gSum / count, bSum / count, aSum / count));
            }
        }
        temp = out;
    }
    return temp;
}

QImage boxBlurHorizontal(const QImage &input, int radius, int passes = 2)
{
    if (input.isNull() || radius <= 0)
        return input;

    const int r = qBound(1, radius, 64);
    QImage temp = input.convertToFormat(QImage::Format_RGBA8888);
    for (int pass = 0; pass < passes; ++pass) {
        QImage out(temp.size(), QImage::Format_RGBA8888);
        for (int y = 0; y < temp.height(); ++y) {
            for (int x = 0; x < temp.width(); ++x) {
                int rSum = 0;
                int gSum = 0;
                int bSum = 0;
                int count = 0;
                for (int k = -r; k <= r; ++k) {
                    const int sx = qBound(0, x + k, temp.width() - 1);
                    const QRgb px = temp.pixel(sx, y);
                    rSum += qRed(px);
                    gSum += qGreen(px);
                    bSum += qBlue(px);
                    ++count;
                }
                out.setPixel(x, y, qRgba(rSum / count, gSum / count, bSum / count, qAlpha(temp.pixel(x, y))));
            }
        }
        temp = out;
    }
    return temp;
}

QImage applyBloom(const QImage &input, const QMap<QString, QVariant> &parameters)
{
    const double intensity = parameters.value(QStringLiteral("intensity"), 0.75).toDouble();
    const double radius = parameters.value(QStringLiteral("radius"), 12.0).toDouble();
    if (intensity <= 0.0)
        return input;

    const QImage base = input.convertToFormat(QImage::Format_RGBA8888);
    const QImage blurred = boxBlur(base, static_cast<int>(radius));

    QImage out = base;
    QPainter painter(&out);
    painter.setCompositionMode(QPainter::CompositionMode_Plus);
    painter.setOpacity(qBound(0.0, intensity, 2.0));
    painter.drawImage(0, 0, blurred);
    return out;
}

QImage extractBrightPass(const QImage &input, double threshold, bool *hasBright = nullptr)
{
    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    QImage bright(src.size(), QImage::Format_RGBA8888);
    bright.fill(Qt::transparent);

    const double thresholdValue = qBound(0.0, threshold, 1.0) * 255.0;
    bool foundBright = false;

    for (int y = 0; y < src.height(); ++y) {
        const uchar *srcLine = src.constScanLine(y);
        uchar *brightLine = bright.scanLine(y);
        for (int x = 0; x < src.width(); ++x) {
            const int idx = x * 4;
            const double r = qMax(0.0, static_cast<double>(srcLine[idx + 0]) - thresholdValue);
            const double g = qMax(0.0, static_cast<double>(srcLine[idx + 1]) - thresholdValue);
            const double b = qMax(0.0, static_cast<double>(srcLine[idx + 2]) - thresholdValue);
            if (r <= 0.0 && g <= 0.0 && b <= 0.0)
                continue;

            foundBright = true;
            brightLine[idx + 0] = clampByte(r);
            brightLine[idx + 1] = clampByte(g);
            brightLine[idx + 2] = clampByte(b);
            brightLine[idx + 3] = srcLine[idx + 3];
        }
    }

    if (hasBright)
        *hasBright = foundBright;
    return bright;
}

QImage applyBloomGlow(const QImage &input, const QMap<QString, QVariant> &parameters)
{
    const double threshold = qBound(0.0, parameters.value(QStringLiteral("threshold"), 0.75).toDouble(), 1.0);
    const double intensity = parameters.value(QStringLiteral("intensity"), 0.6).toDouble();
    const int radius = qBound(1, parameters.value(QStringLiteral("radius"), 8).toInt(), 30);

    if (intensity <= 0.0)
        return input;

    const QImage base = input.convertToFormat(QImage::Format_RGBA8888);
    bool hasBright = false;
    const QImage brightPass = extractBrightPass(base, threshold, &hasBright);
    if (!hasBright)
        return input;

    const QImage blurred = boxBlurHorizontal(brightPass, radius);
    const double blend = qBound(0.0, intensity, 2.0);

    QImage out = base.copy();
    for (int y = 0; y < out.height(); ++y) {
        const uchar *blurLine = blurred.constScanLine(y);
        uchar *outLine = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            const int idx = x * 4;
            outLine[idx + 0] = clampByte(outLine[idx + 0] + blurLine[idx + 0] * blend);
            outLine[idx + 1] = clampByte(outLine[idx + 1] + blurLine[idx + 1] * blend);
            outLine[idx + 2] = clampByte(outLine[idx + 2] + blurLine[idx + 2] * blend);
        }
    }
    return out;
}

QImage applyRipple(const QImage &input, const QMap<QString, QVariant> &parameters)
{
    const double amplitude = parameters.value(QStringLiteral("amplitude"), 8.0).toDouble();
    const double frequency = parameters.value(QStringLiteral("frequency"), 6.0).toDouble();
    if (amplitude <= 0.0)
        return input;

    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    QImage out(src.size(), QImage::Format_RGBA8888);
    out.fill(Qt::transparent);

    const double cx = src.width() * 0.5;
    const double cy = src.height() * 0.5;
    const double maxRadius = qSqrt(cx * cx + cy * cy);

    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            const double dx = x - cx;
            const double dy = y - cy;
            const double dist = qSqrt(dx * dx + dy * dy);
            const double wave = qSin((dist / qMax(1.0, maxRadius)) * frequency * 2.0 * M_PI);
            const int sx = qBound(0, static_cast<int>(x + wave * amplitude), src.width() - 1);
            const int sy = qBound(0, static_cast<int>(y + wave * amplitude * 0.5), src.height() - 1);
            out.setPixel(x, y, src.pixel(sx, sy));
        }
    }
    return out;
}

QImage applyRippleWater(const QImage &input, const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    const double amplitude = qBound(0.0, parameters.value(QStringLiteral("amplitude"), 8.0).toDouble(), 50.0);
    const double frequency = qBound(0.0, parameters.value(QStringLiteral("frequency"), 12.0).toDouble(), 50.0);
    const double speed = qBound(0.0, parameters.value(QStringLiteral("speed"), 1.0).toDouble(), 10.0);
    const double centerX = qBound(0.0, parameters.value(QStringLiteral("centerX"), 0.5).toDouble(), 1.0);
    const double centerY = qBound(0.0, parameters.value(QStringLiteral("centerY"), 0.5).toDouble(), 1.0);

    if (amplitude <= 0.0)
        return input;

    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    QImage out(src.size(), QImage::Format_RGBA8888);

    const int w = src.width();
    const int h = src.height();
    const double cx = centerX * static_cast<double>(w - 1);
    const double cy = centerY * static_cast<double>(h - 1);
    const double maxRadius = qSqrt(cx * cx + cy * cy);
    const double normalize = qMax(1.0, maxRadius);
    const double timePhase = static_cast<double>(timeUs) / static_cast<double>(drift::kUsPerSecond) * speed
                             * 2.0 * M_PI;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double dx = static_cast<double>(x) - cx;
            const double dy = static_cast<double>(y) - cy;
            const double dist = qSqrt(dx * dx + dy * dy);
            if (dist < 1e-6) {
                out.setPixel(x, y, src.pixel(x, y));
                continue;
            }

            const double wave =
                qSin((dist / normalize) * frequency * 2.0 * M_PI + timePhase);
            const double scale = (wave * amplitude) / dist;
            const int sx = qBound(0, static_cast<int>(dx * scale + static_cast<double>(x) + 0.5), w - 1);
            const int sy = qBound(0, static_cast<int>(dy * scale + static_cast<double>(y) + 0.5), h - 1);
            out.setPixel(x, y, samplePixel(src, sx, sy));
        }
    }
    return out;
}

double shockwaveRingEnvelope(double normalizedDist, double waveRadius, double width)
{
    const double delta = qAbs(normalizedDist - waveRadius);
    if (width <= 0.0)
        return delta < 1e-6 ? 1.0 : 0.0;

    const double halfWidth = qMax(width * 0.5, 1e-6);
    if (delta >= halfWidth)
        return 0.0;

    const double t = 1.0 - (delta / halfWidth);
    return t * t * (3.0 - 2.0 * t);
}

QImage applyShockwavePulse(const QImage &input, const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    const double centerX = qBound(0.0, parameters.value(QStringLiteral("centerX"), 0.5).toDouble(), 1.0);
    const double centerY = qBound(0.0, parameters.value(QStringLiteral("centerY"), 0.5).toDouble(), 1.0);
    const double radius = qBound(0.0, parameters.value(QStringLiteral("radius"), 0.0).toDouble(), 1.0);
    const double width = qBound(0.0, parameters.value(QStringLiteral("width"), 0.08).toDouble(), 1.0);
    const double strength = qBound(0.0, parameters.value(QStringLiteral("strength"), 0.35).toDouble(), 1.0);
    const double speed = qBound(0.0, parameters.value(QStringLiteral("speed"), 1.0).toDouble(), 10.0);

    if (strength <= 0.0)
        return input;

    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    QImage out(src.size(), QImage::Format_RGBA8888);

    const int w = src.width();
    const int h = src.height();
    const double cx = centerX * static_cast<double>(w - 1);
    const double cy = centerY * static_cast<double>(h - 1);
    const double maxRadius = qSqrt(cx * cx + cy * cy);
    const double normalize = qMax(1.0, maxRadius);
    const double timeSeconds = static_cast<double>(timeUs) / static_cast<double>(drift::kUsPerSecond);
    const double waveRadius =
        speed > 0.0 ? std::fmod(timeSeconds * speed + radius, 1.0) : radius;
    const double maxDisplacement = strength * static_cast<double>(qMin(w, h)) * 0.25;

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double dx = static_cast<double>(x) - cx;
            const double dy = static_cast<double>(y) - cy;
            const double dist = qSqrt(dx * dx + dy * dy);
            if (dist < 1e-6) {
                out.setPixel(x, y, src.pixel(x, y));
                continue;
            }

            const double normalizedDist = dist / normalize;
            const double envelope = shockwaveRingEnvelope(normalizedDist, waveRadius, width);
            if (envelope <= 0.0) {
                out.setPixel(x, y, src.pixel(x, y));
                continue;
            }

            const double nx = dx / dist;
            const double ny = dy / dist;
            const double displacement = envelope * maxDisplacement;
            const int sx =
                qBound(0, static_cast<int>(static_cast<double>(x) - nx * displacement + 0.5), w - 1);
            const int sy =
                qBound(0, static_cast<int>(static_cast<double>(y) - ny * displacement + 0.5), h - 1);
            out.setPixel(x, y, samplePixel(src, sx, sy));
        }
    }
    return out;
}

QImage applyRgbChannelSplit(const QImage &input, double amount, double angleDeg)
{
    if (amount <= 0.0)
        return input;

    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    QImage out(src.size(), QImage::Format_RGBA8888);

    const double angleRad = qDegreesToRadians(angleDeg);
    const double dx = qCos(angleRad) * amount;
    const double dy = qSin(angleRad) * amount;

    for (int y = 0; y < src.height(); ++y) {
        for (int x = 0; x < src.width(); ++x) {
            const int rx = static_cast<int>(x + dx + (dx >= 0.0 ? 0.5 : -0.5));
            const int ry = static_cast<int>(y + dy + (dy >= 0.0 ? 0.5 : -0.5));
            const int bx = static_cast<int>(x - dx + (dx >= 0.0 ? 0.5 : -0.5));
            const int by = static_cast<int>(y - dy + (dy >= 0.0 ? 0.5 : -0.5));

            const QRgb rPx = samplePixel(src, rx, ry);
            const QRgb gPx = samplePixel(src, x, y);
            const QRgb bPx = samplePixel(src, bx, by);
            out.setPixel(x, y, qRgba(qRed(rPx), qGreen(gPx), qBlue(bPx), qAlpha(gPx)));
        }
    }
    return out;
}

QImage applyBlockDisplacement(const QImage &input, int seed, drift::TimeUs timeUs, double intensity,
                              double frequency, int blockSize, int shiftAmount)
{
    if (intensity <= 0.0 || shiftAmount <= 0 || frequency <= 0.0)
        return input;

    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    QImage out = src.copy();

    const int w = src.width();
    const int h = src.height();
    const int block = qBound(4, blockSize, 128);

    for (int blockY = 0, blockRow = 0; blockY < h; blockY += block, ++blockRow) {
        const int bh = qMin(block, h - blockY);
        for (int blockX = 0, blockCol = 0; blockX < w; blockX += block, ++blockCol) {
            const int bw = qMin(block, w - blockX);

            const uint32_t hash = blockGlitchHash(seed, timeUs, blockCol, blockRow);
            const double trigger = (hash & 0xFFFFu) / 65535.0;
            const double shiftNorm = ((hash >> 16) & 0xFFFFu) / 65535.0;

            if (trigger > frequency)
                continue;

            const int shift =
                static_cast<int>((shiftNorm * 2.0 - 1.0) * static_cast<double>(shiftAmount) * intensity);
            if (shift == 0)
                continue;

            for (int dy = 0; dy < bh; ++dy) {
                const int y = blockY + dy;
                for (int dx = 0; dx < bw; ++dx) {
                    const int x = blockX + dx;
                    const int sx = qBound(0, x - shift, w - 1);
                    out.setPixel(x, y, src.pixel(sx, y));
                }
            }
        }
    }
    return out;
}

QImage applyScanlineTearing(const QImage &input, int seed, drift::TimeUs timeUs, double jitter,
                            double lineStrength, int colorShift, double speed)
{
    if (jitter <= 0.0 && lineStrength <= 0.0 && colorShift <= 0)
        return input;

    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    QImage out(src.size(), QImage::Format_RGBA8888);

    const int w = src.width();
    const int h = src.height();
    const int maxJitterPx = static_cast<int>(jitter * 40.0);
    const drift::TimeUs animatedTimeUs =
        static_cast<drift::TimeUs>(static_cast<double>(timeUs) * qMax(speed, 0.001));

    for (int y = 0; y < h; ++y) {
        const uint32_t rowHash = seed < 0 ? scanlineHash(animatedTimeUs, y)
                                        : blockGlitchHash(seed, animatedTimeUs, y, 1);
        const double jitterNorm = (rowHash & 0xFFFFu) / 65535.0;
        const double tearNorm = ((rowHash >> 8) & 0xFFFFu) / 65535.0;

        int sourceY = y;
        if (jitter > 0.0 && tearNorm < jitter * 0.2) {
            const int tearOffset = static_cast<int>((tearNorm * 2.0 - 0.5) * jitter * 6.0);
            sourceY = qBound(0, y + tearOffset, h - 1);
        }

        const int horizontalOffset =
            maxJitterPx > 0
                ? static_cast<int>((jitterNorm * 2.0 - 1.0) * static_cast<double>(maxJitterPx))
                : 0;

        const uchar *srcLine = src.constScanLine(sourceY);
        uchar *outLine = out.scanLine(y);

        if (colorShift > 0) {
            for (int x = 0; x < w; ++x) {
                const int sx = qBound(0, x - horizontalOffset, w - 1);
                const int rx = qBound(0, sx + colorShift, w - 1);
                const int bx = qBound(0, sx - colorShift, w - 1);
                outLine[x * 4 + 0] = srcLine[rx * 4 + 0];
                outLine[x * 4 + 1] = srcLine[sx * 4 + 1];
                outLine[x * 4 + 2] = srcLine[bx * 4 + 2];
                outLine[x * 4 + 3] = srcLine[sx * 4 + 3];
            }
        } else if (horizontalOffset != 0) {
            for (int x = 0; x < w; ++x) {
                const int sx = qBound(0, x - horizontalOffset, w - 1);
                const int srcIdx = sx * 4;
                const int dstIdx = x * 4;
                outLine[dstIdx + 0] = srcLine[srcIdx + 0];
                outLine[dstIdx + 1] = srcLine[srcIdx + 1];
                outLine[dstIdx + 2] = srcLine[srcIdx + 2];
                outLine[dstIdx + 3] = srcLine[srcIdx + 3];
            }
        } else {
            std::memcpy(outLine, srcLine, static_cast<size_t>(w * 4));
        }

        if (lineStrength > 0.0 && (y & 1) == 0) {
            const double factor = 1.0 - lineStrength * 0.55;
            for (int x = 0; x < w; ++x) {
                const int idx = x * 4;
                outLine[idx + 0] = static_cast<uchar>(outLine[idx + 0] * factor);
                outLine[idx + 1] = static_cast<uchar>(outLine[idx + 1] * factor);
                outLine[idx + 2] = static_cast<uchar>(outLine[idx + 2] * factor);
            }
        }
    }
    return out;
}

void applySeededNoise(QImage &image, int seed, drift::TimeUs timeUs, double amount)
{
    if (amount <= 0.0)
        return;

    const double amp = amount * 36.0;
    for (int y = 0; y < image.height(); ++y) {
        uchar *line = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const int idx = x * 4;
            const uint32_t hash = blockGlitchHash(seed, timeUs, x, y);
            const double n = (static_cast<double>(hash & 0xFFu) / 127.5) - 1.0;
            line[idx + 0] = clampByte(line[idx + 0] + n * amp);
            line[idx + 1] = clampByte(line[idx + 1] + n * amp * 0.92);
            line[idx + 2] = clampByte(line[idx + 2] + n * amp * 0.85);
        }
    }
}

void applyVerticalJump(QImage &image, int jumpY)
{
    if (jumpY == 0)
        return;

    const QImage src = image.copy();
    const int w = image.width();
    const int h = image.height();
    for (int y = 0; y < h; ++y) {
        const int sy = qBound(0, y - jumpY, h - 1);
        std::memcpy(image.scanLine(y), src.constScanLine(sy), static_cast<size_t>(w * 4));
    }
}

void applyFlashOverlay(QImage &image, double strength)
{
    if (strength <= 0.0)
        return;

    const double flash = qBound(0.0, strength, 1.0);
    for (int y = 0; y < image.height(); ++y) {
        uchar *line = image.scanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const int idx = x * 4;
            line[idx + 0] = clampByte(line[idx + 0] + (255.0 - line[idx + 0]) * flash);
            line[idx + 1] = clampByte(line[idx + 1] + (255.0 - line[idx + 1]) * flash);
            line[idx + 2] = clampByte(line[idx + 2] + (255.0 - line[idx + 2]) * flash);
        }
    }
}

void applyDigitalFlashAndJump(QImage &image, int seed, drift::TimeUs timeUs, double flashAmount,
                              double jumpChance)
{
    const uint32_t flashHash = blockGlitchHash(seed, timeUs, 7, 3);
    const double flashTrigger = (flashHash & 0xFFFFu) / 65535.0;
    if (flashTrigger < flashAmount) {
        const double flashStrength =
            ((flashHash >> 16) & 0xFFFFu) / 65535.0 * flashAmount;
        applyFlashOverlay(image, flashStrength);
    }

    const uint32_t jumpHash = blockGlitchHash(seed, timeUs, 11, 5);
    const double jumpTrigger = (jumpHash & 0xFFFFu) / 65535.0;
    if (jumpTrigger >= jumpChance)
        return;

    const int jumpY =
        static_cast<int>(((jumpHash >> 16) & 0xFFFFu) / 65535.0 * 14.0 - 7.0);
    applyVerticalJump(image, jumpY);
}

QImage applyRgbSplit(const QImage &input, const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    double amount = parameters.value(QStringLiteral("amount"), 8.0).toDouble();
    const double angleDeg = parameters.value(QStringLiteral("angle"), 0.0).toDouble();
    const bool animated = parameters.value(QStringLiteral("animated"), false).toBool();
    const double speed = parameters.value(QStringLiteral("speed"), 1.0).toDouble();

    if (animated && speed > 0.0) {
        const drift::TimeUs periodUs =
            static_cast<drift::TimeUs>(static_cast<double>(drift::kUsPerSecond) / qMax(speed, 0.001));
        const drift::TimeUs phaseUs = periodUs > 0 ? (timeUs % periodUs) : 0;
        const double phase =
            static_cast<double>(phaseUs) / static_cast<double>(periodUs) * 2.0 * M_PI;
        amount *= 0.5 + 0.5 * qSin(phase);
    }

    return applyRgbChannelSplit(input, amount, angleDeg);
}

QImage applyBlockGlitch(const QImage &input, const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    const double intensity = qBound(0.0, parameters.value(QStringLiteral("intensity"), 0.35).toDouble(), 1.0);
    const int blockSize = qBound(4, parameters.value(QStringLiteral("blockSize"), 32).toInt(), 128);
    const int shiftAmount = qBound(0, parameters.value(QStringLiteral("shiftAmount"), 24).toInt(), 100);
    const double frequency = qBound(0.0, parameters.value(QStringLiteral("frequency"), 0.25).toDouble(), 1.0);
    const int seed = parameters.value(QStringLiteral("seed"), 1).toInt();

    return applyBlockDisplacement(input, seed, timeUs, intensity, frequency, blockSize, shiftAmount);
}

QImage applyScanlineGlitch(const QImage &input, const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    const double jitter = qBound(0.0, parameters.value(QStringLiteral("jitter"), 0.25).toDouble(), 1.0);
    const double lineStrength =
        qBound(0.0, parameters.value(QStringLiteral("lineStrength"), 0.35).toDouble(), 1.0);
    const int colorShift = qBound(0, parameters.value(QStringLiteral("colorShift"), 4).toInt(), 20);
    const double speed = qBound(0.0, parameters.value(QStringLiteral("speed"), 2.0).toDouble(), 10.0);

    return applyScanlineTearing(input, -1, timeUs, jitter, lineStrength, colorShift, speed);
}

QImage applyDigitalGlitch(const QImage &input, const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    const double intensity = qBound(0.0, parameters.value(QStringLiteral("intensity"), 0.5).toDouble(), 1.0);
    if (intensity <= 0.0)
        return input;

    const double frequency = qBound(0.0, parameters.value(QStringLiteral("frequency"), 0.35).toDouble(), 1.0);
    const double rgbAmount = qBound(0.0, parameters.value(QStringLiteral("rgbAmount"), 8.0).toDouble(), 40.0);
    const double blockAmount = qBound(0.0, parameters.value(QStringLiteral("blockAmount"), 0.4).toDouble(), 1.0);
    const double flashAmount = qBound(0.0, parameters.value(QStringLiteral("flashAmount"), 0.15).toDouble(), 1.0);
    const int seed = parameters.value(QStringLiteral("seed"), 1).toInt();

    QImage result = input.convertToFormat(QImage::Format_RGBA8888);

    if (blockAmount > 0.0 && frequency > 0.0) {
        result = applyBlockDisplacement(result, seed, timeUs, blockAmount * intensity, frequency, 24,
                                        static_cast<int>(40.0 * intensity));
    }

    if (frequency > 0.0) {
        result = applyScanlineTearing(result, seed, timeUs, frequency * intensity * 0.75,
                                      frequency * intensity * 0.3,
                                      static_cast<int>(rgbAmount * 0.35 * intensity), 2.0);
    }

    if (rgbAmount > 0.0) {
        const uint32_t angleHash = blockGlitchHash(seed, timeUs, 3, 3);
        const double angleDeg = (angleHash & 0xFFFFu) / 65535.0 * 360.0;
        result = applyRgbChannelSplit(result, rgbAmount * intensity, angleDeg);
    }

    applySeededNoise(result, seed, timeUs, frequency * intensity * 0.5);
    applyDigitalFlashAndJump(result, seed, timeUs, flashAmount * intensity, frequency * 0.2);

    return result;
}

QImage applyVhsCrt(const QImage &input, const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    const double scanlines = qBound(0.0, parameters.value(QStringLiteral("scanlines"), 0.35).toDouble(), 1.0);
    const double noise = qBound(0.0, parameters.value(QStringLiteral("noise"), 0.25).toDouble(), 1.0);
    const int colorBleed = qBound(0, parameters.value(QStringLiteral("colorBleed"), 3).toInt(), 20);
    const double distortion = qBound(0.0, parameters.value(QStringLiteral("distortion"), 0.2).toDouble(), 1.0);
    const double vignette = qBound(0.0, parameters.value(QStringLiteral("vignette"), 0.25).toDouble(), 1.0);
    const double desaturation =
        qBound(0.0, parameters.value(QStringLiteral("desaturation"), 0.15).toDouble(), 1.0);

    if (scanlines <= 0.0 && noise <= 0.0 && colorBleed <= 0 && distortion <= 0.0 && vignette <= 0.0
        && desaturation <= 0.0)
        return input;

    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    QImage out(src.size(), QImage::Format_RGBA8888);

    const int w = src.width();
    const int h = src.height();
    const double cx = w * 0.5;
    const double cy = h * 0.5;
    const double invCx = cx > 0.0 ? 1.0 / cx : 0.0;
    const double invCy = cy > 0.0 ? 1.0 / cy : 0.0;

    const double timePhase = static_cast<double>(timeUs) / static_cast<double>(drift::kUsPerSecond) * 3.0;
    const int maxDistortPx = static_cast<int>(distortion * 14.0);
    const double noiseAmp = noise * 42.0;

    for (int y = 0; y < h; ++y) {
        const double rowWave =
            distortion > 0.0
                ? qSin((static_cast<double>(y) / qMax(1.0, static_cast<double>(h))) * 24.0 + timePhase)
                      * static_cast<double>(maxDistortPx)
                : 0.0;
        const double scanFactor =
            scanlines > 0.0 && (y & 1) == 0 ? (1.0 - scanlines * 0.55) : 1.0;
        const uchar *srcLine = src.constScanLine(y);
        uchar *outLine = out.scanLine(y);

        for (int x = 0; x < w; ++x) {
            const int baseX = qBound(0, static_cast<int>(static_cast<double>(x) + rowWave + 0.5), w - 1);
            const int rx = qBound(0, baseX + colorBleed, w - 1);
            const int bx = qBound(0, baseX - colorBleed, w - 1);

            double r = srcLine[rx * 4 + 0];
            double g = srcLine[baseX * 4 + 1];
            double b = srcLine[bx * 4 + 2];
            double a = srcLine[baseX * 4 + 3];

            if (desaturation > 0.0) {
                const double lum = 0.299 * r + 0.587 * g + 0.114 * b;
                r = r * (1.0 - desaturation) + lum * desaturation;
                g = g * (1.0 - desaturation) + lum * desaturation;
                b = b * (1.0 - desaturation) + lum * desaturation;
            }

            r *= scanFactor;
            g *= scanFactor;
            b *= scanFactor;

            if (vignette > 0.0) {
                const double nx = (static_cast<double>(x) - cx) * invCx;
                const double ny = (static_cast<double>(y) - cy) * invCy;
                const double vig = 1.0 - vignette * (nx * nx + ny * ny) * 0.85;
                const double vigClamped = qBound(0.0, vig, 1.0);
                r *= vigClamped;
                g *= vigClamped;
                b *= vigClamped;
            }

            if (noiseAmp > 0.0) {
                const uint32_t hash = pixelHash(timeUs, x, y);
                const double n = (static_cast<double>(hash & 0xFFu) / 127.5) - 1.0;
                r += n * noiseAmp;
                g += n * noiseAmp * 0.92;
                b += n * noiseAmp * 0.85;
            }

            outLine[x * 4 + 0] = clampByte(r);
            outLine[x * 4 + 1] = clampByte(g);
            outLine[x * 4 + 2] = clampByte(b);
            outLine[x * 4 + 3] = clampByte(a);
        }
    }
    return out;
}

QImage applyEdgeNeon(const QImage &input, const QMap<QString, QVariant> &parameters)
{
    const double threshold = qBound(0.0, parameters.value(QStringLiteral("threshold"), 0.25).toDouble(), 1.0);
    const double intensity = parameters.value(QStringLiteral("intensity"), 0.8).toDouble();
    const int radius = qBound(1, parameters.value(QStringLiteral("radius"), 4).toInt(), 20);
    const QColor color = parseEffectColor(parameters.value(QStringLiteral("color"), QStringLiteral("#00ffff")));

    if (intensity <= 0.0)
        return input;

    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    const QImage edgeMask = sobelEdgeMask(src, threshold);

    bool hasEdges = false;
    for (int y = 0; y < edgeMask.height() && !hasEdges; ++y) {
        const uchar *line = edgeMask.constScanLine(y);
        for (int x = 0; x < edgeMask.width(); ++x) {
            if (line[x * 4] > 0) {
                hasEdges = true;
                break;
            }
        }
    }
    if (!hasEdges)
        return input;

    const QImage blurredMask = boxBlur(edgeMask, radius);
    const double blend = qBound(0.0, intensity, 2.0);
    const double colorAlpha = static_cast<double>(color.alpha()) / 255.0;

    QImage out = src.copy();
    for (int y = 0; y < out.height(); ++y) {
        const uchar *maskLine = blurredMask.constScanLine(y);
        const uchar *srcLine = src.constScanLine(y);
        uchar *outLine = out.scanLine(y);
        for (int x = 0; x < out.width(); ++x) {
            const int idx = x * 4;
            const double edge = static_cast<double>(maskLine[idx]) / 255.0;
            const double glow = edge * blend * colorAlpha;
            outLine[idx + 0] = clampByte(srcLine[idx + 0] + static_cast<double>(color.red()) * glow);
            outLine[idx + 1] = clampByte(srcLine[idx + 1] + static_cast<double>(color.green()) * glow);
            outLine[idx + 2] = clampByte(srcLine[idx + 2] + static_cast<double>(color.blue()) * glow);
            outLine[idx + 3] = srcLine[idx + 3];
        }
    }
    return out;
}

QString resolveLeakPosition(const QString &position, int seed, drift::TimeUs timeUs)
{
    const QString normalized = position.trimmed().toLower();
    if (normalized != QStringLiteral("random"))
        return normalized.isEmpty() ? QStringLiteral("left") : normalized;

    static const QString kPositions[] = {QStringLiteral("left"), QStringLiteral("right"),
                                         QStringLiteral("top"), QStringLiteral("bottom")};
    const uint32_t hash = blockGlitchHash(seed, timeUs, 2, 4);
    return kPositions[hash % 4];
}

double positionalLeakGradient(double nx, double ny, const QString &position)
{
    if (position == QStringLiteral("right"))
        return qPow(nx, 1.7);
    if (position == QStringLiteral("top"))
        return qPow(1.0 - ny, 1.7);
    if (position == QStringLiteral("bottom"))
        return qPow(ny, 1.7);
    return qPow(1.0 - nx, 1.7);
}

double radialLeakBlob(double nx, double ny, const QString &position)
{
    double cx = 0.05;
    double cy = 0.5;
    if (position == QStringLiteral("right")) {
        cx = 0.95;
        cy = 0.5;
    } else if (position == QStringLiteral("top")) {
        cx = 0.5;
        cy = 0.05;
    } else if (position == QStringLiteral("bottom")) {
        cx = 0.5;
        cy = 0.95;
    }

    const double dx = nx - cx;
    const double dy = ny - cy;
    const double dist = qSqrt(dx * dx + dy * dy);
    return qMax(0.0, 1.0 - dist / 0.75);
}

QImage applyFilmBurn(const QImage &input, const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    const double intensity = qBound(0.0, parameters.value(QStringLiteral("intensity"), 0.45).toDouble(), 1.0);
    if (intensity <= 0.0)
        return input;

    const double warmth = qBound(0.0, parameters.value(QStringLiteral("warmth"), 0.8).toDouble(), 1.0);
    const double flicker = qBound(0.0, parameters.value(QStringLiteral("flicker"), 0.35).toDouble(), 1.0);
    const int seed = parameters.value(QStringLiteral("seed"), 1).toInt();
    const QString position =
        resolveLeakPosition(parameters.value(QStringLiteral("position"), QStringLiteral("left")).toString(),
                            seed, timeUs);

    const QImage src = input.convertToFormat(QImage::Format_RGBA8888);
    QImage out = src.copy();

    const int w = out.width();
    const int h = out.height();
    const double invW = w > 1 ? 1.0 / static_cast<double>(w - 1) : 0.0;
    const double invH = h > 1 ? 1.0 / static_cast<double>(h - 1) : 0.0;

    const uint32_t flickerHash = blockGlitchHash(seed, timeUs, 5, 9);
    const double flickerNorm = (flickerHash & 0xFFFFu) / 65535.0;
    const double flickerMod = 1.0 - flicker + flicker * flickerNorm;

    const double leakR = 255.0;
    const double leakG = 100.0 + warmth * 155.0;
    const double leakB = 15.0 + warmth * 120.0;

    for (int y = 0; y < h; ++y) {
        const double ny = static_cast<double>(y) * invH;
        uchar *outLine = out.scanLine(y);
        const uchar *srcLine = src.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const double nx = static_cast<double>(x) * invW;
            const double gradient = positionalLeakGradient(nx, ny, position);
            const double blob = radialLeakBlob(nx, ny, position);
            const uint32_t noiseHash = blockGlitchHash(seed, timeUs, x, y);
            const double noise = 0.65 + 0.35 * (static_cast<double>(noiseHash & 0xFFu) / 255.0);

            const double leak =
                qBound(0.0, qMax(gradient, blob * 0.85) * noise * flickerMod * intensity, 1.0);
            const int idx = x * 4;
            outLine[idx + 0] = clampByte(srcLine[idx + 0] + leakR * leak);
            outLine[idx + 1] = clampByte(srcLine[idx + 1] + leakG * leak);
            outLine[idx + 2] = clampByte(srcLine[idx + 2] + leakB * leak);
            outLine[idx + 3] = srcLine[idx + 3];
        }
    }
    return out;
}

} // namespace

QImage CompositorEffects::apply(const QString &presetId, const QImage &input,
                                const QMap<QString, QVariant> &parameters, drift::TimeUs timeUs)
{
    if (input.isNull())
        return input;

    if (presetId == QStringLiteral("stylize.bloom"))
        return applyBloom(input, parameters);
    if (presetId == QStringLiteral("bloom_glow"))
        return applyBloomGlow(input, parameters);
    if (presetId == QStringLiteral("edge_neon"))
        return applyEdgeNeon(input, parameters);
    if (presetId == QStringLiteral("stylize.ripple"))
        return applyRipple(input, parameters);
    if (presetId == QStringLiteral("ripple_water"))
        return applyRippleWater(input, parameters, timeUs);
    if (presetId == QStringLiteral("shockwave_pulse"))
        return applyShockwavePulse(input, parameters, timeUs);
    if (presetId == QStringLiteral("rgb_split"))
        return applyRgbSplit(input, parameters, timeUs);
    if (presetId == QStringLiteral("block_glitch"))
        return applyBlockGlitch(input, parameters, timeUs);
    if (presetId == QStringLiteral("scanline_glitch"))
        return applyScanlineGlitch(input, parameters, timeUs);
    if (presetId == QStringLiteral("digital_glitch"))
        return applyDigitalGlitch(input, parameters, timeUs);
    if (presetId == QStringLiteral("vhs_crt"))
        return applyVhsCrt(input, parameters, timeUs);
    if (presetId == QStringLiteral("film_burn"))
        return applyFilmBurn(input, parameters, timeUs);

    return input;
}
