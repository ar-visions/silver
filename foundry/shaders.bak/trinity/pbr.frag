#include <pbr>

layout(location = 0) out vec4 fragColor;
layout(location = 1) in  vec3 v_world_normal;
layout(location = 2) in  vec3 v_world_pos;
layout(location = 3) in  vec3 v_view_pos;
layout(location = 4) in  vec2 v_uv;

// Normal mapping function
vec3 perturbNormal(vec3 N, vec3 V, vec2 texcoord) {
    vec3 map = texture(tx_normal, texcoord).rgb * 2.0 - 1.0;
    
    // Calculate tangent space
    vec3 q1 = dFdx(v_world_pos);
    vec3 q2 = dFdy(v_world_pos);
    vec2 st1 = dFdx(texcoord);
    vec2 st2 = dFdy(texcoord);
    
    vec3 T = normalize(q1 * st2.y - q2 * st1.y);
    vec3 B = normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);
    
    return normalize(TBN * map);
}

// Parallax mapping
vec2 parallaxMapping(vec2 texCoords, vec3 viewDir) {
    float height_scale = 0.04;
    float height = texture(tx_height, texCoords).r;
    vec2 p = viewDir.xy / viewDir.z * (height * height_scale);
    return texCoords - p;
}

vec3 environmentMapping(vec3 R, float roughness) {
    float lod   = roughness * 7.0; // Adjust based on your mipmap count
    float lodf  = floor(lod);
    float lodfr = lod - lodf;
    int   mip0  = int(lodf);
    int   mip1  = min(mip0 + 1, 7); 
    vec3  s0    = texture(tx_environment, vec4(R, mip0)).rgb;
    vec3  s1    = texture(tx_environment, vec4(R, mip1)).rgb;
    return mix(s0, s1, lodfr);
}

// Main PBR function
vec4 calculatePBR(vec2 texCoords, vec3 worldPos, vec3 normal, vec3 viewPos) {
    // Get camera position
    vec3 cameraPos = world.pos.xyz;
    
    // View direction
    vec3 V = normalize(cameraPos - worldPos);
    
    // Apply parallax mapping for texture coordinate
    vec2 texCoordsMapped = parallaxMapping(texCoords, V);
    
    // Get material properties
    vec4  baseColor     = texture(tx_color,    texCoordsMapped);
    float ior           = texture(tx_ior,      texCoordsMapped).r;
    float metallic      = texture(tx_metal,    texCoordsMapped).r;
    float roughness     = texture(tx_rough,    texCoordsMapped).r;
    float ao            = texture(tx_ao,       texCoordsMapped).r;
    vec3  emission      = texture(tx_emission, texCoordsMapped).rgb;
    float intensity     = texture(tx_emission, texCoordsMapped).a;
    
    vec3 rough_color = baseColor.rgb; // energyCompensation(baseColor.rgb, roughness);

    // Apply normal mapping
    vec3 N = perturbNormal(normal, V, texCoordsMapped);
    
    // Reflection vector for environment mapping
    vec3 R = reflect(-V, N);
    
    // Environment lighting
    vec3 envColor = environmentMapping(R, roughness);
    
    // Ambient light
    vec3 ambient = envColor * rough_color * ao;
    
    // Directional light (example light, replace with your light source)
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 lightColor = vec3(1.0);
    
    // Calculate BRDF
    vec3 Lo = BRDF(lightDir, V, N, rough_color, ior, metallic, roughness) * lightColor;
    
    // Combine direct and indirect lighting
    vec3 color = ambient + Lo + emission * intensity;
    
    // Convert from linear to sRGB for display
    color = linearToSRGB(color);
    
    return vec4(color, baseColor.a);
}


void main() {
    // Calculate PBR lighting
    fragColor = calculatePBR(v_uv, v_world_pos, normalize(v_world_normal), v_view_pos);
    //fragColor = vec4(vec3(1.0), 1.0);
}
