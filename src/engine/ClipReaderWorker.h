#pragma once

#include "ClipReader.h"

#include "core/Time.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>

// Owns a ClipReader on a dedicated thread; all decode calls are serialized here.
class ClipReaderWorker : public QObject
{
    Q_OBJECT

public:
    explicit ClipReaderWorker(QObject *parent = nullptr);

public slots:
    void openPath(const QString &path);
    void closePath();
    QImage decodeVideo(drift::TimeUs sourceUs, int targetWidth, int targetHeight);
    int decodeAudio(drift::TimeUs sourceStartUs, int sampleCount, int outputSampleRate,
                    float *interleavedStereoOut);
    void prefetchVideo(drift::TimeUs sourceUs, int targetWidth, int targetHeight);

private:
    ClipReader m_reader;
    QMutex m_mutex;
    drift::TimeUs m_cachedSourceUs = -1;
    QImage m_cachedFrame;
    int m_cachedWidth = 0;
    int m_cachedHeight = 0;
};
