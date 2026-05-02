#include <QtTest>

#include <QTemporaryFile>

#include "models/AppController.h"
#include "models/AssetLibrary.h"

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
    void multiSelectClipboardGuidesAndShortcuts();
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
    QVERIFY(!state.trackMuted(1));
    state.setTrackMuted(1, true);
    QVERIFY(state.trackMuted(1));
    QVERIFY(state.undoAvailable());
    state.undo();
    QVERIFY(!state.trackMuted(1));
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
    state.setTrackMuted(1, true);
    state.addBookmark(2.0, QStringLiteral("Mark"));

    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.close();

    state.saveProject(QUrl::fromLocalFile(tempFile.fileName()));
    state.loadProject(QUrl::fromLocalFile(tempFile.fileName()));

    QVERIFY(state.durationSeconds() > 0.0);
    QVERIFY(state.trackMuted(1));
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
    QCOMPARE(effects.first().toMap().value(QStringLiteral("catalogId")).toString(),
             QStringLiteral("adjust.contrast"));

    state.setEffectParam(track, clip, 0, QStringLiteral("contrast"), 2.5);
    effects = state.selectedClipData().value(QStringLiteral("effects")).toList();
    const QVariantList params = effects.first().toMap().value(QStringLiteral("params")).toList();
    QCOMPARE(params.first().toMap().value(QStringLiteral("value")).toDouble(), 2.5);

    state.removeEffect(track, clip, 0);
    QCOMPARE(state.selectedClipData().value(QStringLiteral("effects")).toList().size(), 0);
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
    state.triggerAction(QStringLiteral("toggleGuides"));
    QCOMPARE(state.guidesEnabled(), false);
}

QTEST_MAIN(EditorStateTest)
#include "tst_editorstate.moc"
