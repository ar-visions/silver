#include <earth>

layout(location = 0) out vec4 outColor;

layout(location = 0) in  vec3 v_normal;
layout(location = 1) in  vec2 v_uv;



void main() {
    vec2 uv = v_uv;

    // === Lighting setup ===
    vec3 normal   = normalize(v_normal);

    // animated FBM-based distortion
    float t = earth.time * 0.2; // pass this as uniform in seconds
    vec3 offset = vec3(uv * 100.0, t * 0.3) * 5.0;
    float noise_x = fbm(offset + vec3(100.0, 0.0, 0.0), 0.0);
    float noise_y = fbm(offset + vec3(0.0, 500.0, 0.0), 0.0);
    float noise_z = fbm(offset + vec3(0.0, 0.0, 15.0), 0.0);

    vec3 distortion = normalize(vec3(noise_x, noise_y, noise_z) * 2.0 - 1.0) * 0.084;
    vec3 perturbed_normal = normalize(normal + distortion);

    vec3 sun_dir  = normalize(vec3(0.2, 0.0, 0.8)); // fixed light source
    vec3 view_dir = normalize(vec3(0.0, 0.0, 1.0)); // camera pointing -Z

    float NdotL   = max(dot(perturbed_normal, sun_dir), 0.0);
    vec3 half_dir = normalize(view_dir);

    float spec    = pow(max(dot(perturbed_normal, half_dir), 0.0) * 0.9, 12.0); // shininess

    // === Textures ===
    //float clouds = texture(tx_earth_cloud, uv).r;
    const float kernel[7] = float[](
        0.006, 0.061, 0.242, 0.383, 0.242, 0.061, 0.006
    );

    vec2 texel = 1.0 / vec2(textureSize(tx_earth_cloud, 0));
    float clouds = 0.0;

    for (int i = -3; i <= 3; ++i) {
        for (int j = -3; j <= 3; ++j) {
            vec2 offset = vec2(i, j) * texel * 2.0;
            float weight = kernel[i + 3] * kernel[j + 3];
            clouds += texture(tx_earth_cloud, uv + offset).r * weight;
        }
    }
 
    //float water = texture(tx_earth_water, uv).r;
    float water = (texture(tx_earth_water, uv).r + texture(tx_earth_water_blur, uv).r) / 2.0;

    // === Water coloring ===
    vec3 water_color = vec3(0.022, 0.11, 0.22);

    // === Specular reflection on water ===
    float edge_fade = pow(1.0 - dot(normal, view_dir), 2.0);
    vec3 s1 = vec3(1.0) * spec  * 0.4 * water;
    s1 += vec3(0.1, 0.2, 0.3) * water * edge_fade;

    // === Ambient + Diffuse ===
    float ambient = 0.005;

    // === Clouds brightening ===
    water_color -= clouds * (0.22 * (1.0 - (2.0 * spec)));
    vec3 final = water_color * (ambient + NdotL) + s1;

    // === Add tight central specular glint ===
    float facing = max(0.0, pow(dot(normal, view_dir), 144.0) * 0.44); // 1.0 when aligned
    vec3 glint   = vec3(1.0) * facing;

    float bathymetry = texture(tx_earth_bathymetry, uv).r;
    vec3 water_color2 = vec3(0.4, 0.4, 0.4) * bathymetry;

    outColor = mix(vec4(water_color2 + glint, water), vec4(final, water), 1.0 - (glint.x * 1.44));
}
