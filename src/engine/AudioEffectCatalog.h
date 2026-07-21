#pragma once

#include "core/Effect.h"
#include "core/EffectPreset.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// File-based audio effect packages: audio-effects/<name>/audio-effect.json.
//
// An audio effect is a libavfilter chain applied to a clip's audio in the mixer. Unlike the GPU
// effect catalog there is no built-in baseline — the whole catalog comes from packages, shipped
// as the "audio-effects" addon kind (src/models/AddonManager) or found under a local override dir.

// One parsed manifest. `chainTemplate` carries {sampleRate} and {<paramIdentifier>} placeholders
// that AudioEffectChain resolves per instance; `prerollMs` is the lookback a correct block needs
// from an arbitrary timeline position (0 for stateless filters, larger for echo/reverb tails).
struct AudioEffectEntry
{
    QString id;
    QString displayName;
    QString category; // stable slug: "voice", "transmission", "texture", "space"
    int order = 0;
    QString chainTemplate;
    int prerollMs = 0;
    QList<drift::EffectParamSpec> parameters;
    QString packageDir; // where it was loaded from; traces the entry back to its addon
};

const QList<AudioEffectEntry> &audioEffectCatalog();
const AudioEffectEntry *audioEffectDefForId(const QString &id);

// Merge each parameter's current instance value (or its default) into a name->value map.
QMap<QString, QVariant> resolvedAudioEffectParameters(const drift::Effect &effect,
                                                      const AudioEffectEntry &def);

QStringList audioEffectPresetIds();

// Browser categories in display order (stable id + user-facing label), derived from the catalog.
QList<QPair<QString, QString>> audioEffectCategories();
QString audioEffectCategoryLabel(const QString &categoryId);

// Reload from the given roots, or defaultAudioEffectSearchPaths() when empty.
void reloadAudioEffectCatalog(const QStringList &packageRoots = {});

// Default roots: DRIFT_AUDIO_EFFECTS_DIR, installed "audio-effects" addons, <appDir>/audio-effects
// and <AppDataLocation>/audio-effects — same precedence GPU effects use.
QStringList defaultAudioEffectSearchPaths();
