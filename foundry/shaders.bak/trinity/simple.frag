#include <simple>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    vec3 background = texture(tx_background, v_uv).xyz;
    vec3 colorize   = texture(tx_colorize, v_uv).xyz;
    vec4 overlay    = texture(tx_overlay, v_uv);
    float glyph     = texture(tx_glyph, v_uv).x;

    // first composite background and overlay
    vec3 f = mix(background, overlay.rgb, overlay.a);

    // mix in overlay
    f = mix(f, overlay.rgb, overlay.a);

    // mix in colorize to the amount of glyph
    if (glyph > 0.001) {
        f = mix(f, colorize, glyph);
    }
    
    fragColor = vec4(f, 1.0);
}
