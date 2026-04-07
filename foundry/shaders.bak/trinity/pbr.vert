#include <pbr>

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec4 color_0;
layout(location = 4) in vec4 color_1;

layout(location = 1) out vec3 v_world_normal;
layout(location = 2) out vec3 v_world_pos;
layout(location = 3) out vec3 v_view_pos;
layout(location = 4) out vec2 v_uv;

void main() {
    mat4 m         = world.proj * world.view * world.model;
    mat4 wm        = world.model;
    vec4 world_pos = wm * vec4(pos, 1.0);
    v_world_normal = mat3(transpose(inverse(world.model))) * normal;
    v_world_pos    = world_pos.xyz;
    v_view_pos     = inverse(world.view)[3].xyz;
    v_uv           = uv;
    gl_Position    = m * vec4(pos, 1.0);
}
