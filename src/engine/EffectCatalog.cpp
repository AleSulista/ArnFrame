#include "EffectCatalog.h"

#include <QSet>

namespace {

const QString kCatGlitch = QStringLiteral("glitch");
const QString kCatRetro = QStringLiteral("retro");
const QString kCatDreamy = QStringLiteral("dreamy");
const QString kCatImpact = QStringLiteral("impact");

drift::EffectParamSpec param(const QString &key, const QString &label, double min, double max, double def)
{
    return {.key = key, .label = label, .min = min, .max = max, .defaultValue = def};
}

drift::EffectParamSpec boolParam(const QString &key, const QString &label, bool def = false)
{
    return {.key = key, .label = label, .min = 0.0, .max = 1.0, .defaultValue = def ? 1.0 : 0.0, .isBoolean = true};
}

const QList<EffectPresetEntry> kCatalog = {
    // --- Impact (color / punch) ---------------------------------------------
    {
        .meta =
            {
                .id = QStringLiteral("adjust.contrast"),
                .displayName = QStringLiteral("Contrast"),
                .category = kCatImpact,
                .parameters = {param(QStringLiteral("contrast"), QStringLiteral("Amount"), 0.0, 3.0, 1.0)},
            },
        .filterName = QStringLiteral("eq"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("adjust.brightness"),
                .displayName = QStringLiteral("Brightness"),
                .category = kCatImpact,
                .parameters =
                    {param(QStringLiteral("brightness"), QStringLiteral("Amount"), -1.0, 1.0, 0.0)},
            },
        .filterName = QStringLiteral("eq"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("adjust.saturation"),
                .displayName = QStringLiteral("Saturation"),
                .category = kCatImpact,
                .parameters =
                    {param(QStringLiteral("saturation"), QStringLiteral("Amount"), 0.0, 3.0, 1.0)},
            },
        .filterName = QStringLiteral("eq"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("adjust.gamma"),
                .displayName = QStringLiteral("Gamma"),
                .category = kCatImpact,
                .parameters = {param(QStringLiteral("gamma"), QStringLiteral("Amount"), 0.1, 4.0, 1.0)},
            },
        .filterName = QStringLiteral("eq"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("adjust.hue"),
                .displayName = QStringLiteral("Hue"),
                .category = kCatImpact,
                .parameters =
                    {param(QStringLiteral("h"), QStringLiteral("Degrees"), -180.0, 180.0, 0.0)},
            },
        .filterName = QStringLiteral("hue"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("adjust.temperature"),
                .displayName = QStringLiteral("Temperature"),
                .category = kCatImpact,
                .parameters = {param(QStringLiteral("temperature"), QStringLiteral("Kelvin"), 3000.0,
                                   7000.0, 6500.0)},
            },
        .filterName = QStringLiteral("colortemperature"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("stylize.sharpen"),
                .displayName = QStringLiteral("Sharpen"),
                .category = kCatImpact,
                .parameters =
                    {param(QStringLiteral("luma_amount"), QStringLiteral("Amount"), 0.0, 3.0, 1.0)},
            },
        .filterName = QStringLiteral("unsharp"),
    },

    // --- Dreamy & Stylish ---------------------------------------------------
    {
        .meta =
            {
                .id = QStringLiteral("stylize.blur"),
                .displayName = QStringLiteral("Blur"),
                .category = kCatDreamy,
                .parameters = {param(QStringLiteral("sigma"), QStringLiteral("Amount"), 0.0, 20.0, 5.0)},
            },
        .filterName = QStringLiteral("gblur"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("stylize.vignette"),
                .displayName = QStringLiteral("Vignette"),
                .category = kCatDreamy,
            },
        .filterName = QStringLiteral("vignette"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("stylize.bloom"),
                .displayName = QStringLiteral("Bloom"),
                .category = kCatDreamy,
                .parameters =
                    {
                        param(QStringLiteral("intensity"), QStringLiteral("Intensity"), 0.0, 2.0, 0.75),
                        param(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 30.0, 12.0),
                    },
                .compositorOnly = true,
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("bloom_glow"),
                .displayName = QStringLiteral("Bloom / Glow"),
                .category = kCatDreamy,
                .parameters =
                    {
                        param(QStringLiteral("threshold"), QStringLiteral("Threshold"), 0.0, 1.0, 0.75),
                        param(QStringLiteral("intensity"), QStringLiteral("Intensity"), 0.0, 2.0, 0.6),
                        param(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 30.0, 8.0),
                    },
                .compositorOnly = true,
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("edge_neon"),
                .displayName = QStringLiteral("Edge Glow / Neon"),
                .category = kCatDreamy,
                .parameters =
                    {
                        param(QStringLiteral("threshold"), QStringLiteral("Threshold"), 0.0, 1.0, 0.25),
                        param(QStringLiteral("intensity"), QStringLiteral("Intensity"), 0.0, 2.0, 0.8),
                        param(QStringLiteral("radius"), QStringLiteral("Radius"), 1.0, 20.0, 4.0),
                    },
                .compositorOnly = true,
            },
        .fixedParams = {{QStringLiteral("color"), QStringLiteral("#00ffff")}},
    },
    {
        .meta =
            {
                .id = QStringLiteral("time_echo"),
                .displayName = QStringLiteral("Time Echo / Trail"),
                .category = kCatDreamy,
                .parameters =
                    {
                        param(QStringLiteral("frames"), QStringLiteral("Frames"), 1.0, 10.0, 4.0),
                        param(QStringLiteral("decay"), QStringLiteral("Decay"), 0.0, 1.0, 0.55),
                    },
                .compositorOnly = true,
            },
        .fixedParams = {{QStringLiteral("blendMode"), QStringLiteral("normal")}},
    },

    // --- Retro / Analog -----------------------------------------------------
    {
        .meta =
            {
                .id = QStringLiteral("stylize.grayscale"),
                .displayName = QStringLiteral("Grayscale"),
                .category = kCatRetro,
            },
        .filterName = QStringLiteral("hue"),
        .fixedParams = {{QStringLiteral("s"), 0.0}},
    },
    {
        .meta =
            {
                .id = QStringLiteral("stylize.sepia"),
                .displayName = QStringLiteral("Sepia"),
                .category = kCatRetro,
            },
        .filterName = QStringLiteral("colorchannelmixer"),
        .fixedParams =
            {
                {QStringLiteral("rr"), 0.393}, {QStringLiteral("rg"), 0.769}, {QStringLiteral("rb"), 0.189},
                {QStringLiteral("gr"), 0.349}, {QStringLiteral("gg"), 0.686}, {QStringLiteral("gb"), 0.168},
                {QStringLiteral("br"), 0.272}, {QStringLiteral("bg"), 0.534}, {QStringLiteral("bb"), 0.131},
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("stylize.vhs"),
                .displayName = QStringLiteral("VHS"),
                .category = kCatRetro,
                .parameters =
                    {
                        param(QStringLiteral("noise"), QStringLiteral("Noise"), 0.0, 80.0, 28.0),
                        param(QStringLiteral("chroma"), QStringLiteral("Chroma shift"), 0.0, 12.0, 3.0),
                        param(QStringLiteral("saturation"), QStringLiteral("Saturation"), 0.2, 1.2, 0.85),
                    },
            },
        .graphTemplate = QStringLiteral(
            "noise=alls={{noise}}:allf=t+u,eq=contrast=1.08:gamma=1.12,hue=s={{saturation}},"
            "rgbashift=rh={{chroma}}:bh={{chroma_neg}}"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("vhs_crt"),
                .displayName = QStringLiteral("VHS / CRT"),
                .category = kCatRetro,
                .parameters =
                    {
                        param(QStringLiteral("scanlines"), QStringLiteral("Scanlines"), 0.0, 1.0, 0.35),
                        param(QStringLiteral("noise"), QStringLiteral("Noise"), 0.0, 1.0, 0.25),
                        param(QStringLiteral("colorBleed"), QStringLiteral("Color bleed"), 0.0, 20.0, 3.0),
                        param(QStringLiteral("distortion"), QStringLiteral("Distortion"), 0.0, 1.0, 0.2),
                        param(QStringLiteral("vignette"), QStringLiteral("Vignette"), 0.0, 1.0, 0.25),
                        param(QStringLiteral("desaturation"), QStringLiteral("Desaturation"), 0.0, 1.0, 0.15),
                    },
                .compositorOnly = true,
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("film_burn"),
                .displayName = QStringLiteral("Film Burn / Light Leak"),
                .category = kCatRetro,
                .parameters =
                    {
                        param(QStringLiteral("intensity"), QStringLiteral("Intensity"), 0.0, 1.0, 0.45),
                        param(QStringLiteral("warmth"), QStringLiteral("Warmth"), 0.0, 1.0, 0.8),
                        param(QStringLiteral("flicker"), QStringLiteral("Flicker"), 0.0, 1.0, 0.35),
                        param(QStringLiteral("seed"), QStringLiteral("Seed"), 0.0, 999999.0, 1.0),
                    },
                .compositorOnly = true,
            },
        .fixedParams = {{QStringLiteral("position"), QStringLiteral("left")}},
    },

    // --- Glitch & Distortion ------------------------------------------------
    {
        .meta =
            {
                .id = QStringLiteral("stylize.pixelate"),
                .displayName = QStringLiteral("Pixelate"),
                .category = kCatGlitch,
                .parameters =
                    {
                        param(QStringLiteral("width"), QStringLiteral("Block width"), 2.0, 64.0, 8.0),
                        param(QStringLiteral("height"), QStringLiteral("Block height"), 2.0, 64.0, 8.0),
                    },
            },
        .filterName = QStringLiteral("pixelize"),
    },
    {
        .meta =
            {
                .id = QStringLiteral("rgb_split"),
                .displayName = QStringLiteral("RGB Split"),
                .category = kCatGlitch,
                .parameters =
                    {
                        param(QStringLiteral("amount"), QStringLiteral("Amount"), 0.0, 50.0, 8.0),
                        param(QStringLiteral("angle"), QStringLiteral("Angle"), 0.0, 360.0, 0.0),
                        boolParam(QStringLiteral("animated"), QStringLiteral("Animated"), false),
                        param(QStringLiteral("speed"), QStringLiteral("Speed"), 0.0, 10.0, 1.0),
                    },
                .compositorOnly = true,
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("block_glitch"),
                .displayName = QStringLiteral("Block Glitch"),
                .category = kCatGlitch,
                .parameters =
                    {
                        param(QStringLiteral("intensity"), QStringLiteral("Intensity"), 0.0, 1.0, 0.35),
                        param(QStringLiteral("blockSize"), QStringLiteral("Block size"), 4.0, 128.0, 32.0),
                        param(QStringLiteral("shiftAmount"), QStringLiteral("Shift"), 0.0, 100.0, 24.0),
                        param(QStringLiteral("frequency"), QStringLiteral("Frequency"), 0.0, 1.0, 0.25),
                        param(QStringLiteral("seed"), QStringLiteral("Seed"), 0.0, 999999.0, 1.0),
                    },
                .compositorOnly = true,
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("scanline_glitch"),
                .displayName = QStringLiteral("Scanline Glitch"),
                .category = kCatGlitch,
                .parameters =
                    {
                        param(QStringLiteral("jitter"), QStringLiteral("Jitter"), 0.0, 1.0, 0.25),
                        param(QStringLiteral("lineStrength"), QStringLiteral("Scanlines"), 0.0, 1.0, 0.35),
                        param(QStringLiteral("colorShift"), QStringLiteral("Color shift"), 0.0, 20.0, 4.0),
                        param(QStringLiteral("speed"), QStringLiteral("Speed"), 0.0, 10.0, 2.0),
                    },
                .compositorOnly = true,
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("digital_glitch"),
                .displayName = QStringLiteral("Digital Glitch"),
                .category = kCatGlitch,
                .parameters =
                    {
                        param(QStringLiteral("intensity"), QStringLiteral("Intensity"), 0.0, 1.0, 0.5),
                        param(QStringLiteral("frequency"), QStringLiteral("Frequency"), 0.0, 1.0, 0.35),
                        param(QStringLiteral("rgbAmount"), QStringLiteral("RGB split"), 0.0, 40.0, 8.0),
                        param(QStringLiteral("blockAmount"), QStringLiteral("Block shift"), 0.0, 1.0, 0.4),
                        param(QStringLiteral("flashAmount"), QStringLiteral("Flash"), 0.0, 1.0, 0.15),
                        param(QStringLiteral("seed"), QStringLiteral("Seed"), 0.0, 999999.0, 1.0),
                    },
                .compositorOnly = true,
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("stylize.ripple"),
                .displayName = QStringLiteral("Ripple"),
                .category = kCatGlitch,
                .parameters =
                    {
                        param(QStringLiteral("amplitude"), QStringLiteral("Amplitude"), 0.0, 30.0, 8.0),
                        param(QStringLiteral("frequency"), QStringLiteral("Frequency"), 1.0, 20.0, 6.0),
                    },
                .compositorOnly = true,
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("ripple_water"),
                .displayName = QStringLiteral("Ripple / Water"),
                .category = kCatGlitch,
                .parameters =
                    {
                        param(QStringLiteral("amplitude"), QStringLiteral("Amplitude"), 0.0, 50.0, 8.0),
                        param(QStringLiteral("frequency"), QStringLiteral("Frequency"), 0.0, 50.0, 12.0),
                        param(QStringLiteral("speed"), QStringLiteral("Speed"), 0.0, 10.0, 1.0),
                        param(QStringLiteral("centerX"), QStringLiteral("Center X"), 0.0, 1.0, 0.5),
                        param(QStringLiteral("centerY"), QStringLiteral("Center Y"), 0.0, 1.0, 0.5),
                    },
                .compositorOnly = true,
            },
    },
    {
        .meta =
            {
                .id = QStringLiteral("shockwave_pulse"),
                .displayName = QStringLiteral("Shockwave / Pulse"),
                .category = kCatGlitch,
                .parameters =
                    {
                        param(QStringLiteral("centerX"), QStringLiteral("Center X"), 0.0, 1.0, 0.5),
                        param(QStringLiteral("centerY"), QStringLiteral("Center Y"), 0.0, 1.0, 0.5),
                        param(QStringLiteral("radius"), QStringLiteral("Radius"), 0.0, 1.0, 0.0),
                        param(QStringLiteral("width"), QStringLiteral("Width"), 0.0, 1.0, 0.08),
                        param(QStringLiteral("strength"), QStringLiteral("Strength"), 0.0, 1.0, 0.35),
                        param(QStringLiteral("speed"), QStringLiteral("Speed"), 0.0, 10.0, 1.0),
                    },
                .compositorOnly = true,
            },
    },
};

QString substituteTemplate(QString templ, const QMap<QString, QVariant> &params)
{
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        const QString placeholder = QStringLiteral("{{%1}}").arg(it.key());
        templ.replace(placeholder, it.value().toString());
    }
    return templ;
}

