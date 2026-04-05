#include <earth>

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
//layout(location = 3) in vec3 tangent;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;

void main() {
    v_uv           = vec2(1.0 - uv.x, uv.y);

    // Correctly rotate normal into world space
    mat3 normal_matrix = transpose(inverse(mat3(earth.model)));
    v_normal = normalize(normal_matrix * normal);

    float scale     = 0.00044;
    vec3  surface   = pos + (normal * 1.0 * scale);
    gl_Position     = earth.proj * earth.view * earth.model * vec4(surface, 1.0);
}
