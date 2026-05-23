#pragma once

#include "Time.h"

#include <QString>

namespace drift {

struct Clip;
struct Track;

enum class TransitionKind {
    Crossfade,
    DipToBlack,
    DipToWhite,
    WipeLeft,
    WipeRight,
    WipeUp,
    WipeDown,
    PushLeft,
    ZoomIn,
};

QString transitionKindToString(TransitionKind kind);
TransitionKind transitionKindFromString(const QString &kind);
QString transitionKindLabel(TransitionKind kind);

struct TransitionBlendOpacities
{
    double outgoing = 1.0;
    double incoming = 0.0;
    double blackOverlay = 0.0;
    double whiteOverlay = 0.0;
};

TransitionBlendOpacities transitionBlendOpacities(TransitionKind kind, double progress);
bool transitionUsesCrossfadeAudio(TransitionKind kind);

struct Transition
{
    QString id;
    QString fromClipId; // outgoing
    QString toClipId;   // incoming
    TransitionKind kind = TransitionKind::Crossfade;
    TimeUs durationUs = 500'000; // 0.5s default
};

const Clip *clipById(const Track &track, const QString &clipId);
bool clipsEligibleForTransition(const Clip &fromClip, const Clip &toClip);
bool clipsPhysicallyOverlap(const Clip &fromClip, const Clip &toClip);
TimeUs physicalOverlapDurationUs(const Clip &fromClip, const Clip &toClip);
TimeUs transitionCenterUs(const Track &track, const Transition &transition);
bool transitionWindow(const Track &track, const Transition &transition, TimeUs &startUs, TimeUs &endUs);
double transitionProgress(TimeUs timelineUs, TimeUs windowStartUs, TimeUs windowEndUs);
const Transition *activeTransitionAt(const Track &track, TimeUs timelineUs, TimeUs &windowStartUs,
                                     TimeUs &windowEndUs);

} // namespace drift
