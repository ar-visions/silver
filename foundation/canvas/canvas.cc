#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"
#pragma clang diagnostic ignored "-Wnullability-completeness" 
#include <cstddef>
#include <core/SkImage.h>
#define  SK_VULKAN
#define  SK_USE_VMA

//#include <assert.h>

#undef assert
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 1
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>


#include <gpu/vk/GrVkBackendContext.h>
#include <gpu/ganesh/vk/GrVkBackendSurface.h>
#include <gpu/ganesh/SkImageGanesh.h>
#include <gpu/GrBackendSurface.h>
#include <gpu/GrDirectContext.h>
#include <gpu/vk/VulkanExtensions.h>
#include <core/SkPath.h>
#include <core/SkFont.h>
#include <core/SkRRect.h>
#include <core/SkBitmap.h>
#include <core/SkCanvas.h>
#include <core/SkColorSpace.h>
#include <core/SkSurface.h>
#include <core/SkFontMgr.h>
#include <core/SkFontMetrics.h>
#include <core/SkPathMeasure.h>
#include <core/SkPathUtils.h>
#include <utils/SkParsePath.h>
#include <core/SkTextBlob.h>
#include <effects/SkGradientShader.h>
#include <effects/SkImageFilters.h>
#include <effects/SkDashPathEffect.h>
#include <core/SkStream.h>
#include <modules/svg/include/SkSVGDOM.h>
#include <modules/svg/include/SkSVGNode.h>
#include <core/SkAlphaType.h>
#include <core/SkBlurTypes.h>
#include <core/SkColor.h>
#include <core/SkMaskFilter.h>
#include <core/SkColorType.h>
#include <core/SkImageInfo.h>
#include <core/SkRefCnt.h>
#include <core/SkTypes.h>
#include <core/SkTypeface.h>
#include <gpu/GrDirectContext.h>
#include <gpu/ganesh/vk/GrVkDirectContext.h>
#include <gpu/ganesh/SkSurfaceGanesh.h>
#include <gpu/vk/GrVkBackendContext.h>
#include <gpu/vk/VulkanExtensions.h>
#include <ports/SkFontMgr_fontconfig.h>
#include <gpu/vk/VulkanAMDMemoryAllocator.h>
#include <gpu/vk/VulkanMemoryAllocator.h>
#include <gpu/vk/VulkanInterface.h>
#include <gpu/vk/VulkanMemory.h>
#include <gpu/vk/VulkanUtilsPriv.h>


#include <gpu/graphite/Surface_Graphite.h>
#include <gpu/graphite/vk/VulkanGraphiteTypes.h>
#include <gpu/graphite/vk/VulkanGraphiteUtils.h>
#include <gpu/vk/VulkanBackendContext.h>
#include <gpu/graphite/Recorder.h>
#include <gpu/graphite/Context.h>
#include <private/gpu/graphite/ContextOptionsPriv.h>


extern "C" {
#include <import>
}

#undef render
#undef get
#undef clear
#undef fill
#undef move
#undef submit

//#include <skia/tools/gpu/vk/VkTestUtils.h>

struct Skia {
#if 0
    std::unique_ptr<skgpu::graphite::Context>  ctx;
    std::unique_ptr<skgpu::graphite::Recorder> rec;
    sk_sp<SkSurface> surface; // optional: reuse
#else
    sk_sp<GrDirectContext> ctx;
#endif
};

