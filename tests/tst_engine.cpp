#include <QtTest>

#include <QFileInfo>
#include <QPainter>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <cmath>

#include "core/Clip.h"
#include "core/Project.h"
#include "engine/ClipReader.h"
#include "engine/CompositorFrameHistory.h"
#include "engine/EffectCatalog.h"
#include "engine/EffectProcessor.h"
#include "engine/FrameCompositor.h"
#include "engine/MaskApplier.h"
#include "core/Transition.h"

class EngineTest : public QObject
{
    Q_OBJECT

private slots:
    void effectProcessorPassthroughWithoutEffects();
    void effectProcessorBrightness();
    void clipReaderSequentialAndSeek();
    void clipReaderAudioSequential();
    void compositorDefaultRenderStaysFullResolution();
    void compositorPreviewScaleRendersLowerResolution();
    void compositorPreviewScaleMapsProjectPixelLayout();
    void compositorAppliesMultiplyBlendMode();
    void compositorRendersShapeClip();
    void adjustmentEffectContrastCatalogEntry();
    void effectPresetStableIds();
    void effectPresetCatalogIncludesStylizePresets();
    void effectBrowserCategories();
    void effectGraphTemplateSubstitution();
    void compositorOnlyPresetsUseCompositorPath();
    void rgbSplitZeroAmountPassthrough();
    void rgbSplitShiftsColorChannels();
    void blockGlitchDeterministicForSameTimeAndSeed();
    void blockGlitchChangesWithTimelineTime();
    void scanlineGlitchZeroStrengthPassthrough();
    void scanlineGlitchDeterministicAtFixedTime();
    void scanlineGlitchVisualChangeAtNonzeroSettings();
    void vhsCrtZeroSettingsPassthrough();
    void vhsCrtNonzeroModifiesOutput();
    void vhsCrtDeterministicAtFixedTime();
    void bloomGlowZeroIntensityPassthrough();
    void bloomGlowDarkFrameUnchanged();
    void bloomGlowBrightSpotBleedsToNeighbors();
    void rippleWaterZeroAmplitudePassthrough();
    void rippleWaterNonzeroDisplacementChangesOutput();
    void edgeNeonZeroIntensityUnchanged();
    void edgeNeonHighContrastRectangleGlow();
    void digitalGlitchZeroIntensityUnchanged();
    void digitalGlitchDeterministicForFixedTimeAndSeed();
    void filmBurnZeroIntensityUnchanged();
    void filmBurnAddsWarmLeakContribution();
    void timeEchoBlendDeterministic();
    void timeEchoBlendIncludesHistoryContribution();
    void timeEchoDeterministicAtFixedTimelineTime();
    void timeEchoBlendsPriorVideoFrames();
    void shockwavePulseZeroStrengthPassthrough();
    void shockwavePulseChangesPixelsNearWavefront();
    void compositorCrossfadeBetweenShapeClips();
    void compositorDipToBlackMidpointIsBlack();
    void compositorWipeRightRevealsIncomingClip();
    void maskApplierEllipseMasksCorners();

private:
    static QString makeColorSegmentsVideo(QTemporaryDir &dir);
    static QString makeToneAudio(QTemporaryDir &dir);
};

void EngineTest::effectProcessorPassthroughWithoutEffects()
{
    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(Qt::red);
    const QImage out = EffectProcessor::applyEffects(image, {});
    QCOMPARE(out.size(), image.size());
}

void EngineTest::effectProcessorBrightness()
{
    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(QColor(100, 100, 100));

    drift::Effect effect;
    effect.name = QStringLiteral("eq");
    effect.parameters.insert(QStringLiteral("brightness"), 0.2);

    const QImage out = EffectProcessor::applyEffects(image, {effect});
    QVERIFY(!out.isNull());
    const QRgb pixel = out.pixel(32, 32);
    QVERIFY(qRed(pixel) > 100 || qGreen(pixel) > 100 || qBlue(pixel) > 100);
}

// Builds a 3-second, 64x64 clip: red [0,1), green [1,2), blue [2,3), sparse
// keyframes so the sequential path differs meaningfully from a per-frame seek.
QString EngineTest::makeColorSegmentsVideo(QTemporaryDir &dir)
{
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        return {};

    const QString out = dir.filePath(QStringLiteral("colors.mp4"));
    QStringList args{
        QStringLiteral("-y"),
        QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
        QStringLiteral("color=c=red:s=64x64:r=25:d=1"),
        QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
        QStringLiteral("color=c=lime:s=64x64:r=25:d=1"),
        QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
        QStringLiteral("color=c=blue:s=64x64:r=25:d=1"),
        QStringLiteral("-filter_complex"), QStringLiteral("[0][1][2]concat=n=3:v=1:a=0[v]"),
        QStringLiteral("-map"), QStringLiteral("[v]"),
        QStringLiteral("-c:v"), QStringLiteral("libx264"),
        QStringLiteral("-g"), QStringLiteral("25"),
        QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
        out,
    };

    QProcess proc;
    proc.start(ffmpeg, args);
    if (!proc.waitForFinished(30000) || proc.exitCode() != 0)
        return {};
    return QFileInfo::exists(out) ? out : QString{};
}

