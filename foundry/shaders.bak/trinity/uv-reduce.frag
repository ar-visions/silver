#include <uv-quad>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    vec2 tsize = textureSize(tx_color, 0);
    vec2 texel = 1.0 / vec2(tsize);
    vec4 sum   = vec4(0.0);
    for (int dy = -1; dy <= 2; dy++)
        for (int dx = -1; dx <= 2; dx++) {
            vec2 offset = vec2(dx, dy) * texel;
            sum += texture(tx_color, v_uv + offset);
        }

    fragColor = sum * (1.0 / 16.0);
}