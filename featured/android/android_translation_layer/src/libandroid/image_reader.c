#include <stdint.h>
#include <stddef.h>

/* Android's AImageReader (camera and video frames): no image source
 * here, so a reader is never made and callers take their own fallback */

typedef void AImageReader;
typedef void AImage;
typedef void AHardwareBuffer;
typedef void ANativeWindow;

#define AMEDIA_OK 0
#define AMEDIA_ERROR_UNSUPPORTED -10000
#define AMEDIA_ERROR_INVALID_PARAMETER -10003

int AImageReader_newWithUsage(int32_t width, int32_t height, int32_t format, uint64_t usage, int32_t max_images, AImageReader **reader)
{
	if (reader)
		*reader = NULL;
	return AMEDIA_ERROR_UNSUPPORTED;
}

int AImageReader_new(int32_t width, int32_t height, int32_t format, int32_t max_images, AImageReader **reader)
{
	return AImageReader_newWithUsage(width, height, format, 0, max_images, reader);
}

void AImageReader_delete(AImageReader *reader)
{
}

int AImageReader_getWindow(AImageReader *reader, ANativeWindow **window)
{
	if (window)
		*window = NULL;
	return AMEDIA_ERROR_INVALID_PARAMETER;
}

int AImageReader_acquireNextImage(AImageReader *reader, AImage **image)
{
	if (image)
		*image = NULL;
	return AMEDIA_ERROR_INVALID_PARAMETER;
}

int AImageReader_acquireLatestImage(AImageReader *reader, AImage **image)
{
	return AImageReader_acquireNextImage(reader, image);
}

void AImage_delete(AImage *image)
{
}

int AImage_getTimestamp(const AImage *image, int64_t *timestamp)
{
	if (timestamp)
		*timestamp = 0;
	return AMEDIA_ERROR_INVALID_PARAMETER;
}

int AImage_getHardwareBuffer(const AImage *image, AHardwareBuffer **buffer)
{
	if (buffer)
		*buffer = NULL;
	return AMEDIA_ERROR_INVALID_PARAMETER;
}

