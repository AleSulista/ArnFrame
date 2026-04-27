#include <QtTest>

#include "models/EditorState.h"
#include "models/AssetLibrary.h"

class EditorStateTest : public QObject
{
    Q_OBJECT

private slots:
    void snapTimeEnabled();
    void addTextClip();
};

void EditorStateTest::snapTimeEnabled()
{
    AssetLibrary library;
    EditorState state(&library);
    state.setSnapEnabled(true);
    QCOMPARE(state.snapTime(0.0), 0.0);
    QVERIFY(state.snapTime(1.234) >= 0.0);
}

void EditorStateTest::addTextClip()
{
    AssetLibrary library;
    EditorState state(&library);
    state.addTextClip(QStringLiteral("Hello"), 0.0);
    QVERIFY(state.durationSeconds() > 0.0);
    QCOMPARE(state.selectedClip(), 0);
}

QTEST_MAIN(EditorStateTest)
#include "tst_editorstate.moc"