extern "C" {

static map font_resources;
static int font_resources_refs;

none sk_font_dealloc(font f) {
    if (--font_resources_refs == -2) {
        pairs(font_resources, i) {
            SkFont* f = (SkFont*)i->value;
            delete f; // this should release SkTypeface, an sk_sp used wit SkFont?
        }

        drop((object)font_resources);
    }
}

font sk_font_init(font f) {
    font_resources_refs++;

    if (!font_resources) {
         font_resources = hold(map());
         verify(font_resources_refs == 1, "font resources out of sync");
    }
    f->res = map_get(font_resources, (object)f->uri);
    if (!f->res) {
        static sk_sp<SkFontMgr> fm;
        if (!fm)
             fm = SkFontMgr_New_FontConfig(nullptr);
        f->tf = (handle)new sk_sp<SkTypeface>();
        *(sk_sp<SkTypeface>*)f->tf = fm->makeFromFile(f->uri->chars);
        //auto t = SkTypeface::MakeFromFile(f->uri->chars);
        f->res = new SkFont(*(sk_sp<SkTypeface>*)f->tf);
        set(font_resources, (object)f->uri, (object)f->res);
    }
    return f;
}

/// this globally adapts font for sk case; useful for dependency management
none sk_font_initialize() {
    set_font_manager((hook)sk_font_init, (hook)sk_font_dealloc);
}

static handle_t vk_prev;
static skia_t   sk_current;
static trinity  t;


#if 0
skia_t skia_init_vk(
        trinity _t, handle_t vk_instance, handle_t /*vma_allocator*/, handle_t phys,
        handle_t device, handle_t queue, unsigned int graphics_family, unsigned int /*vk_version*/) {
    if (sk_current) {
        verify(vk_instance == vk_prev, "skia_init_vk: more than one vulkan instance");
        return sk_current;
    }
    t = _t;
    vk_prev = vk_instance;

    static skgpu::VulkanBackendContext bctx = {};
    bctx.fInstance            = (VkInstance)vk_instance;
    bctx.fPhysicalDevice      = (VkPhysicalDevice)phys;
    bctx.fDevice              = (VkDevice)device;
    bctx.fQueue               = (VkQueue)queue;
    bctx.fGraphicsQueueIndex  = graphics_family;
    bctx.fProtectedContext    = skgpu::Protected::kNo;
    bctx.fGetProc             = [](const char *name, VkInstance inst, VkDevice dev) -> PFN_vkVoidFunction {
        return dev ? vkGetDeviceProcAddr(dev, name) : vkGetInstanceProcAddr(inst, name);
    };

    // Create Graphite Context / Recorder
    Skia* sk = new Skia();
    //static skgpu::graphite::ContextOptions  contextOptionsb;
    static skgpu::graphite::RecorderOptions rec_options;

    static skgpu::graphite::ContextOptions     contextOptions;
    static skgpu::graphite::ContextOptionsPriv contextOptionsPriv;
    // Needed to make ManagedGraphiteTexture::ReleaseProc (w/in CreateProtectedSkSurface) work
    contextOptionsPriv.fStoreContextRefInRecorder = true;
    contextOptions.fOptionsPriv = &contextOptionsPriv;

    auto ctx = skgpu::graphite::ContextFactory::MakeVulkan(bctx, contextOptions);
    sk->ctx = std::move(ctx);
    verify(sk->ctx, "graphite context creation failed");
    auto rec = sk->ctx->makeRecorder(rec_options);
    sk->rec = std::move(rec);
    skgpu::graphite::Recorder* rec2 = sk->rec.get();
    //delete rec2;
    sk_current = sk;
    sk_font_initialize();
    return sk;
}


#else

/// initialize skia from vulkan-resources
skia_t skia_init_vk(
        trinity _t, handle_t vk_instance, handle_t vma_allocator, handle_t phys,
        handle_t device, handle_t queue, unsigned int graphics_family, unsigned int vk_version) {
    if (sk_current) {
        verify(vk_instance == vk_prev, "skia_init_vk: more than one vulkan instance");
        return sk_current;
    }
    t = _t;
    vk_prev = vk_instance;

    //GrBackendFormat gr_conv = GrBackendFormat::MakeVk(VK_FORMAT_R8G8B8_SRGB);
    GrVkBackendContext grc {
        (VkInstance)vk_instance,
        (VkPhysicalDevice)phys,
        (VkDevice)device,
        (VkQueue)queue,
        graphics_family,
        vk_version
    };

    grc.fProtectedContext = skgpu::Protected::kNo;
    grc.fMaxAPIVersion    = vk_version;
    grc.fGetProc = [](const char *name, VkInstance inst, VkDevice dev) -> PFN_vkVoidFunction {
        return !dev ? vkGetInstanceProcAddr(inst, name) : vkGetDeviceProcAddr(dev, name);
    };

    #undef init
    static skgpu::VulkanExtensions extensions;
    extensions.init(
        grc.fGetProc,
        (VkInstance)vk_instance,
        (VkPhysicalDevice)phys,
        t->instance_exts->len, (const char *const *)t->instance_exts->origin,
        t->device_exts->len,   (const char *const *)t->device_exts->origin
    );

    grc.fVkExtensions = &extensions; // internal needs population perhaps
    
    sk_sp<skgpu::VulkanInterface> interface;
    
    interface.reset(new skgpu::VulkanInterface(
        grc.fGetProc,        // this is vkGetInstanceProcAddr
        grc.fInstance,       // your VkInstance
        grc.fDevice,         // your VkDevice
        VK_API_VERSION_1_2,  // your VkInstance version (like VK_API_VERSION_1_2)
        VK_API_VERSION_1_2,    // your VkPhysicalDevice version (same or higher)
        grc.fVkExtensions)); // parsed extension list


    int ii = skgpu::VulkanAMDMemoryAllocator::Make3(1);
    
    sk_sp<skgpu::VulkanMemoryAllocator> skiaAllocator;

    skgpu::VulkanAMDMemoryAllocator::Make2(t->allocator, &interface, (int)false, &skiaAllocator);

    grc.fMemoryAllocator = skiaAllocator;

    Skia* sk = new Skia();
    sk->ctx = GrDirectContexts::MakeVulkan(grc);
    
    assert(sk->ctx, "could not obtain GrVulkanContext");

    sk_font_initialize();
    sk_current = sk;
    return sk;
}

#endif

extern "C" { SkColor sk_color(object any); }

extern "C" { path path_with_cstr(path a, cstr cs); }

none sk_init(sk a);
none sk_dealloc(sk a);

static bool  is_realloc;
static array canvases;

none sk_resize(sk a, i32 w, i32 h) {
    a->width  = w;
    a->height = h;
    is_realloc = true;
    resize(a->tx, w, h);
    sk_dealloc(a);
    sk_init(a);
    A_hold_members((object)a);
    is_realloc = false;
}

none sk_init(sk a) {
    if (!is_realloc) {
        if (!canvases) canvases = hold(hold(array(alloc, 32)));
    }

    push(canvases, (object)a);

    trinity t = a->t;

    a->skia = (Skia*)skia_init_vk(
        t,
        t->instance,
        t->allocator,
        t->physical_device,
        t->device,
        t->queue,
        t->queue_family_index, VK_API_VERSION_1_2);

    verify(a->width > 0 && a->height > 0, "sk requires width and height");
    if (!a->tx) {
        texture tx = texture(t, a->t, width, a->width, height, a->height, 
            surface, Surface_color,
            format, Pixel_rgba8,
            linear, true, mip_levels, 1, layer_count, 1);
        a->tx = (texture)A_hold((Au)tx);
    }
     
    Skia* sk = a->skia;
#if 0
    SkImageInfo info = SkImageInfo::Make(
        a->width,
        a->height,
        kRGBA_8888_SkColorType,
        kPremul_SkAlphaType
    );
    static SkSurfaceProps props;
    sk_sp<SkSurface> surface = skgpu::graphite::Surface::MakeGraphite(
            sk->rec.get(), info, skgpu::Budgeted::kNo, skgpu::Mipmapped::kNo, &props);
    
#else
    GrVkImageInfo info       = {};
    info.fImage              = (VkImage)a->tx->vk_image;
    //info.fImageLayout        = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    info.fImageTiling        = VK_IMAGE_TILING_OPTIMAL;
    info.fFormat             = VK_FORMAT_R8G8B8A8_UNORM;
    info.fLevelCount         = 1;
    info.fCurrentQueueFamily = VK_QUEUE_FAMILY_IGNORED;

    GrBackendTexture backend_texture = GrBackendTextures::MakeVk(
        a->width, a->height, info);
    
    GrDirectContext* direct_ctx = a->skia->ctx.get();
    sk_sp<SkSurface> surface = SkSurfaces::WrapBackendTexture(
        direct_ctx, backend_texture, kTopLeft_GrSurfaceOrigin, 1,
        kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(),
        (const SkSurfaceProps *)null, (SkSurfaces::TextureReleaseProc)null);
    
    SkSafeRef(surface.get());
#endif
    a->sk_surface = (ARef)surface.get();
    a->sk_canvas  = (ARef)((SkSurface*)a->sk_surface)->getCanvas();
    ((SkCanvas*)a->sk_canvas)->clear(SK_ColorWHITE); // <--- clear to black here
    a->sk_path    = (ARef)new SkPath();
    a->state      = array(alloc, 16);
    save(a);
}

none sk_dealloc(sk a) {
    if (!is_realloc) {
        num i = index_of(canvases, (object)a);
        remove(canvases, i);
        if (len(canvases) == 0)
            drop(canvases);
    }
    
    SkSafeUnref((SkSurface*)a->sk_surface);
}

none sk_move_to(sk a, f32 x, f32 y) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->moveTo(x, y);
}

none sk_line_to(sk a, f32 x, f32 y) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->lineTo(x, y);
}

none sk_rect_to(sk a, f32 x, f32 y, f32 w, f32 h) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->x = x;
    ds->y = y;
    ds->w = w;
    ds->h = h;
    ((SkPath*)a->sk_path)->addRect(SkRect::MakeXYWH(x, y, w, h));
}

