#include <conv>

layout(location = 0) in  vec3 in_pos;
layout(location = 1) in  vec2 in_uv;

layout(location = 0) out vec3 dir;

void main() {
    vec3 direction = in_pos;
    dir = (conv.env * vec4(direction, 1.0)).xyz;
    gl_Position  = conv.proj * conv.view * vec4(in_pos, 1.0);
}