void EngineTest::clipReaderSequentialAndSeek()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeColorSegmentsVideo(dir);
    if (path.isEmpty())
        QSKIP("ffmpeg not available to generate a test clip");

    ClipReader reader;
    QVERIFY(reader.open(path));
    QVERIFY(reader.hasVideo());

    auto dominant = [&](drift::TimeUs us) -> QChar {
        QImage frame;
        if (!reader.readVideoFrameAt(us, frame, 64, 64) || frame.isNull())
            return QChar('?');
        const QRgb p = frame.pixel(32, 32);
        if (qRed(p) >= qGreen(p) && qRed(p) >= qBlue(p))
            return QChar('R');
        if (qGreen(p) >= qRed(p) && qGreen(p) >= qBlue(p))
            return QChar('G');
        return QChar('B');
    };

    // Forward sequential requests exercise the no-seek fast path.
    QCOMPARE(dominant(500'000), QChar('R'));   // 0.5s
    QCOMPARE(dominant(700'000), QChar('R'));   // 0.7s, small forward step
    QCOMPARE(dominant(1'500'000), QChar('G')); // 1.5s
    QCOMPARE(dominant(2'500'000), QChar('B')); // 2.5s
    // Backward jump forces a keyframe reseek and must not return a stale frame.
    QCOMPARE(dominant(500'000), QChar('R'));
    QCOMPARE(dominant(1'500'000), QChar('G'));
}

QString EngineTest::makeToneAudio(QTemporaryDir &dir)
{
    const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty())
        return {};

    const QString out = dir.filePath(QStringLiteral("tone.wav"));
    QStringList args{
        QStringLiteral("-y"),
        QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
        QStringLiteral("sine=frequency=440:sample_rate=48000:duration=2"),
        QStringLiteral("-c:a"), QStringLiteral("pcm_s16le"),
        out,
    };

    QProcess proc;
    proc.start(ffmpeg, args);
    if (!proc.waitForFinished(30000) || proc.exitCode() != 0)
        return {};
    return QFileInfo::exists(out) ? out : QString{};
}

// Sequential small buffers must reconstruct the same signal as one contiguous
// read. The old path re-seeked on every buffer, repeating/overlapping audio.
void EngineTest::clipReaderAudioSequential()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeToneAudio(dir);
    if (path.isEmpty())
        QSKIP("ffmpeg not available to generate a test clip");

    constexpr int kRate = 48000;
    constexpr int kChunk = 1024;
    constexpr int kChunks = 20;
    constexpr int kTotal = kChunk * kChunks;
    constexpr drift::TimeUs kStartUs = 200'000;

    ClipReader ref;
    QVERIFY(ref.open(path));
    QVERIFY(ref.hasAudio());
    QVector<float> refBuf(kTotal * 2, 0.0f);
    QCOMPARE(ref.readAudioInterleaved(kStartUs, kTotal, kRate, refBuf.data()), kTotal);

    double sumSq = 0.0;
    for (float s : refBuf)
        sumSq += static_cast<double>(s) * s;
    QVERIFY(std::sqrt(sumSq / refBuf.size()) > 0.05); // audibly non-silent

    ClipReader seq;
    QVERIFY(seq.open(path));
    QVector<float> seqBuf;
    seqBuf.reserve(kTotal * 2);
    QVector<float> chunkBuf(kChunk * 2);
    drift::TimeUs t = kStartUs;
    for (int c = 0; c < kChunks; ++c) {
        const int n = seq.readAudioInterleaved(t, kChunk, kRate, chunkBuf.data());
        QVERIFY(n > 0);
        for (int i = 0; i < n * 2; ++i)
            seqBuf.append(chunkBuf[i]);
        t += static_cast<drift::TimeUs>(n) * drift::kUsPerSecond / kRate;
    }

    const int cmp = qMin(refBuf.size(), seqBuf.size());
    QVERIFY(cmp >= kTotal * 2 - kChunk * 2);
    double err = 0.0;
    for (int i = 0; i < cmp; ++i) {
        const double d = static_cast<double>(refBuf[i]) - seqBuf[i];
        err += d * d;
    }
    QVERIFY(std::sqrt(err / cmp) < 0.02);
}

void EngineTest::compositorDefaultRenderStaysFullResolution()
{
    drift::Project project;
    project.setResolution(192, 108);
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Shape});

    drift::Clip clip;
    clip.id = QStringLiteral("shape");
    clip.type = drift::ClipType::Shape;
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(1.0);
    clip.shapeStyle.kind = drift::ShapeKind::Rectangle;
    clip.shapeStyle.fill = Qt::red;
    project.tracks()[0].clips.append(clip);

    FrameCompositor compositor;
    compositor.setProject(&project);

    const QImage frame = compositor.compositeAt(0);
    QCOMPARE(frame.size(), QSize(192, 108));
}

void EngineTest::compositorPreviewScaleRendersLowerResolution()
{
    drift::Project project;
    project.setResolution(192, 108);
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Shape});

    drift::Clip clip;
    clip.id = QStringLiteral("shape");
    clip.type = drift::ClipType::Shape;
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(1.0);
    clip.shapeStyle.kind = drift::ShapeKind::Rectangle;
    clip.shapeStyle.fill = Qt::red;
    project.tracks()[0].clips.append(clip);

    FrameCompositor compositor;
    compositor.setProject(&project);

    FrameCompositor::RenderOptions options;
    options.previewScale = 0.5;
    options.maxTimeEchoHistoryFrames = 1;
    const QImage frame = compositor.compositeAt(0, options);
    QCOMPARE(frame.size(), QSize(96, 54));

    const QImage fullFrame = compositor.compositeAt(0);
    QCOMPARE(fullFrame.size(), QSize(192, 108));
}

void EngineTest::compositorPreviewScaleMapsProjectPixelLayout()
{
    // Project-pixel layout must be scaled onto the preview canvas so WYSIWYG
    // handles (which map project px → widget) stay aligned with the frame.
    drift::Project project;
    project.setResolution(200, 100);
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Shape});

    drift::Clip clip;
    clip.id = QStringLiteral("shape");
    clip.type = drift::ClipType::Shape;
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(1.0);
    clip.shapeStyle.kind = drift::ShapeKind::Rectangle;
    clip.shapeStyle.fill = Qt::red;
    clip.shapeStyle.strokeWidth = 0.0;
    clip.transformX.setKeyframe(0, 40.0);
    clip.transformY.setKeyframe(0, 20.0);
    clip.transformW.setKeyframe(0, 80.0);
    clip.transformH.setKeyframe(0, 40.0);
    project.tracks()[0].clips.append(clip);

    FrameCompositor compositor;
    compositor.setProject(&project);

    FrameCompositor::RenderOptions options;
    options.previewScale = 0.5;
    const QImage frame = compositor.compositeAt(0, options);
    QCOMPARE(frame.size(), QSize(100, 50));
    // Scaled layout: (20,10)-(60,30) on the half-res canvas.
    QVERIFY(frame.pixelColor(40, 20).red() > 200);
    QCOMPARE(frame.pixelColor(0, 0), QColor(0, 0, 0));
    QCOMPARE(frame.pixelColor(90, 40), QColor(0, 0, 0));
}

