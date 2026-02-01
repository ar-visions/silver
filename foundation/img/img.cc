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
