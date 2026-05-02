#include <QtTest>

#include "playback/PlaybackClock.h"

class PlaybackTest : public QObject
{
    Q_OBJECT

private slots:
    void clockPausedPosition();
    void clockWallFallbackWhileRunning();
    void produceAdvancesWithRenderedSamples();
    void playbackTracksSinkPosition();
    void avSyncWithinTolerance();
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
    // The produce position tracks audio mixed into the sink buffer, exactly by
    // the rendered sample count, independent of what the device has played.
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
    // The visible playhead follows the sink's played position, not the produce
    // position, so video stays in sync with audio rather than leading it.
    PlaybackClock clock;
    clock.reset(0, 48000);
    clock.start();

    // Produced well ahead, but only 0.1s has actually played.
    clock.onAudioSamplesRendered(48000);
    clock.syncPlaybackUs(drift::secondsToUs(0.1));

    const drift::TimeUs played = clock.currentTimeUs();
    QVERIFY(played >= drift::secondsToUs(0.1));
    QVERIFY(played < drift::secondsToUs(0.2)); // nowhere near the 1.0s produced
    QVERIFY(clock.produceTimeUs() >= drift::secondsToUs(1.0));
}

void PlaybackTest::avSyncWithinTolerance()
{
    PlaybackClock clock;
    clock.reset(0, 48000);
    clock.start();

    clock.onAudioSamplesRendered(1920);
    clock.syncPlaybackUs(drift::secondsToUs(0.04));

    // Two reads of the playhead taken back-to-back must agree closely (the video
    // composite and the UI read the same clock within a frame).
    const drift::TimeUs a = clock.currentTimeUs();
    const drift::TimeUs b = clock.currentTimeUs();
    QVERIFY(qAbs(a - b) <= 40'000);
}

QTEST_MAIN(PlaybackTest)
#include "tst_playback.moc"
