#include <import>

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <dispatch/dispatch.h>
#include <unistd.h>

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
    std::vector<u8>           frame;
    u32                       width;
    u32                       height;
    u32                       stride;
    u64                       produced;
    u64                       consumed;

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

static NSString* trinity_camera_preset(Camera a) {
    if (a->width >= 1920 || a->height >= 1080)
        return AVCaptureSessionPreset1920x1080;
    if (a->width >= 1280 || a->height >= 720)
        return AVCaptureSessionPreset1280x720;
    if (a->width >= 640 || a->height >= 480)
        return AVCaptureSessionPreset640x480;
    return AVCaptureSessionPresetHigh;
}

static AVCaptureDevice* trinity_select_camera(string requested) {
    NSArray<AVCaptureDevice*>* devices = trinity_camera_list();
    if (![devices count])
        return nil;

    const char* wanted = requested ? requested->chars : nullptr;
    if (trinity_camera_is_default(wanted))
        return [devices objectAtIndex:0];

    NSString* wanted_name = [NSString stringWithUTF8String:wanted];
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

    size_t plane_count = CVPixelBufferGetPlaneCount(pixel);
    size_t width       = plane_count ? CVPixelBufferGetWidthOfPlane(pixel, 0)      : CVPixelBufferGetWidth(pixel);
    size_t height      = plane_count ? CVPixelBufferGetHeightOfPlane(pixel, 0)     : CVPixelBufferGetHeight(pixel);
    size_t stride      = plane_count ? CVPixelBufferGetBytesPerRowOfPlane(pixel, 0) : CVPixelBufferGetBytesPerRow(pixel);
    u8*    base        = plane_count ? (u8*)CVPixelBufferGetBaseAddressOfPlane(pixel, 0)
                                     : (u8*)CVPixelBufferGetBaseAddress(pixel);

    if (base && width && height) {
        std::lock_guard<std::mutex> lock(backend->mutex);
        backend->frame.resize(width * height);
        for (size_t y = 0; y < height; ++y)
            memcpy(backend->frame.data() + y * width, base + y * stride, width);
        backend->width    = (u32)width;
        backend->height   = (u32)height;
        backend->stride   = (u32)width;
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

array camera_devices(void) {
    @autoreleasepool {
        NSArray<AVCaptureDevice*>* devices = trinity_camera_list();
        array result = array(alloc, [devices count] ? (i32)[devices count] : 1);
        for (AVCaptureDevice* device in devices) {
            const char* name = [[device localizedName] UTF8String];
            push(result, (Au)string(name ? name : ""));
        }
        return result;
    }
}

void Camera_backend_init(Camera a) {
    @autoreleasepool {
        TrinityCameraBackend* backend = new TrinityCameraBackend();
        AVCaptureDevice* device = trinity_select_camera(a->device);
        if (!device) {
            delete backend;
            return;
        }

        backend->device = [device retain];
        backend->queue  = dispatch_queue_create("silver.trinity.camera", DISPATCH_QUEUE_SERIAL);
        backend->sink   = [[TrinityCameraSink alloc] initWithBackend:backend];

        NSError* error = nil;
        backend->input = [[AVCaptureDeviceInput alloc] initWithDevice:device error:&error];
        if (!backend->input || error) {
            [backend->sink release];
            [backend->device release];
#if !OS_OBJECT_USE_OBJC
            if (backend->queue)
                dispatch_release(backend->queue);
#endif
            delete backend;
            return;
        }

        backend->output = [[AVCaptureVideoDataOutput alloc] init];
        backend->output.alwaysDiscardsLateVideoFrames = YES;
        backend->output.videoSettings = @{
            (id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)
        };
        [backend->output setSampleBufferDelegate:backend->sink queue:backend->queue];

        backend->session = [[AVCaptureSession alloc] init];
        NSString* preset = trinity_camera_preset(a);
        if ([backend->session canSetSessionPreset:preset])
            backend->session.sessionPreset = preset;

        [backend->session beginConfiguration];
        if ([backend->session canAddInput:backend->input])
            [backend->session addInput:backend->input];
        if ([backend->session canAddOutput:backend->output])
            [backend->session addOutput:backend->output];
        [backend->session commitConfiguration];

        [backend->session startRunning];
        a->backend = (handle)backend;

        for (int i = 0; i < 200; ++i) {
            {
                std::lock_guard<std::mutex> lock(backend->mutex);
                if (backend->produced) {
                    a->width     = (i32)backend->width;
                    a->height    = (i32)backend->height;
                    a->row_pitch = backend->stride;
                    break;
                }
            }
            usleep(10000);
        }

        if (!a->row_pitch)
            a->row_pitch = (u32)a->width;

        NSString* name = [device localizedName];
        if (name) {
            const char* chars = [name UTF8String];
            if (chars)
                a->device = (string)hold((Au)string(chars));
        }
    }
}

bool Camera_backend_copy(Camera a, u8* dst, u32 dst_bytes) {
    @autoreleasepool {
        TrinityCameraBackend* backend = (TrinityCameraBackend*)a->backend;
        if (!backend || !dst)
            return false;

        std::lock_guard<std::mutex> lock(backend->mutex);
        if (!backend->produced || backend->consumed == backend->produced || backend->frame.empty())
            return false;

        size_t bytes = std::min<size_t>(dst_bytes, backend->frame.size());
        memcpy(dst, backend->frame.data(), bytes);
        backend->consumed = backend->produced;
        return true;
    }
}

void Camera_backend_dealloc(Camera a) {
    @autoreleasepool {
        TrinityCameraBackend* backend = (TrinityCameraBackend*)a->backend;
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
        a->backend = nullptr;
    }
}

}
