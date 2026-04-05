#include <earth>

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
//layout(location = 3) in vec3 tangent;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out float v_cloud_avg;

void main() {
    v_uv           = vec2(1.0 - uv.x, uv.y);
    // Correctly rotate normal into world space
    mat3 normal_matrix = transpose(inverse(mat3(earth.model)));
    v_normal = normalize(normal_matrix * normal);


    // === Textures ===
    //float clouds = texture(tx_earth_cloud, v_uv).r;
    const float kernel[7] = float[](
        0.006, 0.061, 0.242, 0.383, 0.242, 0.061, 0.006
    );

    vec2 texel = 1.0 / vec2(textureSize(tx_earth_cloud, 0));
    float clouds = 0.0;

    for (int i = -3; i <= 3; ++i) {
        for (int j = -3; j <= 3; ++j) {
            vec2 offset = vec2(i, j) * texel * 2.0;
            float weight = kernel[i + 3] * kernel[j + 3];
            clouds += texture(tx_earth_cloud, v_uv + offset).r * weight;
        }
    }

    v_cloud_avg = clouds;

    float scale     = 0.0044; //0.0044 + clouds * 0.0044;
    vec3  surface   = pos + (normal * 1.0 * scale);
    gl_Position     = earth.proj * earth.view * earth.model * vec4(surface, 1.0);
}