void EngineTest::compositorAppliesMultiplyBlendMode()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto writeSolidImage = [&](const QString &name, QColor color) {
        QImage image(64, 64, QImage::Format_RGBA8888);
        image.fill(color);
        const QString path = dir.filePath(name);
        image.save(path, "PNG");
        return path;
    };

    auto compositeOverBackground = [&](QColor background) {
        drift::Project project;
        project.setResolution(64, 64);
        project.tracks().clear();
        project.tracks().append(drift::Track{.type = drift::TrackType::Shape});
        project.tracks().append(drift::Track{.type = drift::TrackType::Shape});

        // Index 0 is the topmost track and composites in front, so the
        // multiplied foreground goes on track 0 and the background on track 1.
        drift::Clip top;
        top.id = QStringLiteral("top");
        top.type = drift::ClipType::Image;
        top.path = writeSolidImage(QStringLiteral("top.png"), Qt::red);
        top.blendMode = drift::BlendMode::Multiply;
        top.timelineStart = 0;
        top.timelineDuration = drift::secondsToUs(1.0);
        project.tracks()[0].clips.append(top);

        drift::Clip bottom;
        bottom.id = QStringLiteral("bottom");
        bottom.type = drift::ClipType::Image;
        bottom.path = writeSolidImage(QStringLiteral("bottom.png"), background);
        bottom.timelineStart = 0;
        bottom.timelineDuration = drift::secondsToUs(1.0);
        project.tracks()[1].clips.append(bottom);

        FrameCompositor compositor;
        compositor.setProject(&project);
        return compositor.compositeAt(0);
    };

    const QImage overGreen = compositeOverBackground(Qt::green);
    QCOMPARE(overGreen.pixelColor(32, 32), QColor(0, 0, 0));

    const QImage overWhite = compositeOverBackground(Qt::white);
    QCOMPARE(overWhite.pixelColor(32, 32), QColor(255, 0, 0));
}

void EngineTest::compositorRendersShapeClip()
{
    drift::Project project;
    project.setResolution(128, 128);
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Shape});

    drift::Clip clip;
    clip.id = QStringLiteral("shape");
    clip.type = drift::ClipType::Shape;
    clip.name = QStringLiteral("triangle");
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(1.0);
    clip.shapeStyle.kind = drift::ShapeKind::Triangle;
    clip.shapeStyle.fill = QColor(255, 0, 0);
    clip.shapeStyle.stroke = Qt::white;
    clip.shapeStyle.strokeWidth = 2.0;
    clip.transformX.setKeyframe(0, 32.0);
    clip.transformY.setKeyframe(0, 32.0);
    clip.transformW.setKeyframe(0, 64.0);
    clip.transformH.setKeyframe(0, 64.0);
    project.tracks()[0].clips.append(clip);

    FrameCompositor compositor;
    compositor.setProject(&project);
    const QImage frame = compositor.compositeAt(0);
    QVERIFY(!frame.isNull());
    QVERIFY(frame.pixelColor(64, 64).red() > 200);
    QVERIFY(frame.pixelColor(0, 0) == QColor(0, 0, 0));
}

void EngineTest::adjustmentEffectContrastCatalogEntry()
{
    const EffectPresetEntry *def = effectDefForId(QStringLiteral("adjust.contrast"));
    QVERIFY(def);
    QCOMPARE(def->filterName, QStringLiteral("eq"));
    QCOMPARE(def->meta.parameters.size(), 1);
    QCOMPARE(def->meta.parameters[0].key, QStringLiteral("contrast"));

    // eq's contrast scales around the 128 midpoint, so a gray above it gets
    // pushed brighter (a gray below it would get pushed darker instead).
    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(QColor(180, 180, 180));

    drift::Effect effect;
    effect.name = def->filterName;
    effect.catalogId = def->meta.id;
    effect.parameters.insert(def->meta.parameters[0].key, 2.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect});
    QVERIFY(!out.isNull());
    const QRgb pixel = out.pixel(32, 32);
    QVERIFY(qRed(pixel) > 180 || qGreen(pixel) > 180 || qBlue(pixel) > 180);
}

void EngineTest::effectPresetStableIds()
{
    const QStringList ids = effectPresetIds();
    QVERIFY(ids.size() >= 16);

    QSet<QString> seen;
    for (const QString &id : ids) {
        QVERIFY2(!id.isEmpty(), "preset id must not be empty");
        QVERIFY2(id.contains(QLatin1Char('.')) || id == QStringLiteral("rgb_split")
                     || id == QStringLiteral("block_glitch")
                     || id == QStringLiteral("scanline_glitch")
                     || id == QStringLiteral("vhs_crt")
                     || id == QStringLiteral("bloom_glow")
                     || id == QStringLiteral("ripple_water")
                     || id == QStringLiteral("edge_neon")
                     || id == QStringLiteral("digital_glitch")
                     || id == QStringLiteral("film_burn")
                     || id == QStringLiteral("time_echo")
                     || id == QStringLiteral("shockwave_pulse"),
                 qPrintable(QStringLiteral("stable id: %1").arg(id)));
        QVERIFY2(!seen.contains(id), qPrintable(QStringLiteral("duplicate id: %1").arg(id)));
        seen.insert(id);

        const EffectPresetEntry *def = effectDefForId(id);
        QVERIFY2(def, qPrintable(QStringLiteral("missing catalog entry for %1").arg(id)));
        QCOMPARE(def->meta.id, id);
        QVERIFY2(!def->meta.displayName.isEmpty(),
                 qPrintable(QStringLiteral("display name missing for %1").arg(id)));
        QVERIFY2(!def->meta.category.isEmpty(),
                 qPrintable(QStringLiteral("category missing for %1").arg(id)));
    }
}

void EngineTest::effectPresetCatalogIncludesStylizePresets()
{
    const auto requirePreset = [&](const char *id, const char *displayName, const char *category,
                                   bool compositorOnly) {
        const EffectPresetEntry *def = effectDefForId(QString::fromLatin1(id));
        QVERIFY2(def, id);
        QCOMPARE(def->meta.displayName, QString::fromLatin1(displayName));
        QCOMPARE(def->meta.category, QString::fromLatin1(category));
        QCOMPARE(def->meta.compositorOnly, compositorOnly);
    };

    requirePreset("rgb_split", "RGB Split", "glitch", true);
    requirePreset("block_glitch", "Block Glitch", "glitch", true);
    requirePreset("scanline_glitch", "Scanline Glitch", "glitch", true);
    requirePreset("vhs_crt", "VHS / CRT", "retro", true);
    requirePreset("film_burn", "Film Burn / Light Leak", "retro", true);
    requirePreset("stylize.vhs", "VHS", "retro", false);
    requirePreset("stylize.bloom", "Bloom", "dreamy", true);
    requirePreset("bloom_glow", "Bloom / Glow", "dreamy", true);
    requirePreset("edge_neon", "Edge Glow / Neon", "dreamy", true);
    requirePreset("time_echo", "Time Echo / Trail", "dreamy", true);
    requirePreset("stylize.ripple", "Ripple", "glitch", true);
    requirePreset("ripple_water", "Ripple / Water", "glitch", true);
    requirePreset("shockwave_pulse", "Shockwave / Pulse", "glitch", true);
    requirePreset("digital_glitch", "Digital Glitch", "glitch", true);
    requirePreset("adjust.contrast", "Contrast", "impact", false);
}

