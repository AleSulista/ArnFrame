#include "EffectCatalog.h"

namespace {

const QList<EffectDef> kCatalog = {
    // --- Color / adjustment -------------------------------------------------
    {
        .id = QStringLiteral("adjust.contrast"),
        .label = QStringLiteral("Contrast"),
        .category = QStringLiteral("adjustment"),
        .filterName = QStringLiteral("eq"),
        .params = {{.key = QStringLiteral("contrast"), .label = QStringLiteral("Amount"), .min = 0.0, .max = 3.0, .def = 1.0}},
    },
    {
        .id = QStringLiteral("adjust.brightness"),
        .label = QStringLiteral("Brightness"),
        .category = QStringLiteral("adjustment"),
        .filterName = QStringLiteral("eq"),
        .params = {{.key = QStringLiteral("brightness"), .label = QStringLiteral("Amount"), .min = -1.0, .max = 1.0, .def = 0.0}},
    },
    {
        .id = QStringLiteral("adjust.saturation"),
        .label = QStringLiteral("Saturation"),
        .category = QStringLiteral("adjustment"),
        .filterName = QStringLiteral("eq"),
        .params = {{.key = QStringLiteral("saturation"), .label = QStringLiteral("Amount"), .min = 0.0, .max = 3.0, .def = 1.0}},
    },
    {
        .id = QStringLiteral("adjust.gamma"),
        .label = QStringLiteral("Gamma"),
        .category = QStringLiteral("adjustment"),
        .filterName = QStringLiteral("eq"),
        .params = {{.key = QStringLiteral("gamma"), .label = QStringLiteral("Amount"), .min = 0.1, .max = 4.0, .def = 1.0}},
    },
    {
        .id = QStringLiteral("adjust.hue"),
        .label = QStringLiteral("Hue"),
        .category = QStringLiteral("adjustment"),
        .filterName = QStringLiteral("hue"),
        .params = {{.key = QStringLiteral("h"), .label = QStringLiteral("Degrees"), .min = -180.0, .max = 180.0, .def = 0.0}},
    },
    {
        .id = QStringLiteral("adjust.temperature"),
        .label = QStringLiteral("Temperature"),
        .category = QStringLiteral("adjustment"),
        .filterName = QStringLiteral("colortemperature"),
        .params = {{.key = QStringLiteral("temperature"), .label = QStringLiteral("Kelvin"), .min = 3000.0, .max = 7000.0, .def = 6500.0}},
    },

    // --- Stylize -------------------------------------------------------------
    {
        .id = QStringLiteral("stylize.blur"),
        .label = QStringLiteral("Blur"),
        .category = QStringLiteral("stylize"),
        .filterName = QStringLiteral("gblur"),
        .params = {{.key = QStringLiteral("sigma"), .label = QStringLiteral("Amount"), .min = 0.0, .max = 20.0, .def = 5.0}},
    },
    {
        .id = QStringLiteral("stylize.sharpen"),
        .label = QStringLiteral("Sharpen"),
        .category = QStringLiteral("stylize"),
        .filterName = QStringLiteral("unsharp"),
        .params = {{.key = QStringLiteral("luma_amount"), .label = QStringLiteral("Amount"), .min = 0.0, .max = 3.0, .def = 1.0}},
    },
    {
        .id = QStringLiteral("stylize.grayscale"),
        .label = QStringLiteral("Grayscale"),
        .category = QStringLiteral("stylize"),
        .filterName = QStringLiteral("hue"),
        .params = {},
        .fixedParams = {{QStringLiteral("s"), 0.0}},
    },
    {
        .id = QStringLiteral("stylize.sepia"),
        .label = QStringLiteral("Sepia"),
        .category = QStringLiteral("stylize"),
        .filterName = QStringLiteral("colorchannelmixer"),
        .params = {},
        .fixedParams = {
            {QStringLiteral("rr"), 0.393}, {QStringLiteral("rg"), 0.769}, {QStringLiteral("rb"), 0.189},
            {QStringLiteral("gr"), 0.349}, {QStringLiteral("gg"), 0.686}, {QStringLiteral("gb"), 0.168},
            {QStringLiteral("br"), 0.272}, {QStringLiteral("bg"), 0.534}, {QStringLiteral("bb"), 0.131},
        },
    },
    {
        .id = QStringLiteral("stylize.vignette"),
        .label = QStringLiteral("Vignette"),
        .category = QStringLiteral("stylize"),
        .filterName = QStringLiteral("vignette"),
        .params = {},
    },
    {
        .id = QStringLiteral("stylize.pixelate"),
        .label = QStringLiteral("Pixelate"),
        .category = QStringLiteral("stylize"),
        .filterName = QStringLiteral("pixelize"),
        .params = {
            {.key = QStringLiteral("width"), .label = QStringLiteral("Block width"), .min = 2.0, .max = 64.0, .def = 8.0},
            {.key = QStringLiteral("height"), .label = QStringLiteral("Block height"), .min = 2.0, .max = 64.0, .def = 8.0},
        },
    },
};

} // namespace

const QList<EffectDef> &effectCatalog()
{
    return kCatalog;
}

const EffectDef *effectDefForId(const QString &id)
{
    for (const EffectDef &def : kCatalog) {
        if (def.id == id)
            return &def;
    }
    return nullptr;
}
