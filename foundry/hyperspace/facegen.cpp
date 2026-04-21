#include <ux/app.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/random.hpp>
//#include <glm/gtx/random.hpp>

using namespace ion;

namespace ion {

struct Light {
    vec4f pos;
    vec4f color;
};

struct HumanVertex {
    vec3f  pos;
    vec3f  normal;
    vec2f  uv0;
    vec2f  uv1;
    vec4f  tangent;
    glm::ivec4 joints; /// needs to be an array thats aligned with the loading
    vec4f  weights;

    properties meta() const {
        return {
            prop { "POSITION",      pos     },
            prop { "NORMAL",        normal  },
            
            prop { "TEXCOORD_0",    uv0     },
            prop { "TEXCOORD_1",    uv1     },
            prop { "TANGENT",       tangent },
            prop { "JOINTS_0",      joints  },
            prop { "WEIGHTS_0",     weights }
        };
    }
};

struct UniformBufferObject;

struct Labels:mx {
    /// data protected by NAN.
    struct M {
        float   x = NAN,  y = NAN,  z = NAN;
        float  qx = NAN, qy = NAN, qz = NAN, qw = NAN;
        float fov = NAN;

        properties meta() {
            return {
                { "x",     x },
                { "y",     y },
                { "z",     z },
                { "qx",   qx },
                { "qy",   qy },
                { "qz",   qz },
                { "qw",   qw },
                { "fov", fov }
            };
        }
        /// NAN check helps to keep it real when used with bool operator
        operator bool() {
            return !std::isnan(x)  && !std::isnan(y)  && !std::isnan(z)  &&
                   !std::isnan(qx) && !std::isnan(qy) && !std::isnan(qz) && !std::isnan(qw);
        }
    };

    mx_basic(Labels);
    Labels(null_t):Labels() { }

    operator bool() {
        return *data;
    }
};

struct FaceGen:mx {
    struct M {
        Vulkan          vk { 1, 0 };        /// this lazy loads 1.0 when GPU performs that action [singleton data]
        vec2i           sz { 256, 256 };    /// store current window size
        Window          gpu;                /// GPU class, responsible for holding onto GPU, Surface and GLFWwindow
        Device          device;             /// Device created with GPU
        Pipes           pipes;              /// pipeline for single object scene
        bool            design = true;      /// design mode
        Labels          labels = null;
        path            output_dir { "gen" };

        static void resized(vec2i &sz, M* app) {
            app->sz = sz;
            app->device->framebufferResized = true;
        }

        void init() {
            gpu      = Window::select(sz, ResizeFn(resized), this);
            device   = Device::create(gpu);

            output_dir.make_dir();
            pipes    = Pipes(
                device, "human", {
                Graphics { "Body", typeof(UniformBufferObject), typeof(HumanVertex), "human",
                    [&](mx &verts, mx &indices, Array<image>& asset_images) { }
                } /// will want to have L and R eyes here
            });
        }

        void run() {
            str odir = output_dir.cs();
            output_dir.make_dir();
            Array<Pipes> a_pipes({ pipes });

            while (!glfwWindowShouldClose(gpu->window)) {
                glfwPollEvents();
                device->mtx.lock();
                device->drawFrame(a_pipes);
                vkDeviceWaitIdle(device);
                
                if (labels) {
                    image img      = device->screenshot();
                    assert(img);
                    
                    str  base      = fmt { "facegen-{0}",  { str::rand(12, 'a', 'z') }};
                    path rel_png   = fmt { "{0}.png",      { base }};
                    path path_png  = fmt { "{1}/{0}.png",  { base, odir }};
                    path path_json = fmt { "{1}/{0}.json", { base, odir }};

                    if (path_png.exists() || path_json.exists())
                        continue;
                    
                    var     annots = map {
                        { "labels", labels  },
                        { "source", rel_png }
                    };
                    assert(path_json.write(annots));
                    assert(img.save(path_png));
                    labels = null;
                }
                device->mtx.unlock();
            }
            vkDeviceWaitIdle(device);
        }
    };
    
    mx_basic(FaceGen);