void EngineTest::effectBrowserCategories()
{
    const QList<QPair<QString, QString>> categories = effectCategories();
    QCOMPARE(categories.size(), 4);
    QCOMPARE(categories[0].first, QStringLiteral("glitch"));
    QCOMPARE(categories[0].second, QStringLiteral("Glitch & Distortion"));
    QCOMPARE(categories[1].first, QStringLiteral("retro"));
    QCOMPARE(categories[2].first, QStringLiteral("dreamy"));
    QCOMPARE(categories[3].first, QStringLiteral("impact"));

    QSet<QString> knownCategories;
    for (const auto &category : categories)
        knownCategories.insert(category.first);

    for (const QString &id : effectPresetIds()) {
        const EffectPresetEntry *def = effectDefForId(id);
        QVERIFY(def);
        QVERIFY2(knownCategories.contains(def->meta.category),
                 qPrintable(QStringLiteral("unknown category for %1: %2").arg(id, def->meta.category)));
        QVERIFY(!effectCategoryLabel(def->meta.category).isEmpty());
    }
}

void EngineTest::effectGraphTemplateSubstitution()
{
    drift::Effect vhs;
    vhs.catalogId = QStringLiteral("stylize.vhs");
    vhs.parameters.insert(QStringLiteral("noise"), 30.0);
    vhs.parameters.insert(QStringLiteral("chroma"), 4.0);
    vhs.parameters.insert(QStringLiteral("saturation"), 0.9);

    const QString vhsGraph = buildFilterGraphForEffect(vhs);
    QVERIFY(vhsGraph.contains(QStringLiteral("noise=alls=30")));
    QVERIFY(vhsGraph.contains(QStringLiteral("rgbashift=rh=4:bh=-4")));
    QVERIFY(vhsGraph.contains(QStringLiteral("hue=s=0.9")));
}

void EngineTest::compositorOnlyPresetsUseCompositorPath()
{
    const EffectPresetEntry *bloom = effectDefForId(QStringLiteral("stylize.bloom"));
    QVERIFY(bloom);
    QVERIFY(bloom->meta.compositorOnly);
    QCOMPARE(buildFilterGraphForEffect({.catalogId = bloom->meta.id}), QString());

    QImage image(32, 32, QImage::Format_RGBA8888);
    image.fill(QColor(200, 120, 80));

    drift::Effect effect;
    effect.catalogId = bloom->meta.id;
    effect.parameters.insert(QStringLiteral("intensity"), 1.0);
    effect.parameters.insert(QStringLiteral("radius"), 4.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect});
    QVERIFY(!out.isNull());
    QVERIFY(out.pixel(16, 16) != image.pixel(16, 16));
}

static QImage makeRedBlueSplitTestImage()
{
    QImage image(64, 32, QImage::Format_RGBA8888);
    image.fill(Qt::transparent);
    for (int x = 0; x < 32; ++x) {
        for (int y = 0; y < 32; ++y)
            image.setPixel(x, y, qRgba(255, 0, 0, 255));
    }
    for (int x = 32; x < 64; ++x) {
        for (int y = 0; y < 32; ++y)
            image.setPixel(x, y, qRgba(0, 0, 255, 255));
    }
    return image;
}

void EngineTest::rgbSplitZeroAmountPassthrough()
{
    const QImage image = makeRedBlueSplitTestImage();

    drift::Effect effect;
    effect.catalogId = QStringLiteral("rgb_split");
    effect.parameters.insert(QStringLiteral("amount"), 0.0);
    effect.parameters.insert(QStringLiteral("angle"), 0.0);
    effect.parameters.insert(QStringLiteral("animated"), false);
    effect.parameters.insert(QStringLiteral("speed"), 1.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 0);
    QCOMPARE(out, image);
}

void EngineTest::rgbSplitShiftsColorChannels()
{
    const QImage image = makeRedBlueSplitTestImage();

    drift::Effect effect;
    effect.catalogId = QStringLiteral("rgb_split");
    effect.parameters.insert(QStringLiteral("amount"), 8.0);
    effect.parameters.insert(QStringLiteral("angle"), 0.0);
    effect.parameters.insert(QStringLiteral("animated"), false);

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 0);
    QVERIFY(!out.isNull());

    const QRgb original = image.pixel(30, 16);
    QCOMPARE(qRed(original), 255);
    QCOMPARE(qGreen(original), 0);
    QCOMPARE(qBlue(original), 0);

    const QRgb shifted = out.pixel(30, 16);
    QVERIFY(shifted != original);
    QCOMPARE(qRed(shifted), 0);
    QCOMPARE(qGreen(shifted), 0);
    QCOMPARE(qBlue(shifted), 0);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("stylize.rgb_split"));
    QVERIFY(def);
    QCOMPARE(def->meta.id, QStringLiteral("rgb_split"));
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("rgb_split")}), QString());
}

static QImage makeBlockGlitchTestImage()
{
    QImage image(128, 64, QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const int stripe = (x / 16) % 2;
            image.setPixel(x, y, qRgba(stripe ? 40 : 220, stripe ? 180 : 60, stripe ? 240 : 90, 255));
        }
    }
    return image;
}

static drift::Effect makeBlockGlitchEffect()
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("block_glitch");
    effect.parameters.insert(QStringLiteral("intensity"), 1.0);
    effect.parameters.insert(QStringLiteral("blockSize"), 16.0);
    effect.parameters.insert(QStringLiteral("shiftAmount"), 32.0);
    effect.parameters.insert(QStringLiteral("frequency"), 1.0);
    effect.parameters.insert(QStringLiteral("seed"), 42.0);
    return effect;
}

