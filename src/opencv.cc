#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv.h>

int type_from_format(int channels, bool f32) {
    int   type        =
        channels == 1 ? (f32 ? CV_32FC1 : CV_8UC1) :
        channels == 3 ? (f32 ? CV_32FC3 : CV_8UC3) :
                        (f32 ? CV_32FC4 : CV_8UC4);
    return type;
}

extern "C" {
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