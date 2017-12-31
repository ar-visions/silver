
#ifndef INCLUDE_STB_IMAGE_WRITE_H
#define INCLUDE_STB_IMAGE_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

#define STBIWDEF extern

#ifndef STBI_WRITE_NO_STDIO
STBIWDEF int stbi_write_png(char const *filename, int w, int h, int comp, const void  *data, int stride_in_bytes);
STBIWDEF int stbi_write_jpg(char const *filename, int x, int y, int comp, const void  *data, int quality);
#endif

typedef void stbi_write_func(void *context, void *data, int size);

STBIWDEF int stbi_write_png_to_func(stbi_write_func *func, void *context, int w, int h, int comp, const void  *data, int stride_in_bytes);
STBIWDEF int stbi_write_jpg_to_func(stbi_write_func *func, void *context, int x, int y, int comp, const void  *data, int quality);

#ifdef __cplusplus
}
#endif

#endif//INCLUDE_STB_IMAGE_WRITE_H