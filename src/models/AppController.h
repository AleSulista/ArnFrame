#pragma once

#include "core/Project.h"
#include "core/Time.h"
#include "ClipListModel.h"
#include "TimelineModel.h"
#include "models/AssetLibrary.h"

#include <QAtomicInt>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QSet>
#include <QUndoStack>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class QTimer;

#include "playback/PlaybackEngine.h"

// QML-facing controller over the core project model and undo stack.
class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(AssetLibrary *assetLibrary READ assetLibrary CONSTANT)
    Q_PROPERTY(TimelineModel *timelineModel READ timelineModel CONSTANT)
    Q_PROPERTY(ClipListModel *clipListModel READ clipListModel CONSTANT)
    Q_PROPERTY(PlaybackEngine *playback READ playback CONSTANT)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(double playheadSeconds READ playheadSeconds WRITE setPlayheadSeconds NOTIFY playheadSecondsChanged)
    Q_PROPERTY(double durationSeconds READ durationSeconds NOTIFY tracksChanged)
    Q_PROPERTY(bool playing READ playing WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapEnabledChanged)
    Q_PROPERTY(bool rippleEnabled READ rippleEnabled WRITE setRippleEnabled NOTIFY rippleEnabledChanged)
    Q_PROPERTY(bool autoKeyEnabled READ autoKeyEnabled WRITE setAutoKeyEnabled NOTIFY autoKeyEnabledChanged)
    Q_PROPERTY(QString keyframeGraphProperty READ keyframeGraphProperty WRITE setKeyframeGraphProperty
                   NOTIFY keyframeGraphPropertyChanged)
    Q_PROPERTY(bool subtitleEditing READ subtitleEditing WRITE setSubtitleEditing NOTIFY subtitleEditingChanged)
    Q_PROPERTY(int selectedSubtitleCue READ selectedSubtitleCue WRITE setSelectedSubtitleCue
                   NOTIFY selectedSubtitleCueChanged)
    Q_PROPERTY(bool undoAvailable READ undoAvailable NOTIFY undoStackChanged)
    Q_PROPERTY(bool redoAvailable READ redoAvailable NOTIFY undoStackChanged)
    Q_PROPERTY(bool exportInProgress READ exportInProgress NOTIFY exportInProgressChanged)
    Q_PROPERTY(double exportProgress READ exportProgress NOTIFY exportProgressChanged)
    Q_PROPERTY(int selectedTrack READ selectedTrack NOTIFY selectionChanged)
    Q_PROPERTY(int selectedClip READ selectedClip NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selection READ selection NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedClipData READ selectedClipData NOTIFY selectedClipDataChanged)
    Q_PROPERTY(QVariantList selectedClipEffects READ selectedClipEffects NOTIFY selectedClipDataChanged)
    Q_PROPERTY(QVariantMap selectedTransitionData READ selectedTransitionData NOTIFY selectedTransitionDataChanged)
    Q_PROPERTY(int selectedTransitionTrack READ selectedTransitionTrack NOTIFY selectedTransitionDataChanged)
    Q_PROPERTY(int selectedTransitionLeftClip READ selectedTransitionLeftClip NOTIFY selectedTransitionDataChanged)
    Q_PROPERTY(bool guidesEnabled READ guidesEnabled WRITE setGuidesEnabled NOTIFY guidesChanged)
    Q_PROPERTY(QString guideType READ guideType WRITE setGuideType NOTIFY guidesChanged)
    Q_PROPERTY(QVariantMap background READ background NOTIFY backgroundChanged)
    Q_PROPERTY(QVariantList actions READ actions NOTIFY shortcutsChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(QString projectName READ projectName WRITE setProjectName NOTIFY projectNameChanged)
    Q_PROPERTY(QString lastMessage READ lastMessage NOTIFY lastMessageChanged)
    Q_PROPERTY(int draggingAssetIndex READ draggingAssetIndex WRITE setDraggingAssetIndex NOTIFY draggingAssetIndexChanged)
    Q_PROPERTY(bool hasUnsavedChanges READ hasUnsavedChanges NOTIFY dirtyChanged)
    Q_PROPERTY(QString currentProjectPath READ currentProjectPath NOTIFY currentProjectPathChanged)
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY recoveryChanged)
    Q_PROPERTY(QVariantMap recoveryInfo READ recoveryInfo NOTIFY recoveryChanged)
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY recentProjectsChanged)

