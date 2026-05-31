#include "TextRaster.h"

#include "FontCatalog.h"

#include <QEasingCurve>
#include <QFontMetricsF>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QTextLayout>
#include <QTextOption>

#include <cmath>
#include <limits>

namespace {

QMutex g_cacheMutex;
QHash<quint64, QImage> g_cache;
constexpr int kMaxCacheEntries = 48;

quint64 rasterKey(const QString &text, const drift::TextStyle &s, int imageW, int imageH,
                  double renderScale)
{
    // Deliberately excludes the animation and the time: motion is applied to the layer, not the
    // raster, so one texture serves every frame of an entrance or exit.
    return qHashMulti(0, text, s.fontFamily, s.pixelSize, s.fontWeight, s.italic, s.color.rgba(),
                      static_cast<int>(s.align), static_cast<int>(s.valign), s.wordWrap, s.lineHeight,
                      s.letterSpacing, s.outlineWidth, s.outlineColor.rgba(), s.shadowEnabled,
                      s.shadowOffsetX, s.shadowOffsetY, s.shadowBlur, s.shadowOpacity,
                      s.shadowColor.rgba(), s.boxEnabled, s.boxColor.rgba(), s.boxPadding, s.boxRadius,
                      imageW, imageH, qRound(renderScale * 1000.0));
}

// Separable box blur over a premultiplied image. Three passes approximate a gaussian well enough
// for a drop shadow, and premultiplied is what keeps transparent pixels from dragging the glyph
// edges toward black.
void blurRows(const QImage &src, QImage &dst, int radius)
{
    const int w = src.width();
    const int h = src.height();
    const int span = radius * 2 + 1;

    for (int y = 0; y < h; ++y) {
        const QRgb *s = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        QRgb *d = reinterpret_cast<QRgb *>(dst.scanLine(y));

        int a = 0, r = 0, g = 0, b = 0;
        for (int i = -radius; i <= radius; ++i) {
            const QRgb px = s[qBound(0, i, w - 1)];
            a += qAlpha(px); r += qRed(px); g += qGreen(px); b += qBlue(px);
        }
        for (int x = 0; x < w; ++x) {
            d[x] = qRgba(r / span, g / span, b / span, a / span);
            const QRgb out = s[qBound(0, x - radius, w - 1)];
            const QRgb in = s[qBound(0, x + radius + 1, w - 1)];
            a += qAlpha(in) - qAlpha(out); r += qRed(in) - qRed(out);
            g += qGreen(in) - qGreen(out); b += qBlue(in) - qBlue(out);
        }
    }
}

void blurColumns(const QImage &src, QImage &dst, int radius)
{
    const int w = src.width();
    const int h = src.height();
    const int span = radius * 2 + 1;

    const int stride = src.bytesPerLine() / 4;
    const QRgb *s = reinterpret_cast<const QRgb *>(src.constBits());
    const int dstStride = dst.bytesPerLine() / 4;
    QRgb *d = reinterpret_cast<QRgb *>(dst.bits());

    for (int x = 0; x < w; ++x) {
        int a = 0, r = 0, g = 0, b = 0;
        for (int i = -radius; i <= radius; ++i) {
            const QRgb px = s[qBound(0, i, h - 1) * stride + x];
            a += qAlpha(px); r += qRed(px); g += qGreen(px); b += qBlue(px);
        }
        for (int y = 0; y < h; ++y) {
            d[y * dstStride + x] = qRgba(r / span, g / span, b / span, a / span);
            const QRgb out = s[qBound(0, y - radius, h - 1) * stride + x];
            const QRgb in = s[qBound(0, y + radius + 1, h - 1) * stride + x];
            a += qAlpha(in) - qAlpha(out); r += qRed(in) - qRed(out);
            g += qGreen(in) - qGreen(out); b += qBlue(in) - qBlue(out);
        }
    }
}

void blurPremultiplied(QImage &image, int radius)
{
    if (radius < 1 || image.isNull())
        return;
    QImage scratch(image.size(), QImage::Format_ARGB32_Premultiplied);
    for (int pass = 0; pass < 3; ++pass) {
        blurRows(image, scratch, radius);
        blurColumns(scratch, image, radius);
    }
}

// Lay the text out and return it as a single path, plus the block's ink bounds.
QPainterPath layoutTextPath(const QString &text, const drift::TextStyle &style, const QFont &font,
                            double wrapWidth, double blockHeight)
{
    QString source = text;
    source.replace(QLatin1Char('\n'), QChar::LineSeparator); // QTextLayout breaks on the separator

    QTextOption option;
    option.setWrapMode(style.wordWrap ? QTextOption::WordWrap : QTextOption::NoWrap);

    QTextLayout layout(source, font);
    layout.setTextOption(option);

    const QFontMetricsF metrics(font);
    const double lineStep = metrics.lineSpacing() * qMax(0.1, style.lineHeight);
    const double effectiveWrap = style.wordWrap ? qMax(1.0, wrapWidth) : std::numeric_limits<double>::max();

    struct Line { int start; int length; double x; double y; };
    QList<Line> lines;

    layout.beginLayout();
    double y = 0.0;
    forever {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(effectiveWrap);

        // Alignment is applied here rather than through QTextOption so it stays correct with
        // NoWrap, where the natural text can be wider than the box.
        const double natural = line.naturalTextWidth();
        double x = 0.0;
        if (style.align == drift::TextAlign::Center)
            x = (wrapWidth - natural) * 0.5;
        else if (style.align == drift::TextAlign::Right)
            x = wrapWidth - natural;

        lines.append({line.textStart(), line.textLength(), x, y});
        y += lineStep;
    }
    layout.endLayout();

    if (lines.isEmpty())
        return {};

    double blockTop = 0.0;
    if (style.valign == drift::TextVAlign::Middle)
        blockTop = (blockHeight - y) * 0.5;
    else if (style.valign == drift::TextVAlign::Bottom)
        blockTop = blockHeight - y;

    const double ascent = metrics.ascent();
    QPainterPath path;
    // Winding, not the odd-even default: at heavy weights adjacent glyph contours overlap, and
    // odd-even punches those overlaps out as holes.
    path.setFillRule(Qt::WindingFill);
    for (const Line &line : lines) {
        QString slice = source.mid(line.start, line.length);
        slice.remove(QChar::LineSeparator);
        if (slice.trimmed().isEmpty())
            continue;
        path.addText(line.x, blockTop + line.y + ascent, font, slice);
    }
    return path;
}

QEasingCurve::Type easingType(drift::TextEase ease)
{
    switch (ease) {
    case drift::TextEase::Linear:
        return QEasingCurve::Linear;
    case drift::TextEase::EaseInOut:
        return QEasingCurve::InOutQuad;
    case drift::TextEase::Back:
        return QEasingCurve::OutBack;
    case drift::TextEase::EaseOut:
        return QEasingCurve::OutCubic;
    }
    return QEasingCurve::OutCubic;
}

// `settled` runs 0 (fully out) to 1 (fully in place). `entering` flips the slide direction: an
// entrance arrives from the opposite side, an exit departs toward the named one.
void applyAnimation(const drift::TextAnimation &anim, double settled, bool entering,
                    const QRectF &layoutRect, double renderScale, TextAnimSample *out)
{
    if (anim.kind == drift::TextAnimKind::None)
        return;

    const double a = QEasingCurve(easingType(anim.ease)).valueForProgress(qBound(0.0, settled, 1.0));
    const double away = 1.0 - a; // how far from settled
    const double travelX = 0.35 * layoutRect.width();
    const double travelY = 0.35 * layoutRect.height();
    const double sign = entering ? 1.0 : -1.0;

    switch (anim.kind) {
    case drift::TextAnimKind::None:
        break;
    case drift::TextAnimKind::Fade:
        out->opacity *= a;
        break;
    case drift::TextAnimKind::SlideUp:
        out->dy += sign * away * travelY;
        out->opacity *= a;
        break;
    case drift::TextAnimKind::SlideDown:
        out->dy -= sign * away * travelY;
        out->opacity *= a;
        break;
    case drift::TextAnimKind::SlideLeft:
        out->dx += sign * away * travelX;
        out->opacity *= a;
        break;
    case drift::TextAnimKind::SlideRight:
        out->dx -= sign * away * travelX;
        out->opacity *= a;
        break;
    case drift::TextAnimKind::Pop:
        out->scale *= 0.6 + 0.4 * a; // Back easing overshoots past 1 here, which is the bounce
        out->opacity *= qBound(0.0, a, 1.0);
        break;
    case drift::TextAnimKind::Blur:
        out->blurPx = qMax(out->blurPx, away * kTextBlurMaxPx * renderScale);
        out->opacity *= qBound(0.0, a, 1.0);
        break;
    }
}

double bleedFor(const drift::TextStyle &style)
{
    double bleed = style.outlineWidth;
    if (style.shadowEnabled)
        bleed += style.shadowBlur * 2.0 + qMax(std::abs(style.shadowOffsetX), std::abs(style.shadowOffsetY));
    if (style.boxEnabled)
        bleed += style.boxPadding + style.boxRadius;
    if (style.animIn.kind == drift::TextAnimKind::Blur || style.animOut.kind == drift::TextAnimKind::Blur)
        bleed += kTextBlurMaxPx;
    return bleed;
}

} // namespace

