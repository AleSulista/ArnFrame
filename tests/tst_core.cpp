#include <QtTest>

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>

#include "core/Keyframe.h"
#include "core/Project.h"
#include "core/SubtitleCue.h"
#include "core/TimelineOps.h"
#include "core/Transition.h"

class CoreTest : public QObject
{
    Q_OBJECT

private slots:
    void timeConversion();
    void keyframeHoldInterpolation();
    void keyframeLinearInterpolation();
    void keyframeEaseInterpolation();
    void keyframeNearestQuery();
    void projectSerializationRoundTrip();
    void clipTransformSerialization();
    void legacyFractionalTransformMigration();
    void volumeKeyframeSerialization();
    void projectLoadsLegacyV1Format();
    void trackAllowsClipTypes();
    void subtitleCueSerialization();
    void subtitleCueLookup();
    void insertTrackAtTopAllowsDuplicateTypes();
    void multiTrackSerializationRoundTrip();
    void textStyleAndBlendModeSerialization();
    void legacyBoldMigratesToFontWeight();
    void textPresetsAreWellFormed();
    void shapeStyleSerialization();
    void effectCatalogIdSerialization();
    void rgbSplitEffectParametersSerialization();
    void blockGlitchEffectParametersSerialization();
    void clipSpeedSourceMapping();
    void clipReverseAndFlipSerialization();
    void clipSplitMergeRoundTrip();
    void maskAndTransitionSerialization();
    void allTransitionKindsRoundTrip();
    void transitionParametersRoundTrip();
    void legacyTransitionJsonStillLoads();
    void transitionAudioCurves();
    void physicalOverlapTransitionWindow();
    void backgroundSerialization();
    void fadeSerializationAndMultiplier();
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

void CoreTest::keyframeEaseInterpolation()
{
    drift::KeyframeTrack<double> track;
    track.setInterpolation(drift::Interpolation::Ease);
    track.setKeyframe(0, 0.0);
    track.setKeyframe(drift::secondsToUs(1.0), 10.0);
    // smoothstep(0.25) = 0.15625 → 1.5625, vs linear 2.5
    const double eased = track.evaluateAt(drift::secondsToUs(0.25));
    QVERIFY(eased < 2.4);
    QVERIFY(eased > 1.0);
    QCOMPARE(drift::interpolationToString(drift::Interpolation::Ease), QStringLiteral("ease"));
    QCOMPARE(drift::interpolationFromString(QStringLiteral("ease")), drift::Interpolation::Ease);
}

void CoreTest::keyframeNearestQuery()
{
    drift::KeyframeTrack<double> track;
    track.setKeyframe(drift::secondsToUs(1.0), 5.0);
    QCOMPARE(track.nearestKeyframe(drift::secondsToUs(1.01), drift::secondsToUs(0.05)),
             drift::secondsToUs(1.0));
    QCOMPARE(track.nearestKeyframe(drift::secondsToUs(2.0), drift::secondsToUs(0.05)), drift::TimeUs{-1});
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
    clip.transformX.setKeyframe(0, 100.0);
    clip.transformY.setKeyframe(0, 200.0);
    clip.transformW.setKeyframe(0, 640.0);
    clip.transformH.setKeyframe(0, 360.0);
    clip.rotation.setKeyframe(0, 45.0);
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    QCOMPARE(loadedClip.transformX.evaluateAt(0), 100.0);
    QCOMPARE(loadedClip.transformY.evaluateAt(0), 200.0);
    QCOMPARE(loadedClip.transformW.evaluateAt(0), 640.0);
    QCOMPARE(loadedClip.transformH.evaluateAt(0), 360.0);
    QCOMPARE(loadedClip.rotation.evaluateAt(0), 45.0);
}

void CoreTest::legacyFractionalTransformMigration()
{
    // Old projects stored center-normalized posX/posY + scale; load them as
    // top-left pixel layout on the project canvas.
    auto kf = [](double value) {
        return QJsonObject{
            {QStringLiteral("interpolation"), QStringLiteral("linear")},
            {QStringLiteral("keyframes"),
             QJsonArray{QJsonObject{{QStringLiteral("timeUs"), 0.0},
                                    {QStringLiteral("value"), value}}}},
        };
    };
    const QJsonObject root{
        {QStringLiteral("version"), 2},
        {QStringLiteral("projectName"), QStringLiteral("LegacyTransform")},
        {QStringLiteral("fps"), 30},
        {QStringLiteral("width"), 1920},
        {QStringLiteral("height"), 1080},
        {QStringLiteral("tracks"),
         QJsonArray{
             QJsonObject{
                 {QStringLiteral("type"), QStringLiteral("video")},
                 {QStringLiteral("clips"),
                  QJsonArray{
                      QJsonObject{
                          {QStringLiteral("id"), QStringLiteral("legacy-clip")},
                          {QStringLiteral("type"), QStringLiteral("video")},
                          {QStringLiteral("name"), QStringLiteral("v")},
                          {QStringLiteral("timelineStartUs"), 0},
                          {QStringLiteral("timelineDurationUs"), 1000000},
                          {QStringLiteral("srcInUs"), 0},
                          {QStringLiteral("srcOutUs"), 1000000},
                          {QStringLiteral("posX"), kf(0.5)},
                          {QStringLiteral("posY"), kf(0.5)},
                          {QStringLiteral("scale"), kf(1.0)},
                      },
                  }},
             },
         }},
    };

    QString error;
    const drift::Project loaded = drift::Project::fromJson(root, &error);
    QVERIFY(error.isEmpty());
    QVERIFY(!loaded.tracks().isEmpty());
    QVERIFY(!loaded.tracks()[0].clips.isEmpty());
    const drift::Clip &clip = loaded.tracks()[0].clips[0];
    QCOMPARE(clip.transformW.evaluateAt(0), 1920.0);
    QCOMPARE(clip.transformH.evaluateAt(0), 1080.0);
    QCOMPARE(clip.transformX.evaluateAt(0), 0.0);
    QCOMPARE(clip.transformY.evaluateAt(0), 0.0);
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

    drift::Track subtitleTrack{.type = drift::TrackType::Subtitle};
    QVERIFY(subtitleTrack.allowsClipType(drift::ClipType::Subtitle));
    QVERIFY(!subtitleTrack.allowsClipType(drift::ClipType::Text));
}

void CoreTest::subtitleCueSerialization()
{
    drift::Project project;
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Subtitle});

    drift::Clip clip;
    clip.id = QStringLiteral("clip-subtitle");
    clip.type = drift::ClipType::Subtitle;
    clip.name = QStringLiteral("Subtitles (2)");
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(30.0);
    clip.subtitleCues = {
        {drift::secondsToUs(1.0), drift::secondsToUs(4.0), QStringLiteral("Hello")},
        {drift::secondsToUs(5.0), drift::secondsToUs(8.0), QStringLiteral("World")},
    };
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    QCOMPARE(loadedClip.type, drift::ClipType::Subtitle);
    QCOMPARE(loadedClip.subtitleCues.size(), 2);
    QCOMPARE(loadedClip.subtitleCues[0].text, QStringLiteral("Hello"));
    QCOMPARE(loadedClip.subtitleCues[1].startUs, drift::secondsToUs(5.0));
}

