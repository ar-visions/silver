#include <earth>

layout(location = 0) out vec4 outColor;

layout(location = 0) in  vec3 v_normal;
layout(location = 1) in  vec2 v_uv;
layout(location = 2) in  vec3 v_tangent;
layout(location = 3) in  vec3 v_bitangent;

void main() {
    vec2 uv = v_uv;

    // === Lighting setup ===
    vec3 normal   = normalize(v_normal);
    vec3 sun_dir  = normalize(vec3(0.2, 0.0, 0.8)); // fixed light source
    vec3 view_dir = normalize(vec3(0.0, 0.0, 1.0)); // camera pointing -Z

    vec3  normal_map = texture(tx_earth_normal, v_uv).rgb * 2.0 - 1.0;
    mat3 TBN = mat3(normalize(v_tangent), normalize(v_bitangent), normal);
    vec3 perturbed_normal2 = normalize(TBN * normal_map);
    
    float NdotL   = max(dot(perturbed_normal2, sun_dir), 0.0);
    vec3 half_dir = normalize(sun_dir + view_dir);
    float spec    = pow(max(dot(perturbed_normal2, half_dir), 0.0), 28.0); // shininess
 
    // === Textures ===
    vec3  color      = texture(tx_earth_color,      uv).rgb;

    float water      = (texture(tx_earth_water,      uv).r + texture(tx_earth_water_blur,      uv).r) / 2.0;
    float clouds     = texture(tx_earth_cloud,      uv).r;
    float elevation  = texture(tx_earth_elevation,  uv).r;
    float bathymetry = texture(tx_earth_bathymetry, uv).r;
    float lights     = texture(tx_earth_lights,     uv).r;

    // === Water coloring ===
    vec3 water_color = mix(vec3(0.0, 0.1, 0.1), vec3(0.2, 0.6, 1.0), bathymetry);
    color = mix(color, water_color, water); // override land with rich ocean

    // === Elevation / bathymetry tint ===
    float elevation_effect = smoothstep(0.5, 1.0, elevation);
    float bathy_effect     = water * smoothstep(0.0, 0.5, bathymetry);
    vec3  terrain_tint     = mix(vec3(0.8, 0.6, 0.3), vec3(0.2, 0.4, 0.7), bathy_effect);
    color = mix(color, terrain_tint, 0.3 * bathy_effect + 0.2 * elevation_effect);

    // === Clouds brightening ===
    //color += clouds;

    // === Specular reflection on water ===
    float edge_fade = pow(1.0 - dot(perturbed_normal2, view_dir), 2.0);
    vec3 specular = vec3(1.0) * spec * 0.4 * water;
    specular += vec3(1.0) * water * edge_fade;

    // === Ambient + Diffuse ===
    float ambient = 0.01;
    vec3 final = color * (ambient + NdotL) + specular;

    // === Optional Gamma Correction (sRGB)
    vec3 f1 = pow(final, vec3(1.0 / 1.8));
    vec3 f2 = pow(final, vec3(1.0 / 6.4));

    float NdotV = max(0.0, pow(1.0 - dot(perturbed_normal2, view_dir), 1.0)) * 2.0;
    


    // animated FBM-based distortion
    float t = earth.time * 0.2; // pass this as uniform in seconds
    vec3 offset = vec3(uv * 50.0, t * 0.3) * 10.0;
    float noise_x = fbm(offset + vec3(100.0, 0.0, 0.0), 0.0);
    float noise_y = fbm(offset + vec3(0.0, 500.0, 0.0), 0.0);
    float noise_z = fbm(offset + vec3(0.0, 0.0, 15.0), 0.0);

    vec3 distortion = normalize(vec3(noise_x, noise_y, noise_z) * 2.0 - 1.0) * 0.06;
    vec3 perturbed_normal = normalize(perturbed_normal2 + distortion);

    float spec2 = 0.0;//pow(max(dot(perturbed_normal2, half_dir), 0.0), 22.0); // shininess


    // === Add tight central specular glint ===
    spec2 += max(0.0, pow(dot(normal, view_dir), 244.0) * 0.88); // 1.0 when aligned

    float e = texture(tx_earth_elevation, v_uv).x;

    outColor = vec4(mix(f1, f2, spec2), 1.0) + vec4(vec3(e * 2.2 * spec2) * vec3(0.8, 1.0, 0.8), 0.0);
}
