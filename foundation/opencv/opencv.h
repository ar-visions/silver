
#ifdef __cplusplus
extern "C" {
#endif

void opencv_resize_area(uint8_t* src, uint8_t* dst, int f32, int in_w, int in_h, int channels, int out_w, int out_h);
void opencv_gaussian   (uint8_t* src, uint8_t* dst, int f32, int w, int h, int channels, float amount);
void opencv_gaussian_fast(uint8_t* src, uint8_t* dst, int f32, int w, int h, int channels, float amount);
void opencv_blur_equirect(uint8_t* input, int w, int h);

#ifdef __cplusplus
}
#endif