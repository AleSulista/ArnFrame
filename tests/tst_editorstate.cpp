#include <QtTest>

#include "models/AppController.h"
#include "models/AssetLibrary.h"

class EditorStateTest : public QObject
{
    Q_OBJECT

private slots:
    void snapTimeEnabled();
    void addTextClip();
    void undoRedoClipAdd();
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

QTEST_MAIN(EditorStateTest)
#include "tst_editorstate.moc"
