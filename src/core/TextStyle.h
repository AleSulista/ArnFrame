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
// Typewriter is a hard binary reveal; Rise/Bounce add per-span overshoot; Wave is continuous
// (it oscillates for the whole clip rather than settling), and only ever reads with a per-span unit.
enum class TextAnimKind {
    None, Fade, SlideUp, SlideDown, SlideLeft, SlideRight, Pop, Blur, Typewriter, Rise, Bounce, Wave
};
enum class TextEase { Linear, EaseOut, EaseInOut, Back };

// Reveal granularity: Block animates the whole text at once (the original behaviour); the others
// stagger the entrance/exit across characters, words or lines for kinetic-typography reveals.
enum class TextAnimUnit { Block, Word, Character, Line };

// Order the staggered spans fire in.
enum class TextAnimOrder { Forward, Backward, CenterOut, Random };

QString textAlignToString(TextAlign align);
TextAlign textAlignFromString(const QString &align);

QString textVAlignToString(TextVAlign valign);
TextVAlign textVAlignFromString(const QString &valign);

QString textAnimKindToString(TextAnimKind kind);
TextAnimKind textAnimKindFromString(const QString &kind);

QString textEaseToString(TextEase ease);
TextEase textEaseFromString(const QString &ease);

QString textAnimUnitToString(TextAnimUnit unit);
TextAnimUnit textAnimUnitFromString(const QString &unit);

QString textAnimOrderToString(TextAnimOrder order);
TextAnimOrder textAnimOrderFromString(const QString &order);

struct TextAnimation
{
    TextAnimKind kind = TextAnimKind::None;
    TimeUs durationUs = 400000;
    TextEase ease = TextEase::EaseOut;

    // Per-span reveal. Block (the default) keeps the original whole-layer motion; the other units
    // stagger the spans by staggerUs, ordered per `order`.
    TextAnimUnit unit = TextAnimUnit::Block;
    TimeUs staggerUs = 60000;
    TextAnimOrder order = TextAnimOrder::Forward;
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
