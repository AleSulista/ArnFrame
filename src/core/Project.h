#pragma once

#include "MediaAsset.h"
#include "Track.h"
#include "Time.h"

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

namespace drift {

struct Bookmark
{
    TimeUs timeUs = 0;
    QString label;
};

// How the canvas area behind/around clips is filled.
enum class BackgroundKind { Color, Blur };

struct Background
{
    BackgroundKind kind = BackgroundKind::Color;
    QColor color = Qt::black;    // used when kind == Color
    double blurStrength = 20.0;  // px blur radius; used when kind == Blur
};

// Root project document: tracks, assets, output settings.
class Project
{
public:
    static constexpr int kCurrentVersion = 3;

    Project() { resetToDefaultTimeline(); }

    QString name() const { return m_name; }
    void setName(const QString &name) { m_name = name; }

    int fps() const { return m_fps; }
    void setFps(int fps) { m_fps = qMax(1, fps); }

    int width() const { return m_width; }
    int height() const { return m_height; }
    int sampleRate() const { return m_sampleRate; }
    void setResolution(int width, int height) { m_width = width; m_height = height; }
    void setSampleRate(int rate) { m_sampleRate = rate; }

    const QList<Track> &tracks() const { return m_tracks; }
    QList<Track> &tracks() { return m_tracks; }

    const QList<QString> &assetOrder() const { return m_assetOrder; }
    QList<QString> &assetOrder() { return m_assetOrder; }
    const QHash<QString, MediaAsset> &assets() const { return m_assetsById; }
    QHash<QString, MediaAsset> &assets() { return m_assetsById; }

    const QList<Bookmark> &bookmarks() const { return m_bookmarks; }
    QList<Bookmark> &bookmarks() { return m_bookmarks; }

    const Background &background() const { return m_background; }
    void setBackground(const Background &background) { m_background = background; }

    void resetToDefaultTimeline();
    TimeUs durationUs() const;

    QString addAsset(MediaAsset asset);
    MediaAsset *asset(const QString &id);
    const MediaAsset *asset(const QString &id) const;
    int assetIndex(const QString &id) const;
    QString assetIdAt(int index) const;

    static Project fromJson(const QJsonObject &object, QString *errorOut = nullptr);
    QJsonObject toJson() const;

private:
    QString m_name = QStringLiteral("Untitled Project");
    int m_fps = 30;
    int m_width = 1920;
    int m_height = 1080;
    int m_sampleRate = 48000;
    QList<Track> m_tracks;
    QList<Bookmark> m_bookmarks;
    Background m_background;
    QList<QString> m_assetOrder;
    QHash<QString, MediaAsset> m_assetsById;
};

} // namespace drift
