#include <purple>

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 uv;
//layout(location = 3) in vec3 tangent;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_uv;
layout(location = 2) out vec3 v_tangent;
layout(location = 3) out vec3 v_bitangent;



void main() {
    const float M_PI = 3.1415926535897932384;

    v_uv           = vec2(1.0 - uv.x, uv.y);

    float theta = v_uv.x * 2.0 * M_PI; // longitude
    float phi   = v_uv.y * M_PI;       // latitude

    // Tangent: derivative of position w.r.t. u (around equator)
    v_tangent = normalize(vec3(-sin(theta), 0.0, cos(theta)));

    // Bitangent: derivative w.r.t. v (from pole to pole)
    v_bitangent = normalize(cross(v_tangent, pos)); // orthogonalized

    // Correctly rotate normal into world space
    mat3 normal_matrix = transpose(inverse(mat3(purple.model)));
    v_normal = normalize(normal_matrix * normal);

    v_tangent   = normalize(normal_matrix * v_tangent);
    v_bitangent = normalize(normal_matrix * v_bitangent);

    gl_Position     = purple.proj * purple.view * purple.model * vec4(pos, 1.0);
}