void CoreTest::subtitleCueLookup()
{
    QList<drift::SubtitleCue> cues;
    cues.append({drift::secondsToUs(1.0), drift::secondsToUs(3.0), QStringLiteral("A")});
    cues.append({drift::secondsToUs(4.0), drift::secondsToUs(6.0), QStringLiteral("B")});

    const drift::SubtitleCue *active =
        drift::activeSubtitleCueAt(cues, drift::secondsToUs(2.5));
    QVERIFY(active);
    QCOMPARE(active->text, QStringLiteral("A"));
    QVERIFY(!drift::activeSubtitleCueAt(cues, drift::secondsToUs(3.5)));
    QCOMPARE(drift::subtitleCueIndexAt(cues, drift::secondsToUs(5.0)), 1);
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

void CoreTest::multiTrackSerializationRoundTrip()
{
    drift::Project project;
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Video, .muted = true});
    project.tracks().append(drift::Track{.type = drift::TrackType::Audio, .showWaveform = true});
    project.tracks().append(drift::Track{.type = drift::TrackType::Video, .hidden = true});
    project.tracks().append(drift::Track{.type = drift::TrackType::Text});

    drift::Clip clip;
    clip.id = QStringLiteral("clip-v2");
    clip.type = drift::ClipType::Video;
    clip.timelineStart = drift::secondsToUs(1.0);
    clip.timelineDuration = drift::secondsToUs(2.0);
    project.tracks()[2].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    QCOMPARE(loaded.tracks().size(), 4);
    QCOMPARE(loaded.tracks()[0].type, drift::TrackType::Video);
    QVERIFY(loaded.tracks()[0].muted);
    QCOMPARE(loaded.tracks()[1].type, drift::TrackType::Audio);
    QVERIFY(loaded.tracks()[1].showWaveform);
    QCOMPARE(loaded.tracks()[2].type, drift::TrackType::Video);
    QVERIFY(loaded.tracks()[2].hidden);
    QCOMPARE(loaded.tracks()[2].clips.size(), 1);
    QCOMPARE(loaded.tracks()[2].clips[0].id, QStringLiteral("clip-v2"));
    QCOMPARE(loaded.tracks()[3].type, drift::TrackType::Text);
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
    clip.textStyle.fontWeight = 300;
    clip.textStyle.italic = true;
    clip.textStyle.color = QColor(10, 20, 30, 200);
    clip.textStyle.align = drift::TextAlign::Right;
    clip.textStyle.valign = drift::TextVAlign::Bottom;
    clip.textStyle.wordWrap = false;
    clip.textStyle.lineHeight = 1.6;
    clip.textStyle.letterSpacing = 3.5;
    clip.textStyle.outlineWidth = 2.5;
    clip.textStyle.outlineColor = QColor(255, 0, 0);
    clip.textStyle.shadowEnabled = true;
    clip.textStyle.shadowOffsetX = -3.0;
    clip.textStyle.shadowOffsetY = 7.0;
    clip.textStyle.shadowBlur = 11.0;
    clip.textStyle.shadowOpacity = 0.42;
    clip.textStyle.shadowColor = QColor(0, 128, 255);
    clip.textStyle.boxEnabled = true;
    clip.textStyle.boxColor = QColor(0, 0, 0, 100);
    clip.textStyle.boxPadding = 12.0;
    clip.textStyle.boxRadius = 5.0;
    clip.textStyle.animIn = {drift::TextAnimKind::Pop, drift::secondsToUs(0.3), drift::TextEase::Back};
    clip.textStyle.animOut = {drift::TextAnimKind::SlideDown, drift::secondsToUs(0.25),
                              drift::TextEase::EaseInOut};
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    const drift::TextStyle &s = loadedClip.textStyle;
    QCOMPARE(loadedClip.blendMode, drift::BlendMode::Multiply);
    QCOMPARE(s.fontFamily, QStringLiteral("Courier New"));
    QCOMPARE(s.pixelSize, 88);
    QCOMPARE(s.fontWeight, 300);
    QCOMPARE(s.italic, true);
    QCOMPARE(s.color, QColor(10, 20, 30, 200));
    QCOMPARE(s.align, drift::TextAlign::Right);
    QCOMPARE(s.valign, drift::TextVAlign::Bottom);
    QCOMPARE(s.wordWrap, false);
    QCOMPARE(s.lineHeight, 1.6);
    QCOMPARE(s.letterSpacing, 3.5);
    QCOMPARE(s.outlineWidth, 2.5);
    QCOMPARE(s.outlineColor, QColor(255, 0, 0));
    QCOMPARE(s.shadowEnabled, true);
    QCOMPARE(s.shadowOffsetX, -3.0);
    QCOMPARE(s.shadowOffsetY, 7.0);
    QCOMPARE(s.shadowBlur, 11.0);
    QCOMPARE(s.shadowOpacity, 0.42);
    QCOMPARE(s.shadowColor, QColor(0, 128, 255));
    QCOMPARE(s.boxEnabled, true);
    QCOMPARE(s.boxColor, QColor(0, 0, 0, 100));
    QCOMPARE(s.boxPadding, 12.0);
    QCOMPARE(s.boxRadius, 5.0);
    QCOMPARE(s.animIn.kind, drift::TextAnimKind::Pop);
    QCOMPARE(s.animIn.durationUs, drift::secondsToUs(0.3));
    QCOMPARE(s.animIn.ease, drift::TextEase::Back);
    QCOMPARE(s.animOut.kind, drift::TextAnimKind::SlideDown);
    QCOMPARE(s.animOut.durationUs, drift::secondsToUs(0.25));
    QCOMPARE(s.animOut.ease, drift::TextEase::EaseInOut);
}

