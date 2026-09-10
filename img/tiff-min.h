// the few libtiff entry points the img module reads a TIFF with; the system
// ships libtiff.so without its headers, so they are declared here
#pragma once
#include <stdint.h>
typedef struct tiff TIFF;
TIFF* TIFFOpen(const char* name, const char* mode);
void  TIFFClose(TIFF* tif);
int   TIFFGetField(TIFF* tif, uint32_t tag, ...);
int   TIFFReadRGBAImageOriented(TIFF* tif, uint32_t w, uint32_t h, uint32_t* raster, int orientation, int stop);
#define TIFFTAG_IMAGEWIDTH   256
#define TIFFTAG_IMAGELENGTH  257
#define ORIENTATION_TOPLEFT  1
