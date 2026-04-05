#include <audrey>

layout(location = 0) in  vec3 pos;
layout(location = 1) in  vec2 uv;

layout(location = 0) out vec2 v_uv;

void main() {
    mat4 m         = au.proj * au.view * au.model;
    v_uv           = uv;
    gl_Position    = m * vec4(pos, 1.0);
}
