#pragma once

#include "Time.h"

#include <QColor>
#include <QList>
#include <QString>

namespace drift {

enum class TextAlign { Left, Center, Right };
enum class TextVAlign { Top, Middle, Bottom };

// Entrance / exit motion. Every kind is expressible as opacity + offset + scale + blur on the
// finished text layer, which is what keeps the rasterized glyphs cacheable across frames.
enum class TextAnimKind { None, Fade, SlideUp, SlideDown, SlideLeft, SlideRight, Pop, Blur };
enum class TextEase { Linear, EaseOut, EaseInOut, Back };

QString textAlignToString(TextAlign align);
TextAlign textAlignFromString(const QString &align);

QString textVAlignToString(TextVAlign valign);
TextVAlign textVAlignFromString(const QString &valign);

QString textAnimKindToString(TextAnimKind kind);
TextAnimKind textAnimKindFromString(const QString &kind);

QString textEaseToString(TextEase ease);
TextEase textEaseFromString(const QString &ease);

struct TextAnimation
{
    TextAnimKind kind = TextAnimKind::None;
    TimeUs durationUs = 400000;
    TextEase ease = TextEase::EaseOut;
};

struct TextStyle
{
    QString fontFamily = QStringLiteral("Inter");
    int pixelSize = 64; // at project height
    int fontWeight = 700; // 100..900
    bool italic = false;
    QColor color = Qt::white;

    TextAlign align = TextAlign::Center;
    TextVAlign valign = TextVAlign::Middle;
    bool wordWrap = true;
    double lineHeight = 1.2; // multiple of the font's natural line spacing
    double letterSpacing = 0.0; // px at pixelSize

    double outlineWidth = 0.0; // px; 0 = no outline. Grows outward from the glyph edge.
    QColor outlineColor = Qt::black;

    bool shadowEnabled = false;
    double shadowOffsetX = 0.0;
    double shadowOffsetY = 4.0;
    double shadowBlur = 8.0;
    double shadowOpacity = 0.6;
    QColor shadowColor = QColor(0, 0, 0);

    bool boxEnabled = false; // filled background behind the glyphs
    QColor boxColor = QColor(0, 0, 0, 128);
    double boxPadding = 8.0;
    double boxRadius = 0.0;

    TextAnimation animIn;
    TextAnimation animOut;
};

struct TextPreset
{
    QString id;
    QString label;
    TextStyle style;
};

const QList<TextPreset> &textPresets();
const TextStyle *textStyleForPresetId(const QString &id);

} // namespace drift
