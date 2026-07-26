#pragma once

#include "core/Time.h"

#include <QtGlobal>

// Pure helpers for CapCut/Premiere-style live preview: drop resolution when a
// frame misses its budget, and decide which completed frames are still worth
// showing. Kept free of Qt threading / GL so the policy is unit-testable.
namespace AdaptivePreviewPolicy {

constexpr double kScaleMin = 0.25;
constexpr double kScaleStepDown = 0.75;
constexpr double kScaleStepUp = 1.12;
// Drop after one over-budget frame so effects-heavy timelines react before the
// preview freezes. Recover only after sustained headroom to avoid oscillation.
constexpr int kOverBudgetBeforeScaleDown = 1;
constexpr int kUnderBudgetBeforeScaleUp = 10;
// Soft headroom: treat a frame as under budget only when it finishes with room
// to spare, so borderline frames do not bounce quality back up.
constexpr double kUnderBudgetRatio = 0.80;

struct State
{
    double scale = 1.0;
    int overBudgetStreak = 0;
    int underBudgetStreak = 0;
};

inline State noteRenderCost(State state, qint64 renderMs, qint64 budgetMs)
{
    if (budgetMs <= 0 || renderMs < 0)
        return state;

    if (renderMs > budgetMs) {
        state.underBudgetStreak = 0;
        ++state.overBudgetStreak;
        if (state.overBudgetStreak >= kOverBudgetBeforeScaleDown
            && state.scale > kScaleMin + 1e-6) {
            state.scale = qMax(kScaleMin, state.scale * kScaleStepDown);
            state.overBudgetStreak = 0;
        }
        return state;
    }

    state.overBudgetStreak = 0;
    if (renderMs <= static_cast<qint64>(budgetMs * kUnderBudgetRatio)) {
        ++state.underBudgetStreak;
        if (state.underBudgetStreak >= kUnderBudgetBeforeScaleUp
            && state.scale < 1.0 - 1e-6) {
            state.scale = qMin(1.0, state.scale * kScaleStepUp);
            state.underBudgetStreak = 0;
        }
    } else {
        state.underBudgetStreak = 0;
    }
    return state;
}

// Present any completed forward frame from the current edit/seek generation.
// Reject reverse seeks and frames from an obsolete project snapshot so a late
// worker result cannot flash old content after an edit. After a seek, frames
// older than minPresentableTimeUs are also dropped so a late pre-seek result
// cannot pin the cursor ahead of the new playhead.
inline bool shouldPresentFrame(drift::TimeUs frameTimeUs, int frameGeneration,
                               drift::TimeUs lastPresentedTimeUs, int liveGeneration,
                               drift::TimeUs minPresentableTimeUs = 0)
{
    if (frameGeneration != liveGeneration)
        return false;
    if (frameTimeUs < minPresentableTimeUs)
        return false;
    if (lastPresentedTimeUs < 0)
        return true;
    return frameTimeUs >= lastPresentedTimeUs;
}

} // namespace AdaptivePreviewPolicy