QString singleFilterGraph(const QString &filterName, const QMap<QString, QVariant> &params)
{
    if (filterName.isEmpty())
        return {};

    QStringList parts;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        const QString value = it.value().toString();
        if (!value.isEmpty())
            parts.append(QStringLiteral("%1=%2").arg(it.key(), value));
    }

    if (parts.isEmpty())
        return filterName;

    return QStringLiteral("%1=%2").arg(filterName, parts.join(QLatin1Char(':')));
}

} // namespace

const QList<EffectPresetEntry> &effectCatalog()
{
    return kCatalog;
}

const EffectPresetEntry *effectDefForId(const QString &id)
{
    QString resolved = id;
    if (resolved == QLatin1String("stylize.rgb_split"))
        resolved = QStringLiteral("rgb_split");

    for (const EffectPresetEntry &def : kCatalog) {
        if (def.meta.id == resolved)
            return &def;
    }
    return nullptr;
}

QStringList effectPresetIds()
{
    QStringList ids;
    ids.reserve(kCatalog.size());
    for (const EffectPresetEntry &def : kCatalog)
        ids.append(def.meta.id);
    return ids;
}

QList<QPair<QString, QString>> effectCategories()
{
    return {
        {kCatGlitch, QStringLiteral("Glitch & Distortion")},
        {kCatRetro, QStringLiteral("Retro / Analog")},
        {kCatDreamy, QStringLiteral("Dreamy & Stylish")},
        {kCatImpact, QStringLiteral("Impact")},
    };
}

