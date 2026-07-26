#include <QtTest>

#include "playback/AdaptivePreviewPolicy.h"
#include "playback/PlaybackClock.h"

class PlaybackTest : public QObject
{
    Q_OBJECT

private slots:
    void clockPausedPosition();
    void clockWallFallbackWhileRunning();
    void produceAdvancesWithRenderedSamples();
    void playbackTracksSinkPosition();
    void seekWhileRunningKeepsClockAlive();
    void avSyncWithinTolerance();

    void adaptiveScalesDownOnOverBudget();
    void adaptiveRecoversSlowly();
    void adaptiveRespectsMinimumScale();
    void presentAcceptsForwardFrames();
    void presentRejectsObsoleteGeneration();
    void presentRejectsPreSeekFrames();
};

void PlaybackTest::clockPausedPosition()
{
    PlaybackClock clock;
    clock.reset(drift::secondsToUs(2.5), 48000);
    QCOMPARE(clock.currentTimeUs(), drift::secondsToUs(2.5));
    QCOMPARE(clock.pausedAt(), drift::secondsToUs(2.5));
}

void PlaybackTest::clockWallFallbackWhileRunning()
{
    PlaybackClock clock;
    clock.reset(0, 48000);
    clock.start();
    QTest::qWait(50);
    QVERIFY(clock.currentTimeUs() >= drift::kUsPerMs * 40);
    clock.pause();
    const drift::TimeUs paused = clock.pausedAt();
    QVERIFY(paused >= drift::kUsPerMs * 40);
}

void PlaybackTest::produceAdvancesWithRenderedSamples()
{
    PlaybackClock clock;
    clock.reset(0, 48000);
    clock.start();
    clock.onAudioSamplesRendered(4800);
    QCOMPARE(clock.produceTimeUs(), drift::secondsToUs(0.1));
    clock.onAudioSamplesRendered(4800);
    QCOMPARE(clock.produceTimeUs(), drift::secondsToUs(0.2));
}

void PlaybackTest::playbackTracksSinkPosition()
{
    PlaybackClock clock;
    clock.reset(0, 48000);
    clock.start();

    clock.onAudioSamplesRendered(48000);
    clock.syncPlaybackUs(drift::secondsToUs(0.1));

    const drift::TimeUs played = clock.currentTimeUs();
    QVERIFY(played >= drift::secondsToUs(0.1));
    QVERIFY(played < drift::secondsToUs(0.2));
    QVERIFY(clock.produceTimeUs() >= drift::secondsToUs(1.0));
}

void PlaybackTest::seekWhileRunningKeepsClockAlive()
{
    PlaybackClock clock;
    clock.reset(drift::secondsToUs(1.0), 48000);
    clock.start();
    clock.onAudioSamplesRendered(4800);
    QCOMPARE(clock.produceTimeUs(), drift::secondsToUs(1.1));

    clock.reset(drift::secondsToUs(2.0), 48000);
    QVERIFY(!clock.isRunning());
    QCOMPARE(clock.produceTimeUs(), drift::secondsToUs(2.0));

    clock.start();
    QVERIFY(clock.isRunning());
    clock.onAudioSamplesRendered(4800);
    QCOMPARE(clock.produceTimeUs(), drift::secondsToUs(2.1));
}

void PlaybackTest::avSyncWithinTolerance()
{
    PlaybackClock clock;
    clock.reset(0, 48000);
    clock.start();

    clock.onAudioSamplesRendered(1920);
    clock.syncPlaybackUs(drift::secondsToUs(0.04));

    const drift::TimeUs a = clock.currentTimeUs();
    const drift::TimeUs b = clock.currentTimeUs();
    QVERIFY(qAbs(a - b) <= 40'000);
}

void PlaybackTest::adaptiveScalesDownOnOverBudget()
{
    AdaptivePreviewPolicy::State state;
    state = AdaptivePreviewPolicy::noteRenderCost(state, 50, 33);
    QVERIFY(state.scale < 1.0);
    QVERIFY(state.scale >= AdaptivePreviewPolicy::kScaleMin);
}

void PlaybackTest::adaptiveRecoversSlowly()
{
    AdaptivePreviewPolicy::State state;
    state.scale = 0.5;
    for (int i = 0; i < AdaptivePreviewPolicy::kUnderBudgetBeforeScaleUp; ++i)
        state = AdaptivePreviewPolicy::noteRenderCost(state, 30, 33);
    QCOMPARE(state.scale, 0.5);

    for (int i = 0; i < AdaptivePreviewPolicy::kUnderBudgetBeforeScaleUp; ++i)
        state = AdaptivePreviewPolicy::noteRenderCost(state, 20, 33);
    QVERIFY(state.scale > 0.5);
}

void PlaybackTest::adaptiveRespectsMinimumScale()
{
    AdaptivePreviewPolicy::State state;
    for (int i = 0; i < 20; ++i)
        state = AdaptivePreviewPolicy::noteRenderCost(state, 100, 33);
    QCOMPARE(state.scale, AdaptivePreviewPolicy::kScaleMin);
}

void PlaybackTest::presentAcceptsForwardFrames()
{
    QVERIFY(AdaptivePreviewPolicy::shouldPresentFrame(1000, 1, -1, 1));
    QVERIFY(AdaptivePreviewPolicy::shouldPresentFrame(2000, 1, 1000, 1));
    QVERIFY(AdaptivePreviewPolicy::shouldPresentFrame(2000, 1, 2000, 1));
    QVERIFY(!AdaptivePreviewPolicy::shouldPresentFrame(1500, 1, 2000, 1));
}

void PlaybackTest::presentRejectsObsoleteGeneration()
{
    QVERIFY(!AdaptivePreviewPolicy::shouldPresentFrame(3000, 1, 1000, 2));
    QVERIFY(AdaptivePreviewPolicy::shouldPresentFrame(3000, 2, -1, 2));
}

void PlaybackTest::presentRejectsPreSeekFrames()
{
    QVERIFY(!AdaptivePreviewPolicy::shouldPresentFrame(2'000'000, 1, -1, 1, 5'000'000));
    QVERIFY(AdaptivePreviewPolicy::shouldPresentFrame(5'000'000, 1, -1, 1, 5'000'000));
}

QTEST_MAIN(PlaybackTest)
#include "tst_playback.moc"
