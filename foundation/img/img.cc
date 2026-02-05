#include <import>

#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfHeader.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv.h>


// read images without conversion; for .png and .exr
// this facilitates grayscale maps, environment color, 
// color maps, grayscale components for various PBR material attributes

extern "C" {

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

int type_from_format(int channels, bool f32) {
    int   type        =
        channels == 1 ? (f32 ? CV_32FC1 : CV_8UC1) :
        channels == 3 ? (f32 ? CV_32FC3 : CV_8UC3) :
                        (f32 ? CV_32FC4 : CV_8UC4);
    return type;
}

void opencv_resize_area(uint8_t* src, uint8_t* dst, int f32, int in_w, int in_h, int channels, int out_w, int out_h) {
    // Convert to OpenCV Mat
    int   type = type_from_format(channels, f32);
    cv::Mat input_mat(in_h, in_w, type, src);  // Assuming grayscale, adjust CV_8UC3 for RGB
    cv::Mat output_mat(out_h, out_w, type, dst);

    // Perform resizing using OpenCV's INTER_AREA (better for downscaling)
    cv::resize(input_mat, output_mat, cv::Size(out_w, out_h), 0, 0, cv::INTER_AREA);
}

void opencv_gaussian(uint8_t* src, uint8_t* dst, int f32, int w, int h, int channels, float amount) {
    float sigma       = amount;
    int   type        = type_from_format(channels, f32);
    cv::Mat input_mat (h, w, type, src);
    cv::Mat output_mat(h, w, type, dst);
    cv::GaussianBlur  (input_mat, output_mat, cv::Size(0, 0), sigma);

}

// we want to reduce the image to w / (amount / 3)
// then we blur with a 3x kernel
// then resize?

void opencv_gaussian_fast(uint8_t* src, uint8_t* dst, int f32, int w, int h, int channels, float amount) {
    float sigma       = amount;
    int   type        = type_from_format(channels, f32);
    cv::Mat input_mat (h, w, type, src);

    // assume input is a cv::Mat (src) of type CV_8UC3 or CV_32FC3
    cv::Mat small_mat(h / (amount / 3), w / (amount / 3), type);
    cv::Mat blur_mat(h / (amount / 3), w / (amount / 3), type);

    // 1. downscale aggressively (e.g., 1/8th size)
    cv::resize(input_mat, small_mat, small_mat.size(), cv::INTER_AREA);

    // 2. apply light gaussian blur (optional, can be skipped for even faster path)
    cv::GaussianBlur(small_mat, blur_mat, cv::Size(5, 5), 1.0);

    cv::Mat output_mat(h, w, type, dst);

    // 3. upscale back to original size
    cv::resize(blur_mat, output_mat, output_mat.size(), 0, 0, cv::INTER_LANCZOS4);
}

void blur_equirect_wrap_rgbaf(float* input, int w, int h, float sigma_base) {
    float* temp = (float*)malloc(w * h * 4 * sizeof(float)); // output buffer

    int max_radius = (int)(sigma_base * 3);
    float* kernel = (float*)malloc((2 * max_radius + 1) * sizeof(float));

    for (int y = 0; y < h; ++y) {
        float v = (y + 0.5f) / h;
        float scale = powf(sinf(v * M_PI), 0.22f);
        float sigma = fmaxf(0.01f, sigma_base * scale);
        int radius = (int)(sigma * 3);
        int ksize = 2 * radius + 1;

        // create gaussian kernel
        float sum = 0;
        for (int i = -radius; i <= radius; ++i) {
            float w = expf(-0.5f * (i * i) / (sigma * sigma));
            kernel[i + radius] = w;
            sum += w;
        }
        for (int i = 0; i < ksize; ++i) kernel[i] /= sum;

        for (int x = 0; x < w; ++x) {
            float result[4] = {0};

            for (int ky = -radius; ky <= radius; ++ky) {
                int sy = (y + ky + h) % h;
                for (int kx = -radius; kx <= radius; ++kx) {
                    int sx = (x + kx + w) % w;
                    float* src = &input[4 * (sy * w + sx)];
                    float weight = kernel[ky + radius] * kernel[kx + radius];
                    for (int c = 0; c < 4; ++c)
                        result[c] += src[c] * weight;
                }
            }

            float* dst = &temp[4 * (y * w + x)];
            for (int c = 0; c < 4; ++c)
                dst[c] = result[c];
        }
    }

    memcpy(input, temp, w * h * 4 * sizeof(float));
    free(temp);
    free(kernel);
}


void opencv_blur_equirect(uint8_t* input, int w, int h) {
    blur_equirect_wrap_rgbaf((float*)input, w, h, 22.0f);
}

}