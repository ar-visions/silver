#include <import>

#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfHeader.h>

// read images without conversion; for .png and .exr
// this facilitates grayscale maps, environment color, 
// color maps, grayscale components for various PBR material attributes

// save gray or colored png based on channel count; if we want conversion we may just use methods to alter an object
i32 image_exr(image a, path uri) {
    string e = ext(uri);
    if (eq(e, "exr")) {
        using namespace OPENEXR_IMF_NAMESPACE;
        using namespace IMATH_NAMESPACE;
        using namespace Imf;

        if (a->format != Pixel_rgbaf32)
            verify(false, "Only Pixel_rgbaf32 supported for EXR save");

        int width  = a->width;
        int height = a->height;
        f32* data  = (f32*)vdata((Au)a); // assumes planar RGBA32F

        Array2D<Rgba> pixels;
        pixels.resizeErase(height, width); // [y][x]

        int index = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Rgba px;
                px.r = data[index++];
                px.g = data[index++];
                px.b = data[index++];
                px.a = data[index++];
                pixels[y][x] = px;
            }
        }

        RgbaOutputFile out(uri->chars, width, height, WRITE_RGBA);
        out.setFrameBuffer(&pixels[0][0], 1, width);
        out.writePixels(height);
        return 1;
    }
    return 0;
}


none image_init(image a) {
    A info = header((Au)a);
    Pixel f = a->format;

    if (!a->channels)
        a->channels = f == Pixel_none ? 1 : f == Pixel_rgba8   ? 4 : f == Pixel_rgbf32 ? 4 :
                      f == Pixel_u8   ? 1 : f == Pixel_rgbaf32 ? 4 : 1;
    
    if (a->source) {
        A source_header = header((Au)a->source);
        info->data    = hold(a->source);
        info->scalar  = source_header->scalar;
        info->count   = source_header->count;
        info->shape   = (shape)hold((Au)source_header->shape);
        return;
    }

    if (!a->uri) {
        Au_t pixel_type =
            f == Pixel_none ? typeid(i8) : f == Pixel_rgba8   ? typeid(rgba8) : f == Pixel_rgbf32 ? typeid(vec3f) :
            f == Pixel_u8   ? typeid(i8) : f == Pixel_rgbaf32 ? typeid(vec4f) : typeid(f32);
        Au_t component_type =
            f == Pixel_none ? typeid(i8) : f == Pixel_rgba8   ? typeid(i8)  : f == Pixel_rgbf32 ? typeid(f32) :
            f == Pixel_u8   ? typeid(i8) : f == Pixel_rgbaf32 ? typeid(f32) : typeid(f32);

        if (a->res_bits) {
            info->data = (Au)a->res_bits; // we leave this up to res, but may support fallback cases where thats not provided
        } else {
            /// validate with channels if set?
            info->data = alloc2(
                pixel_type, component_type, shape_new(a->height, a->width, pixel_type->size, 0));
        }
        return;
    }

    string ext = ext(a->uri);
    symbol uri = (symbol)a->uri->chars;
    if (eq(ext, "exr")) {
        using namespace OPENEXR_IMF_NAMESPACE;
        using namespace IMATH_NAMESPACE;
        using namespace Imath;
        using namespace Imf;

        RgbaInputFile ifile(uri);
        Imath::Box2i dw = ifile.dataWindow();

        int width  = dw.max.x - dw.min.x + 1;
        int height = dw.max.y - dw.min.y + 1;

        a->width = width;
        a->height = height;
        a->channels = 4;
        a->format = Pixel_rgbaf32;
        a->pixel_size = sizeof(f32) * a->channels;

        int total_floats = width * height * 4;
        f32* data = (f32*)alloc2(typeid(rgbaf), typeid(f32), shape_new(height, width, sizeof(f32), 0));

        Imf::Array2D<Rgba> pixels;
        pixels.resizeErase(height, width); // [y][x] format

        ifile.setFrameBuffer(&pixels[0][0] - dw.min.x - dw.min.y * width, 1, width);
        ifile.readPixels(dw.min.y, dw.max.y);

        int index = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const Imf::Rgba& px = pixels[y][x];
                data[index++] = px.r;
                data[index++] = px.g;
                data[index++] = px.b;
                data[index++] = px.a;
            }
        }

        info->count = total_floats;
        info->scalar = typeid(f32);
        info->data = (Au)data;
    } else if (eq(ext, "png")) {
        FILE* file = fopen(uri, "rb");
        verify (file, "could not open PNG: %o", a->uri);

        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        verify (png, "could not init PNG: %o", a->uri);

        png_infop png_info = png_create_info_struct(png);
        setjmp (png_jmpbuf(png));
        png_init_io   (png, file);
        png_read_info (png, png_info);

        a->width            = png_get_image_width  (png, png_info);
        a->height           = png_get_image_height (png, png_info);
        a->channels         = png_get_channels     (png, png_info);
        a->format           = Pixel_rgba8;
        png_byte bit_depth  = png_get_bit_depth    (png, png_info);
        png_byte color_type = png_get_color_type   (png, png_info);

        /// store the exact format read
        png_read_update_info (png, png_info);
        png_bytep* rows = (png_bytep*)malloc (sizeof(png_bytep) * a->height);
        u8*        data = (u8*)alloc(typeid(u8), a->width * a->height * a->channels * (bit_depth / 8));
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
        info->scalar = (bit_depth == 16) ? typeid(u16) : typeid(u8);
        info->data   = (Au)data;
        a->pixel_size  = (bit_depth / 8) * a->channels;
    }
}