TextRasterResult rasterizeText(const drift::Clip &clip, const QString &text, const QRectF &layoutRect,
                               double renderScale)
{
    if (text.isEmpty() || layoutRect.width() < 1.0 || layoutRect.height() < 1.0)
        return {};

    const drift::TextStyle &style = clip.textStyle;

    // The bleed is derived from style constants — never from the current animation — so the image
    // size holds still while an entrance plays and the cached raster stays usable.
    const double bleed = std::ceil(bleedFor(style) * renderScale) + 2.0;
    const int imageW = qMax(1, qRound(layoutRect.width() + bleed * 2.0));
    const int imageH = qMax(1, qRound(layoutRect.height() + bleed * 2.0));

    TextRasterResult result;
    result.rect = QRectF(layoutRect.x() - bleed, layoutRect.y() - bleed, imageW, imageH);

    const quint64 key = rasterKey(text, style, imageW, imageH, renderScale);
    {
        QMutexLocker lock(&g_cacheMutex);
        const auto it = g_cache.constFind(key);
        if (it != g_cache.constEnd()) {
            result.image = it.value();
            return result;
        }
    }

    const QFont font = fontForStyle(style, qRound(style.pixelSize * renderScale));
    QFont spaced = font;
    if (!qFuzzyIsNull(style.letterSpacing))
        spaced.setLetterSpacing(QFont::AbsoluteSpacing, style.letterSpacing * renderScale);

    QPainterPath path =
        layoutTextPath(text, style, spaced, layoutRect.width(), layoutRect.height());
    if (path.isEmpty())
        return {};
    path.translate(bleed, bleed);

    QImage image(imageW, imageH, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    // The shape the stroke and shadow both use: glyphs grown by the outline.
    QPainterPath shape = path;
    if (style.outlineWidth > 0.0) {
        QPainterPathStroker stroker;
        // The stroker is centred on the path, so doubling the width yields an outline that grows
        // entirely outward and leaves the glyph shape intact.
        stroker.setWidth(style.outlineWidth * renderScale * 2.0);
        stroker.setJoinStyle(Qt::RoundJoin);
        stroker.setCapStyle(Qt::RoundCap);
        shape = stroker.createStroke(path).united(path);
        shape.setFillRule(Qt::WindingFill);
    }

    if (style.boxEnabled) {
        const double padding = style.boxPadding * renderScale;
        const QRectF box = shape.boundingRect().adjusted(-padding, -padding, padding, padding);
        p.setPen(Qt::NoPen);
        p.setBrush(style.boxColor);
        p.drawRoundedRect(box, style.boxRadius * renderScale, style.boxRadius * renderScale);
    }

    if (style.shadowEnabled && style.shadowOpacity > 0.0) {
        QImage shadow(imageW, imageH, QImage::Format_ARGB32_Premultiplied);
        shadow.fill(Qt::transparent);
        QPainter sp(&shadow);
        sp.setRenderHint(QPainter::Antialiasing);
        sp.fillPath(shape, style.shadowColor);
        sp.end();

        blurPremultiplied(shadow, qRound(style.shadowBlur * renderScale));

        p.setOpacity(qBound(0.0, style.shadowOpacity, 1.0));
        p.drawImage(QPointF(style.shadowOffsetX * renderScale, style.shadowOffsetY * renderScale), shadow);
        p.setOpacity(1.0);
    }

    if (style.outlineWidth > 0.0)
        p.fillPath(shape, style.outlineColor); // behind the glyphs, so it never eats into them
    p.fillPath(path, style.color);
    p.end();

    {
        QMutexLocker lock(&g_cacheMutex);
        if (g_cache.size() >= kMaxCacheEntries)
            g_cache.clear();
        g_cache.insert(key, image);
    }

    result.image = image;
    return result;
}

TextRasterResult rasterizeText(const drift::Clip &clip, const QRectF &layoutRect, double renderScale)
{
    const QString text = clip.textContent.isEmpty() ? clip.name : clip.textContent;
    return rasterizeText(clip, text, layoutRect, renderScale);
}

TextAnimSample sampleTextAnimation(const drift::Clip &clip, drift::TimeUs timelineUs,
                                   const QRectF &layoutRect, double renderScale)
{
    const drift::TextStyle &style = clip.textStyle;
    TextAnimSample sample;

    const drift::TimeUs elapsed = timelineUs - clip.timelineStart;
    const drift::TimeUs remaining = clip.timelineDuration - elapsed;

    if (style.animIn.kind != drift::TextAnimKind::None && style.animIn.durationUs > 0) {
        const double settled = static_cast<double>(elapsed) / static_cast<double>(style.animIn.durationUs);
        applyAnimation(style.animIn, settled, true, layoutRect, renderScale, &sample);
    }
    if (style.animOut.kind != drift::TextAnimKind::None && style.animOut.durationUs > 0) {
        const double settled = static_cast<double>(remaining) / static_cast<double>(style.animOut.durationUs);
        applyAnimation(style.animOut, settled, false, layoutRect, renderScale, &sample);
    }

    sample.opacity = qBound(0.0, sample.opacity, 1.0);
    return sample;
}
