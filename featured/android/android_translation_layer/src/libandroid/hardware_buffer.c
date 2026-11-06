#include <jni.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* AHardwareBuffer: a buffer in plain memory, its planes locked as they are */

typedef struct AHardwareBuffer_Desc {
	uint32_t width;
	uint32_t height;
	uint32_t layers;
	uint32_t format;
	uint64_t usage;
	uint32_t stride;
	uint32_t rfu0;
	uint64_t rfu1;
} AHardwareBuffer_Desc;

typedef struct AHardwareBuffer_Plane {
	void *data;
	uint32_t pixelStride;
	uint32_t rowStride;
} AHardwareBuffer_Plane;

typedef struct AHardwareBuffer_Planes {
	uint32_t planeCount;
	AHardwareBuffer_Plane planes[4];
} AHardwareBuffer_Planes;

typedef struct ARect {
	int32_t left;
	int32_t top;
	int32_t right;
	int32_t bottom;
} ARect;

typedef struct AHardwareBuffer {
	AHardwareBuffer_Desc desc;
	int refcount;
	uint32_t planes;
	uint32_t bytes_per_pixel;
	uint8_t *data[3];
	uint32_t row_stride[3];
} AHardwareBuffer;

enum {
	FORMAT_R8G8B8A8_UNORM = 1,
	FORMAT_R8G8B8X8_UNORM = 2,
	FORMAT_R8G8B8_UNORM = 3,
	FORMAT_R5G6B5_UNORM = 4,
	FORMAT_R16G16B16A16_FLOAT = 0x16,
	FORMAT_R10G10B10A2_UNORM = 0x2b,
	FORMAT_BLOB = 0x21,
	FORMAT_Y8Cb8Cr8_420 = 0x23,
	FORMAT_D16_UNORM = 0x30,
	FORMAT_D24_UNORM = 0x31,
	FORMAT_D24_UNORM_S8_UINT = 0x32,
	FORMAT_D32_FLOAT = 0x33,
	FORMAT_D32_FLOAT_S8_UINT = 0x34,
	FORMAT_S8_UINT = 0x35,
	FORMAT_R8_UNORM = 0x38,
};

static uint32_t bytes_per_pixel(uint32_t format)
{
	switch (format) {
	case FORMAT_R8G8B8A8_UNORM:
	case FORMAT_R8G8B8X8_UNORM:
	case FORMAT_R10G10B10A2_UNORM:
	case FORMAT_D24_UNORM:
	case FORMAT_D24_UNORM_S8_UINT:
	case FORMAT_D32_FLOAT:
		return 4;
	case FORMAT_R8G8B8_UNORM:
		return 3;
	case FORMAT_R5G6B5_UNORM:
	case FORMAT_D16_UNORM:
		return 2;
	case FORMAT_R16G16B16A16_FLOAT:
		return 8;
	case FORMAT_D32_FLOAT_S8_UINT:
		return 5;
	case FORMAT_BLOB:
	case FORMAT_S8_UINT:
	case FORMAT_R8_UNORM:
	case FORMAT_Y8Cb8Cr8_420:
		return 1;
	default:
		return 0;
	}
}

int AHardwareBuffer_allocate(const AHardwareBuffer_Desc *desc, AHardwareBuffer **out)
{
	if (!desc || !out || !desc->width || !desc->height || desc->layers != 1)
		return -22; // -EINVAL
	uint32_t bpp = bytes_per_pixel(desc->format);
	if (!bpp)
		return -22;
	AHardwareBuffer *b = calloc(1, sizeof(*b));
	b->desc = *desc;
	b->refcount = 1;
	b->bytes_per_pixel = bpp;
	if (desc->format == FORMAT_Y8Cb8Cr8_420) {
		b->planes = 3;
		b->row_stride[0] = (desc->width + 15) & ~15u;
		b->row_stride[1] = b->row_stride[2] = ((desc->width + 1) / 2 + 15) & ~15u;
		b->data[0] = calloc(b->row_stride[0], desc->height);
		b->data[1] = calloc(b->row_stride[1], (desc->height + 1) / 2);
		b->data[2] = calloc(b->row_stride[2], (desc->height + 1) / 2);
		b->desc.stride = b->row_stride[0];
	} else {
		b->planes = 1;
		uint32_t stride = desc->format == FORMAT_BLOB ? desc->width : ((desc->width * bpp + 15) & ~15u);
		b->row_stride[0] = stride;
		b->data[0] = calloc(stride, desc->height);
		b->desc.stride = stride / bpp;
	}
	*out = b;
	return 0;
}

void AHardwareBuffer_acquire(AHardwareBuffer *b)
{
	if (b)
		__atomic_add_fetch(&b->refcount, 1, __ATOMIC_SEQ_CST);
}

void AHardwareBuffer_release(AHardwareBuffer *b)
{
	if (!b || __atomic_sub_fetch(&b->refcount, 1, __ATOMIC_SEQ_CST))
		return;
	for (uint32_t i = 0; i < b->planes; i++)
		free(b->data[i]);
	free(b);
}

void AHardwareBuffer_describe(const AHardwareBuffer *b, AHardwareBuffer_Desc *desc)
{
	if (!desc)
		return;
	*desc = b ? b->desc : (AHardwareBuffer_Desc){0};
}

int AHardwareBuffer_lock(AHardwareBuffer *b, uint64_t usage, int32_t fence, const ARect *rect, void **out)
{
	if (!b || !out || b->planes != 1)
		return -22;
	*out = b->data[0];
	return 0;
}

int AHardwareBuffer_lockPlanes(AHardwareBuffer *b, uint64_t usage, int32_t fence, const ARect *rect, AHardwareBuffer_Planes *out)
{
	if (!b || !out)
		return -22;
	out->planeCount = b->planes;
	for (uint32_t i = 0; i < b->planes; i++) {
		out->planes[i].data = b->data[i];
		out->planes[i].pixelStride = b->planes == 3 ? 1 : b->bytes_per_pixel;
		out->planes[i].rowStride = b->row_stride[i];
	}
	return 0;
}

int AHardwareBuffer_unlock(AHardwareBuffer *b, int32_t *fence)
{
	if (fence)
		*fence = -1;
	return b ? 0 : -22;
}

int AHardwareBuffer_isSupported(const AHardwareBuffer_Desc *desc)
{
	return desc && bytes_per_pixel(desc->format) != 0;
}

/* no Java HardwareBuffer object here */
jobject AHardwareBuffer_toHardwareBuffer(JNIEnv *env, AHardwareBuffer *b)
{
	return NULL;
}

AHardwareBuffer *AHardwareBuffer_fromHardwareBuffer(JNIEnv *env, jobject hardwareBufferObj)
{
	return NULL;
}