void CoreTest::legacyBoldMigratesToFontWeight()
{
    // Projects written before the weight ladder carried a bold flag instead.
    const auto weightForLegacy = [](const QJsonObject &textStyle) {
        QJsonObject clip{
            {QStringLiteral("id"), QStringLiteral("c1")},
            {QStringLiteral("type"), QStringLiteral("text")},
            {QStringLiteral("textContent"), QStringLiteral("Hi")},
            {QStringLiteral("timelineStart"), 0},
            {QStringLiteral("timelineDuration"), 1000000},
            {QStringLiteral("textStyle"), textStyle},
        };
        QJsonObject track{
            {QStringLiteral("type"), QStringLiteral("text")},
            {QStringLiteral("clips"), QJsonArray{clip}},
        };
        QJsonObject project{
            {QStringLiteral("version"), 2},
            {QStringLiteral("width"), 1920},
            {QStringLiteral("height"), 1080},
            {QStringLiteral("fps"), 30},
            {QStringLiteral("tracks"), QJsonArray{track}},
        };
        QString error;
        const drift::Project loaded = drift::Project::fromJson(project, &error);
        return loaded.tracks().at(0).clips.at(0).textStyle.fontWeight;
    };

    QCOMPARE(weightForLegacy({{QStringLiteral("bold"), true}}), 700);
    QCOMPARE(weightForLegacy({{QStringLiteral("bold"), false}}), 400);
    // A style object with neither key keeps the struct default.
    QCOMPARE(weightForLegacy({{QStringLiteral("pixelSize"), 40}}), 700);
    // A new-format style wins over any stale bold flag.
    QCOMPARE(weightForLegacy({{QStringLiteral("bold"), false}, {QStringLiteral("fontWeight"), 900}}), 900);
}

