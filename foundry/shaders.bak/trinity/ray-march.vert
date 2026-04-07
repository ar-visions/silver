#include <pbr>

layout(set = 0, binding = 1) uniform Rays {
    mat4 model;
    mat4 view;
    mat4 proj;
    float time;
} r;

layout(location = 0) in vec3 in_pos;

void main() {
    gl_Position  = world.proj * world.view * world.model * vec4(in_pos, 1.0);
}
