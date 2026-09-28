// SPDX-License-Identifier: BSD-2-Clause

#include "config.h"
#include "FilterTrinityAppliers.h"

#if USE(TRINITY)

#include "ColorConversion.h"
#include "FEColorMatrix.h"
#include "FEComponentTransfer.h"
#include "FEDropShadow.h"
#include "FEGaussianBlur.h"
#include "Filter.h"
#include "FilterImage.h"
#include "GraphicsContextTrinity.h"
#include "ImageBuffer.h"
#include "SourceGraphic.h"
#include "WebGfx.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SourceGraphicTrinityApplier);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FEGaussianBlurTrinityApplier);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FEDropShadowTrinityApplier);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FEColorMatrixTrinityApplier);
WTF_MAKE_TZONE_ALLOCATED_IMPL(FEComponentTransferTrinityApplier);

// the trinity canvas behind an image buffer; 0 for any other kind
static int canvasOf(ImageBuffer& buffer)
{
    auto* context = static_cast<GraphicsContextTrinity*>(buffer.context().platformContext());
    return context ? context->canvas() : 0;
}

// the input's and result's canvases, and where the input sits
struct FilterCanvases {
    int source { 0 };
    int result { 0 };
    FloatPoint at;
    IntSize size;
};

static std::optional<FilterCanvases> canvasesFor(const FilterImage& input, FilterImage& result)
{
    RefPtr resultImage = result.imageBuffer();
    RefPtr sourceImage = const_cast<FilterImage&>(input).imageBuffer();
    if (!resultImage || !sourceImage)
        return std::nullopt;
    FilterCanvases canvases { canvasOf(*sourceImage), canvasOf(*resultImage), input.absoluteImageRectRelativeTo(result).location(), sourceImage->backendSize() };
    if (!canvases.source || !canvases.result)
        return std::nullopt;
    return canvases;
}

// source drawn into result as it is, in pixels
static void copyInto(const FilterCanvases& canvases)
{
    float const identity[6] = { 1, 0, 0, 1, 0, 0 };
    webgfx_canvas_save(canvases.result);
    webgfx_canvas_transform(canvases.result, identity);
    webgfx_canvas_compose(canvases.result, canvases.source, 0, false, 1, canvases.at.x(), canvases.at.y(), canvases.size.width(), canvases.size.height());
    webgfx_canvas_restore(canvases.result);
}

bool SourceGraphicTrinityApplier::apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    auto canvases = canvasesFor(inputs[0].get(), result);
    if (!canvases)
        return false;
    copyInto(*canvases);
    return true;
}

bool FEGaussianBlurTrinityApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    auto canvases = canvasesFor(inputs[0].get(), result);
    if (!canvases)
        return false;
    FloatSize sigma = FloatSize(m_effect->stdDeviationX(), m_effect->stdDeviationY()) * filter.filterScale();
    webgfx_canvas_filter_blur(canvases->result, canvases->source, canvases->at.x(), canvases->at.y(), sigma.width(), sigma.height(), nullptr);
    return true;
}

bool FEDropShadowTrinityApplier::apply(const Filter& filter, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    auto canvases = canvasesFor(inputs[0].get(), result);
    if (!canvases)
        return false;
    auto offset = filter.scaledByFilterScale(filter.resolvedSize({ m_effect->dx(), m_effect->dy() }));
    auto sigma = filter.scaledByFilterScale(filter.resolvedSize({ m_effect->stdDeviationX(), m_effect->stdDeviationY() }));
    auto color = m_effect->shadowColor().colorWithAlphaMultipliedBy(m_effect->shadowOpacity()).toColorTypeLossy<SRGBA<float>>().resolved();
    float const tint[4] = { color.red, color.green, color.blue, color.alpha };
    webgfx_canvas_filter_blur(canvases->result, canvases->source, canvases->at.x() + offset.width(), canvases->at.y() + offset.height(), sigma.width(), sigma.height(), tint);
    copyInto(*canvases);
    return true;
}

bool FEColorMatrixTrinityApplier::apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    auto canvases = canvasesFor(inputs[0].get(), result);
    if (!canvases)
        return false;
    auto values = FEColorMatrix::normalizedFloats(m_effect->values());
    std::array<float, 9> c;
    std::array<float, 20> matrix { };
    switch (m_effect->type()) {
    case ColorMatrixType::FECOLORMATRIX_TYPE_MATRIX:
        if (values.size() != 20)
            return false;
        std::copy(values.begin(), values.end(), matrix.begin());
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE:
    case ColorMatrixType::FECOLORMATRIX_TYPE_HUEROTATE:
        if (m_effect->type() == ColorMatrixType::FECOLORMATRIX_TYPE_SATURATE)
            FEColorMatrix::calculateSaturateComponents(c, values[0]);
        else
            FEColorMatrix::calculateHueRotateComponents(c, values[0]);
        matrix = { c[0], c[1], c[2], 0, 0, c[3], c[4], c[5], 0, 0, c[6], c[7], c[8], 0, 0, 0, 0, 0, 1, 0 };
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_LUMINANCETOALPHA:
        matrix = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0.2125, 0.7154, 0.0721, 0, 0 };
        break;
    case ColorMatrixType::FECOLORMATRIX_TYPE_UNKNOWN:
        return false;
    }
    webgfx_canvas_filter_matrix(canvases->result, canvases->source, canvases->at.x(), canvases->at.y(), matrix.data());
    return true;
}

bool FEComponentTransferTrinityApplier::apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    auto canvases = canvasesFor(inputs[0].get(), result);
    if (!canvases)
        return false;
    std::array<FEComponentTransfer::LookupTable, 4> tables {
        FEComponentTransfer::computeLookupTable(m_effect->redFunction()),
        FEComponentTransfer::computeLookupTable(m_effect->greenFunction()),
        FEComponentTransfer::computeLookupTable(m_effect->blueFunction()),
        FEComponentTransfer::computeLookupTable(m_effect->alphaFunction()),
    };
    std::array<uint8_t, 1024> rgba;
    for (size_t i = 0; i < 256; ++i) {
        for (size_t channel = 0; channel < 4; ++channel)
            rgba[i * 4 + channel] = tables[channel][i];
    }
    webgfx_canvas_filter_lut(canvases->result, canvases->source, canvases->at.x(), canvases->at.y(), rgba.data());
    return true;
}

} // namespace WebCore

#endif // USE(TRINITY)