none sk_rounded_rect_to(sk a, f32 x, f32 y, f32 w, f32 h, f32 sx, f32 sy) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->x = x;
    ds->y = y;
    ds->w = w;
    ds->h = h;
    ((SkPath*)a->sk_path)->addRoundRect(SkRect::MakeXYWH(x, y, w, h), sx, sy);
}

none sk_arc_to(sk a, f32 x1, f32 y1, f32 x2, f32 y2, f32 radius) {
    SkCanvas*  sk     = (SkCanvas*)a->sk_canvas;
    draw_state ds     = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->arcTo(x1, y1, x2, y2, radius);
}

none sk_opacity(sk a, f32 o) {
    draw_state ds = (draw_state)last(a->state);
    ds->opacity = o;
}

none sk_arc(sk a, f32 center_x, f32 center_y, f32 radius, f32 start_angle, f32 end_angle) {
    SkCanvas*  sk     = (SkCanvas*)a->sk_canvas;
    draw_state ds     = (draw_state)last(a->state);
    SkRect     rect   = SkRect::MakeLTRB(
        center_x - radius, center_y - radius,
        center_x + radius, center_y + radius);
    f32 start_deg = start_angle * 180.0 / M_PI;
    f32 sweep_deg = (end_angle - start_angle) * 180.0 / M_PI;
    ((SkPath*)a->sk_path)->addArc(rect, start_deg, sweep_deg);
}

none sk_set_texture(sk a, texture tx) {
    SkCanvas*  sk     = (SkCanvas*)a->sk_canvas;
    draw_state ds     = (draw_state)last(a->state);
    ds->tx = tx;
}

static texture bs_tx;

none sk_set_bs(texture tx) {
    bs_tx = tx;
}

none sk_draw_fill_preserve(sk a) {
    SkCanvas*  sk     = (SkCanvas*)a->sk_canvas;
    draw_state ds     = (draw_state)last(a->state);
    SkPaint    paint;
    paint.setStyle(SkPaint::kFill_Style);

    if (ds->blur_radius > 0.0f)
        paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, ds->blur_radius));
    paint.setColor(ds->fill_color);
    paint.setAntiAlias(true);

    if (ds->tx) {

        //ds->tx = bs_tx;

        VkDevice device = t->device;
        int width = ds->tx->width, height = ds->tx->height;
        
        //image img = cast(image, ds->tx);
        //png(img, f(path, "test.png"));

        static int dont_start = 0;
        dont_start++;

        if (dont_start > 0) {
            GrDirectContext* direct_ctx = a->skia->ctx.get();
            texture tx = ds->tx;

            GrVkImageInfo vk_info = {};
            vk_info.fImage       = tx->vk_image;
            vk_info.fImageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vk_info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
            vk_info.fFormat      = tx->vk_format;
            vk_info.fLevelCount  = 1;

            GrBackendTexture backendTex = GrBackendTextures::MakeVk(tx->width, tx->height, vk_info);
            sk_sp<SkImage> sk_image = SkImages::BorrowTextureFrom(
                direct_ctx,
                backendTex,
                kTopLeft_GrSurfaceOrigin,
                kBGRA_8888_SkColorType,
                kUnpremul_SkAlphaType,
                SkColorSpace::MakeSRGB()
            );
            SkRect dst = SkRect::MakeXYWH(0, 0, ds->w, ds->h);
            //SkSamplingOptions sampling(SkFilterMode::kLinear, SkMipmapMode::kNone);
            SkSamplingOptions sampling(SkCubicResampler::CatmullRom());

            paint.setColor(SK_ColorBLACK);
            //sk->drawRect(dst, paint);
            sk->drawImageRect(sk_image, dst, sampling, &paint);
        }
        sk_sync();

    } else
        sk->drawPath(*(SkPath*)a->sk_path, paint);
}

none sk_blur_radius(sk a, f32 radius) {
    draw_state ds = (draw_state)last(a->state);
    ds->blur_radius = radius;
}

none sk_draw_fill(sk a) {
    sk_draw_fill_preserve(a);
    ((SkPath*)a->sk_path)->reset();
}

SkPaint::Cap sk_cap(cap cap) {
    switch (cap) {
        case cap_none:   return SkPaint::kButt_Cap;
        case cap_round:  return SkPaint::kRound_Cap;
        case cap_square: return SkPaint::kSquare_Cap;
        default:         return SkPaint::kDefault_Cap;
    }
}

SkPaint::Join sk_join(join j) {
    switch (j) {
        case join_none:  return SkPaint::kMiter_Join;
        case join_miter: return SkPaint::kMiter_Join;
        case join_round: return SkPaint::kRound_Join;
        case join_bevel: return SkPaint::kBevel_Join;
        default:         return SkPaint::kDefault_Join;
    }
}

none sk_draw_stroke_preserve(sk a) {
    SkCanvas*  sk     = (SkCanvas*)a->sk_canvas;
    draw_state ds     = (draw_state)last(a->state);
    if (ds->stroke_size <= 0) return;
    SkPaint    paint;
    paint.setStyle(SkPaint::kStroke_Style);
    if (ds->blur_radius > 0.0f)
        paint.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, 2.0f));

    paint.setStrokeCap(sk_cap(ds->stroke_cap));
    paint.setStrokeJoin(sk_join(ds->stroke_join));
    paint.setStrokeWidth(ds->stroke_size);
    paint.setColor(ds->stroke_color); // assuming this exists in your draw_state
    paint.setAntiAlias(true); 
    sk->drawPath(*(SkPath*)a->sk_path, paint);
}

none sk_draw_stroke(sk a) {
    sk_draw_stroke_preserve(a);
    ((SkPath*)a->sk_path)->reset();
}

none sk_cubic(sk a, f32 cp1_x, f32 cp1_y, f32 cp2_x, f32 cp2_y, f32 ep_x, f32 ep_y) {
    SkCanvas*  sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->cubicTo(cp1_x, cp1_y, cp2_x, cp2_y, ep_x, ep_y);
}

none sk_quadratic(sk a, f32 cp_x, f32 cp_y, f32 ep_x, f32 ep_y) {
    SkCanvas*  sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ((SkPath*)a->sk_path)->quadTo(cp_x, cp_y, ep_x, ep_y);
}

none sk_save(sk a) {
    draw_state ds;
    if (len(a->state)) {
        draw_state prev = (draw_state)last(a->state);
        font f = prev->font;
        A header = A_header((object)f);
        ds = (draw_state)copy(prev);

        font_resources_refs++; // easier doing this than making drop/hold a set of poly functions
    } else {
        ds = draw_state();
        set_default(ds);
    }
    push(a->state, (object)ds);
    #undef save
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    sk->save();
}

