#ifndef TRINITY_SCREEN_H
#define TRINITY_SCREEN_H
#include <vulkan/vulkan.h>

// a swapchain on a window of its own that shows one picture, scaled
// to fit (a fullscreen game on the real monitor); its device is the
// app's, its queue taken under the app's queue lock by the caller
typedef struct screen_out screen_out;

screen_out* screen_out_new    (VkPhysicalDevice pd, VkDevice dev, uint32_t family,
                               VkSurfaceKHR surf, int width, int height);
// the next image, the picture blitted in; 1 when there is one to submit
int         screen_out_frame  (screen_out* s, VkImage src, VkImageLayout layout,
                               int src_w, int src_h);
// 0 shown; 1 the window changed size (screen_out_resize next)
int         screen_out_submit (screen_out* s, VkQueue queue);
void        screen_out_resize (screen_out* s, int width, int height);
void        screen_out_free   (screen_out* s);
#endif
