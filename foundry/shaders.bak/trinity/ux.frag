#include <ux>

layout(location = 0) out vec4 fragColor;
layout(location = 0) in  vec2 v_uv;

void main() {
    vec3  compose  = texture(tx_compose,  v_uv).xyz;
    vec3  colorize = texture(tx_colorize, v_uv).xyz;
    vec4  overlay  = texture(tx_overlay,  v_uv);
    float glyph    = texture(tx_glyph,    v_uv).x;
    float lum_adj  = 1.0 - ((compose.b - 0.5020) * 2.0);
    vec3  f;

    if (compose.r < 0.5020) {
        f = mix(
            texture(tx_background, v_uv).rgb,
            texture(tx_blur,       v_uv).rgb,
            compose.r / 0.5020) * lum_adj;
    } else {
        f = mix(
            texture(tx_blur,       v_uv).rgb,
            texture(tx_frost,      v_uv).rgb,
            (compose.r - 0.5020) / (1.0 - 0.5020)) * lum_adj;
    }

    // set global hue based on luminosity
    if (compose.g > 0.0) {
        float hue = compose.g;
        vec3  hsv = rgb2hsv(f);
        vec3  clr = vec3(compose.g, hsv.y, hsv.z);
        f = hsv2rgb(clr);
    }

    // mix in overlay
    f = mix(f, overlay.rgb, overlay.a);

    // mix in colorize to the amount of glyph
    if (glyph > 0.001) {
        f = mix(f, colorize, glyph);
    }
    
    fragColor = vec4(f, 1.0);
}