none sk_set_font(sk a, font f) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->font = f;
}

#undef translate
none sk_translate(sk a, f32 x, f32 y) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    sk->translate(x, y);
}

#undef scale
none sk_scale(sk a, f32 scale) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    sk->scale(scale, scale);
}

none sk_clip(sk a, rect r, f32 radius_x, f32 radius_y) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    SkRRect rr;
    rr.setRectXY(SkRect::MakeXYWH(
        SkScalar(r->x), 
        SkScalar(r->y), 
        SkScalar(r->w), 
        SkScalar(r->h)),
        SkScalar(radius_x),
        SkScalar(radius_y));
    sk->clipRRect(rr, SkClipOp::kIntersect, true);
    //rr.setRectXY(SkRect::MakeXYWH(x, y, w, h), rx, ry);
}

none sk_stroke_size(sk a, f32 size) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    //assert(size == 0, "size set to %f", size);
    ds->stroke_size = size;
}

none sk_stroke_cap(sk a, cap stroke_cap) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->stroke_cap = stroke_cap;
}

none sk_stroke_join(sk a, join stroke_join) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->stroke_join = stroke_join;
}

none sk_stroke_miter_limit(sk a, f32 stroke_miter_limit) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->stroke_miter_limit = stroke_miter_limit;
}

none sk_stroke_dash_offset(sk a, f32 stroke_dash_offset) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->stroke_dash_offset = stroke_dash_offset;
}

none sk_restore(sk a) {
    if (!len(a->state))
        return;
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    pop(a->state);
    #undef restore
    sk->restore();
}

none sk_fill_color(sk a, object clr) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->fill_color = sk_color(clr);
}

none sk_stroke_color(sk a, object clr) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    ds->stroke_color = sk_color(clr);
}

none sk_clear(sk a, object clr) {
    SkCanvas* sk = (SkCanvas*)a->sk_canvas;
    SkColor fill = clr ? sk_color(clr) : SK_ColorWHITE;
    sk->clear(fill);
}

/// ASMR AR/AI ramblings -- Work on Orbiter, an open app platform where any app may be edited
/// -----------------
/// its steam without costing anything and being completely open; also the consumers of apps in store are now 1:1 developers.
/// its probably not just another ide, I think.
/// to me this is what 'platform' is, and what an 'sdk' is.
/// its not something we install on our machines to obtain
/// some development headers for our compiler.
/// apps are native built C apps

/// no tabs, just panels where we load in entire folders at once.
/// mini map navigator has a bit more to it, as a result, however one does
/// not need to scroll manually because we use our voice to navigate contextually 
/// with intelligent voice tracking using whisper to llama-4-8bit.


/// console would just think of everything in char units. like it is.
/// measuring text would just be its length, line height 1.

SkFont* sk_font(sk a) {
    draw_state ds = (draw_state)last(a->state);
    return (SkFont*)ds->font->res;
}

text_metrics sk_measure(sk a, string text) {
    SkFontMetrics mx;
    draw_state    ds = (draw_state)last(a->state);
    SkFont     *font = (SkFont*)ds->font->res;
    font->setSize(ds->font->size);
    auto         adv = font->measureText(text->chars, text->len, SkTextEncoding::kUTF8);
    auto          lh = font->getMetrics(&mx);

    return text_metrics {
        adv,
        abs(mx.fAscent) + abs(mx.fDescent),
        mx.fAscent,
        mx.fDescent,
        lh,
        mx.fCapHeight
    };
}


/// the text out has a rect, controls line height, scrolling offset and all of that nonsense we need to handle
/// as a generic its good to have the rect and alignment enums given.  there simply isnt a user that doesnt benefit
/// it effectively knocks out several redundancies to allow some components to be consolidated with style difference alone

string sk_ellipsis(sk a, string text, rect r, ARef r_tm) {
    text_metrics* tm = (text_metrics*)r_tm;
    static string el;
    if (!el) el = (string)hold((object)string("..."));
    
    string cur = text;
    int tr = cur->len;
    *tm = sk_measure(a, el);
    
    if (tm->w >= r->w)
        tr = 0;
    else
        for (;;) {
            *tm = sk_measure(a, cur);
            if (tm->w <= r->w || tr == 0)
                break;
            if (tm->w > r->w && tr >= 1) {
                cur = mid(text, 0, --tr);
                concat(cur, el);
            }
        }
    return (tr == 0) ? string("") : cur;
}


SVG SVG_with_path(SVG a, path uri) {
    fault("svg not implemented");
    /*
    SkStream* stream = new SkFILEStream((symbol)uri->chars);
    a->svg_dom = (handle)new sk_sp<SkSVGDOM>(SkSVGDOM::MakeFromStream(*stream));
    SkSize size = (*(sk_sp<SkSVGDOM>*)a->svg_dom)->containerSize();
    a->w = size.fWidth;
    a->h = size.fHeight;
    delete stream;
    */
    return a;
}


void SVG_render(SVG a, SkCanvas *sk, int w, int h) {
    /*
    if (w == -1) w = a->w;
    if (h == -1) h = a->h;
    if (a->rw != w || a->rh != h) {
        a->rw  = w;
        a->rh  = h;
        (*(sk_sp<SkSVGDOM>*)a->svg_dom)->setContainerSize(
            SkSize::Make(a->rw, a->rh));
    }
    (*(sk_sp<SkSVGDOM>*)a->svg_dom)->render(sk);
    */
}

void SVG_dealloc(SVG a) {
    //delete (sk_sp<SkSVGDOM>*)a->svg_dom;
}

define_class(SVG, Au)

void sk_draw_svg(sk a, SVG svg, rect r, vec2f align, vec2f offset) {
    SkCanvas*  sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    
    vec2f  pos = { 0, 0 };
    vec2f  fsz = { svg->w, svg->h };

    /// now its just of matter of scaling the little guy to fit in the box.
    f32 scx = r->w / fsz.x;
    f32 scy = r->h / fsz.y;
    f32 sc  = (scy > scx) ? scx : scy;
    
    /// no enums were harmed during the making of this function (again)
    vec2f v2_a = vec2f(r->x, r->y);
    vec2f v2_b = vec2f(r->x + r->w - fsz.x * scx, 
                        r->y + r->h - fsz.y * scy);
    pos.x = v2_a.x * (1.0f - align.x) + v2_b.x * align.x;
    pos.y = v2_a.y * (1.0f - align.y) + v2_b.y * align.y;
    
    sk->save();
    sk->translate(pos.x + offset.x, pos.y + offset.y);
    
    sk->scale(sc, sc);
    SVG_render(svg, sk, (i32)r->w, (i32)r->h);
    sk->restore();
}

void sk_image_dealloc(image img) {
    delete (sk_sp<SkImage>*)img->res;
}