    operator int() {
        try {
            for (Pipeline &pipeline: data->pipes->pipelines)
                pipeline->user = mem->hold();
            data->run();
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
};

glm::quat quaternion_rotate(vec3f v, float rads) {
    return glm::angleAxis(rads, glm::normalize(v));
}

glm::quat rand_quaternion() {
    vec3f rv(
        glm::linearRand(-1.0f, 1.0f),
        glm::linearRand(-1.0f, 1.0f),
        glm::linearRand(-1.0f, 1.0f)
    );
    float angle = glm::linearRand(0.0f, glm::pi<float>());
    return glm::angleAxis(angle, glm::normalize(rv));
}

/// get gltf model output running nominal; there seems to be a skew in the coordinates so it may be being misread
/// uniform has an update method with a pipeline arg
#define MAX_JOINTS 512
#define MAX_LIGHTS 3
struct UniformBufferObject {
    alignas(16) m44f       model;
    alignas(16) m44f       view;
    alignas(16) m44f       proj;
    alignas(16) vec4f  eye;
    alignas(16) m44f       joints[MAX_JOINTS];
    alignas(16) Light      lights[MAX_LIGHTS];

    void process(Pipeline pipeline) { /// memory* -> Pipeline conversion implicit from the function in static
        VkExtent2D &ext = pipeline->device->swapChainExtent;
        FaceGen    fgen = pipeline->user.hold();
        bool     design = fgen->design;

        eye = vec4f(vec3f(0.0f, 0.0f, 0.0f), 0.0f); /// these must be padded in general
        
        //image img = path { "textures/rubiks.color2.png" };
        //pipeline->textures[Asset::color].update(img); /// updating in here is possible because the next call is to check for updates to descriptor

        static bool did_perlin = false;
        if (!did_perlin) {
            float scales[3] = { 256, 256, 256 };
            image img = simplex_equirect_normal(1, 1024, 512, 15.0f, scales);
            img.save("noise_map.png");
            did_perlin = true;
        }

        /// deviate more on some features than others.  cheeks for example dont deviate more than noses in their protrusions lol
        /// deviate on how far eyes are outset and inset, with the thickness of the skin around the eyes taken into account
        /// wrinkles should be generated from other images, masked with perlin against a wrinkle mask to create lots of potential
        /// hair is to be overlaid after the fact
        /// 

        float z_clip = 0.0575f / 2.0f * sin(radians(45.0f));
        float z_far  = 10.0f;
        float z_max_train = 1.0f;
        proj = glm::perspective(
            glm::radians(70.0f),
            ext.width / (float) ext.height,
            z_clip, z_far); /// radius of 45 degrees * object size / 2 = near, 10m far (clip only)
        proj[1][1] *= -1;

        //pipeline->textures[Asset::color].update(img)
        view = glm::lookAt(
            vec3f(eye),
            vec3f(0.0f, 0.0f, 1.0f),
            vec3f(0.0f, 1.0f, 0.0f));
        
        float px = design ? 0.00f           : rand::uniform( 0.0f, 0.0f);
        float py = design ? 0.00f           : rand::uniform( 0.0f, 0.0f);
        float pz = design ? (z_clip * 4.0f) : rand::uniform(z_clip / z_far * 2.0f, z_max_train / z_far); /// train to 1 meters out.  thats a very far away cube from the camera

        vec3f face_center = vec3f(px, py, pz);
        m44f      position    = glm::translate(m44f(1.0f), face_center);

        if (design) {
            static float r = 0.0f;
            static const float rads = M_PI * 2.0f;
            vec3f v  = vec3f(0.0f, 1.0f, 0.0f);
            glm::quat qt = quaternion_rotate(v, r * rads);
            r += rads * 0.00002;
            if (r > rads)
                r -= rads;
            model = position * glm::toMat4(qt);
        } else {
            /// vary model rotation
            glm::quat qt = rand_quaternion();
            model = position * glm::toMat4(qt);

            /// set all fields in Labels
            fgen->labels = Labels::M {
                .x   = face_center.x,
                .y   = face_center.y,
                .z   = face_center.z,
                .qx  = qt.x,
                .qy  = qt.y,
                .qz  = qt.z,
                .qw  = qt.w,
                .fov = 70.0f / 90.0f // normalize by 90
            };
        }

        float cube_rads = 0.0575f * 5;
        m44f      VP     = proj * view;
        vec4f left   = glm::normalize(glm::row(VP, 3) + glm::row(VP, 0));
        vec4f right  = glm::normalize(glm::row(VP, 3) - glm::row(VP, 0));
        vec4f bottom = glm::normalize(glm::row(VP, 3) + glm::row(VP, 1));
        vec4f top    = glm::normalize(glm::row(VP, 3) - glm::row(VP, 1));
        vec4f vnear  = glm::normalize(glm::row(VP, 3) + glm::row(VP, 2));
        vec4f vfar   = glm::normalize(glm::row(VP, 3) - glm::row(VP, 2));

        lights[0] = { vec4f(vec3f(2.0f, 0.0f,  4.0f),  25.0f), vec4f(1.0, 1.0, 1.0, 1.0) };
        lights[1] = { vec4f(vec3f(0.0f, 0.0f, -5.0f), 100.0f), vec4f(1.0, 1.0, 1.0, 1.0) };
        lights[2] = { vec4f(vec3f(0.0f, 0.0f, -5.0f), 100.0f), vec4f(1.0, 1.0, 1.0, 1.0) };
    }
};
}

int main() {
    return FaceGen();
}
