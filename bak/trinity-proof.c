#include "dawn/webgpu.h"
#include "dawn/dawn_proc.h"
#include "glfw/glfw3.h"
#include "a_type.h"

typedef struct {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    GLFWwindow* window;
    WGPUSurface surface;
    WGPUSwapChain swapchain;
} Dawn;

void Dawn_process_events(Dawn* dawn) {
    // Handle events specific to WGPU, if any
    wgpuInstanceProcessEvents(dawn->instance);
}

float Dawn_get_dpi() {
    glfwInit();
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    float xscale, yscale;
    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    return xscale;
}

void Dawn_initialize(Dawn* dawn) {
    dawn->instance = wgpuCreateInstance(NULL);

    WGPURequestAdapterOptions adapterOptions = {};
    adapterOptions.powerPreference = WGPUPowerPreference_HighPerformance;

    // Request Adapter
    wgpuInstanceRequestAdapter(dawn->instance, &adapterOptions, [](WGPURequestAdapterStatus status, WGPUAdapter adapter, const char* message, void* userdata) {
        Dawn* dawn = (Dawn*)userdata;
        if (status == WGPURequestAdapterStatus_Success) {
            dawn->adapter = adapter;
        } else {
            printf("Failed to get WGPU adapter: %s\n", message);
        }
    }, dawn);

    // Create Device
    dawn->device = wgpuAdapterCreateDevice(dawn->adapter, NULL);
    dawn->queue = wgpuDeviceGetQueue(dawn->device);
}

2. Device and Buffer Creation:

    Adapt the buffer creation functions and other device-related functions to C.

c

typedef struct {
    WGPUBuffer buffer;
    size_t size;
    WGPUBufferUsage usage;
} ABuffer;

ABuffer Device_create_buffer(Dawn* dawn, void* data, size_t size, WGPUBufferUsage usage) {
    WGPUBufferDescriptor desc = {
        .usage = usage | WGPUBufferUsage_CopyDst,
        .size = size
    };
    ABuffer aBuffer = {
        .buffer = wgpuDeviceCreateBuffer(dawn->device, &desc),
        .size = size,
        .usage = usage
    };

    wgpuQueueWriteBuffer(dawn->queue, aBuffer.buffer, 0, data, size);
    return aBuffer;
}



typedef struct {
    WGPUTexture texture;
    WGPUTextureView view;
    WGPUTextureFormat format;
    WGPUTextureUsage usage;
    int width, height;
} ATexture;

ATexture Device_create_texture(Dawn* dawn, int width, int height, WGPUTextureFormat format, WGPUTextureUsage usage) {
    WGPUTextureDescriptor textureDesc = {
        .size = {
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .depthOrArrayLayers = 1,
        },
        .format = format,
        .usage = usage,
        .dimension = WGPUTextureDimension_2D,
        .mipLevelCount = 1,
        .sampleCount = 1,
    };

    ATexture aTexture = {
        .texture = wgpuDeviceCreateTexture(dawn->device, &textureDesc),
        .view = NULL,
        .format = format,
        .usage = usage,
        .width = width,
        .height = height
    };

    if (usage & (WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment)) {
        aTexture.view = wgpuTextureCreateView(aTexture.texture, NULL);
    }

    return aTexture;
}




typedef struct {
    WGPURenderPipeline pipeline;
    WGPUPipelineLayout pipelineLayout;
    WGPUBindGroupLayout bindGroupLayout;
    WGPUShaderModule shaderModule;
    WGPUBuffer vertexBuffer;
} APipeline;