void EngineTest::blockGlitchDeterministicForSameTimeAndSeed()
{
    const QImage image = makeBlockGlitchTestImage();
    const drift::Effect effect = makeBlockGlitchEffect();
    constexpr drift::TimeUs timeUs = 750'000;

    const QImage first = EffectProcessor::applyEffects(image, {effect}, timeUs);
    const QImage second = EffectProcessor::applyEffects(image, {effect}, timeUs);
    QCOMPARE(first, second);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("block_glitch"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("block_glitch")}), QString());
}

void EngineTest::blockGlitchChangesWithTimelineTime()
{
    const QImage image = makeBlockGlitchTestImage();
    const drift::Effect effect = makeBlockGlitchEffect();

    const QImage atT0 = EffectProcessor::applyEffects(image, {effect}, 0);
    const QImage atT1 = EffectProcessor::applyEffects(image, {effect}, 500'000);
    const QImage atT2 = EffectProcessor::applyEffects(image, {effect}, 1'000'000);

    QVERIFY(atT0 != atT1);
    QVERIFY(atT1 != atT2);

    drift::Effect otherSeed = effect;
    otherSeed.parameters.insert(QStringLiteral("seed"), 99.0);
    const QImage other = EffectProcessor::applyEffects(image, {otherSeed}, 500'000);
    QVERIFY(other != atT1);
}

static drift::Effect makeScanlineGlitchEffect()
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("scanline_glitch");
    effect.parameters.insert(QStringLiteral("jitter"), 0.5);
    effect.parameters.insert(QStringLiteral("lineStrength"), 0.5);
    effect.parameters.insert(QStringLiteral("colorShift"), 6.0);
    effect.parameters.insert(QStringLiteral("speed"), 2.0);
    return effect;
}

void EngineTest::scanlineGlitchZeroStrengthPassthrough()
{
    const QImage image = makeBlockGlitchTestImage();

    drift::Effect effect;
    effect.catalogId = QStringLiteral("scanline_glitch");
    effect.parameters.insert(QStringLiteral("jitter"), 0.0);
    effect.parameters.insert(QStringLiteral("lineStrength"), 0.0);
    effect.parameters.insert(QStringLiteral("colorShift"), 0.0);
    effect.parameters.insert(QStringLiteral("speed"), 2.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 500'000);
    QCOMPARE(out, image);
}

void EngineTest::scanlineGlitchDeterministicAtFixedTime()
{
    const QImage image = makeBlockGlitchTestImage();
    const drift::Effect effect = makeScanlineGlitchEffect();
    constexpr drift::TimeUs timeUs = 333'000;

    const QImage first = EffectProcessor::applyEffects(image, {effect}, timeUs);
    const QImage second = EffectProcessor::applyEffects(image, {effect}, timeUs);
    QCOMPARE(first, second);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("scanline_glitch"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("scanline_glitch")}), QString());
}

void EngineTest::scanlineGlitchVisualChangeAtNonzeroSettings()
{
    const QImage image = makeBlockGlitchTestImage();
    const drift::Effect effect = makeScanlineGlitchEffect();

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 250'000);
    QVERIFY(out != image);

    const QImage later = EffectProcessor::applyEffects(image, {effect}, 750'000);
    QVERIFY(later != out);
}

static QImage makeVhsCrtTestImage()
{
    QImage image(96, 64, QImage::Format_RGBA8888);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixel(x, y, qRgba(40 + x, 80 + y * 2, 160 - x / 2, 255));
        }
    }
    return image;
}

static drift::Effect makeVhsCrtEffect()
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("vhs_crt");
    effect.parameters.insert(QStringLiteral("scanlines"), 0.5);
    effect.parameters.insert(QStringLiteral("noise"), 0.4);
    effect.parameters.insert(QStringLiteral("colorBleed"), 5.0);
    effect.parameters.insert(QStringLiteral("distortion"), 0.35);
    effect.parameters.insert(QStringLiteral("vignette"), 0.4);
    effect.parameters.insert(QStringLiteral("desaturation"), 0.25);
    return effect;
}

void EngineTest::vhsCrtZeroSettingsPassthrough()
{
    const QImage image = makeVhsCrtTestImage();

    drift::Effect effect;
    effect.catalogId = QStringLiteral("vhs_crt");
    effect.parameters.insert(QStringLiteral("scanlines"), 0.0);
    effect.parameters.insert(QStringLiteral("noise"), 0.0);
    effect.parameters.insert(QStringLiteral("colorBleed"), 0.0);
    effect.parameters.insert(QStringLiteral("distortion"), 0.0);
    effect.parameters.insert(QStringLiteral("vignette"), 0.0);
    effect.parameters.insert(QStringLiteral("desaturation"), 0.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 500'000);
    QCOMPARE(out, image);
}

void EngineTest::vhsCrtNonzeroModifiesOutput()
{
    const QImage image = makeVhsCrtTestImage();
    const drift::Effect effect = makeVhsCrtEffect();

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 420'000);
    QVERIFY(out != image);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("vhs_crt"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("vhs_crt")}), QString());
}

void EngineTest::vhsCrtDeterministicAtFixedTime()
{
    const QImage image = makeVhsCrtTestImage();
    const drift::Effect effect = makeVhsCrtEffect();
    constexpr drift::TimeUs timeUs = 420'000;

    const QImage first = EffectProcessor::applyEffects(image, {effect}, timeUs);
    const QImage second = EffectProcessor::applyEffects(image, {effect}, timeUs);
    QCOMPARE(first, second);

    const QImage otherTime = EffectProcessor::applyEffects(image, {effect}, 900'000);
    QVERIFY(otherTime != first);
}

static drift::Effect makeBloomGlowEffect()
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("bloom_glow");
    effect.parameters.insert(QStringLiteral("threshold"), 0.5);
    effect.parameters.insert(QStringLiteral("intensity"), 1.0);
    effect.parameters.insert(QStringLiteral("radius"), 8.0);
    return effect;
}

void EngineTest::bloomGlowZeroIntensityPassthrough()
{
    QImage image(32, 32, QImage::Format_RGBA8888);
    image.fill(QColor(255, 255, 255));

    drift::Effect effect = makeBloomGlowEffect();
    effect.parameters.insert(QStringLiteral("intensity"), 0.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect});
    QCOMPARE(out, image);
}

void EngineTest::bloomGlowDarkFrameUnchanged()
{
    QImage image(32, 32, QImage::Format_RGBA8888);
    image.fill(QColor(20, 25, 30));

    const drift::Effect effect = makeBloomGlowEffect();
    const QImage out = EffectProcessor::applyEffects(image, {effect});
    QCOMPARE(out, image);
}