void CoreTest::textPresetsAreWellFormed()
{
    const QList<drift::TextPreset> &presets = drift::textPresets();
    QVERIFY(!presets.isEmpty());

    QSet<QString> ids;
    for (const drift::TextPreset &preset : presets) {
        QVERIFY(!preset.id.isEmpty());
        QVERIFY(!preset.label.isEmpty());
        QVERIFY(!ids.contains(preset.id));
        ids.insert(preset.id);
        QVERIFY(preset.style.pixelSize > 0);
        QVERIFY(!preset.style.fontFamily.isEmpty());
        QVERIFY(preset.style.fontWeight >= 100 && preset.style.fontWeight <= 900);
        QCOMPARE(drift::textStyleForPresetId(preset.id)->fontFamily, preset.style.fontFamily);
    }
    QVERIFY(drift::textStyleForPresetId(QStringLiteral("nope")) == nullptr);
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
    clip.transformX.setKeyframe(0, 100.0);
    clip.transformY.setKeyframe(0, 200.0);
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
    QCOMPARE(loadedClip.transformX.evaluateAt(0), 100.0);
    QCOMPARE(loadedClip.transformY.evaluateAt(0), 200.0);
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

void CoreTest::rgbSplitEffectParametersSerialization()
{
    drift::Project project;
    drift::Clip clip;
    clip.id = QStringLiteral("clip-rgb-split");
    clip.type = drift::ClipType::Video;
    clip.name = QStringLiteral("Video");
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(2.0);

    drift::Effect effect;
    effect.name = QStringLiteral("rgb_split");
    effect.catalogId = QStringLiteral("rgb_split");
    effect.parameters.insert(QStringLiteral("amount"), 12.0);
    effect.parameters.insert(QStringLiteral("angle"), 45.0);
    effect.parameters.insert(QStringLiteral("animated"), true);
    effect.parameters.insert(QStringLiteral("speed"), 2.5);
    clip.effects.append(effect);
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    QCOMPARE(loadedClip.effects.size(), 1);
    QCOMPARE(loadedClip.effects[0].catalogId, QStringLiteral("rgb_split"));
    const QMap<QString, QVariant> &params = loadedClip.effects[0].parameters;
    QCOMPARE(params.value(QStringLiteral("amount")).toDouble(), 12.0);
    QCOMPARE(params.value(QStringLiteral("angle")).toDouble(), 45.0);
    QCOMPARE(params.value(QStringLiteral("animated")).toBool(), true);
    QCOMPARE(params.value(QStringLiteral("speed")).toDouble(), 2.5);
}

void CoreTest::blockGlitchEffectParametersSerialization()
{
    drift::Project project;
    drift::Clip clip;
    clip.id = QStringLiteral("clip-block-glitch");
    clip.type = drift::ClipType::Video;
    clip.name = QStringLiteral("Video");
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(2.0);

    drift::Effect effect;
    effect.name = QStringLiteral("block_glitch");
    effect.catalogId = QStringLiteral("block_glitch");
    effect.parameters.insert(QStringLiteral("intensity"), 0.5);
    effect.parameters.insert(QStringLiteral("blockSize"), 48.0);
    effect.parameters.insert(QStringLiteral("shiftAmount"), 36.0);
    effect.parameters.insert(QStringLiteral("frequency"), 0.4);
    effect.parameters.insert(QStringLiteral("seed"), 7.0);
    clip.effects.append(effect);
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    const drift::Clip &loadedClip = loaded.tracks()[0].clips[0];
    QCOMPARE(loadedClip.effects.size(), 1);
    QCOMPARE(loadedClip.effects[0].catalogId, QStringLiteral("block_glitch"));
    const QMap<QString, QVariant> &params = loadedClip.effects[0].parameters;
    QCOMPARE(params.value(QStringLiteral("intensity")).toDouble(), 0.5);
    QCOMPARE(params.value(QStringLiteral("blockSize")).toDouble(), 48.0);
    QCOMPARE(params.value(QStringLiteral("shiftAmount")).toDouble(), 36.0);
    QCOMPARE(params.value(QStringLiteral("frequency")).toDouble(), 0.4);
    QCOMPARE(params.value(QStringLiteral("seed")).toDouble(), 7.0);
}

void CoreTest::clipSpeedSourceMapping()
{
    drift::Clip clip;
    clip.timelineStart = drift::secondsToUs(1.0);
    clip.timelineDuration = drift::secondsToUs(4.0);
    clip.srcIn = drift::secondsToUs(2.0);
    clip.speed = 2.0;
    clip.srcOut = clip.srcIn + clip.sourceSpanUs();

    QCOMPARE(clip.sourceSpanUs(), drift::secondsToUs(8.0));
    QCOMPARE(clip.timelineToSourceUs(drift::secondsToUs(3.0)), drift::secondsToUs(6.0));

    clip.syncSrcOutFromSpeed(drift::secondsToUs(20.0));
    QCOMPARE(clip.srcOut, clip.srcIn + drift::secondsToUs(8.0));

    clip.reverse = true;
    // At timeline start → near srcOut; at +1s timeline with speed 2 → srcOut - 2s
    QCOMPARE(clip.timelineToSourceUs(drift::secondsToUs(1.0)), clip.srcOut);
    QCOMPARE(clip.timelineToSourceUs(drift::secondsToUs(2.0)), clip.srcOut - drift::secondsToUs(2.0));
}

void CoreTest::clipReverseAndFlipSerialization()
{
    drift::Project project;
    drift::Clip clip;
    clip.id = QStringLiteral("clip-flip");
    clip.type = drift::ClipType::Video;
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(2.0);
    clip.srcIn = drift::secondsToUs(1.0);
    clip.srcOut = drift::secondsToUs(3.0);
    clip.reverse = true;
    clip.flipH = true;
    clip.flipV = true;
    clip.speed = 1.5;
    project.tracks()[0].clips.append(clip);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);
    QVERIFY(error.isEmpty());
    const drift::Clip &out = loaded.tracks()[0].clips[0];
    QCOMPARE(out.reverse, true);
    QCOMPARE(out.flipH, true);
    QCOMPARE(out.flipV, true);
    QCOMPARE(out.speed, 1.5);
}