void sk_draw_image(sk a, image img, rect r, vec2f align, vec2f offset) {
    SVG svg = (SVG)instanceof((object)img, SVG);
    if (svg) return sk_draw_svg(a, svg, r, align, offset);
    
    SkCanvas*  sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    vec2f  pos = vec2f(0, 0);
    vec2f  fsz = vec2f((f32)img->width, (f32)img->height);
    
    /// cache SkImage on image
    if (!img->res) {
        verify(img->format == Pixel_rgba8, "sk_image: unsupported image format: %o", e_str(Pixel, img->format));
        rgba8 *px = (rgba8*)data(img);
        SkImageInfo info = SkImageInfo::Make(
            (i32)fsz.x, (i32)fsz.y, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
        
        SkBitmap bm;
        bm.installPixels(info, px, img->width * sizeof(rgba8));
        sk_sp<SkImage> bm_image = bm.asImage();
        img->res         = (handle)new sk_sp<SkImage>(bm_image);
        img->res_dealloc = (hook)  sk_image_dealloc;
    }
    
    /// now its just of matter of scaling the little guy to fit in the box.
    f32 scx = r->w / fsz.x;
    f32 scy = r->h / fsz.y;
    scx     = scy = (scy > scx) ? scx : scy;
    
    vec2f v2_a = vec2f(r->x, r->y);
    vec2f v2_b = vec2f(r->x + r->w - fsz.x * scx, 
                       r->y + r->h - fsz.y * scy);
    
    pos.x = v2_a.x * (1.0f - align.x) + v2_b.x * align.x;
    pos.y = v2_a.y * (1.0f - align.y) + v2_b.y * align.y;
    
    sk->save();
    sk->translate(pos.x + offset.x, pos.y + offset.y);
    
    SkCubicResampler cubicResampler { 1.0f / 3, 1.0f / 3 };
    SkSamplingOptions samplingOptions(cubicResampler);

    sk->scale(scx, scy);
    SkImage *sk_img = ((sk_sp<SkImage>*)img->res)->get();
    sk->drawImage(sk_img, SkScalar(0), SkScalar(0), samplingOptions);
    sk->restore();
}

/// the lines are most definitely just text() calls, it should be up to the user to perform multiline.
rect sk_draw_text(sk a, string text, rect r, vec2f align, vec2f offset, bool ellip) {
    SkCanvas*  sk = (SkCanvas*)a->sk_canvas;
    draw_state ds = (draw_state)last(a->state);
    SkPaint    ps;
    ps.setColor(ds->fill_color);
    if (ds->opacity != 1.0f)
        ps.setAlpha(ds->opacity);
    ps.setAntiAlias(true);
    ps.setStyle(SkPaint::kFill_Style);
    if (ds->blur_radius > 0.0f)
        ps.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, ds->blur_radius));
        
    SkFont    *f = sk_font(a);
    vec2f  pos   = { 0, 0 };
    string ptext = text;
    text_metrics tm;
    if (ellip) {
        ptext  = sk_ellipsis(a, text, r, (ARef)&tm);
    } else
        tm     = sk_measure(a, ptext);
    auto    tb = SkTextBlob::MakeFromText(ptext->chars, ptext->len, *(const SkFont *)f, SkTextEncoding::kUTF8);

    vec2f v2_a = vec2f(r->x,  r->y);
    vec2f v2_b = vec2f(r->x + r->w - tm.w, 
                       r->y + r->h - tm.h);
    pos.x = v2_a.x * (1.0f - align.x) + v2_b.x * align.x;
    pos.y = v2_a.y * (1.0f - align.y) + v2_b.y * align.y;

    double skia_y_offset = (tm.descent + -tm.ascent) / 1.5;
    
    sk->drawTextBlob(
        tb, SkScalar(pos.x + offset.x),
            SkScalar(pos.y + offset.y + skia_y_offset), ps);

    /// return where its rendered
    return rect(
        x, pos.x + offset.x,
        y, pos.y + offset.y,
        w, tm.w,
        h, tm.h);
}


void transition_image_layout(trinity, VkImage, VkImageLayout, VkImageLayout, int, int, int, int, bool);

none sk_prepare(sk a) {
    
}

