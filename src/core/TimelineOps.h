#pragma once

#include "Project.h"
#include "Time.h"

namespace drift {

constexpr TimeUs kImageClipDurationUs = 5 * kUsPerSecond;
constexpr TimeUs kTextClipDurationUs = 5 * kUsPerSecond;
constexpr TimeUs kMinClipDurationUs = kUsPerSecond / 10;
constexpr TimeUs kSnapThresholdUs = 150'000;

TimeUs snapTime(const Project &project, TimeUs time, bool snapEnabled, TimeUs playheadUs);

TimeUs resolveClipStart(const Project &project, const Track &track, int excludeClipIndex,
                        TimeUs desiredStart, TimeUs duration, bool snapEnabled, TimeUs playheadUs);

int defaultTrackForClipType(const Project &project, ClipType type);

TimeUs clipDurationForAsset(const MediaAsset *asset);

TimeUs sourceDurationForClip(const Project &project, const Clip &clip);

} // namespace drift
