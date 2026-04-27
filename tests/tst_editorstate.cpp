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

QTEST_MAIN(EditorStateTest)
#include "tst_editorstate.moc"
