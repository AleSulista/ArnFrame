#include "Transition.h"

#include "Track.h"

namespace drift {

QString transitionKindToString(TransitionKind kind)
{
    switch (kind) {
    case TransitionKind::DipToBlack:
        return QStringLiteral("dip");
    case TransitionKind::DipToWhite:
        return QStringLiteral("dip_white");
    case TransitionKind::WipeLeft:
        return QStringLiteral("wipe_left");
    case TransitionKind::WipeRight:
        return QStringLiteral("wipe_right");
    case TransitionKind::WipeUp:
        return QStringLiteral("wipe_up");
    case TransitionKind::WipeDown:
        return QStringLiteral("wipe_down");
    case TransitionKind::PushLeft:
        return QStringLiteral("push_left");
    case TransitionKind::ZoomIn:
        return QStringLiteral("zoom_in");
    case TransitionKind::Crossfade:
        break;
    }
    return QStringLiteral("crossfade");
}

QString transitionKindLabel(TransitionKind kind)
{
    switch (kind) {
    case TransitionKind::DipToBlack:
        return QStringLiteral("Dip to black");
    case TransitionKind::DipToWhite:
        return QStringLiteral("Dip to white");
    case TransitionKind::WipeLeft:
        return QStringLiteral("Wipe left");
    case TransitionKind::WipeRight:
        return QStringLiteral("Wipe right");
    case TransitionKind::WipeUp:
        return QStringLiteral("Wipe up");
    case TransitionKind::WipeDown:
        return QStringLiteral("Wipe down");
    case TransitionKind::PushLeft:
        return QStringLiteral("Push left");
    case TransitionKind::ZoomIn:
        return QStringLiteral("Zoom in");
    case TransitionKind::Crossfade:
        return QStringLiteral("Crossfade");
    }
    return QStringLiteral("Crossfade");
}

TransitionKind transitionKindFromString(const QString &kind)
{
    if (kind == QStringLiteral("dip"))
        return TransitionKind::DipToBlack;
    if (kind == QStringLiteral("dip_white"))
        return TransitionKind::DipToWhite;
    if (kind == QStringLiteral("wipe_left"))
        return TransitionKind::WipeLeft;
    if (kind == QStringLiteral("wipe_right"))
        return TransitionKind::WipeRight;
    if (kind == QStringLiteral("wipe_up"))
        return TransitionKind::WipeUp;
    if (kind == QStringLiteral("wipe_down"))
        return TransitionKind::WipeDown;
    if (kind == QStringLiteral("push_left"))
        return TransitionKind::PushLeft;
    if (kind == QStringLiteral("zoom_in"))
        return TransitionKind::ZoomIn;
    return TransitionKind::Crossfade;
}

TransitionBlendOpacities transitionBlendOpacities(TransitionKind kind, double progress)
{
    const double p = qBound(0.0, progress, 1.0);
    TransitionBlendOpacities blend;

    switch (kind) {
    case TransitionKind::Crossfade:
        blend.outgoing = 1.0 - p;
        blend.incoming = p;
        break;
    case TransitionKind::DipToBlack:
    case TransitionKind::DipToWhite:
        blend.outgoing = p < 0.5 ? 1.0 - p * 2.0 : 0.0;
        blend.incoming = p >= 0.5 ? (p - 0.5) * 2.0 : 0.0;
        if (kind == TransitionKind::DipToBlack)
            blend.blackOverlay = p < 0.5 ? p * 2.0 : (1.0 - p) * 2.0;
        else
            blend.whiteOverlay = p < 0.5 ? p * 2.0 : (1.0 - p) * 2.0;
        break;
    case TransitionKind::WipeLeft:
    case TransitionKind::WipeRight:
    case TransitionKind::WipeUp:
    case TransitionKind::WipeDown:
        blend.outgoing = 1.0;
        blend.incoming = 1.0;
        break;
    case TransitionKind::PushLeft:
        blend.outgoing = 1.0 - p;
        blend.incoming = p;
        break;
    case TransitionKind::ZoomIn:
        blend.outgoing = 1.0 - p;
        blend.incoming = p;
        break;
    }
    return blend;
}

bool transitionUsesCrossfadeAudio(TransitionKind kind)
{
    switch (kind) {
    case TransitionKind::DipToBlack:
    case TransitionKind::DipToWhite:
        return false;
    case TransitionKind::Crossfade:
    case TransitionKind::WipeLeft:
    case TransitionKind::WipeRight:
    case TransitionKind::WipeUp:
    case TransitionKind::WipeDown:
    case TransitionKind::PushLeft:
    case TransitionKind::ZoomIn:
        return true;
    }
    return true;
}

const Clip *clipById(const Track &track, const QString &clipId)
{
    for (const Clip &clip : track.clips) {
        if (clip.id == clipId)
            return &clip;
    }
    return nullptr;
}

TimeUs transitionCenterUs(const Track &track, const Transition &transition)
{
    const Clip *fromClip = clipById(track, transition.fromClipId);
    if (!fromClip)
        return 0;
    return fromClip->timelineEnd();
}

bool transitionWindow(const Track &track, const Transition &transition, TimeUs &startUs, TimeUs &endUs)
{
    if (transition.durationUs <= 0)
        return false;

    const Clip *fromClip = clipById(track, transition.fromClipId);
    const Clip *toClip = clipById(track, transition.toClipId);
    if (!fromClip || !toClip)
        return false;

    const TimeUs center = fromClip->timelineEnd();
    const TimeUs half = transition.durationUs / 2;
    startUs = center - half;
    endUs = center + half;
    return true;
}

double transitionProgress(TimeUs timelineUs, TimeUs windowStartUs, TimeUs windowEndUs)
{
    if (windowEndUs <= windowStartUs)
        return 0.0;
    const double p = static_cast<double>(timelineUs - windowStartUs)
                     / static_cast<double>(windowEndUs - windowStartUs);
    return qBound(0.0, p, 1.0);
}

const Transition *activeTransitionAt(const Track &track, TimeUs timelineUs, TimeUs &windowStartUs,
                                     TimeUs &windowEndUs)
{
    for (const Transition &transition : track.transitions) {
        TimeUs startUs = 0;
        TimeUs endUs = 0;
        if (!transitionWindow(track, transition, startUs, endUs))
            continue;
        if (timelineUs >= startUs && timelineUs < endUs) {
            windowStartUs = startUs;
            windowEndUs = endUs;
            return &transition;
        }
    }
    return nullptr;
}

} // namespace drift