APipeline Pipeline_create(Dawn* dawn, const char* shaderCode, size_t vertexSize) {
    APipeline aPipeline = {};

    WGPUShaderModuleDescriptor shaderDesc = {
        .code = shaderCode
    };
    aPipeline.shaderModule = wgpuDeviceCreateShaderModule(dawn->device, &shaderDesc);

    WGPUPipelineLayoutDescriptor layoutDesc = {
        .bindGroupLayoutCount = 1,
        .bindGroupLayouts = &aPipeline.bindGroupLayout,
    };
    aPipeline.pipelineLayout = wgpuDeviceCreatePipelineLayout(dawn->device, &layoutDesc);

    WGPURenderPipelineDescriptor pipelineDesc = {
        .layout = aPipeline.pipelineLayout,
        .vertex = {
            .module = aPipeline.shaderModule,
            .entryPoint = "vertex_main",
            .bufferCount = 1,
            .buffers = &vertexBufferLayout,
        },
        .fragment = {
            .module = aPipeline.shaderModule,
            .entryPoint = "fragment_main",
            .targetCount = 1,
            .targets = &colorTargetState,
        },
        .primitive = {
            .topology = WGPUPrimitiveTopology_TriangleList,
            .cullMode = WGPUCullMode_None,
        },
        .depthStencil = {
            .format = WGPUTextureFormat_Depth24PlusStencil8,
            .depthWriteEnabled = true,
            .depthCompare = WGPUCompareFunction_Less,
        },
        .multisample = {
            .count = 1,
        },
    };

    aPipeline.pipeline = wgpuDeviceCreateRenderPipeline(dawn->device, &pipelineDesc);

    return aPipeline;
}




typedef struct {
    Dawn* dawn;
    GLFWwindow* window;
    int width, height;
    ATexture depthStencil;
    ATexture colorTexture;
    WGPUSwapChain swapChain;
} AWindow;

void Window_initialize(AWindow* window, Dawn* dawn, int width, int height) {
    window->dawn = dawn;
    window->width = width;
    window->height = height;

    window->window = glfwCreateWindow(width, height, "WGPU Window", NULL, NULL);
    glfwSetWindowUserPointer(window->window, window);

    window->surface = wgpuCreateSurfaceForWindow(dawn->instance, window->window);

    WGPUSwapChainDescriptor swapChainDesc = {
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = WGPUTextureFormat_BGRA8Unorm,
        .width = width,
        .height = height,
        .presentMode = WGPUPresentMode_Mailbox,
    };
    window->swapChain = wgpuDeviceCreateSwapChain(dawn->device, window->surface, &swapChainDesc);

    window->depthStencil = Device_create_texture(dawn, width, height, WGPUTextureFormat_Depth24PlusStencil8, WGPUTextureUsage_RenderAttachment);
    window->colorTexture = Device_create_texture(dawn, width, height, WGPUTextureFormat_BGRA8Unorm, WGPUTextureUsage_RenderAttachment);
}

void Window_render(AWindow* window, APipeline* pipeline) {
    while (!glfwWindowShouldClose(window->window)) {
        glfwPollEvents();

        WGPUTextureView swapChainView = wgpuSwapChainGetCurrentTextureView(window->swapChain);

        WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(window->dawn->device, NULL);

        WGPURenderPassColorAttachment colorAttachment = {
            .view = swapChainView,
            .resolveTarget = NULL,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = {0.0, 0.0, 0.0, 1.0},
        };

        WGPURenderPassDescriptor renderPassDesc = {
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
            .depthStencilAttachment = &(WGPURenderPassDepthStencilAttachment){
                .view = window->depthStencil.view,
                .depthLoadOp = WGPULoadOp_Clear,
                .depthStoreOp = WGPUStoreOp_Store,
                .depthClearValue = 1.0f,
                .stencilLoadOp = WGPULoadOp_Clear,
                .stencilStoreOp = WGPUStoreOp_Store,
                .stencilClearValue = 0,
            },
        };

        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
        wgpuRenderPassEncoderSetPipeline(pass, pipeline->pipeline);
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, pipeline->vertexBuffer, 0, pipeline->vertexBuffer.size);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass);

        WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, NULL);
        wgpuQueueSubmit(window->dawn->queue, 1, &commands);

        wgpuSwapChainPresent(window->swapChain);
    }
}

int main() {
    Dawn dawn;
    Dawn_initialize(&dawn);

    AWindow window;
    Window_initialize(&window, &dawn, 800, 600);

    const char* shaderCode = "..."; // Replace with actual shader code
    APipeline pipeline = Pipeline_create(&dawn, shaderCode, sizeof(Vertex));

    Window_render(&window, &pipeline);

    glfwDestroyWindow(window.window);
    glfwTerminate();
    return 0;
}



