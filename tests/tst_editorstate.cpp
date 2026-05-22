#include <QtTest>

#include <QSet>
#include <QTemporaryFile>

#include "models/AppController.h"
#include "models/AssetLibrary.h"

#include "core/Clip.h"
#include "core/Project.h"
#include "core/Track.h"

class EditorStateTest : public QObject
{
    Q_OBJECT

private slots:
    void snapTimeEnabled();
    void addTextClip();
    void undoRedoClipAdd();
    void undoTrackMute();
    void undoBookmarkAdd();
    void projectPersistenceRoundTrip();
    void textStyleBlendModeKeyframesAndEffects();
    void effectBrowserCategoriesAndApply();
    void multiSelectClipboardGuidesAndShortcuts();
    void addTransitionBetweenAdjacentClips();
    void setTransitionKindAndDurationPersist();
};

void EditorStateTest::snapTimeEnabled()
{
    AssetLibrary library;
    AppController state(&library);
    state.setSnapEnabled(true);
    QCOMPARE(state.snapTime(0.0), 0.0);
    QVERIFY(state.snapTime(1.234) >= 0.0);
}

void EditorStateTest::addTextClip()
{
    AssetLibrary library;
    AppController state(&library);
    state.addTextClip(QStringLiteral("Hello"), 0.0);
    QVERIFY(state.durationSeconds() > 0.0);
    QCOMPARE(state.selectedClip(), 0);
}

void EditorStateTest::undoRedoClipAdd()
{
    AssetLibrary library;
    AppController state(&library);
    state.addTextClip(QStringLiteral("Undo me"), 0.0);
    QVERIFY(state.undoAvailable());
    state.undo();
    QCOMPARE(state.durationSeconds(), 0.0);
    QVERIFY(state.redoAvailable());
    state.redo();
    QVERIFY(state.durationSeconds() > 0.0);
}

void EditorStateTest::undoTrackMute()
{
    AssetLibrary library;
    AppController state(&library);
    QVERIFY(!state.trackMuted(0));
    state.setTrackMuted(0, true);
    QVERIFY(state.trackMuted(0));
    QVERIFY(state.undoAvailable());
    state.undo();
    QVERIFY(!state.trackMuted(0));
}

void EditorStateTest::undoBookmarkAdd()
{
    AssetLibrary library;
    AppController state(&library);
    QCOMPARE(state.bookmarks().size(), 0);
    state.addBookmark(1.5, QStringLiteral("Test"));
    QCOMPARE(state.bookmarks().size(), 1);
    QVERIFY(state.undoAvailable());
    state.undo();
    QCOMPARE(state.bookmarks().size(), 0);
}

void EditorStateTest::projectPersistenceRoundTrip()
{
    AssetLibrary library;
    AppController state(&library);
    state.addTextClip(QStringLiteral("Persist"), 0.0);
    state.setTrackMuted(0, true);
    state.addBookmark(2.0, QStringLiteral("Mark"));

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.close();

    state.saveProject(QUrl::fromLocalFile(tempFile.fileName()));
    state.loadProject(QUrl::fromLocalFile(tempFile.fileName()));

    QVERIFY(state.durationSeconds() > 0.0);
    QVERIFY(state.trackMuted(0));
    QCOMPARE(state.bookmarks().size(), 1);
}

