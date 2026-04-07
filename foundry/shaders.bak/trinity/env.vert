#include <env>

layout(location = 0) in  vec3 in_pos;
layout(location = 1) in  vec2 in_uv;

layout(location = 0) out vec2 uv;
layout(location = 1) out vec3 dir;

void main() {
    uv          = in_uv;
    dir         = in_pos;
    gl_Position = e.proj * e.view * vec4(in_pos, 1.0);
}