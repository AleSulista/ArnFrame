#include <QtTest>

#include <QJsonArray>
#include <QJsonObject>

#include "core/Keyframe.h"
#include "core/Project.h"
#include "core/TimelineOps.h"

class CoreTest : public QObject
{
    Q_OBJECT

private slots:
    void timeConversion();
    void keyframeHoldInterpolation();
    void keyframeLinearInterpolation();
    void projectSerializationRoundTrip();
    void projectLoadsLegacyV1Format();
    void trackAllowsClipTypes();
};

void CoreTest::timeConversion()
{
    QCOMPARE(drift::secondsToUs(1.0), drift::TimeUs{1'000'000});
    QCOMPARE(drift::usToSeconds(2'500'000), 2.5);
    QCOMPARE(drift::frameDurationUs(30), drift::TimeUs{33'333});
}

void CoreTest::keyframeHoldInterpolation()
{
    drift::KeyframeTrack<double> track;
    track.setInterpolation(drift::Interpolation::Hold);
    track.setKeyframe(0, 0.0);
    track.setKeyframe(drift::secondsToUs(2.0), 1.0);
    QCOMPARE(track.evaluateAt(drift::secondsToUs(1.5)), 0.0);
    QCOMPARE(track.evaluateAt(drift::secondsToUs(2.0)), 1.0);
}

void CoreTest::keyframeLinearInterpolation()
{
    drift::KeyframeTrack<double> track;
    track.setKeyframe(0, 0.0);
    track.setKeyframe(drift::secondsToUs(2.0), 1.0);
    QCOMPARE(track.evaluateAt(drift::secondsToUs(1.0)), 0.5);
}

void CoreTest::projectSerializationRoundTrip()
{
    drift::Project project;
    project.setName(QStringLiteral("Test Project"));
    project.setFps(24);
    project.setResolution(1280, 720);

    drift::MediaAsset asset;
    asset.name = QStringLiteral("clip.mp4");
    asset.kind = drift::MediaKind::Video;
    asset.path = QStringLiteral("/tmp/clip.mp4");
    asset.durationUs = drift::secondsToUs(10.0);
    const QString assetId = project.addAsset(asset);

    drift::Clip clip;
    clip.id = QStringLiteral("clip-1");
    clip.assetId = assetId;
    clip.type = drift::ClipType::Video;
    clip.name = asset.name;
    clip.path = asset.path;
    clip.timelineStart = drift::secondsToUs(1.0);
    clip.timelineDuration = drift::secondsToUs(5.0);
    clip.srcIn = 0;
    clip.srcOut = drift::secondsToUs(5.0);
    project.tracks()[0].clips.append(clip);

    project.bookmarks().append({.timeUs = drift::secondsToUs(3.0), .label = QStringLiteral("Mark")});

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    QCOMPARE(loaded.name(), project.name());
    QCOMPARE(loaded.fps(), 24);
    QCOMPARE(loaded.width(), 1280);
    QCOMPARE(loaded.tracks().size(), 3);
    QCOMPARE(loaded.tracks()[0].clips.size(), 1);
    QCOMPARE(loaded.tracks()[0].clips[0].timelineStart, clip.timelineStart);
    QCOMPARE(loaded.bookmarks().size(), 1);
    QCOMPARE(loaded.bookmarks()[0].label, QStringLiteral("Mark"));
}

void CoreTest::projectLoadsLegacyV1Format()
{
    const QJsonObject root{
        {QStringLiteral("version"), 1},
        {QStringLiteral("projectName"), QStringLiteral("Legacy")},
        {QStringLiteral("assets"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("name"), QStringLiteral("a.mp4")},
                 {QStringLiteral("kind"), QStringLiteral("video")},
                 {QStringLiteral("durationSeconds"), 12.0},
                 {QStringLiteral("path"), QStringLiteral("/tmp/a.mp4")},
             },
         }},
        {QStringLiteral("tracks"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("type"), QStringLiteral("video")},
                 {QStringLiteral("clips"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("name"), QStringLiteral("a.mp4")},
                          {QStringLiteral("kind"), QStringLiteral("video")},
                          {QStringLiteral("path"), QStringLiteral("/tmp/a.mp4")},
                          {QStringLiteral("start"), 1.0},
                          {QStringLiteral("duration"), 4.0},
                          {QStringLiteral("inPoint"), 0.5},
                          {QStringLiteral("outPoint"), 4.5},
                          {QStringLiteral("assetIndex"), 0},
                      },
                  }},
             },
             QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}},
             QJsonObject{{QStringLiteral("type"), QStringLiteral("audio")}},
         }},
    };

    QString error;
    const drift::Project project = drift::Project::fromJson(root, &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(project.name(), QStringLiteral("Legacy"));
    QCOMPARE(project.tracks()[0].clips.size(), 1);
    QCOMPARE(project.tracks()[0].clips[0].timelineStart, drift::secondsToUs(1.0));
    QCOMPARE(project.tracks()[0].clips[0].srcIn, drift::secondsToUs(0.5));
    QVERIFY(!project.tracks()[0].clips[0].assetId.isEmpty());
}

void CoreTest::trackAllowsClipTypes()
{
    drift::Track videoTrack{.type = drift::TrackType::Video};
    QVERIFY(videoTrack.allowsClipType(drift::ClipType::Video));
    QVERIFY(videoTrack.allowsClipType(drift::ClipType::Image));
    QVERIFY(!videoTrack.allowsClipType(drift::ClipType::Audio));

    drift::Track audioTrack{.type = drift::TrackType::Audio};
    QVERIFY(audioTrack.allowsClipType(drift::ClipType::Audio));
    QVERIFY(!audioTrack.allowsClipType(drift::ClipType::Video));
}

QTEST_MAIN(CoreTest)
#include "tst_core.moc"
