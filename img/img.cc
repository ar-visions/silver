#include <import>

// this module asserts the C++ way: one expression, no message
#undef assert
#include <cassert>

// silver free functions are not emitted into the generated header
extern "C" Image jpeg_decode(path uri);
extern "C" Image exr_decode(path uri);

// read Images without conversion; for .png and .exr
// this facilitates grayscale maps, environment color, 
// color maps, grayscale components for various PBR material attributes

extern "C" {

shape shape_from(i64, i64*);
Au alloc2(Au_t type, Au_t scalar, shape s, const char* source, int line, int seq);
Au alloc(Au_t type, num count, shape shape_data, Au_t meta_a, Au meta_b, const char* source, int line, int seq);

none Image_init(Image a) {
    Au info = header((Au)a);
    Pixel f = a->format;

    if (!a->channels)
        a->channels = f == Pixel_none ? 1 : f == Pixel_rgba8   ? 4 : f == Pixel_rgbf32 ? 4 :
                      f == Pixel_u8   ? 1 : f == Pixel_rgbaf32 ? 4 : 1;

    if (!a->pixel_size && a->channels)
        a->pixel_size = (f == Pixel_rgbaf32 || f == Pixel_f32 || f == Pixel_rgbf32)
            ? (int)(sizeof(f32) * a->channels) : a->channels;

    if (a->source) {
        Au source_header = header((Au)a->source);
        info->data    = Au_hold(a->source);
        info->scalar  = source_header->scalar;
        info->count   = source_header->count;
        info->data_shape = (shape)Au_hold((Au)source_header->data_shape);
        a->pixels = (u8*)a->source;
        return;
    }

    if (!a->uri) {
        Au_t pixel_type =
            f == Pixel_none ? (Au_t)_typeid(i8) : f == Pixel_rgba8   ?
                (Au_t)_typeid(rgba8) : f == Pixel_rgbf32 ? (Au_t)_typeid(vec3f) :
            f == Pixel_u8   ? (Au_t)_typeid(i8) : f == Pixel_rgbaf32 ?
                (Au_t)_typeid(vec4f) : (Au_t)_typeid(f32);
        Au_t component_type =
            f == Pixel_none ? (Au_t)_typeid(i8) : f == Pixel_rgba8   ?
                (Au_t)_typeid(i8)  : f == Pixel_rgbf32 ? (Au_t)_typeid(f32) :
            f == Pixel_u8   ? (Au_t)_typeid(i8) : f == Pixel_rgbaf32 ?
                (Au_t)_typeid(f32) : (Au_t)_typeid(f32);

        if (a->res_bits) {
            info->data = (Au)a->res_bits;
            a->pixels = (u8*)a->res_bits;
        } else {
            i64 dims[] = { a->height, a->width, pixel_type->typesize };
            u8* bytes = (u8*)alloc2(pixel_type, component_type, shape_from(3, dims), __FILE__, __LINE__, 0);
            info->data = Au_hold((Au)bytes);
            a->pixels = bytes;
        }
        return;
    }

    string ext = path_ext(a->uri);
    symbol uri = (symbol)a->uri->chars;
    if (string_eq(ext, "jpg") || string_eq(ext, "jpeg")) {
        // baseline decode lives in silver (jpeg_decode in img.ag);
        // adopt its pixels rather than copying the whole surface
        Image src = jpeg_decode(a->uri);
        assert (src);
        Au src_header = header((Au)src);
        a->width      = src->width;
        a->height     = src->height;
        a->channels   = 4;
        a->format     = Pixel_rgba8;
        a->pixel_size = 4;
        info->count   = src_header->count;
        info->scalar  = src_header->scalar;
        info->data    = Au_hold(src_header->data);
        a->pixels     = src->pixels;
    } else if (string_eq(ext, "exr")) {
        // decode lives in silver (exr_decode in img.ag); adopt its pixels
        Image src = exr_decode(a->uri);
        assert (src);
        Au src_header = header((Au)src);
        a->width      = src->width;
        a->height     = src->height;
        a->channels   = 4;
        a->format     = Pixel_rgbaf32;
        a->pixel_size = sizeof(f32) * a->channels;
        info->count   = src_header->count;
        info->scalar  = src_header->scalar;
        info->data    = Au_hold(src_header->data);
        a->pixels     = src->pixels;
    } else if (string_eq(ext, "png")) {
        FILE* file = fopen(uri, "rb");
        if (!file) {
            fprintf(stderr, "Image: cannot open %s\n", uri);
            return;
        }

        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        assert (png);

        png_infop png_info = png_create_info_struct(png);
        setjmp (png_jmpbuf(png));
        png_init_io   (png, file);
        png_read_info (png, png_info);

        a->width            = png_get_image_width  (png, png_info);
        a->height           = png_get_image_height (png, png_info);
        a->format           = Pixel_rgba8;
        png_byte bit_depth  = png_get_bit_depth    (png, png_info);
        png_byte color_type = png_get_color_type   (png, png_info);

        // normalize EVERY png to 8-bit RGBA so the packed bytes match the declared
        // format (Pixel_rgba8 = 4 bytes/px). without this an RGB (3ch) or palette png
        // stays <4 bytes/px and a texture upload reads it with a 4-byte stride → the
        // image shears and repeats. expand palette/gray/tRNS, strip 16-bit, add alpha.
        if (color_type == PNG_COLOR_TYPE_PALETTE)               png_set_palette_to_rgb(png);
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
        if (png_get_valid(png, png_info, PNG_INFO_tRNS))        png_set_tRNS_to_alpha(png);
        if (bit_depth == 16)                                    png_set_strip_16(png);
        if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
            png_set_gray_to_rgb(png);
        png_set_add_alpha(png, 0xFF, PNG_FILLER_AFTER);

        /// store the exact format read
        png_read_update_info (png, png_info);
        a->channels = png_get_channels (png, png_info);   // now 4 (RGBA)
        bit_depth   = png_get_bit_depth (png, png_info);   // now 8
        png_bytep* rows = (png_bytep*)malloc (sizeof(png_bytep) * a->height);
        u8*        data = (u8*)alloc(
            (Au_t)_typeid(u8), a->width * a->height * a->channels * (bit_depth / 8), null, null, null, __FILE__, __LINE__, 0);
        for (int y = 0; y < a->height; y++) {
            rows[y] = data + (y * a->width * a->channels * (bit_depth / 8));
        }

        /// read-image-rows
        png_read_image(png, rows);
        free(rows);
        png_destroy_read_struct(&png, &png_info, NULL);
        fclose(file);

        /// Store in header
        info->count  = a->width * a->height * a->channels;
        info->scalar = (bit_depth == 16) ? (Au_t)_typeid(u16) : (Au_t)_typeid(u8);
        info->data   = Au_hold((Au)data);
        a->pixel_size  = (bit_depth / 8) * a->channels;
        a->pixels = (u8*)data;
    }
}
}
