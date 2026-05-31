#include "SubtitleCue.h"

#include <algorithm>

namespace drift {

const SubtitleCue *activeSubtitleCueAt(const QList<SubtitleCue> &cues, TimeUs localUs)
{
    for (const SubtitleCue &cue : cues) {
        if (localUs >= cue.startUs && localUs < cue.endUs)
            return &cue;
    }
    return nullptr;
}

int subtitleCueIndexAt(const QList<SubtitleCue> &cues, TimeUs localUs)
{
    for (int i = 0; i < cues.size(); ++i) {
        const SubtitleCue &cue = cues.at(i);
        if (localUs >= cue.startUs && localUs < cue.endUs)
            return i;
    }
    return -1;
}

void sortSubtitleCues(QList<SubtitleCue> &cues)
{
    std::sort(cues.begin(), cues.end(), [](const SubtitleCue &a, const SubtitleCue &b) {
        if (a.startUs != b.startUs)
            return a.startUs < b.startUs;
        return a.endUs < b.endUs;
    });
}

} // namespace drift
