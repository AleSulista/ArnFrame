#pragma once

#include <QColor>
#include <QString>

namespace drift {

enum class TextAlign { Left, Center, Right };

QString textAlignToString(TextAlign align);
TextAlign textAlignFromString(const QString &align);

struct TextStyle
{
    QString fontFamily = QStringLiteral("Sans Serif");
    int pixelSize = 64; // at project height; scaled by clip.scale
    QColor color = Qt::white;
    bool bold = true;
    bool italic = false;
    TextAlign align = TextAlign::Center;

    double outlineWidth = 0.0; // px; 0 = no outline
    QColor outlineColor = Qt::black;

    bool boxEnabled = false; // filled background behind glyphs
    QColor boxColor = QColor(0, 0, 0, 128);
    double boxPadding = 8.0; // px
};

} // namespace drift
