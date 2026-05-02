#include <QtTest>

#include <QColor>
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
    void clipTransformSerialization();
    void volumeKeyframeSerialization();
    void projectLoadsLegacyV1Format();
    void trackAllowsClipTypes();
    void insertTrackAtTopAllowsDuplicateTypes();
    void textStyleAndBlendModeSerialization();
    void shapeStyleSerialization();
    void effectCatalogIdSerialization();
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
    QCOMPARE(loaded.tracks().size(), 1);
    QCOMPARE(loaded.tracks()[0].clips.size(), 1);
    QCOMPARE(loaded.tracks()[0].clips[0].timelineStart, clip.timelineStart);
    QCOMPARE(loaded.bookmarks().size(), 1);
    QCOMPARE(loaded.bookmarks()[0].label, QStringLiteral("Mark"));
}

void CoreTest::clipTransformSerialization()
{
    drift::Project project;
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Text});

    drift::Clip clip;
    clip.id = QStringLiteral("clip-transform");
    clip.type = drift::ClipType::Text;
    clip.name = QStringLiteral("Title");
    clip.textContent = QStringLiteral("Hello");
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(3.0);
    clip.posX.setKeyframe(0, 0.25);
    clip.posY.setKeyframe(0, 0.75);
    clip.scale.setKeyframe(0, 1.5);
    clip.rotation.setKeyframe(0, 45.0);
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    QCOMPARE(loadedClip.posX.evaluateAt(0), 0.25);
    QCOMPARE(loadedClip.posY.evaluateAt(0), 0.75);
    QCOMPARE(loadedClip.scale.evaluateAt(0), 1.5);
    QCOMPARE(loadedClip.rotation.evaluateAt(0), 45.0);
}

void CoreTest::volumeKeyframeSerialization()
{
    drift::Project project;
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Audio});

    drift::Clip clip;
    clip.id = QStringLiteral("clip-volume");
    clip.type = drift::ClipType::Audio;
    clip.name = QStringLiteral("Audio");
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(4.0);
    clip.volume.setKeyframe(0, 1.0);
    clip.volume.setKeyframe(drift::secondsToUs(2.0), 0.5);
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    QCOMPARE(loadedClip.volume.evaluateAt(0), 1.0);
    QCOMPARE(loadedClip.volume.evaluateAt(drift::secondsToUs(2.0)), 0.5);
    QCOMPARE(loadedClip.volume.evaluateAt(drift::secondsToUs(1.0)), 0.75);
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
    QVERIFY(!videoTrack.allowsClipType(drift::ClipType::Image));
    QVERIFY(!videoTrack.allowsClipType(drift::ClipType::Audio));

    drift::Track audioTrack{.type = drift::TrackType::Audio};
    QVERIFY(audioTrack.allowsClipType(drift::ClipType::Audio));
    QVERIFY(!audioTrack.allowsClipType(drift::ClipType::Video));

    drift::Track shapeTrack{.type = drift::TrackType::Shape};
    QVERIFY(shapeTrack.allowsClipType(drift::ClipType::Image));
    QVERIFY(shapeTrack.allowsClipType(drift::ClipType::Shape));
    QVERIFY(!shapeTrack.allowsClipType(drift::ClipType::Video));
}

void CoreTest::insertTrackAtTopAllowsDuplicateTypes()
{
    drift::Project project;
    QCOMPARE(project.tracks().size(), 1);
    QCOMPARE(project.tracks()[0].type, drift::TrackType::Video);

    const int first = drift::insertTrackAtTopForClipType(project, drift::ClipType::Video);
    QCOMPARE(first, 0);
    QCOMPARE(project.tracks().size(), 2);
    QCOMPARE(project.tracks()[0].type, drift::TrackType::Video);
    QCOMPARE(project.tracks()[1].type, drift::TrackType::Video);

    const int second = drift::insertTrackAtTopForClipType(project, drift::ClipType::Audio);
    QCOMPARE(second, 0);
    QCOMPARE(project.tracks().size(), 3);
    QCOMPARE(project.tracks()[0].type, drift::TrackType::Audio);
}

