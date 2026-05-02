#include "TimelineOps.h"

#include <algorithm>

namespace drift {

TimeUs snapTime(const Project &project, TimeUs time, bool snapEnabled, TimeUs playheadUs)
{
    if (!snapEnabled)
        return qMax<TimeUs>(0, time);

    QList<TimeUs> targets = {0, playheadUs};
    for (const Track &track : project.tracks()) {
        for (const Clip &clip : track.clips) {
            targets.append(clip.timelineStart);
            targets.append(clip.timelineEnd());
        }
    }

    TimeUs best = time;
    TimeUs bestDistance = kSnapThresholdUs;
    for (TimeUs target : targets) {
        const TimeUs distance = qAbs(target - time);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = target;
        }
    }

    return qMax<TimeUs>(0, best);
}

TimeUs resolveClipStart(const Project &project, const Track &track, int excludeClipIndex,
                        TimeUs desiredStart, TimeUs duration, bool snapEnabled, TimeUs playheadUs)
{
    TimeUs start = snapTime(project, desiredStart, snapEnabled, playheadUs);

    struct Interval {
        TimeUs begin;
        TimeUs end;
    };
    QList<Interval> intervals;
    intervals.reserve(track.clips.size());

    for (int i = 0; i < track.clips.size(); ++i) {
        if (i == excludeClipIndex)
            continue;
        const Clip &clip = track.clips.at(i);
        intervals.append({clip.timelineStart, clip.timelineEnd()});
    }

    std::sort(intervals.begin(), intervals.end(),
              [](const Interval &a, const Interval &b) { return a.begin < b.begin; });

    bool adjusted = true;
    while (adjusted) {
        adjusted = false;
        for (const Interval &interval : intervals) {
            if (start < interval.end && start + duration > interval.begin) {
                start = interval.end;
                adjusted = true;
            }
        }
    }

    return qMax<TimeUs>(0, start);
}

TrackType trackTypeForClipType(ClipType type)
{
    switch (type) {
    case ClipType::Audio:
        return TrackType::Audio;
    case ClipType::Text:
        return TrackType::Text;
    case ClipType::Image:
        return TrackType::Shape;
    case ClipType::Video:
        break;
    }
    return TrackType::Video;
}

int defaultTrackForClipType(const Project &project, ClipType type)
{
    const TrackType trackType = trackTypeForClipType(type);
    const QList<Track> &tracks = project.tracks();
    for (int i = 0; i < tracks.size(); ++i) {
        if (tracks[i].type == trackType && tracks[i].allowsClipType(type))
            return i;
    }
    return -1;
}

int ensureTrackForClipType(Project &project, ClipType type, bool insertAtTop)
{
    const int existing = defaultTrackForClipType(project, type);
    if (existing >= 0)
        return existing;

    const Track track{.type = trackTypeForClipType(type)};
    if (insertAtTop)
        project.tracks().prepend(track);
    else
        project.tracks().append(track);
    return insertAtTop ? 0 : project.tracks().size() - 1;
}

int insertTrackAtTopForClipType(Project &project, ClipType type)
{
    project.tracks().prepend(Track{.type = trackTypeForClipType(type)});
    return 0;
}

TimeUs clipDurationForAsset(const MediaAsset *asset)
{
    if (!asset)
        return kImageClipDurationUs;

    if (asset->kind == MediaKind::Image)
        return kImageClipDurationUs;

    if (asset->durationUs > 0)
        return asset->durationUs;

    return kImageClipDurationUs;
}

TimeUs sourceDurationForClip(const Project &project, const Clip &clip)
{
    if (!clip.assetId.isEmpty()) {
        if (const MediaAsset *asset = project.asset(clip.assetId)) {
            if (asset->durationUs > 0)
                return asset->durationUs;
        }
    }

    if (clip.type == ClipType::Image)
        return kImageClipDurationUs;

    return qMax(clip.srcOut, clip.timelineDuration);
}

} // namespace drift
