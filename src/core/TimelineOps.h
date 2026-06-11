#pragma once

#include "Project.h"
#include "Time.h"

namespace drift {

constexpr TimeUs kImageClipDurationUs = 5 * kUsPerSecond;
constexpr TimeUs kTextClipDurationUs = 5 * kUsPerSecond;
constexpr TimeUs kSubtitleClipDurationUs = 30 * kUsPerSecond;
constexpr TimeUs kMinClipDurationUs = kUsPerSecond / 10;
constexpr TimeUs kSnapThresholdUs = 150'000;

TimeUs snapTime(const Project &project, TimeUs time, bool snapEnabled, TimeUs playheadUs);

TimeUs resolveClipStart(const Project &project, const Track &track, int excludeClipIndex,
                        TimeUs desiredStart, TimeUs duration, bool snapEnabled, TimeUs playheadUs);

TrackType trackTypeForClipType(ClipType type);

int defaultTrackForClipType(const Project &project, ClipType type);

int ensureTrackForClipType(Project &project, ClipType type, bool insertAtTop = false);

// Always prepends a fresh track (multiple tracks of the same type are allowed).
int insertTrackAtTopForClipType(Project &project, ClipType type);

TimeUs clipDurationForAsset(const MediaAsset *asset);

TimeUs sourceDurationForClip(const Project &project, const Clip &clip);

// Split `head` at `offset` from its timeline start into head + tail (same reverse/speed).
// Caller assigns `tail.id`. Returns false if the offset is too close to either end.
bool splitClipAtOffset(Clip &head, Clip &tail, TimeUs offset);

// True when `left` ends where `right` begins, shares media + reverse/speed, and source ranges abut.
bool clipsCanMerge(const Clip &left, const Clip &right);

// Merge abutting clips. Keeps left transforms/effects; takes right's fade-out.
Clip mergeClips(const Clip &left, const Clip &right);

struct ClipRef
{
    int trackIndex = -1;
    int clipIndex = -1;
};

// All clips sharing `clip.linkId` (excluding `clip` itself).
QList<ClipRef> linkedPartners(const Project &project, const Clip &clip);

// Mirror timeline/source timing from a linked source clip.
void syncLinkedTiming(Clip &dst, const Clip &src);

// Copy link fields when splitting a linked clip so both halves stay paired.
// Returns the link id assigned to `tail` (empty when `head` was not linked).
QString assignSplitLinkIds(Clip &head, Clip &tail);

} // namespace drift