void EditorStateTest::textStyleBlendModeKeyframesAndEffects()
{
    AssetLibrary library;
    AppController state(&library);
    state.addTextClip(QStringLiteral("Hello"), 0.0);

    const int track = state.selectedTrack();
    const int clip = state.selectedClip();
    QVERIFY(track >= 0);
    QVERIFY(clip >= 0);

    // Text style: partial update only touches the given keys.
    state.setTextStyle(track, clip,
                       QVariantMap{{"pixelSize", 120}, {"bold", false}, {"color", QStringLiteral("#ffff0000")}});
    QVariantMap style = state.selectedClipData().value(QStringLiteral("textStyle")).toMap();
    QCOMPARE(style.value(QStringLiteral("pixelSize")).toInt(), 120);
    QCOMPARE(style.value(QStringLiteral("bold")).toBool(), false);
    QCOMPARE(style.value(QStringLiteral("color")).toString(), QStringLiteral("#ffff0000"));

    // Presets overwrite the whole style.
    state.applyTextPreset(track, clip, QStringLiteral("title"));
    style = state.selectedClipData().value(QStringLiteral("textStyle")).toMap();
    QCOMPARE(style.value(QStringLiteral("pixelSize")).toInt(), 96);
    QCOMPARE(style.value(QStringLiteral("bold")).toBool(), true);

    // Blend mode.
    state.setClipBlendMode(track, clip, QStringLiteral("multiply"));
    QCOMPARE(state.selectedClipData().value(QStringLiteral("blendMode")).toString(), QStringLiteral("multiply"));

    // Keyframes: add, list, remove.
    state.setClipKeyframe(track, clip, QStringLiteral("opacity"), 0.0, 0.5);
    QVariantList keyframes = state.clipKeyframes(track, clip, QStringLiteral("opacity"));
    QCOMPARE(keyframes.size(), 1);
    QCOMPARE(keyframes.first().toMap().value(QStringLiteral("value")).toDouble(), 0.5);

    state.removeClipKeyframe(track, clip, QStringLiteral("opacity"), 0.0);
    QCOMPARE(state.clipKeyframes(track, clip, QStringLiteral("opacity")).size(), 0);

    // Effect catalog wiring: add a known effect, tweak its param, remove it.
    const QVariantList catalog = state.effectCatalog();
    QVERIFY(!catalog.isEmpty());

    state.addEffect(track, clip, QStringLiteral("adjust.contrast"));
    QVariantList effects = state.selectedClipData().value(QStringLiteral("effects")).toList();
    QCOMPARE(effects.size(), 1);
    QCOMPARE(state.selectedClipEffects().size(), 1);
    QCOMPARE(effects.first().toMap().value(QStringLiteral("catalogId")).toString(),
             QStringLiteral("adjust.contrast"));

    state.setEffectParam(track, clip, 0, QStringLiteral("contrast"), 2.5);
    effects = state.selectedClipData().value(QStringLiteral("effects")).toList();
    const QVariantList params = effects.first().toMap().value(QStringLiteral("params")).toList();
    QCOMPARE(params.first().toMap().value(QStringLiteral("value")).toDouble(), 2.5);

    state.removeEffect(track, clip, 0);
    QCOMPARE(state.selectedClipData().value(QStringLiteral("effects")).toList().size(), 0);
    QCOMPARE(state.selectedClipEffects().size(), 0);
}

void EditorStateTest::effectBrowserCategoriesAndApply()
{
    AssetLibrary library;
    AppController state(&library);

    const QVariantList categories = state.effectCategories();
    QCOMPARE(categories.size(), 4);
    QCOMPARE(categories.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("glitch"));
    QCOMPARE(categories.first().toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("Glitch & Distortion"));

    const QVariantList catalog = state.effectCatalog();
    QVERIFY(catalog.size() >= 16);

    QSet<QString> categoryIds;
    for (const QVariant &category : categories)
        categoryIds.insert(category.toMap().value(QStringLiteral("id")).toString());

    for (const QVariant &entry : catalog) {
        const QVariantMap preset = entry.toMap();
        QVERIFY(categoryIds.contains(preset.value(QStringLiteral("category")).toString()));
        QVERIFY(!preset.value(QStringLiteral("categoryLabel")).toString().isEmpty());
    }

    state.addTextClip(QStringLiteral("FX"), 0.0);
    const int track = state.selectedTrack();
    const int clip = state.selectedClip();
    QVERIFY(track >= 0);
    QVERIFY(clip >= 0);

    state.addEffect(track, clip, QStringLiteral("rgb_split"));
    QVariantList effects = state.selectedClipData().value(QStringLiteral("effects")).toList();
    QCOMPARE(effects.size(), 1);
    QCOMPARE(state.selectedClipEffects().size(), 1);
    QCOMPARE(effects.first().toMap().value(QStringLiteral("catalogId")).toString(),
             QStringLiteral("rgb_split"));
    QCOMPARE(effects.first().toMap().value(QStringLiteral("label")).toString(), QStringLiteral("RGB Split"));
    QCOMPARE(state.project()->tracks()[track].clips[clip].effects.size(), 1);

    state.removeEffect(track, clip, 0);
    QCOMPARE(state.selectedClipData().value(QStringLiteral("effects")).toList().size(), 0);
    QCOMPARE(state.selectedClipEffects().size(), 0);
    QCOMPARE(state.project()->tracks()[track].clips[clip].effects.size(), 0);
}

