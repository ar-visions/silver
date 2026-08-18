// macOS camera backend (AVFoundation). PURE Cocoa/ObjC++ — this TU includes no
// silver/Au headers on purpose: mixing them with Cocoa pulls in Carbon/QuickDraw
// type collisions (Style/Pattern/Polygon) and the Au construction macros rely on
// `typeid`, which the C++ path #undefs. So the bridge is plain C primitives and
// all Au/Camera work happens on the silver side (Camera.ag).

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <dispatch/dispatch.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <vector>

struct TrinityCameraBackend;

@interface TrinityCameraSink : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate> {
@public
    TrinityCameraBackend* backend;
}

- (id)initWithBackend:(TrinityCameraBackend*)value;

@end

struct TrinityCameraBackend {
    AVCaptureSession*         session;
    AVCaptureDeviceInput*     input;
    AVCaptureVideoDataOutput* output;
    AVCaptureDevice*          device;
    dispatch_queue_t          queue;
    TrinityCameraSink*        sink;
    std::mutex                mutex;
    std::vector<uint8_t>      frame;
    uint32_t                  width;
    uint32_t                  height;
    uint32_t                  stride;
    uint64_t                  produced;
    uint64_t                  consumed;

    TrinityCameraBackend()
        : session(nil),
          input(nil),
          output(nil),
          device(nil),
          queue(nullptr),
          sink(nil),
          width(0),
          height(0),
          stride(0),
          produced(0),
          consumed(0) {}
};

static NSArray<AVCaptureDevice*>* trinity_camera_list(void) {
    NSArray<AVCaptureDevice*>* devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    if (!devices)
        return @[];
    return devices;
}

static bool trinity_camera_is_default(const char* name) {
    return !name || !name[0] || !strcmp(name, "default") || !strncmp(name, "/dev/", 5);
}

static NSString* trinity_camera_preset(int req_w, int req_h) {
    if (req_w >= 1920 || req_h >= 1080)
        return AVCaptureSessionPreset1920x1080;
    if (req_w >= 1280 || req_h >= 720)
        return AVCaptureSessionPreset1280x720;
    if (req_w >= 640 || req_h >= 480)
        return AVCaptureSessionPreset640x480;
    return AVCaptureSessionPresetHigh;
}

static AVCaptureDevice* trinity_select_camera(const char* requested) {
    NSArray<AVCaptureDevice*>* devices = trinity_camera_list();
    if (![devices count])
        return nil;

    if (trinity_camera_is_default(requested))
        return [devices objectAtIndex:0];

    NSString* wanted_name = [NSString stringWithUTF8String:requested];
    for (AVCaptureDevice* device in devices) {
        if ([[device localizedName] isEqualToString:wanted_name] ||
            [[device uniqueID] isEqualToString:wanted_name]) {
            return device;
        }
    }

    return [devices objectAtIndex:0];
}

static void trinity_store_frame(TrinityCameraBackend* backend, CVImageBufferRef image) {
    if (!backend || !image)
        return;

    CVPixelBufferRef pixel = (CVPixelBufferRef)image;
    CVPixelBufferLockBaseAddress(pixel, kCVPixelBufferLock_ReadOnly);

    size_t   plane_count = CVPixelBufferGetPlaneCount(pixel);
    size_t   width       = plane_count ? CVPixelBufferGetWidthOfPlane(pixel, 0)      : CVPixelBufferGetWidth(pixel);
    size_t   height      = plane_count ? CVPixelBufferGetHeightOfPlane(pixel, 0)     : CVPixelBufferGetHeight(pixel);
    size_t   stride      = plane_count ? CVPixelBufferGetBytesPerRowOfPlane(pixel, 0) : CVPixelBufferGetBytesPerRow(pixel);
    uint8_t* base        = plane_count ? (uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pixel, 0)
                                       : (uint8_t*)CVPixelBufferGetBaseAddress(pixel);

    if (base && width && height) {
        std::lock_guard<std::mutex> lock(backend->mutex);
        backend->frame.resize(width * height);
        for (size_t y = 0; y < height; ++y)
            memcpy(backend->frame.data() + y * width, base + y * stride, width);
        backend->width    = (uint32_t)width;
        backend->height   = (uint32_t)height;
        backend->stride   = (uint32_t)width;
        backend->produced += 1;
    }

    CVPixelBufferUnlockBaseAddress(pixel, kCVPixelBufferLock_ReadOnly);
}

@implementation TrinityCameraSink

- (id)initWithBackend:(TrinityCameraBackend*)value {
    self = [super init];
    if (self)
        backend = value;
    return self;
}

- (void)captureOutput:(AVCaptureOutput*)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection*)connection {
    (void)output;
    (void)connection;
    @autoreleasepool {
        CVImageBufferRef image = CMSampleBufferGetImageBuffer(sampleBuffer);
        trinity_store_frame(backend, image);
    }
}

@end