void EngineTest::bloomGlowBrightSpotBleedsToNeighbors()
{
    QImage image(32, 32, QImage::Format_RGBA8888);
    image.fill(QColor(10, 10, 10));
    image.setPixel(16, 16, qRgba(255, 255, 255, 255));

    const drift::Effect effect = makeBloomGlowEffect();
    const QImage out = EffectProcessor::applyEffects(image, {effect});
    QVERIFY(out != image);

    const QRgb center = out.pixel(16, 16);
    QVERIFY(qRed(center) > 200 || qGreen(center) > 200 || qBlue(center) > 200);

    const QRgb neighbor = out.pixel(18, 16);
    QVERIFY(qRed(neighbor) > 10 || qGreen(neighbor) > 10 || qBlue(neighbor) > 10);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("bloom_glow"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("bloom_glow")}), QString());
}

static drift::Effect makeRippleWaterEffect()
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("ripple_water");
    effect.parameters.insert(QStringLiteral("amplitude"), 12.0);
    effect.parameters.insert(QStringLiteral("frequency"), 10.0);
    effect.parameters.insert(QStringLiteral("speed"), 1.0);
    effect.parameters.insert(QStringLiteral("centerX"), 0.5);
    effect.parameters.insert(QStringLiteral("centerY"), 0.5);
    return effect;
}

void EngineTest::rippleWaterZeroAmplitudePassthrough()
{
    const QImage image = makeBlockGlitchTestImage();

    drift::Effect effect = makeRippleWaterEffect();
    effect.parameters.insert(QStringLiteral("amplitude"), 0.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 500'000);
    QCOMPARE(out, image);
}

void EngineTest::rippleWaterNonzeroDisplacementChangesOutput()
{
    const QImage image = makeBlockGlitchTestImage();
    const drift::Effect effect = makeRippleWaterEffect();

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 250'000);
    QVERIFY(out != image);

    const QImage later = EffectProcessor::applyEffects(image, {effect}, 750'000);
    QVERIFY(later != out);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("ripple_water"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("ripple_water")}), QString());
}

static QImage makeHighContrastRectangleImage()
{
    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(QColor(0, 0, 0));
    for (int y = 16; y < 48; ++y) {
        for (int x = 16; x < 48; ++x)
            image.setPixel(x, y, qRgba(255, 255, 255, 255));
    }
    return image;
}

static drift::Effect makeEdgeNeonEffect()
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("edge_neon");
    effect.parameters.insert(QStringLiteral("threshold"), 0.15);
    effect.parameters.insert(QStringLiteral("intensity"), 1.0);
    effect.parameters.insert(QStringLiteral("radius"), 4.0);
    effect.parameters.insert(QStringLiteral("color"), QStringLiteral("#00ffff"));
    return effect;
}

void EngineTest::edgeNeonZeroIntensityUnchanged()
{
    const QImage image = makeHighContrastRectangleImage();

    drift::Effect effect = makeEdgeNeonEffect();
    effect.parameters.insert(QStringLiteral("intensity"), 0.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect});
    QCOMPARE(out, image);
}

void EngineTest::edgeNeonHighContrastRectangleGlow()
{
    const QImage image = makeHighContrastRectangleImage();
    const drift::Effect effect = makeEdgeNeonEffect();

    const QImage out = EffectProcessor::applyEffects(image, {effect});
    QVERIFY(out != image);

    const QRgb outside = out.pixel(14, 32);
    QVERIFY(qGreen(outside) > qGreen(image.pixel(14, 32)));
    QVERIFY(qBlue(outside) > qBlue(image.pixel(14, 32)));

    const QRgb inside = out.pixel(32, 32);
    QCOMPARE(qRed(inside), 255);
    QCOMPARE(qGreen(inside), 255);
    QCOMPARE(qBlue(inside), 255);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("edge_neon"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(def->fixedParams.value(QStringLiteral("color")).toString(), QStringLiteral("#00ffff"));
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("edge_neon")}), QString());
}

static drift::Effect makeDigitalGlitchEffect()
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("digital_glitch");
    effect.parameters.insert(QStringLiteral("intensity"), 0.75);
    effect.parameters.insert(QStringLiteral("frequency"), 0.5);
    effect.parameters.insert(QStringLiteral("rgbAmount"), 12.0);
    effect.parameters.insert(QStringLiteral("blockAmount"), 0.6);
    effect.parameters.insert(QStringLiteral("flashAmount"), 0.25);
    effect.parameters.insert(QStringLiteral("seed"), 42.0);
    return effect;
}

void EngineTest::digitalGlitchZeroIntensityUnchanged()
{
    const QImage image = makeBlockGlitchTestImage();

    drift::Effect effect = makeDigitalGlitchEffect();
    effect.parameters.insert(QStringLiteral("intensity"), 0.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 500'000);
    QCOMPARE(out, image);
}

void EngineTest::digitalGlitchDeterministicForFixedTimeAndSeed()
{
    const QImage image = makeBlockGlitchTestImage();
    const drift::Effect effect = makeDigitalGlitchEffect();
    constexpr drift::TimeUs timeUs = 620'000;

    const QImage first = EffectProcessor::applyEffects(image, {effect}, timeUs);
    const QImage second = EffectProcessor::applyEffects(image, {effect}, timeUs);
    QCOMPARE(first, second);
    QVERIFY(first != image);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("digital_glitch"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("digital_glitch")}), QString());

    drift::Effect otherSeed = effect;
    otherSeed.parameters.insert(QStringLiteral("seed"), 99.0);
    const QImage other = EffectProcessor::applyEffects(image, {otherSeed}, timeUs);
    QVERIFY(other != first);
}

static drift::Effect makeFilmBurnEffect()
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("film_burn");
    effect.parameters.insert(QStringLiteral("intensity"), 0.8);
    effect.parameters.insert(QStringLiteral("warmth"), 0.85);
    effect.parameters.insert(QStringLiteral("flicker"), 0.4);
    effect.parameters.insert(QStringLiteral("position"), QStringLiteral("left"));
    effect.parameters.insert(QStringLiteral("seed"), 7.0);
    return effect;
}

