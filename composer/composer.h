#ifndef COMPOSER_H
#define COMPOSER_H
#include <stdint.h>

// an animated gif, every frame composed to full-size rgba8
typedef struct GifAnim GifAnim;
GifAnim*       gif_open   (const char* path);
void           gif_free   (GifAnim* g);
int            gif_width  (GifAnim* g);
int            gif_height (GifAnim* g);
int            gif_count  (GifAnim* g);
// straight rgba8, width x height
const uint8_t* gif_frame  (GifAnim* g, int i);
// how long frame i shows, milliseconds
int            gif_delay  (GifAnim* g, int i);
#endif