public:
    explicit AppController(AssetLibrary *assetLibrary, QObject *parent = nullptr);
    ~AppController() override;

    AssetLibrary *assetLibrary() const { return m_assetLibrary; }
    TimelineModel *timelineModel() { return &m_timelineModel; }
    ClipListModel *clipListModel() { return &m_clipListModel; }
    PlaybackEngine *playback() { return &m_playback; }
    drift::Project *project() { return &m_project; }
    const drift::Project *project() const { return &m_project; }

    QVariantList tracks() const;
    double playheadSeconds() const;
    double durationSeconds() const;
    bool playing() const { return m_playing; }
    bool snapEnabled() const { return m_snapEnabled; }
    bool rippleEnabled() const { return m_rippleEnabled; }
    bool autoKeyEnabled() const { return m_autoKeyEnabled; }
    QString keyframeGraphProperty() const { return m_keyframeGraphProperty; }
    bool subtitleEditing() const { return m_subtitleEditing; }
    int selectedSubtitleCue() const { return m_selectedSubtitleCue; }
    bool undoAvailable() const { return m_undoStack.canUndo(); }
    bool redoAvailable() const { return m_undoStack.canRedo(); }
    bool exportInProgress() const { return m_exportInProgress; }
    double exportProgress() const;
    int selectedTrack() const { return m_selectedTrack; }
    int selectedClip() const { return m_selectedClip; }
    QVariantList selection() const;
    QVariantMap selectedClipData() const;
    QVariantList selectedClipEffects() const;
    QVariantMap selectedTransitionData() const;
    int selectedTransitionTrack() const { return m_selectedTransitionTrack; }
    int selectedTransitionLeftClip() const { return m_selectedTransitionLeftClip; }
    bool guidesEnabled() const { return m_guidesEnabled; }
    QString guideType() const { return m_guideType; }
    QVariantMap background() const;
    QVariantList actions() const;
    QVariantList bookmarks() const;
    QString projectName() const;
    QString lastMessage() const { return m_lastMessage; }
    int draggingAssetIndex() const { return m_draggingAssetIndex; }
    void setDraggingAssetIndex(int index);
    bool hasUnsavedChanges() const { return m_dirty; }
    QString currentProjectPath() const { return m_currentProjectPath; }
    bool recoveryAvailable() const { return m_recoveryAvailable; }
    QVariantMap recoveryInfo() const { return m_recoveryInfo; }
    QVariantList recentProjects() const;

    void setPlayheadSeconds(double seconds);
    void setPlaying(bool playing);
    void setSnapEnabled(bool enabled);
    void setRippleEnabled(bool enabled);
    void setAutoKeyEnabled(bool enabled);
    void setKeyframeGraphProperty(const QString &prop);
    void setSubtitleEditing(bool editing);
    void setSelectedSubtitleCue(int index);
    void setProjectName(const QString &name);
    void setGuidesEnabled(bool enabled);
    void setGuideType(const QString &type);

    Q_INVOKABLE void addClipFromAsset(int assetIndex);
    Q_INVOKABLE void addClipFromAssetAt(int assetIndex, int trackIndex, double atSeconds);
    Q_INVOKABLE void addClipFromAssetOnNewTrack(int assetIndex, double atSeconds);
    Q_INVOKABLE bool trackAcceptsAsset(int trackIndex, int assetIndex) const;
    Q_INVOKABLE QString trackTypeForAsset(int assetIndex) const;
    Q_INVOKABLE void addTextClip(const QString &text, double atSeconds);
    Q_INVOKABLE void addSubtitleClip(double atSeconds);
    Q_INVOKABLE void addShapeClip(const QString &shapeKind, double atSeconds);
    Q_INVOKABLE void addShapeClipAt(const QString &shapeKind, int trackIndex, double atSeconds);
    Q_INVOKABLE void addStickerClip(const QString &stickerId, double atSeconds);
    Q_INVOKABLE QVariantList builtinStickers() const;
    Q_INVOKABLE QVariantList builtinShapes() const;
    Q_INVOKABLE QVariantList previewClipsAtPlayhead() const;
    Q_INVOKABLE void beginPreviewDrag(const QString &undoText = QStringLiteral("Edit clip"));
    Q_INVOKABLE void previewSetClipPosition(int trackIndex, int clipIndex, double xPixels, double yPixels);
    Q_INVOKABLE void previewSetClipSize(int trackIndex, int clipIndex, double widthPixels, double heightPixels);
    Q_INVOKABLE void previewSetClipRect(int trackIndex, int clipIndex, double xPixels, double yPixels,
                                        double widthPixels, double heightPixels);
    // Text resizes scale the glyphs along with the box, so the size rides with the rect.
    Q_INVOKABLE void previewSetTextRect(int trackIndex, int clipIndex, double xPixels, double yPixels,
                                        double widthPixels, double heightPixels, int pixelSize);
    Q_INVOKABLE void previewSetClipRotation(int trackIndex, int clipIndex, double degrees);
    Q_INVOKABLE void previewSetClipKeyframe(int trackIndex, int clipIndex, const QString &prop,
                                            double atSeconds, double value);
    Q_INVOKABLE void previewSetEffectParam(int trackIndex, int clipIndex, int effectIndex,
                                           const QString &key, double value);
    Q_INVOKABLE void previewSetClipSpeed(int trackIndex, int clipIndex, double speed);
    Q_INVOKABLE void previewSetClipMask(int trackIndex, int clipIndex, const QVariantMap &mask);
    Q_INVOKABLE void previewSetClipFade(int trackIndex, int clipIndex, double fadeInSeconds, double fadeOutSeconds);
    Q_INVOKABLE void commitPreviewDrag();
    Q_INVOKABLE int projectWidth() const;
    Q_INVOKABLE int projectHeight() const;
    Q_INVOKABLE int projectFps() const;
    Q_INVOKABLE void setProjectResolution(int width, int height);
    Q_INVOKABLE void setProjectSetup(int width, int height, int fps);
    Q_INVOKABLE void setBackground(const QVariantMap &background);
    Q_INVOKABLE bool timelineHasVisualClips() const;
    Q_INVOKABLE bool shouldConfigureProjectForAsset(int assetIndex) const;
    Q_INVOKABLE QVariantMap suggestedProjectSetupForAsset(int assetIndex) const;
    Q_INVOKABLE void selectClip(int trackIndex, int clipIndex);
    Q_INVOKABLE void addToSelection(int trackIndex, int clipIndex);
    Q_INVOKABLE void setSelection(const QVariantList &pairs);
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
    Q_INVOKABLE void splitSelectedClipLeft();
    Q_INVOKABLE void splitSelectedClipRight();
    Q_INVOKABLE void splitAtPlayhead();
    Q_INVOKABLE void trimClipLeft(int trackIndex, int clipIndex, double newStart);
    Q_INVOKABLE void trimClipRight(int trackIndex, int clipIndex, double newEnd);
    Q_INVOKABLE void setClipTrim(int trackIndex, int clipIndex, double inPoint, double outPoint);
    Q_INVOKABLE void setClipStart(int trackIndex, int clipIndex, double start);
    Q_INVOKABLE void setClipDuration(int trackIndex, int clipIndex, double duration);
    Q_INVOKABLE void setClipTextContent(int trackIndex, int clipIndex, const QString &text);
    Q_INVOKABLE void setSubtitleCues(int trackIndex, int clipIndex, const QVariantList &cues);
    Q_INVOKABLE void previewSetSubtitleCues(int trackIndex, int clipIndex, const QVariantList &cues);
    Q_INVOKABLE double subtitleLocalPlayheadSeconds(int trackIndex, int clipIndex) const;
    Q_INVOKABLE void upsertSubtitleCueAtPlayhead(int trackIndex, int clipIndex, const QString &text);
    Q_INVOKABLE void seekToSubtitleCue(int trackIndex, int clipIndex, int cueIndex);
    Q_INVOKABLE void setTextStyle(int trackIndex, int clipIndex, const QVariantMap &style);
    Q_INVOKABLE void applyTextPreset(int trackIndex, int clipIndex, const QString &presetId);
    Q_INVOKABLE QVariantList textPresets() const;
    Q_INVOKABLE QVariantList fontCatalog() const;
    Q_INVOKABLE QVariantList fontCategories() const;
    Q_INVOKABLE void setClipBlendMode(int trackIndex, int clipIndex, const QString &mode);
    Q_INVOKABLE void setClipSpeed(int trackIndex, int clipIndex, double speed);
    Q_INVOKABLE void setClipReverse(int trackIndex, int clipIndex, bool reverse);
    Q_INVOKABLE void setClipFlip(int trackIndex, int clipIndex, bool flipH, bool flipV);
    Q_INVOKABLE void setClipRotationSnap(int trackIndex, int clipIndex, double degrees);
    Q_INVOKABLE bool canMergeSelection() const;
    Q_INVOKABLE void mergeSelectedClips();
    Q_INVOKABLE void setClipMask(int trackIndex, int clipIndex, const QVariantMap &mask);
    Q_INVOKABLE void setClipFade(int trackIndex, int clipIndex, double fadeInSeconds, double fadeOutSeconds);
    Q_INVOKABLE void setClipFadeCurve(int trackIndex, int clipIndex, const QString &curve);
    Q_INVOKABLE void addTransition(int trackIndex, int clipIndex, const QString &kind, double durationSeconds);
    Q_INVOKABLE void removeTransition(int trackIndex, const QString &transitionId);
    Q_INVOKABLE void setTransitionDuration(int trackIndex, const QString &transitionId, double durationSeconds);
    Q_INVOKABLE void setTransitionKind(int trackIndex, const QString &transitionId, const QString &kind);
    Q_INVOKABLE void setTransitionParam(int trackIndex, const QString &transitionId, const QString &key,
                                        double value);
    Q_INVOKABLE void previewSetTransitionParam(int trackIndex, const QString &transitionId,
                                               const QString &key, double value);
    Q_INVOKABLE QVariantMap transitionBetweenClips(int trackIndex, int clipIndex) const;
    Q_INVOKABLE QVariantList transitionKinds() const;
    Q_INVOKABLE QVariantList transitionCategories() const;
    Q_INVOKABLE void selectTransition(int trackIndex, int leftClipIndex);
    Q_INVOKABLE void clearTransitionSelection();
    Q_INVOKABLE void setClipKeyframe(int trackIndex, int clipIndex, const QString &prop, double atSeconds,
                                     double value);
    Q_INVOKABLE void removeClipKeyframe(int trackIndex, int clipIndex, const QString &prop, double atSeconds);
    Q_INVOKABLE void previewMoveClipKeyframe(int trackIndex, int clipIndex, const QString &prop,
                                             double fromSeconds, double toSeconds, double value);
    Q_INVOKABLE QVariantList clipKeyframes(int trackIndex, int clipIndex, const QString &prop) const;
    Q_INVOKABLE void setKeyframeInterpolation(int trackIndex, int clipIndex, const QString &prop,
                                              const QString &mode);
    Q_INVOKABLE void resetClipTransform(int trackIndex, int clipIndex);
    Q_INVOKABLE QVariantList effectCatalog() const;
    Q_INVOKABLE QVariantList effectCategories() const;
    Q_INVOKABLE void addEffect(int trackIndex, int clipIndex, const QString &effectId);
    Q_INVOKABLE void removeEffect(int trackIndex, int clipIndex, int effectIndex);
    Q_INVOKABLE void setEffectParam(int trackIndex, int clipIndex, int effectIndex, const QString &key,
                                    double value);
    Q_INVOKABLE void setTrackMuted(int trackIndex, bool muted);
    Q_INVOKABLE void setTrackHidden(int trackIndex, bool hidden);
    Q_INVOKABLE bool trackMuted(int trackIndex) const;
    Q_INVOKABLE bool trackHidden(int trackIndex) const;
    Q_INVOKABLE void addBookmark(double seconds, const QString &label);
    Q_INVOKABLE void removeBookmark(int index);
    Q_INVOKABLE void goToBookmark(int index);
    Q_INVOKABLE void freezeFrameAtPlayhead();
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void cutSelection();
    Q_INVOKABLE void pasteAtPlayhead();
    Q_INVOKABLE void nudgeSelection(double deltaSeconds);
    Q_INVOKABLE bool selectionContains(int trackIndex, int clipIndex) const;
    Q_INVOKABLE QString shortcutFor(const QString &actionId) const;
    Q_INVOKABLE void setShortcut(const QString &actionId, const QString &keys);
    Q_INVOKABLE void triggerAction(const QString &actionId);
    Q_INVOKABLE void togglePlayback();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE double snapTime(double seconds) const;
    Q_INVOKABLE QVariantList waveformPeaks(const QString &path) const;
    Q_INVOKABLE void saveProject(const QUrl &url);
    Q_INVOKABLE void loadProject(const QUrl &url);
    Q_INVOKABLE void newProject();
    Q_INVOKABLE void openRecentProject(const QString &path);
    Q_INVOKABLE void clearRecentProjects();
    Q_INVOKABLE void restoreAutosave();
    Q_INVOKABLE void discardAutosave();
    Q_INVOKABLE QVariantList exportPresets() const;
    Q_INVOKABLE void exportProject(const QUrl &outputUrl);
    Q_INVOKABLE void exportWithPreset(const QUrl &outputUrl, const QString &presetId);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE QUrl fileUrl(const QString &path) const;
    Q_INVOKABLE QString imageUrl(const QString &path) const;

