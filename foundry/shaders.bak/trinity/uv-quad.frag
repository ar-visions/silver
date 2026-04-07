#include <uv-quad>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    fragColor = vec4(texture(tx_color, v_uv).xyz, 1.0);
}