void EngineTest::filmBurnZeroIntensityUnchanged()
{
    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(QColor(30, 30, 40));

    drift::Effect effect = makeFilmBurnEffect();
    effect.parameters.insert(QStringLiteral("intensity"), 0.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 400'000);
    QCOMPARE(out, image);
}

void EngineTest::filmBurnAddsWarmLeakContribution()
{
    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(QColor(20, 22, 35));

    const drift::Effect effect = makeFilmBurnEffect();
    constexpr drift::TimeUs timeUs = 400'000;

    const QImage out = EffectProcessor::applyEffects(image, {effect}, timeUs);
    QVERIFY(out != image);

    const QRgb edge = out.pixel(0, 32);
    const QRgb original = image.pixel(0, 32);
    QVERIFY(qRed(edge) > qRed(original));
    QVERIFY(qGreen(edge) > qGreen(original));
    QVERIFY(qRed(edge) > qBlue(edge));

    const QImage second = EffectProcessor::applyEffects(image, {effect}, timeUs);
    QCOMPARE(out, second);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("film_burn"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(def->fixedParams.value(QStringLiteral("position")).toString(), QStringLiteral("left"));
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("film_burn")}), QString());
}

void EngineTest::timeEchoBlendDeterministic()
{
    auto makeFrame = [](const QColor &color, int rectX) {
        QImage image(32, 32, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.fillRect(rectX, 10, 10, 10, color);
        return image;
    };

    const QList<QImage> samples = {makeFrame(Qt::red, 18), makeFrame(Qt::blue, 4)};
    const QImage first =
        CompositorFrameHistory::applyTimeEcho(samples, 0.55, CompositorFrameHistory::EchoBlendMode::Normal);
    const QImage second =
        CompositorFrameHistory::applyTimeEcho(samples, 0.55, CompositorFrameHistory::EchoBlendMode::Normal);
    QCOMPARE(first, second);
    QVERIFY(first != samples.first());
    QVERIFY(qBlue(first.pixel(8, 14)) > 0);
    QVERIFY(qRed(first.pixel(22, 14)) > 200);
}

void EngineTest::timeEchoBlendIncludesHistoryContribution()
{
    auto makeFrame = [](const QColor &color, int rectX) {
        QImage image(32, 32, QImage::Format_RGBA8888);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.fillRect(rectX, 10, 10, 10, color);
        return image;
    };

    const QList<QImage> samples = {makeFrame(Qt::red, 18), makeFrame(Qt::blue, 4)};
    const QImage normal =
        CompositorFrameHistory::applyTimeEcho(samples, 0.5, CompositorFrameHistory::EchoBlendMode::Normal);
    const QImage currentOnly =
        CompositorFrameHistory::applyTimeEcho({samples.first()}, 0.5, CompositorFrameHistory::EchoBlendMode::Normal);
    QVERIFY(normal != currentOnly);
    QVERIFY(qBlue(normal.pixel(8, 14)) > qBlue(currentOnly.pixel(8, 14)));
}

static drift::Effect makeTimeEchoEffect(const QString &blendMode = QStringLiteral("add"))
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("time_echo");
    effect.parameters.insert(QStringLiteral("frames"), 4);
    effect.parameters.insert(QStringLiteral("decay"), 0.55);
    effect.parameters.insert(QStringLiteral("blendMode"), blendMode);
    return effect;
}

void EngineTest::timeEchoDeterministicAtFixedTimelineTime()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeColorSegmentsVideo(dir);
    if (path.isEmpty())
        QSKIP("ffmpeg not available to generate a test clip");

    drift::Project project;
    project.setResolution(64, 64);
    project.setFps(25);
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Video});

    drift::Clip clip;
    clip.id = QStringLiteral("video");
    clip.type = drift::ClipType::Video;
    clip.path = path;
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(3.0);
    clip.effects.append(makeTimeEchoEffect());
    project.tracks()[0].clips.append(clip);

    FrameCompositor compositor;
    compositor.setProject(&project);

    constexpr drift::TimeUs timeUs = drift::secondsToUs(2.1);
    const QImage first = compositor.compositeAt(timeUs);
    const QImage second = compositor.compositeAt(timeUs);
    QCOMPARE(first, second);
    QVERIFY(!first.isNull());
}

void EngineTest::timeEchoBlendsPriorVideoFrames()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = makeColorSegmentsVideo(dir);
    if (path.isEmpty())
        QSKIP("ffmpeg not available to generate a test clip");

    drift::Project project;
    project.setResolution(64, 64);
    project.setFps(25);
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Video});

    drift::Clip clip;
    clip.id = QStringLiteral("video");
    clip.type = drift::ClipType::Video;
    clip.path = path;
    clip.timelineStart = 0;
    clip.timelineDuration = drift::secondsToUs(3.0);
    project.tracks()[0].clips.append(clip);

    FrameCompositor compositor;
    compositor.setProject(&project);

    constexpr drift::TimeUs timeUs = drift::secondsToUs(2.1);
    const QImage withoutEcho = compositor.compositeAt(timeUs);

    clip.effects.append(makeTimeEchoEffect(QStringLiteral("add")));
    project.tracks()[0].clips.clear();
    project.tracks()[0].clips.append(clip);
    compositor.setProject(&project);

    const QImage withEcho = compositor.compositeAt(timeUs);
    QVERIFY(withEcho != withoutEcho);

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("time_echo"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(def->fixedParams.value(QStringLiteral("blendMode")).toString(), QStringLiteral("normal"));
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("time_echo")}), QString());
}

static drift::Effect makeShockwavePulseEffect()
{
    drift::Effect effect;
    effect.catalogId = QStringLiteral("shockwave_pulse");
    effect.parameters.insert(QStringLiteral("centerX"), 0.5);
    effect.parameters.insert(QStringLiteral("centerY"), 0.5);
    effect.parameters.insert(QStringLiteral("radius"), 0.0);
    effect.parameters.insert(QStringLiteral("width"), 0.12);
    effect.parameters.insert(QStringLiteral("strength"), 0.6);
    effect.parameters.insert(QStringLiteral("speed"), 1.0);
    return effect;
}

