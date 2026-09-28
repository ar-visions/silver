// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#if USE(TRINITY)

#include "FilterEffectApplier.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class FEColorMatrix;
class FEComponentTransfer;
class FEDropShadow;
class FEGaussianBlur;
class SourceGraphic;

// CSS filters on the gpu: each step reads and writes trinity canvases
#define TRINITY_FILTER_APPLIER(Name, Effect) \
    class Name final : public FilterEffectConcreteApplier<Effect> { \
        WTF_MAKE_TZONE_ALLOCATED(Name); \
        using Base = FilterEffectConcreteApplier<Effect>; \
    public: \
        using Base::Base; \
    private: \
        bool apply(const Filter&, std::span<const Ref<FilterImage>>, FilterImage&) const final; \
    };

TRINITY_FILTER_APPLIER(SourceGraphicTrinityApplier, SourceGraphic)
TRINITY_FILTER_APPLIER(FEGaussianBlurTrinityApplier, FEGaussianBlur)
TRINITY_FILTER_APPLIER(FEDropShadowTrinityApplier, FEDropShadow)
TRINITY_FILTER_APPLIER(FEColorMatrixTrinityApplier, FEColorMatrix)
TRINITY_FILTER_APPLIER(FEComponentTransferTrinityApplier, FEComponentTransfer)

#undef TRINITY_FILTER_APPLIER

} // namespace WebCore

#endif // USE(TRINITY)