extern "C" {

// number of available video capture devices
int trinity_camera_count(void) {
    @autoreleasepool {
        return (int)[trinity_camera_list() count];
    }
}

// localized name of device `index`; returns a pointer valid until the next call
// (empty string if out of range). copied into a static buffer so the silver side
// can build its own string from it.
const char* trinity_camera_name(int index) {
    static char buf[256];
    @autoreleasepool {
        buf[0] = 0;
        NSArray<AVCaptureDevice*>* devices = trinity_camera_list();
        if (index < 0 || index >= (int)[devices count])
            return buf;
        const char* name = [[[devices objectAtIndex:index] localizedName] UTF8String];
        if (!name) name = "";
        size_t n = strlen(name);
        if (n > sizeof(buf) - 1) n = sizeof(buf) - 1;
        memcpy(buf, name, n);
        buf[n] = 0;
        return buf;
    }
}

// open the requested camera; on success returns an opaque backend handle and
// reports the negotiated width/height/row-pitch. returns null on failure.
void* trinity_camera_open(const char* device, int req_w, int req_h,
                          int* out_w, int* out_h, unsigned* out_pitch) {
    @autoreleasepool {
        TrinityCameraBackend* backend = new TrinityCameraBackend();
        AVCaptureDevice* dev = trinity_select_camera(device);
        if (!dev) {
            delete backend;
            return nullptr;
        }

        backend->device = [dev retain];
        backend->queue  = dispatch_queue_create("silver.trinity.camera", DISPATCH_QUEUE_SERIAL);
        backend->sink   = [[TrinityCameraSink alloc] initWithBackend:backend];

        NSError* error = nil;
        backend->input = [[AVCaptureDeviceInput alloc] initWithDevice:dev error:&error];
        if (!backend->input || error) {
            [backend->sink release];
            [backend->device release];
#if !OS_OBJECT_USE_OBJC
            if (backend->queue)
                dispatch_release(backend->queue);
#endif
            delete backend;
            return nullptr;
        }

        backend->output = [[AVCaptureVideoDataOutput alloc] init];
        backend->output.alwaysDiscardsLateVideoFrames = YES;
        backend->output.videoSettings = @{
            (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
        };
        [backend->output setSampleBufferDelegate:backend->sink queue:backend->queue];

        backend->session = [[AVCaptureSession alloc] init];
        NSString* preset = trinity_camera_preset(req_w, req_h);
        if ([backend->session canSetSessionPreset:preset])
            backend->session.sessionPreset = preset;

        [backend->session beginConfiguration];
        if ([backend->session canAddInput:backend->input])
            [backend->session addInput:backend->input];
        if ([backend->session canAddOutput:backend->output])
            [backend->session addOutput:backend->output];
        [backend->session commitConfiguration];

        [backend->session startRunning];

        for (int i = 0; i < 200; ++i) {
            {
                std::lock_guard<std::mutex> lock(backend->mutex);
                if (backend->produced) {
                    if (out_w)     *out_w     = (int)backend->width;
                    if (out_h)     *out_h     = (int)backend->height;
                    if (out_pitch) *out_pitch = backend->stride;
                    return backend;
                }
            }
            usleep(10000);
        }

        // no frame yet — still return the handle; caller falls back on its
        // requested dimensions.
        if (out_w)     *out_w     = req_w;
        if (out_h)     *out_h     = req_h;
        if (out_pitch) *out_pitch = (unsigned)req_w;
        return backend;
    }
}

// copy the latest frame; returns 1 if a fresh frame was copied, 0 otherwise.
int trinity_camera_copy(void* handle, unsigned char* dst, unsigned dst_bytes) {
    @autoreleasepool {
        TrinityCameraBackend* backend = (TrinityCameraBackend*)handle;
        if (!backend || !dst)
            return 0;

        std::lock_guard<std::mutex> lock(backend->mutex);
        if (!backend->produced || backend->consumed == backend->produced || backend->frame.empty())
            return 0;

        size_t bytes = std::min<size_t>(dst_bytes, backend->frame.size());
        memcpy(dst, backend->frame.data(), bytes);
        backend->consumed = backend->produced;
        return 1;
    }
}

void trinity_camera_close(void* handle) {
    @autoreleasepool {
        TrinityCameraBackend* backend = (TrinityCameraBackend*)handle;
        if (!backend)
            return;

        if (backend->output)
            [backend->output setSampleBufferDelegate:nil queue:nil];
        if (backend->session) {
            [backend->session stopRunning];
            if (backend->input)
                [backend->session removeInput:backend->input];
            if (backend->output)
                [backend->session removeOutput:backend->output];
        }

        [backend->sink release];
        [backend->output release];
        [backend->input release];
        [backend->session release];
        [backend->device release];
#if !OS_OBJECT_USE_OBJC
        if (backend->queue)
            dispatch_release(backend->queue);
#endif
        delete backend;
    }
}

}
