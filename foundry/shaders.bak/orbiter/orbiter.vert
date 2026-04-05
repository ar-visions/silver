#include <pbr>

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
layout(location = 3) in vec3 tangent;
layout(location = 4) in vec4 color_0;
layout(location = 5) in vec3 color_1;

layout(location = 1) out vec3 v_world_normal;
layout(location = 2) out vec3 v_world_pos;
layout(location = 3) out vec3 v_view_pos;
layout(location = 4) out vec2 v_uv;
layout(location = 5) out vec3 v_color_0;
layout(location = 6) out vec3 v_color_1;

void main() {
    mat4 m         = world.proj * world.view * world.model;
    mat4 wm        = world.model;
    vec4 world_pos = wm * vec4(pos, 1.0);
    v_world_normal = mat3(transpose(inverse(world.model))) * normal;
    v_world_pos    = world_pos.xyz;
    v_view_pos     = inverse(world.view)[3].xyz;
    v_color_0      = color_0.xyz;
    v_color_1      = color_1;
    v_uv           = uv;
    gl_Position    = m * vec4(pos, 1.0);
}
