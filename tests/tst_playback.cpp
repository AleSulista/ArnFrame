#include <QtTest>

#include "playback/PlaybackClock.h"

class PlaybackTest : public QObject
{
    Q_OBJECT

private slots:
    void clockPausedPosition();
    void clockWallFallbackWhileRunning();
    void clockAudioMasterAdvances();
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

void PlaybackTest::clockAudioMasterAdvances()
{
    PlaybackClock clock;
    clock.reset(0, 48000);
    clock.start();
    clock.onAudioSamplesRendered(4800);
    QCOMPARE(clock.currentTimeUs(), drift::secondsToUs(0.1));
    clock.onAudioSamplesRendered(4800);
    QCOMPARE(clock.currentTimeUs(), drift::secondsToUs(0.2));
}

void PlaybackTest::avSyncWithinTolerance()
{
    PlaybackClock clock;
    clock.reset(0, 48000);
    clock.start();

    clock.onAudioSamplesRendered(1920);
    const drift::TimeUs audioTime = clock.currentTimeUs();

    const drift::TimeUs videoCompositeTime = clock.currentTimeUs();
    const drift::TimeUs driftUs = qAbs(audioTime - videoCompositeTime);
    QVERIFY(driftUs <= 40'000);
}

QTEST_MAIN(PlaybackTest)
#include "tst_playback.moc"