void EditorStateTest::multiSelectClipboardGuidesAndShortcuts()
{
    AssetLibrary library;
    AppController state(&library);

    state.addTextClip(QStringLiteral("A"), 0.0);
    state.addTextClip(QStringLiteral("B"), 2.0);

    const int track = state.selectedTrack();
    QVERIFY(track >= 0);

    // Build a two-clip selection.
    state.selectClip(track, 0);
    state.addToSelection(track, 1);
    QCOMPARE(state.selection().size(), 2);
    QVERIFY(state.selectionContains(track, 0));
    QVERIFY(state.selectionContains(track, 1));

    // Copy/paste at playhead keeps both clips.
    state.setPlayheadSeconds(10.0);
    state.copySelection();
    state.pasteAtPlayhead();
    QCOMPARE(state.tracks().at(track).toMap().value(QStringLiteral("clips")).toList().size(), 4);

    // Nudge and cut do not crash and remain undoable.
    state.nudgeSelection(0.25);
    QVERIFY(state.undoAvailable());
    state.cutSelection();
    QVERIFY(state.tracks().at(track).toMap().value(QStringLiteral("clips")).toList().size() <= 2);

    // Guides state is writable.
    state.setGuidesEnabled(true);
    QCOMPARE(state.guidesEnabled(), true);
    state.setGuideType(QStringLiteral("safe"));
    QCOMPARE(state.guideType(), QStringLiteral("safe"));

    // Shortcut/action layer wiring.
    state.setShortcut(QStringLiteral("nudgeRight"), QStringLiteral("Ctrl+Alt+Right"));
    QCOMPARE(state.shortcutFor(QStringLiteral("nudgeRight")), QStringLiteral("Ctrl+Alt+Right"));

    bool found = false;
    for (const QVariant &entry : state.actions()) {
        const QVariantMap action = entry.toMap();
        if (action.value(QStringLiteral("id")).toString() != QStringLiteral("nudgeRight"))
            continue;
        QCOMPARE(action.value(QStringLiteral("shortcut")).toString(), QStringLiteral("Ctrl+Alt+Right"));
        found = true;
    }
    QVERIFY(found);

    state.triggerAction(QStringLiteral("toggleGuides"));
    QCOMPARE(state.guidesEnabled(), false);
}

static void appendAdjacentShapeClips(drift::Project &project, drift::TimeUs gapUs = 0)
{
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Video});

    drift::Clip clipA;
    clipA.id = QStringLiteral("clip-a");
    clipA.type = drift::ClipType::Shape;
    clipA.timelineStart = 0;
    clipA.timelineDuration = drift::secondsToUs(2.0);

    drift::Clip clipB;
    clipB.id = QStringLiteral("clip-b");
    clipB.type = drift::ClipType::Shape;
    clipB.timelineStart = clipA.timelineEnd() + gapUs;
    clipB.timelineDuration = drift::secondsToUs(2.0);

    project.tracks()[0].clips.append(clipA);
    project.tracks()[0].clips.append(clipB);
}

void EditorStateTest::addTransitionBetweenAdjacentClips()
{
    AssetLibrary library;
    AppController state(&library);
    appendAdjacentShapeClips(*state.project(), 500);

    state.selectClip(0, 0);
    state.addTransition(0, 0, QStringLiteral("wipe_left"), 0.75);

    const QVariantMap transition = state.transitionBetweenClips(0, 0);
    QVERIFY(!transition.isEmpty());
    QCOMPARE(transition.value(QStringLiteral("kind")).toString(), QStringLiteral("wipe_left"));
    QCOMPARE(transition.value(QStringLiteral("duration")).toDouble(), 0.75);
}

void EditorStateTest::setTransitionKindAndDurationPersist()
{
    AssetLibrary library;
    AppController state(&library);
    appendAdjacentShapeClips(*state.project());

    state.addTransition(0, 0, QStringLiteral("crossfade"), 0.5);
    const QString transitionId = state.transitionBetweenClips(0, 0).value(QStringLiteral("id")).toString();
    QVERIFY(!transitionId.isEmpty());

    state.setTransitionKind(0, transitionId, QStringLiteral("zoom_in"));
    state.setTransitionDuration(0, transitionId, 1.25);

    const QVariantMap transition = state.transitionBetweenClips(0, 0);
    QCOMPARE(transition.value(QStringLiteral("kind")).toString(), QStringLiteral("zoom_in"));
    QCOMPARE(transition.value(QStringLiteral("duration")).toDouble(), 1.25);
}

QTEST_MAIN(EditorStateTest)
#include "tst_editorstate.moc"
