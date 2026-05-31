#pragma once

#include "Time.h"

#include <QList>
#include <QString>

namespace drift {

struct SubtitleCue
{
    TimeUs startUs = 0; // relative to the parent clip's timeline start
    TimeUs endUs = 0;
    QString text;
};

const SubtitleCue *activeSubtitleCueAt(const QList<SubtitleCue> &cues, TimeUs localUs);
int subtitleCueIndexAt(const QList<SubtitleCue> &cues, TimeUs localUs);
void sortSubtitleCues(QList<SubtitleCue> &cues);

} // namespace drift