void EngineTest::shockwavePulseZeroStrengthPassthrough()
{
    const QImage image = makeBlockGlitchTestImage();

    drift::Effect effect = makeShockwavePulseEffect();
    effect.parameters.insert(QStringLiteral("strength"), 0.0);

    const QImage out = EffectProcessor::applyEffects(image, {effect}, 500'000);
    QCOMPARE(out, image);
}

void EngineTest::shockwavePulseChangesPixelsNearWavefront()
{
    const QImage image = makeBlockGlitchTestImage();
    const drift::Effect effect = makeShockwavePulseEffect();

    // speed=1 => wave radius 0.233 at t=233ms; pixel (80,32) lies on that ring from center (64,32).
    constexpr drift::TimeUs timeUs = 233'000;
    const QImage out = EffectProcessor::applyEffects(image, {effect}, timeUs);
    QVERIFY(out != image);
    QVERIFY(out.pixel(80, 32) != image.pixel(80, 32));

    const QImage awayFromWave = EffectProcessor::applyEffects(image, {effect}, 50'000);
    QVERIFY(awayFromWave.pixel(80, 32) != out.pixel(80, 32));

    const EffectPresetEntry *def = effectDefForId(QStringLiteral("shockwave_pulse"));
    QVERIFY(def);
    QVERIFY(def->meta.compositorOnly);
    QCOMPARE(buildFilterGraphForEffect({.catalogId = QStringLiteral("shockwave_pulse")}), QString());
}

void EngineTest::compositorCrossfadeBetweenShapeClips()
{
    drift::Project project;
    project.setResolution(128, 128);
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Video});

    drift::Clip clipA;
    clipA.id = QStringLiteral("a");
    clipA.type = drift::ClipType::Shape;
    clipA.timelineStart = 0;
    clipA.timelineDuration = drift::secondsToUs(2.0);
    clipA.shapeStyle.kind = drift::ShapeKind::Rectangle;
    clipA.shapeStyle.fill = Qt::red;

    drift::Clip clipB;
    clipB.id = QStringLiteral("b");
    clipB.type = drift::ClipType::Shape;
    clipB.timelineStart = drift::secondsToUs(2.0);
    clipB.timelineDuration = drift::secondsToUs(2.0);
    clipB.shapeStyle.kind = drift::ShapeKind::Rectangle;
    clipB.shapeStyle.fill = Qt::blue;

    project.tracks()[0].clips.append(clipA);
    project.tracks()[0].clips.append(clipB);

    drift::Transition transition;
    transition.id = QStringLiteral("tr");
    transition.fromClipId = clipA.id;
    transition.toClipId = clipB.id;
    transition.kind = drift::TransitionKind::Crossfade;
    transition.durationUs = drift::secondsToUs(1.0);
    project.tracks()[0].transitions.append(transition);

    FrameCompositor compositor;
    compositor.setProject(&project);

    const QImage redOnly = compositor.compositeAt(drift::secondsToUs(1.0));
    const QImage blueOnly = compositor.compositeAt(drift::secondsToUs(3.0));
    const QImage mid = compositor.compositeAt(drift::secondsToUs(2.0));
    QVERIFY(!mid.isNull());
    const QRgb center = mid.pixel(64, 64);
    const QRgb redCenter = redOnly.pixel(64, 64);
    const QRgb blueCenter = blueOnly.pixel(64, 64);
    QVERIFY(center != redCenter);
    QVERIFY(center != blueCenter);
    QVERIFY(qRed(center) > 0);
    QVERIFY(qBlue(center) > 0);
}

static void appendRedBlueShapeTransition(drift::Project &project, drift::TransitionKind kind)
{
    project.setResolution(128, 128);
    project.tracks().clear();
    project.tracks().append(drift::Track{.type = drift::TrackType::Video});

    drift::Clip clipA;
    clipA.id = QStringLiteral("a");
    clipA.type = drift::ClipType::Shape;
    clipA.timelineStart = 0;
    clipA.timelineDuration = drift::secondsToUs(2.0);
    clipA.shapeStyle.kind = drift::ShapeKind::Rectangle;
    clipA.shapeStyle.fill = Qt::red;

    drift::Clip clipB;
    clipB.id = QStringLiteral("b");
    clipB.type = drift::ClipType::Shape;
    clipB.timelineStart = drift::secondsToUs(2.0);
    clipB.timelineDuration = drift::secondsToUs(2.0);
    clipB.shapeStyle.kind = drift::ShapeKind::Rectangle;
    clipB.shapeStyle.fill = Qt::blue;

    project.tracks()[0].clips.append(clipA);
    project.tracks()[0].clips.append(clipB);

    drift::Transition transition;
    transition.id = QStringLiteral("tr");
    transition.fromClipId = clipA.id;
    transition.toClipId = clipB.id;
    transition.kind = kind;
    transition.durationUs = drift::secondsToUs(1.0);
    project.tracks()[0].transitions.append(transition);
}

void EngineTest::compositorDipToBlackMidpointIsBlack()
{
    drift::Project project;
    appendRedBlueShapeTransition(project, drift::TransitionKind::DipToBlack);

    FrameCompositor compositor;
    compositor.setProject(&project);

    const QImage mid = compositor.compositeAt(drift::secondsToUs(2.0));
    QVERIFY(!mid.isNull());
    const QRgb center = mid.pixel(64, 64);
    QVERIFY(qRed(center) < 30);
    QVERIFY(qGreen(center) < 30);
    QVERIFY(qBlue(center) < 30);
}

void EngineTest::compositorWipeRightRevealsIncomingClip()
{
    drift::Project project;
    appendRedBlueShapeTransition(project, drift::TransitionKind::WipeRight);

    FrameCompositor compositor;
    compositor.setProject(&project);

    const QImage early = compositor.compositeAt(drift::secondsToUs(1.75));
    const QImage late = compositor.compositeAt(drift::secondsToUs(2.25));
    QVERIFY(!early.isNull());
    QVERIFY(!late.isNull());
    // Shape clips are small and centered; sample canvas center, not edges.
    QVERIFY(qRed(early.pixel(64, 64)) > qBlue(early.pixel(64, 64)));
    QVERIFY(qBlue(late.pixel(64, 64)) > qRed(late.pixel(64, 64)));
}

void EngineTest::maskApplierEllipseMasksCorners()
{
    QImage image(64, 64, QImage::Format_RGBA8888);
    image.fill(Qt::white);

    drift::Mask mask;
    mask.shape = drift::MaskShape::Ellipse;
    mask.x = 0.5;
    mask.y = 0.5;
    mask.w = 0.5;
    mask.h = 0.5;

    const QImage masked = drift::applyMask(image, mask, 64, 64);
    QVERIFY(qAlpha(masked.pixel(32, 32)) > 200);
    QVERIFY(qAlpha(masked.pixel(0, 0)) < 20);
}

QTEST_MAIN(EngineTest)
#include "tst_engine.moc"
