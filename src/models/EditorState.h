#pragma once

#include <QJsonArray>
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
    Q_PROPERTY(bool rippleEnabled READ rippleEnabled WRITE setRippleEnabled NOTIFY rippleEnabledChanged)
    Q_PROPERTY(bool undoAvailable READ undoAvailable NOTIFY undoStackChanged)
    Q_PROPERTY(bool redoAvailable READ redoAvailable NOTIFY undoStackChanged)
    Q_PROPERTY(bool exportInProgress READ exportInProgress NOTIFY exportInProgressChanged)
    Q_PROPERTY(int selectedTrack READ selectedTrack NOTIFY selectionChanged)
    Q_PROPERTY(int selectedClip READ selectedClip NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedClipData READ selectedClipData NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
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
    bool rippleEnabled() const { return m_rippleEnabled; }
    bool undoAvailable() const { return !m_undoStack.isEmpty(); }
    bool redoAvailable() const { return !m_redoStack.isEmpty(); }
    bool exportInProgress() const { return m_exportInProgress; }
    int selectedTrack() const { return m_selectedTrack; }
    int selectedClip() const { return m_selectedClip; }
    QVariantMap selectedClipData() const;
    QVariantList bookmarks() const;
    QString projectName() const { return m_projectName; }
    QString lastMessage() const { return m_lastMessage; }

    void setPlayheadSeconds(double seconds);
    void setPlaying(bool playing);
    void setSnapEnabled(bool enabled);
    void setRippleEnabled(bool enabled);
    void setProjectName(const QString &name);

    Q_INVOKABLE void addClipFromAsset(int assetIndex);
    Q_INVOKABLE void addClipFromAssetAt(int assetIndex, int trackIndex, double atSeconds);
    Q_INVOKABLE void addTextClip(const QString &text, double atSeconds);
    Q_INVOKABLE void selectClip(int trackIndex, int clipIndex);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QVariantMap clipAt(int trackIndex, int clipIndex) const;
    Q_INVOKABLE QVariantMap activeVideoClipAtPlayhead() const;
    Q_INVOKABLE QVariantMap activeAudioClipAtPlayhead() const;
    Q_INVOKABLE double sourceTimeAtPlayhead() const;
    Q_INVOKABLE double sourceTimeForClip(const QVariantMap &clip) const;
    Q_INVOKABLE void deleteSelectedClip();
    Q_INVOKABLE void duplicateSelectedClip();
    Q_INVOKABLE void moveClip(int trackIndex, int clipIndex, double newStart);
    Q_INVOKABLE void moveClipToTrack(int trackIndex, int clipIndex, int newTrackIndex, double newStart);
    Q_INVOKABLE void alignSelectedClipLeft();
    Q_INVOKABLE void alignSelectedClipRight();
    Q_INVOKABLE void splitAtPlayhead();
    Q_INVOKABLE void trimClipLeft(int trackIndex, int clipIndex, double newStart);
    Q_INVOKABLE void trimClipRight(int trackIndex, int clipIndex, double newEnd);
    Q_INVOKABLE void setClipTrim(int trackIndex, int clipIndex, double inPoint, double outPoint);
    Q_INVOKABLE void setClipStart(int trackIndex, int clipIndex, double start);
    Q_INVOKABLE void setClipDuration(int trackIndex, int clipIndex, double duration);
    Q_INVOKABLE void setClipTextContent(int trackIndex, int clipIndex, const QString &text);
    Q_INVOKABLE void setTrackMuted(int trackIndex, bool muted);
    Q_INVOKABLE void setTrackHidden(int trackIndex, bool hidden);
    Q_INVOKABLE bool trackMuted(int trackIndex) const;
    Q_INVOKABLE bool trackHidden(int trackIndex) const;
    Q_INVOKABLE void addBookmark(double seconds, const QString &label);
    Q_INVOKABLE void removeBookmark(int index);
    Q_INVOKABLE void goToBookmark(int index);
    Q_INVOKABLE void freezeFrameAtPlayhead();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE double snapTime(double seconds) const;
    Q_INVOKABLE QVariantList waveformPeaks(const QString &path) const;
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
    void rippleEnabledChanged();
    void undoStackChanged();
    void exportInProgressChanged();
    void selectionChanged();
    void bookmarksChanged();
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
        QString textContent;
        double start = 0.0;
        double duration = 0.0;
        double inPoint = 0.0;
        double outPoint = 0.0;
        int assetIndex = -1;
    };

    struct Track {
        QString type;
        QList<Clip> clips;
        bool muted = false;
        bool hidden = false;
    };

    struct Bookmark {
        double seconds = 0.0;
        QString label;
    };

    struct Snapshot {
        QList<Track> tracks;
        QList<Bookmark> bookmarks;
        double playheadSeconds = 0.0;
        int selectedTrack = -1;
        int selectedClip = -1;
    };

    QVariantMap clipToMap(const Clip &clip) const;
    int defaultTrackForKind(const QString &kind) const;
    double clipDurationForAsset(int assetIndex) const;
    double sourceDurationForClip(const Clip &clip) const;
    double resolveClipStart(const Track &track, int excludeClipIndex, double desiredStart, double duration) const;
    bool clipContainsTime(const Clip &clip, double seconds) const;
    bool trackAllowsKind(const Track &track, const QString &kind) const;
    void setLastMessage(const QString &message);
    void resetTimeline();
    void loadTracksFromJson(const QJsonArray &tracksArray);
    void pushUndo();
    void clearRedo();
    Snapshot captureSnapshot() const;
    void restoreSnapshot(const Snapshot &snapshot);
    void applyRippleShift(Track &track, int fromClipIndex, double delta);
    void finishEdit(const QString &message);

    AssetLibrary *m_assetLibrary = nullptr;
    QList<Track> m_tracks;
    QList<Bookmark> m_bookmarks;
    QList<Snapshot> m_undoStack;
    QList<Snapshot> m_redoStack;
    QTimer m_playbackTimer;
    double m_playheadSeconds = 0.0;
    bool m_playing = false;
    bool m_snapEnabled = true;
    bool m_rippleEnabled = false;
    bool m_exportInProgress = false;
    int m_selectedTrack = -1;
    int m_selectedClip = -1;
    QString m_projectName = QStringLiteral("Untitled Project");
    QString m_lastMessage;

    static constexpr double kImageClipDurationSeconds = 5.0;
    static constexpr double kTextClipDurationSeconds = 5.0;
    static constexpr double kPlaybackTickSeconds = 1.0 / 30.0;
    static constexpr double kMinClipDurationSeconds = 0.1;
    static constexpr double kSnapThresholdSeconds = 0.15;
    static constexpr int kMaxUndoSteps = 50;
};