signals:
    void tracksChanged();
    void playheadSecondsChanged();
    void playingChanged();
    void snapEnabledChanged();
    void rippleEnabledChanged();
    void autoKeyEnabledChanged();
    void keyframeGraphPropertyChanged();
    void subtitleEditingChanged();
    void selectedSubtitleCueChanged();
    void undoStackChanged();
    void exportInProgressChanged();
    void exportProgressChanged();
    void selectionChanged();
    void selectedClipDataChanged();
    void selectedTransitionDataChanged();
    void bookmarksChanged();
    void projectNameChanged();
    void lastMessageChanged();
    void draggingAssetIndexChanged();
    void exportFinished(bool success);
    void projectMutated();
    void waveformReady(const QString &path);
    void guidesChanged();
    void shortcutsChanged();
    void backgroundChanged();
    void dirtyChanged();
    void currentProjectPathChanged();
    void recoveryChanged();
    void recentProjectsChanged();
    void transformBlocked(const QString &reason);

protected:
    void pushProjectEdit(const drift::Project &before, const QString &text);
    void finishEdit(const QString &message);
    void setLastMessage(const QString &message);
    drift::TimeUs playheadUs() const { return m_playheadUs; }
    void setPlayheadUs(drift::TimeUs us);

    QVariantMap clipToMap(const drift::Clip &clip) const;
    int assetIndexForClip(const drift::Clip &clip) const;
    drift::TimeUs clipDurationForAssetIndex(int assetIndex) const;
    drift::TimeUs sourceDurationForClip(const drift::Clip &clip) const;
    void applyRippleShift(drift::Track &track, int fromClipIndex, drift::TimeUs delta);
    void restoreFilmstripsAfterLoad();
    void normalizeSelection();
    bool isValidClipIndex(int trackIndex, int clipIndex) const;

    QByteArray serializeProjectJson() const;
    bool applyProjectJson(const QByteArray &data, QString *error);
    void setDirty(bool dirty);
    void setCurrentProjectPath(const QString &path);
    void addRecentProject(const QString &path);
    void writeRecoveryFile();
    void deleteRecoveryFile();
    void detectRecoveryFile();
    static QString recoveryFilePath();

    AssetLibrary *m_assetLibrary = nullptr;
    TimelineModel m_timelineModel;
    ClipListModel m_clipListModel;
    // m_project must outlive m_playback: the playback engine's compositor thread
    // holds a bare pointer to it and may still be mid-composite at teardown.
    // Members are destroyed in reverse declaration order, so the project is
    // declared first and torn down last.
    drift::Project m_project;
    PlaybackEngine m_playback;
    QUndoStack m_undoStack;
    drift::TimeUs m_playheadUs = 0;
    bool m_playing = false;
    bool m_snapEnabled = true;
    bool m_rippleEnabled = false;
    bool m_autoKeyEnabled = true;
    QString m_keyframeGraphProperty = QStringLiteral("x");
    bool m_subtitleEditing = false;
    int m_selectedSubtitleCue = -1;
    bool m_exportInProgress = false;
    double m_exportProgress = 0.0;
    QAtomicInt m_exportCancel = 0;
    int m_selectedTrack = -1;
    int m_selectedClip = -1;
    int m_selectedTransitionTrack = -1;
    int m_selectedTransitionLeftClip = -1;
    QList<QPair<int, int>> m_selection;
    bool m_guidesEnabled = false;
    QString m_guideType = QStringLiteral("thirds");
    QHash<QString, QString> m_shortcuts;
    int m_draggingAssetIndex = -1;
    QString m_lastMessage;
    bool m_previewDragActive = false;
    drift::Project m_previewDragBefore;
    QString m_previewDragText = QStringLiteral("Edit clip");
    void emitPreviewFrame();
    struct ClipboardItem
    {
        drift::Clip clip;
        drift::TrackType trackType = drift::TrackType::Video;
    };
    QList<ClipboardItem> m_clipboard;

    // Waveform peaks are expensive (full-file decode); compute once off-thread
    // and cache by path so timeline refreshes don't re-decode on the GUI thread.
    mutable QHash<QString, QVariantList> m_waveformCache;
    mutable QSet<QString> m_waveformPending;

    // Save state / autosave / crash recovery.
    QString m_currentProjectPath;
    bool m_dirty = false;
    QTimer *m_autosaveTimer = nullptr;
    bool m_recoveryAvailable = false;
    QVariantMap m_recoveryInfo;

    static constexpr int kMaxUndoSteps = 50;
    static constexpr int kAutosaveIntervalMs = 15000;
    static constexpr int kMaxRecentProjects = 10;
};
