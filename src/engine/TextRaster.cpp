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
#include <QtMath>

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
    case drift::TextAnimKind::Typewriter:
        // Hard binary reveal: a span is off until the playhead reaches its staggered start
        // (settled >= 0), then fully on. Duration is irrelevant, which is what makes it snap.
        out->opacity *= (settled >= 0.0 ? 1.0 : 0.0);
        break;
    case drift::TextAnimKind::Rise:
        out->dy += sign * away * travelY;
        out->scale *= 0.9 + 0.1 * a;
        out->opacity *= a;
        break;
    case drift::TextAnimKind::Bounce: {
        const double b = QEasingCurve(QEasingCurve::OutBounce).valueForProgress(qBound(0.0, settled, 1.0));
        out->dy += sign * (1.0 - b) * travelY;
        out->opacity *= qBound(0.0, settled * 4.0, 1.0);
        break;
    }
    case drift::TextAnimKind::Wave:
        break; // continuous; applied by applyWave in the samplers, not from a settle progress
    }
}

// Continuous vertical oscillation. Unlike the entrance/exit kinds it never settles — it bobs for the
// whole clip, phase-shifted per span so a wave travels across the characters.
void applyWave(drift::TimeUs clipLocalUs, int spanIndex, const QRectF &layoutRect, double renderScale,
               TextAnimSample *out)
{
    const double t = static_cast<double>(clipLocalUs) / 1'000'000.0;
    const double amp = qMin(layoutRect.height() * 0.12, 36.0 * renderScale);
    const double freq = 2.0 * M_PI * 1.1; // ~1.1 Hz
    const double phase = 0.6 * spanIndex;
    out->dy += amp * std::sin(t * freq + phase);
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

// Grow the glyph path outward by the outline width. Shared by the box background (which sizes to its
// bounds) and the fill, so both agree on the shape's extent.
QPainterPath outlineShape(const QPainterPath &path, const drift::TextStyle &style, double renderScale)
{
    if (style.outlineWidth <= 0.0)
        return path;
    QPainterPathStroker stroker;
    // The stroker is centred on the path, so doubling the width yields an outline that grows entirely
    // outward and leaves the glyph shape intact.
    stroker.setWidth(style.outlineWidth * renderScale * 2.0);
    stroker.setJoinStyle(Qt::RoundJoin);
    stroker.setCapStyle(Qt::RoundCap);
    QPainterPath shape = stroker.createStroke(path).united(path);
    shape.setFillRule(Qt::WindingFill);
    return shape;
}

// Draw the shadow, outline and glyph fill for a laid-out path. The box background is drawn by the
// caller (it is per-block, not per-span), so this stays reusable for both the whole-layer raster and
// the per-span reveal rasters.
void paintGlyphs(QPainter &p, const QPainterPath &path, const QPainterPath &shape,
                 const drift::TextStyle &style, double renderScale, const QSize &imageSize)
{
    if (style.shadowEnabled && style.shadowOpacity > 0.0) {
        QImage shadow(imageSize, QImage::Format_ARGB32_Premultiplied);
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
}

// Lay the text out and split it into per-`unit` spans, each a path in block-local coordinates
// (0,0 = layout rect top-left, matching layoutTextPath) plus its ink bounds. Whitespace-only spans
// are dropped. Spans are returned in reading order.
struct SpanPath
{
    QPainterPath path;
    QRectF inkRect;
};

QList<SpanPath> layoutTextSpans(const QString &text, const drift::TextStyle &style, const QFont &font,
                                double wrapWidth, double blockHeight, drift::TextAnimUnit unit)
{
    QString source = text;
    source.replace(QLatin1Char('\n'), QChar::LineSeparator);

    QTextOption option;
    option.setWrapMode(style.wordWrap ? QTextOption::WordWrap : QTextOption::NoWrap);

    QTextLayout layout(source, font);
    layout.setTextOption(option);

    const QFontMetricsF metrics(font);
    const double lineStep = metrics.lineSpacing() * qMax(0.1, style.lineHeight);
    const double effectiveWrap = style.wordWrap ? qMax(1.0, wrapWidth) : std::numeric_limits<double>::max();
    const double ascent = metrics.ascent();

    layout.beginLayout();
    forever {
        QTextLine line = layout.createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(effectiveWrap);
    }
    layout.endLayout();

    const int lineCount = layout.lineCount();
    if (lineCount == 0)
        return {};

    const double totalH = lineCount * lineStep;
    double blockTop = 0.0;
    if (style.valign == drift::TextVAlign::Middle)
        blockTop = (blockHeight - totalH) * 0.5;
    else if (style.valign == drift::TextVAlign::Bottom)
        blockTop = blockHeight - totalH;

    QList<SpanPath> spans;
    auto makeSpan = [&](double glyphX, double baselineY, const QString &glyphText) {
        if (glyphText.trimmed().isEmpty())
            return;
        QPainterPath p;
        p.setFillRule(Qt::WindingFill);
        p.addText(glyphX, baselineY, font, glyphText);
        const QRectF ink = p.boundingRect();
        if (ink.isEmpty())
            return;
        spans.append({p, ink});
    };

    for (int i = 0; i < lineCount; ++i) {
        const QTextLine line = layout.lineAt(i);
        const double natural = line.naturalTextWidth();
        double lineX = 0.0;
        if (style.align == drift::TextAlign::Center)
            lineX = (wrapWidth - natural) * 0.5;
        else if (style.align == drift::TextAlign::Right)
            lineX = wrapWidth - natural;
        const double baselineY = blockTop + i * lineStep + ascent;
        const int start = line.textStart();
        const int len = line.textLength();
        if (len <= 0)
            continue;
        const int end = start + len;

        if (unit == drift::TextAnimUnit::Line) {
            QString slice = source.mid(start, len);
            slice.remove(QChar::LineSeparator);
            makeSpan(lineX, baselineY, slice);
        } else if (unit == drift::TextAnimUnit::Word) {
            int w = start;
            while (w < end) {
                while (w < end && source.at(w).isSpace())
                    ++w;
                if (w >= end)
                    break;
                int wEnd = w;
                while (wEnd < end && !source.at(wEnd).isSpace())
                    ++wEnd;
                makeSpan(lineX + line.cursorToX(w), baselineY, source.mid(w, wEnd - w));
                w = wEnd;
            }
        } else { // Character
            for (int c = start; c < end; ++c) {
                const QChar ch = source.at(c);
                if (ch == QChar::LineSeparator || ch.isSpace())
                    continue;
                makeSpan(lineX + line.cursorToX(c), baselineY, QString(ch));
            }
        }
    }
    return spans;
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
    const QPainterPath shape = outlineShape(path, style, renderScale);

    if (style.boxEnabled) {
        const double padding = style.boxPadding * renderScale;
        const QRectF box = shape.boundingRect().adjusted(-padding, -padding, padding, padding);
        p.setPen(Qt::NoPen);
        p.setBrush(style.boxColor);
        p.drawRoundedRect(box, style.boxRadius * renderScale, style.boxRadius * renderScale);
    }

    paintGlyphs(p, path, shape, style, renderScale, image.size());
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

namespace {

// The whole span list for a clip depends only on text + style + layout size + scale + unit, and is
// time-independent (motion rides on the layer), so it is cached wholesale. Rects are stored relative
// to the layout rect's top-left and offset to absolute canvas coords on retrieval, so moving the
// clip does not invalidate the cache.
QMutex g_spanCacheMutex;
QHash<quint64, QList<TextSpanRaster>> g_spanCache;
constexpr int kMaxSpanCacheEntries = 16;

quint64 spanRasterKey(const QString &text, const drift::TextStyle &s, const QRectF &layoutRect,
                      double renderScale, drift::TextAnimUnit unit)
{
    return qHashMulti(0, text, s.fontFamily, s.pixelSize, s.fontWeight, s.italic, s.color.rgba(),
                      static_cast<int>(s.align), static_cast<int>(s.valign), s.wordWrap, s.lineHeight,
                      s.letterSpacing, s.outlineWidth, s.outlineColor.rgba(), s.shadowEnabled,
                      s.shadowOffsetX, s.shadowOffsetY, s.shadowBlur, s.shadowOpacity,
                      s.shadowColor.rgba(), s.boxEnabled, s.boxColor.rgba(), s.boxPadding, s.boxRadius,
                      qRound(layoutRect.width()), qRound(layoutRect.height()),
                      qRound(renderScale * 1000.0), static_cast<int>(unit));
}

QList<TextSpanRaster> offsetSpans(const QList<TextSpanRaster> &local, const QPointF &origin)
{
    QList<TextSpanRaster> out = local;
    for (TextSpanRaster &s : out)
        s.rect.translate(origin);
    return out;
}

} // namespace

QList<TextSpanRaster> rasterizeTextSpans(const drift::Clip &clip, const QString &text,
                                         const QRectF &layoutRect, double renderScale,
                                         drift::TextAnimUnit unit)
{
    if (text.isEmpty() || layoutRect.width() < 1.0 || layoutRect.height() < 1.0)
        return {};
    if (unit == drift::TextAnimUnit::Block)
        return {}; // whole-layer path — callers use rasterizeText instead

    const drift::TextStyle &style = clip.textStyle;

    const quint64 key = spanRasterKey(text, style, layoutRect, renderScale, unit);
    {
        QMutexLocker lock(&g_spanCacheMutex);
        const auto it = g_spanCache.constFind(key);
        if (it != g_spanCache.constEnd())
            return offsetSpans(it.value(), layoutRect.topLeft());
    }

    const double bleed = std::ceil(bleedFor(style) * renderScale) + 2.0;

    const QFont font = fontForStyle(style, qRound(style.pixelSize * renderScale));
    QFont spaced = font;
    if (!qFuzzyIsNull(style.letterSpacing))
        spaced.setLetterSpacing(QFont::AbsoluteSpacing, style.letterSpacing * renderScale);

    const QList<SpanPath> spans =
        layoutTextSpans(text, style, spaced, layoutRect.width(), layoutRect.height(), unit);
    if (spans.isEmpty())
        return {};

    // Built with layout-local rects (relative to layoutRect.topLeft()); cached, then offset to canvas.
    QList<TextSpanRaster> local;
    local.reserve(spans.size() + 1);

    // A single static box behind every span, so the background never staggers with the glyphs.
    if (style.boxEnabled) {
        QRectF blockInk;
        for (const SpanPath &s : spans)
            blockInk = blockInk.isNull() ? s.inkRect : blockInk.united(s.inkRect);
        if (!blockInk.isEmpty()) {
            const double padding = style.boxPadding * renderScale;
            const QRectF boxLocal = blockInk.adjusted(-padding, -padding, padding, padding);
            const int bw = qMax(1, qCeil(boxLocal.width()));
            const int bh = qMax(1, qCeil(boxLocal.height()));
            QImage boxImg(bw, bh, QImage::Format_ARGB32_Premultiplied);
            boxImg.fill(Qt::transparent);
            QPainter bp(&boxImg);
            bp.setRenderHint(QPainter::Antialiasing);
            bp.setPen(Qt::NoPen);
            bp.setBrush(style.boxColor);
            bp.drawRoundedRect(QRectF(0, 0, bw, bh), style.boxRadius * renderScale,
                               style.boxRadius * renderScale);
            bp.end();

            TextSpanRaster box;
            box.image = boxImg;
            box.rect = QRectF(boxLocal.x(), boxLocal.y(), bw, bh);
            box.index = -1;
            box.count = spans.size();
            local.append(box);
        }
    }

    for (int i = 0; i < spans.size(); ++i) {
        const QRectF ink = spans.at(i).inkRect;
        const int iw = qMax(1, qCeil(ink.width() + bleed * 2.0));
        const int ih = qMax(1, qCeil(ink.height() + bleed * 2.0));

        QImage image(iw, ih, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter p(&image);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        QPainterPath glyph = spans.at(i).path;
        glyph.translate(bleed - ink.x(), bleed - ink.y());
        const QPainterPath shape = outlineShape(glyph, style, renderScale);
        paintGlyphs(p, glyph, shape, style, renderScale, image.size());
        p.end();

        TextSpanRaster span;
        span.image = image;
        span.rect = QRectF(ink.x() - bleed, ink.y() - bleed, iw, ih);
        span.index = i;
        span.count = spans.size();
        local.append(span);
    }

    {
        QMutexLocker lock(&g_spanCacheMutex);
        if (g_spanCache.size() >= kMaxSpanCacheEntries)
            g_spanCache.clear();
        g_spanCache.insert(key, local);
    }

    return offsetSpans(local, layoutRect.topLeft());
}

// Sample entrance/exit motion for a text span occupying [windowStartUs, windowStartUs +
// windowDurationUs). Text clips pass the clip's span; subtitles pass the active cue's span
// so every cue animates in and out on its own.
static TextAnimSample sampleTextAnimationWindow(const drift::TextStyle &style, drift::TimeUs timelineUs,
                                                drift::TimeUs windowStartUs, drift::TimeUs windowDurationUs,
                                                const QRectF &layoutRect, double renderScale)
{
    TextAnimSample sample;

    const drift::TimeUs elapsed = timelineUs - windowStartUs;
    const drift::TimeUs remaining = windowDurationUs - elapsed;

    // Whole-layer (Block) wave: the entire text bobs together. Per-span waves are handled by
    // sampleTextSpanAnimation with a per-span phase.
    if (style.animIn.kind == drift::TextAnimKind::Wave) {
        applyWave(elapsed, 0, layoutRect, renderScale, &sample);
        return sample;
    }

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

TextAnimSample sampleTextAnimation(const drift::Clip &clip, drift::TimeUs timelineUs,
                                   const QRectF &layoutRect, double renderScale)
{
    return sampleTextAnimationWindow(clip.textStyle, timelineUs, clip.timelineStart,
                                     clip.timelineDuration, layoutRect, renderScale);
}

TextAnimSample sampleSubtitleCueAnimation(const drift::Clip &clip, const drift::SubtitleCue &cue,
                                          drift::TimeUs timelineUs, const QRectF &layoutRect,
                                          double renderScale)
{
    return sampleTextAnimationWindow(clip.textStyle, timelineUs, clip.timelineStart + cue.startUs,
                                     cue.endUs - cue.startUs, layoutRect, renderScale);
}

namespace {

// The stagger "slot" a span fires in, as a fractional index. Forward = reading order; the others
// remap it without changing the drawn order.
double reindexForOrder(int index, int count, drift::TextAnimOrder order)
{
    if (count <= 1)
        return 0.0;
    switch (order) {
    case drift::TextAnimOrder::Forward:
        return index;
    case drift::TextAnimOrder::Backward:
        return count - 1 - index;
    case drift::TextAnimOrder::CenterOut:
        return std::abs(index - (count - 1) / 2.0);
    case drift::TextAnimOrder::Random: {
        // Stable per-index pseudo-random slot so the shuffle holds still across frames.
        const quint32 h = qHash(static_cast<quint32>(index) * 2654435761u) ^ 0x9e3779b9u;
        return (h & 0xffffu) / 65535.0 * (count - 1);
    }
    }
    return index;
}

// Largest slot any span can occupy for a given order — used to anchor the staggered exit so the last
// span leaves exactly at the clip's end.
double maxReindex(int count, drift::TextAnimOrder order)
{
    if (count <= 1)
        return 0.0;
    if (order == drift::TextAnimOrder::CenterOut)
        return (count - 1) / 2.0;
    return count - 1;
}

} // namespace

TextAnimSample sampleTextSpanAnimation(const drift::Clip &clip, drift::TimeUs timelineUs, int spanIndex,
                                       int spanCount, const QRectF &layoutRect, double renderScale)
{
    TextAnimSample sample;
    const drift::TextStyle &style = clip.textStyle;
    const drift::TimeUs clipStart = clip.timelineStart;
    const drift::TimeUs clipEnd = clip.timelineStart + clip.timelineDuration;

    const drift::TextAnimation &in = style.animIn;
    if (in.kind == drift::TextAnimKind::Wave) {
        applyWave(timelineUs - clipStart, spanIndex, layoutRect, renderScale, &sample);
    } else if (in.kind != drift::TextAnimKind::None && in.durationUs > 0) {
        const double slot = reindexForOrder(spanIndex, spanCount, in.order);
        const drift::TimeUs spanStart = clipStart + static_cast<drift::TimeUs>(slot * in.staggerUs);
        const double settled =
            static_cast<double>(timelineUs - spanStart) / static_cast<double>(in.durationUs);
        applyAnimation(in, settled, true, layoutRect, renderScale, &sample);
    }

    const drift::TextAnimation &out = style.animOut;
    if (out.kind != drift::TextAnimKind::None && out.kind != drift::TextAnimKind::Wave
        && out.durationUs > 0) {
        const double slot = reindexForOrder(spanIndex, spanCount, out.order);
        const double maxSlot = maxReindex(spanCount, out.order);
        // Each span finishes exiting slot-staggered before the clip end; the last-out span lands on
        // clipEnd. settled runs 1 (in place) -> 0 (gone) as the finish time approaches.
        const drift::TimeUs finish = clipEnd - static_cast<drift::TimeUs>((maxSlot - slot) * out.staggerUs);
        const double settled = static_cast<double>(finish - timelineUs) / static_cast<double>(out.durationUs);
        applyAnimation(out, settled, false, layoutRect, renderScale, &sample);
    }

    sample.opacity = qBound(0.0, sample.opacity, 1.0);
    return sample;
}
