#pragma once

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class AssetLibrary;

class EditorState : public QObject
{
    Q_OBJECT

    Q_PROPERTY(AssetLibrary *assetLibrary READ assetLibrary CONSTANT)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(double playheadSeconds READ playheadSeconds WRITE setPlayheadSeconds NOTIFY playheadSecondsChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY tracksChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapEnabledChanged)
    Q_PROPERTY(int selectedTrack READ selectedTrack NOTIFY selectionChanged)
    Q_PROPERTY(int selectedClip READ selectedClip NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedClipData READ selectedClipData NOTIFY selectionChanged)
    Q_PROPERTY(QString projectName READ projectName WRITE setProjectName NOTIFY projectNameChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)

public:
    explicit EditorState(AssetLibrary *assetLibrary, QObject *parent = nullptr);

    AssetLibrary *assetLibrary() const { return m_assetLibrary; }
    QVariantList tracks() const;
    double playheadSeconds() const { return m_playheadSeconds; }
    double durationSeconds() const;
    bool playing() const { return m_playing; }
    bool snapEnabled() const { return m_snapEnabled; }
    int selectedTrack() const { return m_selectedTrack; }
    int selectedClip() const { return m_selectedClip; }
    QVariantMap selectedClipData() const;
    QString projectName() const { return m_projectName; }
    QString lastMessage() const { return m_lastMessage; }

    void setPlayheadSeconds(double seconds);
    void setPlaying(bool playing);
    void setSnapEnabled(bool enabled);
    void setProjectName(const QString &name);

    Q_INVOKABLE void addClipFromAsset(int assetIndex);
    Q_INVOKABLE void addClipFromAssetAt(int assetIndex, int trackIndex, double atSeconds);
    Q_INVOKABLE void selectClip(int trackIndex, int clipIndex);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QVariantMap clipAt(int trackIndex, int clipIndex) const;
    Q_INVOKABLE QVariantMap activeVideoClipAtPlayhead() const;
    Q_INVOKABLE QVariantMap activeAudioClipAtPlayhead() const;
    Q_INVOKABLE double sourceTimeAtPlayhead() const;
    Q_INVOKABLE double sourceTimeForClip(const QVariantMap &clip) const;
    Q_INVOKABLE QString thumbnailForAsset(int assetIndex) const;
    Q_INVOKABLE void deleteSelectedClip();
    Q_INVOKABLE void moveClip(int trackIndex, int clipIndex, double newStart);
    Q_INVOKABLE void splitAtPlayhead();
    Q_INVOKABLE void trimClipLeft(int trackIndex, int clipIndex, double newStart);
    Q_INVOKABLE void trimClipRight(int trackIndex, int clipIndex, double newEnd);
    Q_INVOKABLE void setClipTrim(int trackIndex, int clipIndex, double inPoint, double outPoint);
    Q_INVOKABLE double snapTime(double seconds) const;
    Q_INVOKABLE void saveProject(const QUrl &url);
    Q_INVOKABLE void loadProject(const QUrl &url);
    Q_INVOKABLE void exportProject(const QUrl &outputUrl);
    Q_INVOKABLE QUrl fileUrl(const QString &path) const;
    Q_INVOKABLE QString imageUrl(const QString &path) const;

signals:
    void tracksChanged();
    void playheadSecondsChanged();
    void playingChanged();
    void snapEnabledChanged();
    void selectionChanged();
    void projectNameChanged();
    void lastMessageChanged();
    void exportFinished(bool success);

private:
    struct Clip {
        QString name;
        QString path;
        QString kind;
        QString thumbnailPath;
        QString filmstripPath;
        double start = 0.0;
        double duration = 0.0;
        double inPoint = 0.0;
        double outPoint = 0.0;
        int assetIndex = -1;
    };

    struct Track {
        QString type;
        QList<Clip> clips;
    };

    QVariantMap clipToMap(const Clip &clip) const;
    int defaultTrackForKind(const QString &kind) const;
    double clipDurationForAsset(int assetIndex) const;
    double sourceDurationForClip(const Clip &clip) const;
    double resolveClipStart(const Track &track, int excludeClipIndex, double desiredStart, double duration) const;
    bool clipContainsTime(const Clip &clip, double seconds) const;
    void setLastMessage(const QString &message);
    void resetTimeline();
    void loadTracksFromJson(const QJsonArray &tracksArray);

    AssetLibrary *m_assetLibrary = nullptr;
    QList<Track> m_tracks;
    QTimer m_playbackTimer;
    double m_playheadSeconds = 0.0;
    bool m_playing = false;
    bool m_snapEnabled = true;
    int m_selectedTrack = -1;
    int m_selectedClip = -1;
    QString m_projectName = QStringLiteral("Untitled Project");
    QString m_lastMessage;

    static constexpr double kImageClipDurationSeconds = 5.0;
    static constexpr double kPlaybackTickSeconds = 1.0 / 30.0;
    static constexpr double kMinClipDurationSeconds = 0.1;
    static constexpr double kSnapThresholdSeconds = 0.15;
};