none sk_sync() {
    // the issue is the need to transition ALL skia canvas before a sync.
    // that means we must gather handles to canvases intern

    int ln = len(canvases);
    for(int i = 0; i < ln; i++) {
        sk cv = (sk)canvases->origin[i];
        cv->tx->vk_layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        transition(cv->tx,  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    Skia* g = (Skia*)sk_current;

#if 0
    auto recording = g->rec->snap();
    g->ctx->insertRecording({recording.get()});
    g->ctx->submit();
#else
    g->ctx->flush();
    g->ctx->submit();
#endif

    for(int i = 0; i < ln; i++) {
        sk cv = (sk)canvases->origin[i];
        cv->tx->vk_layout  = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        transition(cv->tx, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}

define_class(sk, canvas)

define_struct(text_metrics, f32)
define_vector(text_metrics, f32, 6)


}

/*

after RT render, port all of canvas.hpp and this code in trinity

struct state {
    image       img;
    f32         outline_sz = 0.0;
    f32         font_scale = 1.0;
    f32         opacity    = 1.0;
    mat4f       m;
    i32         color;
    vec4f       clip;
    vec2f       blur;
    ion::font   font;
    SkPaint     ps;
    mat4f       model, view, proj;
};

/// canvas renders to image, and can manage the renderer/resizing
struct ICanvas {
    GrDirectContext     *ctx = null;
    VkEngine               e = null;
    VkhPresenter    renderer = null;
    sk_sp<SkSurface> sk_surf = null;
    SkCanvas      *sk_canvas = null;
    vec2i                 sz = { 0, 0 };
    VkhImage        vk_image = null;
    vec2d          dpi_scale = { 1, 1 };

    struct state {
        ion::image  img;
        double      outline_sz = 0.0;
        double      font_scale = 1.0;
        double      opacity    = 1.0;
        m44d        m;
        rgbad       color;
        graphics::shape clip;
        vec2d       blur;
        ion::font   font;
        SkPaint     ps;
        glm::mat4   model, view, proj;
    };

    state *top = null;
    doubly<state> stack;

    void outline_sz(double sz) {
        top->outline_sz = sz;
    }

    void color(rgbad &c) {
        top->color = c;
    }

    void opacity(double o) {
        top->opacity = o;
    }

    /// can be given an image
    void sk_resize(VkhImage &image, int width, int height) {
        if (vk_image)
            vkh_image_drop(vk_image);
        if (!image) {
            vk_image = vkh_image_create(
                e->vkh, VK_FORMAT_R8G8B8A8_UNORM, u32(width), u32(height),
                VK_IMAGE_TILING_OPTIMAL, VKH_MEMORY_USAGE_GPU_ONLY,
                VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);

            // test code to clear the image with a color (no result from SkCanvas currently!)
            // this results in a color drawn after the renderer blits the VkImage to the window
            // this image is given to 
            VkDevice device = e->vk_device->device;
            VkQueue  queue  = e->renderer->queue;
            VkImage  image  = vk_image->image;

            VkCommandBuffer commandBuffer = e->vk_device->command_begin();

            // Assume you have a VkDevice, VkPhysicalDevice, VkQueue, and VkCommandBuffer already set up.
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            // Clear the image with blue color
            VkClearColorValue clearColor = { 0.4f, 0.0f, 0.5f, 1.0f };
            vkCmdClearColorImage(
                commandBuffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                &clearColor,
                1,
                &barrier.subresourceRange
            );

            // Transition image layout to SHADER_READ_ONLY_OPTIMAL
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &barrier
            );

            e->vk_device->command_submit(commandBuffer);

        } else {
            /// option: we can know dpi here as declared by the user
            vk_image = vkh_image_grab(image);
            assert(width == vk_image->width && height == vk_image->height);
        }
        
        sz = vec2i { width, height };
        ///
        ctx                     = Skia::Context(e)->sk_context;
        auto imi                = GrVkImageInfo { };
        imi.fImage              = vk_image->image;
        imi.fImageTiling        = VK_IMAGE_TILING_OPTIMAL;
        imi.fImageLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
        imi.fFormat             = VK_FORMAT_R8G8B8A8_UNORM;
        imi.fImageUsageFlags    = VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;//VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // i dont think so.
        imi.fSampleCount        = 1;
        imi.fLevelCount         = 1;
        imi.fCurrentQueueFamily = e->vk_gpu->indices.graphicsFamily.value();
        imi.fProtected          = GrProtected::kNo;
        imi.fSharingMode        = VK_SHARING_MODE_EXCLUSIVE;

        auto color_space = SkColorSpace::MakeSRGB();
        auto rt = GrBackendRenderTarget { sz.x, sz.y, imi };
        sk_surf = SkSurfaces::WrapBackendRenderTarget(ctx, rt,
                    kTopLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
                    color_space, null);
        sk_canvas = sk_surf->getCanvas();
        dpi_scale = e->vk_gpu->dpi_scale;
        identity();
    }

    void app_resize() {
        /// these should be updated (VkEngine can have VkWindow of sort eventually if we want multiple)
        float sx, sy;
        u32 width, height;
        vkh_presenter_get_size(renderer, &width, &height, &sx, &sy); /// vkh/vk should have both vk engine and glfw facility
        VkhImage img = null;
        sk_resize(img, width, height);
        vkh_presenter_build_blit_cmd(renderer, vk_image->image,
            width / dpi_scale.x, height / dpi_scale.y);
    }

    SkPath *sk_path(graphics::shape &sh) {
        graphics::shape::sdata &shape = sh.ref<graphics::shape::sdata>();
        // shape.sk_path 
        if (!shape.sk_path) {
            shape.sk_path = new SkPath { };
            SkPath &p     = *(SkPath*)shape.sk_path;

            /// efficient serialization of types as Skia does not spend the time to check for these primitives
            if (shape.type == typeof(rectd)) {
                rectd &m = sh->bounds;
                SkRect r = SkRect {
                    float(m.x), float(m.y), float(m.x + m.w), float(m.y + m.h)
                };
                p.Rect(r);
            } else if (shape.type == typeof(Rounded<double>)) {
                Rounded<double>::rdata &m = sh->bounds.mem->ref<Rounded<double>::rdata>();
                p.moveTo  (m.tl_x.x, m.tl_x.y);
                p.lineTo  (m.tr_x.x, m.tr_x.y);
                p.cubicTo (m.c0.x,   m.c0.y, m.c1.x, m.c1.y, m.tr_y.x, m.tr_y.y);
                p.lineTo  (m.br_y.x, m.br_y.y);
                p.cubicTo (m.c0b.x,  m.c0b.y, m.c1b.x, m.c1b.y, m.br_x.x, m.br_x.y);
                p.lineTo  (m.bl_x.x, m.bl_x.y);
                p.cubicTo (m.c0c.x,  m.c0c.y, m.c1c.x, m.c1c.y, m.bl_y.x, m.bl_y.y);
                p.lineTo  (m.tl_y.x, m.tl_y.y);
                p.cubicTo (m.c0d.x,  m.c0d.y, m.c1d.x, m.c1d.y, m.tl_x.x, m.tl_x.y);
            } else {
                graphics::shape::sdata &m = *sh.data;
                for (mx &o:m.ops) {
                    type_t t = o.type();
                    if (t == typeof(Movement)) {
                        Movement m(o);
                        p.moveTo(m->x, m->y);
                    } else if (t == typeof(Line)) {
                        Line l(o);
                        p.lineTo(l->origin.x, l->origin.y); /// todo: origin and to are swapped, i believe
                    }
                }
                p.close();
            }
        }

        /// issue here is reading the data, which may not be 'sdata', but Rect, Rounded
        /// so the case below is 
        if (bool(shape.sk_offset) && shape.cache_offset == shape.offset)
            return (SkPath*)shape.sk_offset;
        ///
        if (!std::isnan(shape.offset) && shape.offset != 0) {
            assert(shape.sk_path); /// must have an actual series of shape operations in skia
            ///
            delete (SkPath*)shape.sk_offset;

            SkPath *o = (SkPath*)shape.sk_offset;
            shape.cache_offset = shape.offset;
            ///
            SkPath  fpath;
            SkPaint cp = SkPaint(top->ps);
            cp.setStyle(SkPaint::kStroke_Style);
            cp.setStrokeWidth(std::abs(shape.offset) * 2);
            cp.setStrokeJoin(SkPaint::kRound_Join);

            SkPathStroker2 stroker;
            SkPath offset_path = stroker.getFillPath(*(SkPath*)shape.sk_path, cp);
            shape.sk_offset = new SkPath(offset_path);
            
            auto vrbs = ((SkPath*)shape.sk_path)->countVerbs();
            auto pnts = ((SkPath*)shape.sk_path)->countPoints();
            std::cout << "sk_path = " << (void *)shape.sk_path << ", pointer = " << (void *)this << " verbs = " << vrbs << ", points = " << pnts << "\n";
            ///
            if (shape.offset < 0) {
                o->reverseAddPath(fpath);
                o->setFillType(SkPathFillType::kWinding);
            } else
                o->addPath(fpath);
            ///
            return o;
        }
        return (SkPath*)shape.sk_path;
    }

    void font(ion::font &f) { 
        top->font = f;
    }
    
    void save() {
        state &s = stack->push();
        if (top) {
            s = *top;
        } else {
            s.ps = SkPaint { };
        }
        sk_canvas->save();
        top = &s;
    }

    void identity() {
        sk_canvas->resetMatrix();
        sk_canvas->scale(dpi_scale.x, dpi_scale.y);
    }

    void set_matrix() {
    }

    m44d get_matrix() {
        SkM44 skm = sk_canvas->getLocalToDevice();
        m44d res(0.0);
        return res;
    }


    void    clear()        { sk_canvas->clear(sk_color(top->color)); }
    void    clear(rgbad c) { sk_canvas->clear(sk_color(c)); }

    void    flush() {
        ctx->flush();
        ctx->submit();
    }

    void  restore() {
        stack->pop();
        top = stack->len() ? &stack->last() : null;
        sk_canvas->restore();
    }

    vec2i size() { return sz; }

    /// console would just think of everything in char units. like it is.
    /// measuring text would just be its length, line height 1.
    text_metrics measure(str &text) {
        SkFontMetrics mx;
        SkFont     &font = font_handle(top->font);
        auto         adv = font.measureText(text.cs(), text.len(), SkTextEncoding::kUTF8);
        auto          lh = font.getMetrics(&mx);

        return text_metrics {
            adv,
            abs(mx.fAscent) + abs(mx.fDescent),
            mx.fAscent,
            mx.fDescent,
            lh,
            mx.fCapHeight
        };
    }

    double measure_advance(char *text, size_t len) {
        SkFont     &font = font_handle(top->font);
        auto         adv = font.measureText(text, len, SkTextEncoding::kUTF8);
        return (double)adv;
    }

    /// the text out has a rect, controls line height, scrolling offset and all of that nonsense we need to handle
    /// as a generic its good to have the rect and alignment enums given.  there simply isnt a user that doesnt benefit
    /// it effectively knocks out several redundancies to allow some components to be consolidated with style difference alone
    str ellipsis(str &text, rectd &rect, text_metrics &tm) {
        const str el = "...";
        str       cur, *p = &text;
        int       trim = p->len();
        tm             = measure((str &)el);
        
        if (tm.w >= rect.w)
            trim = 0;
        else
            for (;;) {
                tm = measure(*p);
                if (tm.w <= rect.w || trim == 0)
                    break;
                if (tm.w > rect.w && trim >= 1) {
                    cur = text.mid(0, --trim) + el;
                    p   = &cur;
                }
            }
        return (trim == 0) ? "" : (p == &text) ? text : cur;
    }


    void image(ion::SVG &image, rectd &rect, alignment &align, vec2d &offset) {
        SkPaint ps = SkPaint(top->ps);
        vec2d  pos = { 0, 0 };
        vec2i  isz = image.sz();
        
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        /// now its just of matter of scaling the little guy to fit in the box.
        real scx = rect.w / isz.x;
        real scy = rect.h / isz.y;
        real sc  = (scy > scx) ? scx : scy;
        
        /// no enums were harmed during the making of this function (again)
        pos.x = mix(rect.x, rect.x + rect.w - isz.x * sc, align.x);
        pos.y = mix(rect.y, rect.y + rect.h - isz.y * sc, align.y);
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        sk_canvas->scale(sc, sc);
        image.render(sk_canvas, rect.w, rect.h);
        sk_canvas->restore();
    }

    void image(ion::image &image, rectd &rect, alignment &align, vec2d &offset, bool attach_tx) {
        SkPaint ps = SkPaint(top->ps);
        vec2d  pos = { 0, 0 };
        vec2i  isz = image.sz();
        
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        /// cache SkImage using memory attachments
        attachment *att = image.mem->find_attachment("sk-image");
        sk_sp<SkImage> *im;
        if (!att) {
            SkBitmap bm;
            rgba8          *px = image.pixels();
            //memset(px, 255, 640 * 360 * 4);
            SkImageInfo   info = SkImageInfo::Make(isz.x, isz.y, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType);
            sz_t        stride = image.stride() * sizeof(rgba8);
            bm.installPixels(info, px, stride);
            sk_sp<SkImage> bm_image = bm.asImage();
            im = new sk_sp<SkImage>(bm_image);
            if (attach_tx)
                att = image.mem->attach("sk-image", im, [im]() { delete im; });
        }
        
        /// now its just of matter of scaling the little guy to fit in the box.
        real scx = rect.w / isz.x;
        real scy = rect.h / isz.y;
        
        if (!align.is_default) {
            scx   = scy = (scy > scx) ? scx : scy;
            /// no enums were harmed during the making of this function
            pos.x = mix(rect.x, rect.x + rect.w - isz.x * scx, align.x);
            pos.y = mix(rect.y, rect.y + rect.h - isz.y * scy, align.y);
            
        } else {
            /// if alignment is default state, scale directly by bounds w/h, position at bounds x/y
            pos.x = rect.x;
            pos.y = rect.y;
        }
        
        sk_canvas->save();
        sk_canvas->translate(pos.x + offset.x, pos.y + offset.y);
        
        SkCubicResampler cubicResampler { 1.0f/3, 1.0f/3 };
        SkSamplingOptions samplingOptions(cubicResampler);

        sk_canvas->scale(scx, scy);
        SkImage *img = im->get();
        sk_canvas->drawImage(img, SkScalar(0), SkScalar(0), samplingOptions);
        sk_canvas->restore();

        if (!attach_tx) {
            delete im;
            ctx->flush();
            ctx->submit();
        }
    }

    /// the lines are most definitely just text() calls, it should be up to the user to perform multiline.
    void text(str &text, rectd &rect, alignment &align, vec2d &offset, bool ellip, rectd *placement) {
        SkPaint ps = SkPaint(top->ps);
        ps.setColor(sk_color(top->color));
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        SkFont  &f = font_handle(top->font);
        vec2d  pos = { 0, 0 };
        str  stext;
        str *ptext = &text;
        text_metrics tm;
        if (ellip) {
            stext  = ellipsis(text, rect, tm);
            ptext  = &stext;
        } else
            tm     = measure(*ptext);
        auto    tb = SkTextBlob::MakeFromText(ptext->cs(), ptext->len(), (const SkFont &)f, SkTextEncoding::kUTF8);
        pos.x = mix(rect.x, rect.x + rect.w - tm.w, align.x);
        pos.y = mix(rect.y, rect.y + rect.h - tm.h, align.y);
        double skia_y_offset = (tm.descent + -tm.ascent) / 1.5;
        /// set placement rect if given (last paint is what was rendered)
        if (placement) {
            placement->x = pos.x + offset.x;
            placement->y = pos.y + offset.y;
            placement->w = tm.w;
            placement->h = tm.h;
        }
        sk_canvas->drawTextBlob(
            tb, SkScalar(pos.x + offset.x),
                SkScalar(pos.y + offset.y + skia_y_offset), ps);
    }

    void clip(rectd &rect) {
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        sk_canvas->clipRect(r);
    }

    void outline(array<glm::vec2> &line, bool is_fill = false) {
        SkPaint ps = SkPaint(top->ps);
        ps.setAntiAlias(true);
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(is_fill ? 0 : top->outline_sz);
        ps.setStroke(!is_fill);
        glm::vec2 *a = null;
        SkPath path;
        for (glm::vec2 &b: line) {
            SkPoint bp = { b.x, b.y };
            if (a) {
                path.lineTo(bp);
            } else {
                path.moveTo(bp);
            }
            a = &b;
        }
        sk_canvas->drawPath(path, ps);
    }

    void projection(glm::mat4 &m, glm::mat4 &v, glm::mat4 &p) {
        top->model      = m;
        top->view       = v;
        top->proj       = p;
    }

    void outline(array<glm::vec3> &v3, bool is_fill = false) {
        glm::vec2 sz = { this->sz.x / this->dpi_scale.x, this->sz.y / this->dpi_scale.y };
        array<glm::vec2> projected { v3.len() };
        for (glm::vec3 &vertex: v3) {
            glm::vec4 cs  = top->proj * top->view * top->model * glm::vec4(vertex, 1.0f);
            glm::vec3 ndc = cs / cs.w;
            float screenX = ((ndc.x + 1) / 2.0) * sz.x;
            float screenY = ((1 - ndc.y) / 2.0) * sz.y;
            glm::vec2  v2 = { screenX, screenY };
            projected    += v2;
        }
        outline(projected, is_fill);
    }

    void line(glm::vec3 &a, glm::vec3 &b) {
        array<glm::vec3> ab { size_t(2) };
        ab.push(a);
        ab.push(b);
        outline(ab);
    }

    void arc(glm::vec3 position, real radius, real startAngle, real endAngle, bool is_fill = false) {
        const int segments = 36;
        ion::array<glm::vec3> arcPoints { size_t(segments) };
        float angleStep = (endAngle - startAngle) / segments;

        for (int i = 0; i <= segments; ++i) {
            float     angle = glm::radians(startAngle + angleStep * i);
            glm::vec3 point;
            point.x = position.x + radius * cos(angle);
            point.y = position.y;
            point.z = position.z + radius * sin(angle);
            glm::vec4 viewSpacePoint = top->view * glm::vec4(point, 1.0f);
            glm::vec3 clippingSp     = glm::vec3(viewSpacePoint);
            arcPoints += clippingSp;
        }
        arcPoints.set_size(segments);
        outline(arcPoints, is_fill);
    }

    void outline(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(true);
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(top->outline_sz);
        ps.setStroke(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        draw_rect(rect, ps);
    }

    void outline(graphics::shape &shape) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!shape.is_rect());
        ps.setColor(sk_color(top->color));
        ps.setStrokeWidth(top->outline_sz);
        ps.setStroke(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->drawPath(*sk_path(shape), ps);
    }

    void cap(graphics::cap &c) {
        top->ps.setStrokeCap(c == graphics::cap::blunt ? SkPaint::kSquare_Cap :
                             c == graphics::cap::round ? SkPaint::kRound_Cap  :
                                                         SkPaint::kButt_Cap);
    }

    void join(graphics::join &j) {
        top->ps.setStrokeJoin(j == graphics::join::bevel ? SkPaint::kBevel_Join :
                              j == graphics::join::round ? SkPaint::kRound_Join  :
                                                          SkPaint::kMiter_Join);
    }

    void translate(vec2d &tr) {
        sk_canvas->translate(SkScalar(tr.x), SkScalar(tr.y));
    }

    void scale(vec2d &sc) {
        sk_canvas->scale(SkScalar(sc.x), SkScalar(sc.y));
    }

    void rotate(double degs) {
        sk_canvas->rotate(degs);
    }

    void draw_rect(rectd &rect, SkPaint &ps) {
        SkRect   r = SkRect {
            SkScalar(rect.x),          SkScalar(rect.y),
            SkScalar(rect.x + rect.w), SkScalar(rect.y + rect.h) };
        
        if (rect.rounded) {
            SkRRect rr;
            SkVector corners[4] = {
                { float(rect.r_tl.x), float(rect.r_tl.y) },
                { float(rect.r_tr.x), float(rect.r_tr.y) },
                { float(rect.r_br.x), float(rect.r_br.y) },
                { float(rect.r_bl.x), float(rect.r_bl.y) }
            };
            rr.setRectRadii(r, corners);
            sk_canvas->drawRRect(rr, ps);
        } else {
            sk_canvas->drawRect(r, ps);
        }
    }

    void fill(rectd &rect) {
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setColor(sk_color(top->color));
        ps.setAntiAlias(true);
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));

        draw_rect(rect, ps);
    }

    // we are to put everything in path.
    void fill(graphics::shape &path) {
        if (path.is_rect())
            return fill(path->bounds);
        ///
        SkPaint ps = SkPaint(top->ps);
        ///
        ps.setAntiAlias(!path.is_rect());
        ps.setColor(sk_color(top->color));
        ///
        if (top->opacity != 1.0f)
            ps.setAlpha(float(ps.getAlpha()) * float(top->opacity));
        
        sk_canvas->drawPath(*sk_path(path), ps);
    }

    void clip(graphics::shape &path) {
        sk_canvas->clipPath(*sk_path(path));
    }

    void gaussian(float *sz, rectd &crop) {
        SkImageFilters::CropRect crect = { };
        if (crop) {
            SkRect rect = { SkScalar(crop.x),          SkScalar(crop.y),
                            SkScalar(crop.x + crop.w), SkScalar(crop.y + crop.h) };
            crect       = SkImageFilters::CropRect(rect);
        }
        sk_sp<SkImageFilter> filter = SkImageFilters::Blur(sz[0], sz[1], nullptr, crect);
        memcpy(top->blur, sz, sizeof(float) * 2);
        top->ps.setImageFilter(std::move(filter));
    }

    SkFont *font_handle(const char* font) {
        /// dpi scaling is supported at the SkTypeface level, just add the scale x/y
        auto t = SkTypeface::MakeFromFile(font);
        font->sk_font = new SkFont(t);
        ((SkFont*)font->sk_font)->setSize(font->sz);
        return *(SkFont*)font->sk_font;
    }
    type_register(ICanvas);
};

mx_implement(Canvas, mx);

// we need to give VkEngine too, if we want to support image null
Canvas::Canvas(VkhImage image) : Canvas() {
    data->e = image->vkh->e;
    data->sk_resize(image, image->width, image->height);
    data->save();
}

*/