#include <purple>

layout(location = 0) out vec4 outColor;

layout(location = 0) in  vec3 v_normal;
layout(location = 1) in  vec2 v_uv;
layout(location = 2) in  vec3 v_tangent;
layout(location = 3) in  vec3 v_bitangent;

void main() {
    vec2 uv = v_uv;

    // === Lighting setup ===
    vec3  normal   = normalize(v_normal);
    vec3  sun_dir  = normalize(vec3(0.2, 0.0, 0.8)); // fixed light source
    vec3  view_dir = normalize(vec3(0.0, 0.0, 1.0)); // camera pointing -Z

    // animated FBM-based distortion
    float t          = purple.time * 0.2; // pass this as uniform in seconds
    vec3  offset     = vec3(uv * 10.0, t * 0.3) * 5.0;
    float noise_x    = fbm(offset + vec3(100.0 + (1.0 - (uv.y - 0.5)) * 100.0, 0.0, 0.0), 0.0);
    float noise_y    = fbm(offset + vec3(0.0, 500.0, 0.0), 0.0);
    float noise_z    = fbm(offset + vec3(0.0, 0.0, 15.0), 0.0);

    vec3  distortion = normalize(vec3(noise_x, noise_y, noise_z) * 2.0 - 1.0) * 0.188;

    float NdotL      = max(dot(normal, sun_dir), 0.0);
    vec3  half_dir   = normalize(view_dir);
    float spec       = pow(max(dot(normal, half_dir), 0.0) * 0.9, 12.0); // shininess

    float edge_fade   = pow(1.0 - dot(normal, view_dir), 2.0);
    vec3 s1           = vec3(1.0) * spec  * 10.4;
    s1 += vec3(0.3, 0.0, 0.6) * edge_fade;

    vec3  color    = texture(tx_purple_color, uv).rgb;
    vec3  cloud    = texture(tx_purple_cloud, uv).rgb;
    outColor       = vec4(distortion + s1 * color + cloud, 1.0);
}