void CoreTest::clipSplitMergeRoundTrip()
{
    drift::Clip head;
    head.id = QStringLiteral("head");
    head.type = drift::ClipType::Video;
    head.assetId = QStringLiteral("asset-a");
    head.path = QStringLiteral("/tmp/a.mp4");
    head.timelineStart = 0;
    head.timelineDuration = drift::secondsToUs(4.0);
    head.srcIn = drift::secondsToUs(1.0);
    head.srcOut = drift::secondsToUs(5.0);
    head.speed = 1.0;

    drift::Clip tail;
    QVERIFY(drift::splitClipAtOffset(head, tail, drift::secondsToUs(2.0)));
    tail.id = QStringLiteral("tail");
    QCOMPARE(head.timelineDuration, drift::secondsToUs(2.0));
    QCOMPARE(tail.timelineStart, drift::secondsToUs(2.0));
    QCOMPARE(head.srcOut, drift::secondsToUs(3.0));
    QCOMPARE(tail.srcIn, drift::secondsToUs(3.0));
    QVERIFY(drift::clipsCanMerge(head, tail));

    const drift::Clip merged = drift::mergeClips(head, tail);
    QCOMPARE(merged.timelineDuration, drift::secondsToUs(4.0));
    QCOMPARE(merged.srcIn, drift::secondsToUs(1.0));
    QCOMPARE(merged.srcOut, drift::secondsToUs(5.0));

    // Reverse split: earlier half maps to higher source.
    drift::Clip rev;
    rev.id = QStringLiteral("rev");
    rev.type = drift::ClipType::Video;
    rev.assetId = QStringLiteral("asset-a");
    rev.path = QStringLiteral("/tmp/a.mp4");
    rev.timelineStart = 0;
    rev.timelineDuration = drift::secondsToUs(4.0);
    rev.srcIn = drift::secondsToUs(1.0);
    rev.srcOut = drift::secondsToUs(5.0);
    rev.reverse = true;
    drift::Clip revTail;
    QVERIFY(drift::splitClipAtOffset(rev, revTail, drift::secondsToUs(2.0)));
    revTail.id = QStringLiteral("rev-tail");
    QCOMPARE(rev.srcIn, drift::secondsToUs(3.0));
    QCOMPARE(rev.srcOut, drift::secondsToUs(5.0));
    QCOMPARE(revTail.srcIn, drift::secondsToUs(1.0));
    QCOMPARE(revTail.srcOut, drift::secondsToUs(3.0));
    QVERIFY(drift::clipsCanMerge(rev, revTail));
}

