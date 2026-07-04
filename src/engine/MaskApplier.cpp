#include "MaskApplier.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {

QPainterPath regularPolygonPath(const QPointF &center, double radius, int sides, double rotationDeg)
{
    QPainterPath path;
    const double start = qDegreesToRadians(rotationDeg - 90.0);
    for (int i = 0; i < sides; ++i) {
        const double angle = start + i * 2.0 * M_PI / sides;
        const QPointF pt(center.x() + radius * qCos(angle), center.y() + radius * qSin(angle));
        if (i == 0)
            path.moveTo(pt);
        else
            path.lineTo(pt);
    }
    path.closeSubpath();
    return path;
}

QPainterPath heartPath(const QPointF &center, double radius)
{
    QPainterPath path;
    const QPointF c = center;
    const double r = radius;
    path.moveTo(c.x(), c.y() + r * 0.35);
    path.cubicTo(c.x() - r * 0.95, c.y() - r * 0.35, c.x() - r * 0.55, c.y() - r * 1.05, c.x(),
                 c.y() - r * 0.45);
    path.cubicTo(c.x() + r * 0.55, c.y() - r * 1.05, c.x() + r * 0.95, c.y() - r * 0.35, c.x(),
                 c.y() + r * 0.35);
    path.closeSubpath();
    return path;
}

QPainterPath maskPath(const drift::Mask &mask, int canvasWidth, int canvasHeight)
{
    const QPointF center(mask.x * canvasWidth, mask.y * canvasHeight);
    const double halfW = qMax(1.0, mask.w * canvasWidth * 0.5);
    const double halfH = qMax(1.0, mask.h * canvasHeight * 0.5);

    switch (mask.shape) {
    case drift::MaskShape::Rectangle: {
        QPainterPath path;
        path.addRect(QRectF(center.x() - halfW, center.y() - halfH, halfW * 2.0, halfH * 2.0));
        return path;
    }
    case drift::MaskShape::Ellipse: {
        QPainterPath path;
        path.addEllipse(center, halfW, halfH);
        return path;
    }
    case drift::MaskShape::Star:
        return regularPolygonPath(center, qMin(halfW, halfH), 5, mask.rotation);
    case drift::MaskShape::Heart:
        return heartPath(center, qMin(halfW, halfH));
    case drift::MaskShape::Bars: {
        const double barH = halfH;
        QPainterPath path;
        path.addRect(QRectF(0, 0, canvasWidth, barH));
        path.addRect(QRectF(0, canvasHeight - barH, canvasWidth, barH));
        return path;
    }
    case drift::MaskShape::Freeform: {
        QPainterPath path;
        if (mask.points.isEmpty())
            return path;
        for (int i = 0; i < mask.points.size(); ++i) {
            const QPointF pt(mask.points.at(i).x() * canvasWidth, mask.points.at(i).y() * canvasHeight);
            if (i == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        path.closeSubpath();
        return path;
    }
    case drift::MaskShape::Matte:
        // Raster, not parametric: the coverage map is decoded per frame in FrameCompositor and
        // rides on GpuLayer::matte. There is no path to rasterize.
        break;
    case drift::MaskShape::None:
        break;
    }
    return {};
}

QImage blurAlpha(const QImage &alpha, int radius)
{
    if (radius <= 0 || alpha.isNull())
        return alpha;

    QImage out = alpha;
    const int passes = qBound(1, radius / 2, 8);
    for (int pass = 0; pass < passes; ++pass) {
        QImage blurred(out.size(), QImage::Format_Grayscale8);
        for (int y = 0; y < out.height(); ++y) {
            auto *line = blurred.scanLine(y);
            for (int x = 0; x < out.width(); ++x) {
                int sum = 0;
                int count = 0;
                for (int dy = -radius; dy <= radius; ++dy) {
                    const int sy = qBound(0, y + dy, out.height() - 1);
                    for (int dx = -radius; dx <= radius; ++dx) {
                        const int sx = qBound(0, x + dx, out.width() - 1);
                        sum += qGray(out.pixel(sx, sy));
                        ++count;
                    }
                }
                line[x] = static_cast<uchar>(sum / qMax(1, count));
            }
        }
        out = blurred;
    }
    return out;
}

} // namespace

namespace drift {

QImage maskAlphaMap(const Mask &mask, int canvasWidth, int canvasHeight)
{
    if (mask.shape == MaskShape::None || mask.shape == MaskShape::Matte || canvasWidth <= 0
        || canvasHeight <= 0)
        return {};

    QImage alpha(canvasWidth, canvasHeight, QImage::Format_Grayscale8);
    alpha.fill(mask.invert ? 255 : 0);

    QPainter mp(&alpha);
    mp.setRenderHint(QPainter::Antialiasing);
    mp.setBrush(mask.invert ? Qt::black : Qt::white);
    mp.setPen(Qt::NoPen);

    QPainterPath path = maskPath(mask, canvasWidth, canvasHeight);
    if (!path.isEmpty()) {
        const QPointF center(mask.x * canvasWidth, mask.y * canvasHeight);
        if (!qFuzzyIsNull(mask.rotation) && mask.shape != MaskShape::Bars && mask.shape != MaskShape::Freeform) {
            QTransform transform;
            transform.translate(center.x(), center.y());
            transform.rotate(mask.rotation);
            transform.translate(-center.x(), -center.y());
            path = transform.map(path);
        }
        mp.drawPath(path);
    }
    mp.end();

    if (mask.feather > 0.0)
        alpha = blurAlpha(alpha, qMax(1, static_cast<int>(mask.feather)));

    return alpha;
}

QImage applyMask(const QImage &frame, const Mask &mask, int canvasWidth, int canvasHeight)
{
    if (mask.shape == MaskShape::None || mask.shape == MaskShape::Matte || frame.isNull())
        return frame;

    const QImage alpha = maskAlphaMap(mask, canvasWidth, canvasHeight);
    if (alpha.isNull())
        return frame;

  QImage rgba = frame.convertToFormat(QImage::Format_RGBA8888);
    if (rgba.size() != alpha.size())
        rgba = rgba.scaled(canvasWidth, canvasHeight, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage out(rgba.size(), QImage::Format_RGBA8888);
    for (int y = 0; y < rgba.height(); ++y) {
        const QRgb *src = reinterpret_cast<const QRgb *>(rgba.constScanLine(y));
        QRgb *dst = reinterpret_cast<QRgb *>(out.scanLine(y));
        const uchar *alphaLine = alpha.constScanLine(y);
        for (int x = 0; x < rgba.width(); ++x) {
            const int a = qAlpha(src[x]) * alphaLine[x] / 255;
            dst[x] = qRgba(qRed(src[x]), qGreen(src[x]), qBlue(src[x]), a);
        }
    }
    return out;
}

} // namespace drift
