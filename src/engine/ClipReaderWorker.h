#pragma once

#include "ClipReader.h"

#include "core/Time.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QString>

// Owns a ClipReader on a dedicated thread; all decode calls are serialized here.
// The reader keeps its own frame cache, so this class holds no cache of its own.
class ClipReaderWorker : public QObject
{
    Q_OBJECT

public:
    explicit ClipReaderWorker(QObject *parent = nullptr);

public slots:
    void openPath(const QString &path);
    void closePath();
    QImage decodeVideo(drift::TimeUs sourceUs, int maxWidth, int maxHeight);
    int decodeAudio(drift::TimeUs sourceStartUs, int sampleCount, int outputSampleRate,
                    float *interleavedStereoOut);
    void prefetchNextVideo(int maxWidth, int maxHeight);

private:
    ClipReader m_reader;
    QMutex m_mutex;
};
