#include <simple>

layout(location = 0) in  vec3 pos;
layout(location = 1) in  vec2 uv;

layout(location = 0) out vec2 v_uv;

void main() {
    v_uv           = uv;
    gl_Position    = vec4(pos.x, pos.y, 0.0, 1.0);
}