void CoreTest::maskAndTransitionSerialization()
{
    drift::Project project;
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Video});

    drift::Clip clipA;
    clipA.id = QStringLiteral("clip-a");
    clipA.type = drift::ClipType::Video;
    clipA.timelineStart = 0;
    clipA.timelineDuration = drift::secondsToUs(2.0);
    clipA.speed = 2.0;
    clipA.mask.shape = drift::MaskShape::Ellipse;
    clipA.mask.w = 0.5;
    clipA.mask.feather = 4.0;

    drift::Clip clipB;
    clipB.id = QStringLiteral("clip-b");
    clipB.type = drift::ClipType::Video;
    clipB.timelineStart = drift::secondsToUs(2.0);
    clipB.timelineDuration = drift::secondsToUs(2.0);

    project.tracks()[0].clips.append(clipA);
    project.tracks()[0].clips.append(clipB);

    drift::Transition transition;
    transition.id = QStringLiteral("tr-1");
    transition.fromClipId = clipA.id;
    transition.toClipId = clipB.id;
    transition.kindId = QStringLiteral("dip");
    transition.durationUs = drift::secondsToUs(0.5);
    project.tracks()[0].transitions.append(transition);

    const QJsonObject json = project.toJson();
    QString error;
    const drift::Project loaded = drift::Project::fromJson(json, &error);

    QVERIFY(error.isEmpty());
    QCOMPARE(loaded.tracks()[0].clips[0].speed, 2.0);
    QCOMPARE(loaded.tracks()[0].clips[0].mask.shape, drift::MaskShape::Ellipse);
    QCOMPARE(loaded.tracks()[0].transitions.size(), 1);
    QCOMPARE(loaded.tracks()[0].transitions[0].kindId, QStringLiteral("dip"));
    QCOMPARE(loaded.tracks()[0].transitions[0].fromClipId, QStringLiteral("clip-a"));
}

// The pre-shader enum serialized exactly these strings, so a project written by an older build
// must still resolve to the right transition package.
static drift::Project projectWithTransition(const QString &kindId,
                                            const QMap<QString, QVariant> &params = {})
{
    drift::Project project;
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Video});

    drift::Clip clipA;
    clipA.id = QStringLiteral("a");
    clipA.type = drift::ClipType::Video;
    clipA.timelineStart = 0;
    clipA.timelineDuration = drift::secondsToUs(1.0);

    drift::Clip clipB;
    clipB.id = QStringLiteral("b");
    clipB.type = drift::ClipType::Video;
    clipB.timelineStart = drift::secondsToUs(1.0);
    clipB.timelineDuration = drift::secondsToUs(1.0);

    project.tracks()[0].clips.append(clipA);
    project.tracks()[0].clips.append(clipB);

    drift::Transition transition;
    transition.id = QStringLiteral("tr");
    transition.fromClipId = clipA.id;
    transition.toClipId = clipB.id;
    transition.kindId = kindId;
    transition.parameters = params;
    project.tracks()[0].transitions.append(transition);
    return project;
}

