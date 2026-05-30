#include "TextStyle.h"

namespace drift {

QString textAlignToString(TextAlign align)
{
    switch (align) {
    case TextAlign::Left:
        return QStringLiteral("left");
    case TextAlign::Right:
        return QStringLiteral("right");
    case TextAlign::Center:
        return QStringLiteral("center");
    }
    return QStringLiteral("center");
}

TextAlign textAlignFromString(const QString &align)
{
    if (align == QStringLiteral("left"))
        return TextAlign::Left;
    if (align == QStringLiteral("right"))
        return TextAlign::Right;
    return TextAlign::Center;
}

QString textVAlignToString(TextVAlign valign)
{
    switch (valign) {
    case TextVAlign::Top:
        return QStringLiteral("top");
    case TextVAlign::Bottom:
        return QStringLiteral("bottom");
    case TextVAlign::Middle:
        return QStringLiteral("middle");
    }
    return QStringLiteral("middle");
}

TextVAlign textVAlignFromString(const QString &valign)
{
    if (valign == QStringLiteral("top"))
        return TextVAlign::Top;
    if (valign == QStringLiteral("bottom"))
        return TextVAlign::Bottom;
    return TextVAlign::Middle;
}

QString textAnimKindToString(TextAnimKind kind)
{
    switch (kind) {
    case TextAnimKind::None:
        return QStringLiteral("none");
    case TextAnimKind::Fade:
        return QStringLiteral("fade");
    case TextAnimKind::SlideUp:
        return QStringLiteral("slideUp");
    case TextAnimKind::SlideDown:
        return QStringLiteral("slideDown");
    case TextAnimKind::SlideLeft:
        return QStringLiteral("slideLeft");
    case TextAnimKind::SlideRight:
        return QStringLiteral("slideRight");
    case TextAnimKind::Pop:
        return QStringLiteral("pop");
    case TextAnimKind::Blur:
        return QStringLiteral("blur");
    }
    return QStringLiteral("none");
}

TextAnimKind textAnimKindFromString(const QString &kind)
{
    if (kind == QStringLiteral("fade"))
        return TextAnimKind::Fade;
    if (kind == QStringLiteral("slideUp"))
        return TextAnimKind::SlideUp;
    if (kind == QStringLiteral("slideDown"))
        return TextAnimKind::SlideDown;
    if (kind == QStringLiteral("slideLeft"))
        return TextAnimKind::SlideLeft;
    if (kind == QStringLiteral("slideRight"))
        return TextAnimKind::SlideRight;
    if (kind == QStringLiteral("pop"))
        return TextAnimKind::Pop;
    if (kind == QStringLiteral("blur"))
        return TextAnimKind::Blur;
    return TextAnimKind::None;
}

QString textEaseToString(TextEase ease)
{
    switch (ease) {
    case TextEase::Linear:
        return QStringLiteral("linear");
    case TextEase::EaseInOut:
        return QStringLiteral("easeInOut");
    case TextEase::Back:
        return QStringLiteral("back");
    case TextEase::EaseOut:
        return QStringLiteral("easeOut");
    }
    return QStringLiteral("easeOut");
}

TextEase textEaseFromString(const QString &ease)
{
    if (ease == QStringLiteral("linear"))
        return TextEase::Linear;
    if (ease == QStringLiteral("easeInOut"))
        return TextEase::EaseInOut;
    if (ease == QStringLiteral("back"))
        return TextEase::Back;
    return TextEase::EaseOut;
}

namespace {

QList<TextPreset> buildPresets()
{
    QList<TextPreset> presets;

    {
        TextStyle s;
        s.fontFamily = QStringLiteral("Montserrat");
        s.pixelSize = 96;
        s.fontWeight = 800;
        s.outlineWidth = 2.0;
        s.animIn = {TextAnimKind::Fade, 400000, TextEase::EaseOut};
        presets.append({QStringLiteral("title"), QStringLiteral("Title"), s});
    }
    {
        TextStyle s;
        s.fontFamily = QStringLiteral("Inter");
        s.pixelSize = 40;
        s.fontWeight = 500;
        s.valign = TextVAlign::Bottom;
        s.boxEnabled = true;
        s.boxColor = QColor(0, 0, 0, 140);
        s.boxPadding = 10.0;
        s.boxRadius = 6.0;
        presets.append({QStringLiteral("subtitle"), QStringLiteral("Subtitle"), s});
    }
    {
        TextStyle s;
        s.fontFamily = QStringLiteral("Poppins");
        s.pixelSize = 48;
        s.fontWeight = 700;
        s.align = TextAlign::Left;
        s.valign = TextVAlign::Bottom;
        s.boxEnabled = true;
        s.boxColor = QColor(0, 0, 0, 160);
        s.boxPadding = 12.0;
        s.animIn = {TextAnimKind::SlideRight, 500000, TextEase::EaseOut};
        s.animOut = {TextAnimKind::SlideLeft, 400000, TextEase::EaseInOut};
        presets.append({QStringLiteral("lower-third"), QStringLiteral("Lower third"), s});
    }
    {
        TextStyle s;
        s.fontFamily = QStringLiteral("Oswald");
        s.pixelSize = 44;
        s.fontWeight = 600;
        s.valign = TextVAlign::Bottom;
        s.outlineWidth = 3.0;
        s.shadowEnabled = true;
        presets.append({QStringLiteral("caption"), QStringLiteral("Caption"), s});
    }
    {
        TextStyle s;
        s.fontFamily = QStringLiteral("Playfair Display");
        s.pixelSize = 64;
        s.fontWeight = 500;
        s.italic = true;
        s.lineHeight = 1.4;
        s.animIn = {TextAnimKind::Blur, 700000, TextEase::EaseOut};
        presets.append({QStringLiteral("quote"), QStringLiteral("Quote"), s});
    }
    {
        TextStyle s;
        s.fontFamily = QStringLiteral("Anton");
        s.pixelSize = 120;
        s.fontWeight = 400;
        s.letterSpacing = 2.0;
        s.outlineWidth = 6.0;
        s.shadowEnabled = true;
        s.shadowBlur = 12.0;
        s.shadowOffsetY = 6.0;
        s.animIn = {TextAnimKind::Pop, 350000, TextEase::Back};
        presets.append({QStringLiteral("impact"), QStringLiteral("Impact"), s});
    }
    {
        TextStyle s;
        s.fontFamily = QStringLiteral("Fredoka");
        s.pixelSize = 80;
        s.fontWeight = 600;
        s.color = QColor(255, 214, 64);
        s.outlineWidth = 5.0;
        s.animIn = {TextAnimKind::Pop, 450000, TextEase::Back};
        s.animOut = {TextAnimKind::Pop, 300000, TextEase::EaseInOut};
        presets.append({QStringLiteral("pop"), QStringLiteral("Pop"), s});
    }
    {
        TextStyle s;
        s.fontFamily = QStringLiteral("Bebas Neue");
        s.pixelSize = 110;
        s.fontWeight = 400;
        s.letterSpacing = 4.0;
        s.color = QColor(120, 255, 245);
        s.outlineWidth = 2.0;
        s.outlineColor = QColor(0, 90, 120);
        s.shadowEnabled = true;
        s.shadowColor = QColor(0, 220, 255);
        s.shadowBlur = 24.0;
        s.shadowOffsetX = 0.0;
        s.shadowOffsetY = 0.0;
        s.shadowOpacity = 0.9;
        presets.append({QStringLiteral("neon"), QStringLiteral("Neon"), s});
    }
    {
        TextStyle s;
        s.fontFamily = QStringLiteral("Pacifico");
        s.pixelSize = 72;
        s.fontWeight = 400;
        s.lineHeight = 1.35;
        s.shadowEnabled = true;
        s.shadowBlur = 6.0;
        s.animIn = {TextAnimKind::SlideUp, 550000, TextEase::EaseOut};
        presets.append({QStringLiteral("handwritten"), QStringLiteral("Handwritten"), s});
    }

    return presets;
}

} // namespace

const QList<TextPreset> &textPresets()
{
    static const QList<TextPreset> presets = buildPresets();
    return presets;
}

const TextStyle *textStyleForPresetId(const QString &id)
{
    for (const TextPreset &preset : textPresets()) {
        if (preset.id == id)
            return &preset.style;
    }
    return nullptr;
}

} // namespace drift