void CoreTest::textStyleAndBlendModeSerialization()
{
    drift::Project project;
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Text});

    drift::Clip clip;
    clip.id = QStringLiteral("clip-textstyle");
    clip.type = drift::ClipType::Text;
    clip.name = QStringLiteral("Title");
    clip.textContent = QStringLiteral("Hello");
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(3.0);
    clip.blendMode = drift::BlendMode::Multiply;
    clip.textStyle.fontFamily = QStringLiteral("Courier New");
    clip.textStyle.pixelSize = 88;
    clip.textStyle.color = QColor(10, 20, 30, 200);
    clip.textStyle.bold = false;
    clip.textStyle.italic = true;
    clip.textStyle.align = drift::TextAlign::Right;
    clip.textStyle.outlineWidth = 2.5;
    clip.textStyle.outlineColor = QColor(255, 0, 0);
    clip.textStyle.boxEnabled = true;
    clip.textStyle.boxColor = QColor(0, 0, 0, 100);
    clip.textStyle.boxPadding = 12.0;
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    QCOMPARE(loadedClip.blendMode, drift::BlendMode::Multiply);
    QCOMPARE(loadedClip.textStyle.fontFamily, QStringLiteral("Courier New"));
    QCOMPARE(loadedClip.textStyle.pixelSize, 88);
    QCOMPARE(loadedClip.textStyle.color, QColor(10, 20, 30, 200));
    QCOMPARE(loadedClip.textStyle.bold, false);
    QCOMPARE(loadedClip.textStyle.italic, true);
    QCOMPARE(loadedClip.textStyle.align, drift::TextAlign::Right);
    QCOMPARE(loadedClip.textStyle.outlineWidth, 2.5);
    QCOMPARE(loadedClip.textStyle.outlineColor, QColor(255, 0, 0));
    QCOMPARE(loadedClip.textStyle.boxEnabled, true);
    QCOMPARE(loadedClip.textStyle.boxColor, QColor(0, 0, 0, 100));
    QCOMPARE(loadedClip.textStyle.boxPadding, 12.0);
}

void CoreTest::shapeStyleSerialization()
{
    drift::Project project;
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Shape});

    drift::Clip clip;
    clip.id = QStringLiteral("clip-shape");
    clip.type = drift::ClipType::Shape;
    clip.name = QStringLiteral("Hexagon");
    clip.timelineStart = 0;
    clip.timelineDuration = drift::kImageClipDurationUs;
    clip.shapeStyle.kind = drift::ShapeKind::Hexagon;
    clip.shapeStyle.fill = QColor(10, 20, 30, 200);
    clip.shapeStyle.stroke = QColor(255, 255, 255);
    clip.shapeStyle.strokeWidth = 6.0;
    clip.posX.setKeyframe(0, 0.25);
    clip.posY.setKeyframe(0, 0.75);
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    QCOMPARE(loadedClip.type, drift::ClipType::Shape);
    QCOMPARE(loadedClip.shapeStyle.kind, drift::ShapeKind::Hexagon);
    QCOMPARE(loadedClip.shapeStyle.fill, QColor(10, 20, 30, 200));
    QCOMPARE(loadedClip.shapeStyle.stroke, QColor(255, 255, 255));
    QCOMPARE(loadedClip.shapeStyle.strokeWidth, 6.0);
    QCOMPARE(loadedClip.posX.evaluateAt(0), 0.25);
    QCOMPARE(loadedClip.posY.evaluateAt(0), 0.75);
}

void CoreTest::effectCatalogIdSerialization()
{
    drift::Project project;
    drift::Clip clip;
    clip.id = QStringLiteral("clip-effects");
    clip.type = drift::ClipType::Video;
    clip.name = QStringLiteral("Video");
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(2.0);

    drift::Effect effect;
    effect.name = QStringLiteral("eq");
    effect.catalogId = QStringLiteral("adjust.contrast");
    effect.parameters.insert(QStringLiteral("contrast"), 1.4);
    clip.effects.append(effect);
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    QCOMPARE(loadedClip.effects.size(), 1);
    QCOMPARE(loadedClip.effects[0].catalogId, QStringLiteral("adjust.contrast"));
    QCOMPARE(loadedClip.effects[0].parameters.value(QStringLiteral("contrast")).toDouble(), 1.4);
}

QTEST_MAIN(CoreTest)
#include "tst_core.moc"