void CoreTest::allTransitionKindsRoundTrip()
{
    const QStringList kinds = {
        QStringLiteral("crossfade"),  QStringLiteral("dip"),        QStringLiteral("dip_white"),
        QStringLiteral("wipe_left"),  QStringLiteral("wipe_right"), QStringLiteral("wipe_up"),
        QStringLiteral("wipe_down"),  QStringLiteral("push_left"),  QStringLiteral("zoom_in"),
    };

    for (const QString &kind : kinds) {
        const drift::Project project = projectWithTransition(kind);
        QString error;
        const drift::Project loaded = drift::Project::fromJson(project.toJson(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(loaded.tracks()[0].transitions[0].kindId, kind);
    }
}

void CoreTest::transitionParametersRoundTrip()
{
    QMap<QString, QVariant> params;
    params.insert(QStringLiteral("softness"), 0.25);
    params.insert(QStringLiteral("invert"), true);

    const drift::Project project = projectWithTransition(QStringLiteral("luma_fade"), params);
    QString error;
    const drift::Project loaded = drift::Project::fromJson(project.toJson(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));

    const drift::Transition &t = loaded.tracks()[0].transitions[0];
    QCOMPARE(t.kindId, QStringLiteral("luma_fade"));
    QCOMPARE(t.parameters.value(QStringLiteral("softness")).toDouble(), 0.25);
    QCOMPARE(t.parameters.value(QStringLiteral("invert")).toBool(), true);
}

// A project file written before transitions became packages has no "parameters" key at all.
void CoreTest::legacyTransitionJsonStillLoads()
{
    QJsonObject legacy = projectWithTransition(QStringLiteral("wipe_up")).toJson();
    QJsonArray tracks = legacy.value(QStringLiteral("tracks")).toArray();
    QJsonObject track = tracks.at(0).toObject();
    QJsonArray transitions = track.value(QStringLiteral("transitions")).toArray();
    QJsonObject t = transitions.at(0).toObject();
    t.remove(QStringLiteral("parameters"));
    transitions.replace(0, t);
    track.insert(QStringLiteral("transitions"), transitions);
    tracks.replace(0, track);
    legacy.insert(QStringLiteral("tracks"), tracks);

    QString error;
    const drift::Project loaded = drift::Project::fromJson(legacy, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(loaded.tracks()[0].transitions[0].kindId, QStringLiteral("wipe_up"));
    QVERIFY(loaded.tracks()[0].transitions[0].parameters.isEmpty());
}

void CoreTest::transitionAudioCurves()
{
    // crossfade: linear, sums to 1 at every point.
    const auto mid = drift::transitionAudioGains(QStringLiteral("crossfade"), 0.5);
    QCOMPARE(mid.outgoing, 0.5);
    QCOMPARE(mid.incoming, 0.5);

    // dip: silent at the midpoint, matching the visual dip through black.
    const auto dip = drift::transitionAudioGains(QStringLiteral("dip"), 0.5);
    QCOMPARE(dip.outgoing, 0.0);
    QCOMPARE(dip.incoming, 0.0);
    QCOMPARE(drift::transitionAudioGains(QStringLiteral("dip"), 0.0).outgoing, 1.0);
    QCOMPARE(drift::transitionAudioGains(QStringLiteral("dip"), 1.0).incoming, 1.0);

    // hold: no ducking at all.
    const auto hold = drift::transitionAudioGains(QStringLiteral("hold"), 0.5);
    QCOMPARE(hold.outgoing, 1.0);
    QCOMPARE(hold.incoming, 1.0);
}

void CoreTest::physicalOverlapTransitionWindow()
{
    drift::Track track;
    track.type = drift::TrackType::Video;

    drift::Clip clipA;
    clipA.id = QStringLiteral("a");
    clipA.timelineStart = 0;
    clipA.timelineDuration = drift::secondsToUs(2.0);

    drift::Clip clipB;
    clipB.id = QStringLiteral("b");
    clipB.timelineStart = drift::secondsToUs(1.5);
    clipB.timelineDuration = drift::secondsToUs(2.0);

    track.clips.append(clipA);
    track.clips.append(clipB);

    QVERIFY(drift::clipsPhysicallyOverlap(clipA, clipB));
    QCOMPARE(drift::physicalOverlapDurationUs(clipA, clipB), drift::secondsToUs(0.5));

    drift::Transition transition;
    transition.fromClipId = clipA.id;
    transition.toClipId = clipB.id;
    transition.durationUs = drift::secondsToUs(1.0); // ignored when overlapping

    drift::TimeUs startUs = 0;
    drift::TimeUs endUs = 0;
    QVERIFY(drift::transitionWindow(track, transition, startUs, endUs));
    QCOMPARE(startUs, drift::secondsToUs(1.5));
    QCOMPARE(endUs, drift::secondsToUs(2.0));
}

void CoreTest::backgroundSerialization()
{
    // Default background is opaque black / Color and must survive a round-trip.
    {
        drift::Project project;
        const drift::Project loaded = drift::Project::fromJson(project.toJson());
        QCOMPARE(loaded.background().kind, drift::BackgroundKind::Color);
        QCOMPARE(loaded.background().color, QColor(Qt::black));
    }

    // Non-default (blur + color + strength) round-trips.
    {
        drift::Project project;
        drift::Background bg;
        bg.kind = drift::BackgroundKind::Blur;
        bg.color = QColor(QStringLiteral("#ff2563eb"));
        bg.blurStrength = 42.0;
        project.setBackground(bg);

        QString error;
        const drift::Project loaded = drift::Project::fromJson(project.toJson(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(loaded.background().kind, drift::BackgroundKind::Blur);
        QCOMPARE(loaded.background().color, QColor(QStringLiteral("#ff2563eb")));
        QCOMPARE(loaded.background().blurStrength, 42.0);
    }

    // Projects saved before this field default to solid black.
    {
        const QJsonObject root{
            {QStringLiteral("version"), 3},
            {QStringLiteral("projectName"), QStringLiteral("NoBackground")},
            {QStringLiteral("fps"), 30},
            {QStringLiteral("width"), 1920},
            {QStringLiteral("height"), 1080},
        };
        const drift::Project loaded = drift::Project::fromJson(root);
        QCOMPARE(loaded.background().kind, drift::BackgroundKind::Color);
        QCOMPARE(loaded.background().color, QColor(Qt::black));
    }
}

void CoreTest::fadeSerializationAndMultiplier()
{
    drift::Project project;
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Video});

    drift::Clip clip;
    clip.id = QStringLiteral("fade-clip");
    clip.type = drift::ClipType::Video;
    clip.timelineStart = drift::secondsToUs(1.0);
    clip.timelineDuration = drift::secondsToUs(4.0);
    clip.fadeInUs = drift::secondsToUs(1.0);
    clip.fadeOutUs = drift::secondsToUs(2.0);
    clip.fadeCurve = drift::FadeCurve::Linear;
    project.tracks()[0].clips.append(clip);

    QString error;
    const drift::Project loaded = drift::Project::fromJson(project.toJson(), &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const drift::Clip &c = loaded.tracks()[0].clips[0];
    QCOMPARE(c.fadeInUs, drift::secondsToUs(1.0));
    QCOMPARE(c.fadeOutUs, drift::secondsToUs(2.0));
    QCOMPARE(c.fadeCurve, drift::FadeCurve::Linear);

    // Linear ramp: at the very edges gain is 0, at the fade midpoints 0.5, and
    // fully present between the fades.
    QCOMPARE(c.fadeMultiplier(c.timelineStart), 0.0);
    QVERIFY(qAbs(c.fadeMultiplier(c.timelineStart + drift::secondsToUs(0.5)) - 0.5) < 1e-6);
    QVERIFY(qAbs(c.fadeMultiplier(c.timelineStart + drift::secondsToUs(1.5)) - 1.0) < 1e-6);
    QVERIFY(qAbs(c.fadeMultiplier(c.timelineEnd() - drift::secondsToUs(1.0)) - 0.5) < 1e-6);

    // A clip with no fades is always fully present.
    drift::Clip plain;
    plain.timelineStart = 0;
    plain.timelineDuration = drift::secondsToUs(2.0);
    QCOMPARE(plain.fadeMultiplier(drift::secondsToUs(1.0)), 1.0);
}

QTEST_MAIN(CoreTest)
#include "tst_core.moc"