QString effectCategoryLabel(const QString &categoryId)
{
    for (const auto &category : effectCategories()) {
        if (category.first == categoryId)
            return category.second;
    }
    return {};
}

QMap<QString, QVariant> resolvedEffectParameters(const drift::Effect &effect, const EffectPresetEntry &def)
{
    QMap<QString, QVariant> params = def.fixedParams;
    for (const drift::EffectParamSpec &spec : def.meta.parameters) {
        if (spec.isBoolean)
            params.insert(spec.key, spec.defaultValue > 0.5);
        else
            params.insert(spec.key, spec.defaultValue);
    }
    for (auto it = effect.parameters.constBegin(); it != effect.parameters.end(); ++it)
        params.insert(it.key(), it.value());

    // Derived placeholders used by graph templates.
    if (params.contains(QStringLiteral("offset"))) {
        const double offset = params.value(QStringLiteral("offset")).toDouble();
        params.insert(QStringLiteral("offset_neg"), -offset);
    }
    if (params.contains(QStringLiteral("chroma"))) {
        const double chroma = params.value(QStringLiteral("chroma")).toDouble();
        params.insert(QStringLiteral("chroma_neg"), -chroma);
    }

    return params;
}

QString buildFilterGraphForEffect(const drift::Effect &effect, const EffectPresetEntry *def)
{
    const EffectPresetEntry *entry = def;
    if (!entry && !effect.catalogId.isEmpty())
        entry = effectDefForId(effect.catalogId);

    if (entry) {
        if (entry->meta.compositorOnly)
            return {};

        const QMap<QString, QVariant> params = resolvedEffectParameters(effect, *entry);
        if (!entry->graphTemplate.isEmpty())
            return substituteTemplate(entry->graphTemplate, params);

        const QString filterName = entry->filterName.isEmpty() ? effect.name : entry->filterName;
        return singleFilterGraph(filterName, params);
    }

    return effect.filterGraphString();
}
