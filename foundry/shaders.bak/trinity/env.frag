#version 450

layout(set = 1, binding = 0)  uniform sampler2D tx_env; // color + alpha

layout(location = 0) in  vec2 uv;
layout(location = 1) in  vec3 dir;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

void main() {
    vec3  R  = normalize(dir);
    vec2  eq = vec2(0.5 + atan(R.z, R.x) / (2.0  * PI), 
                          acos(R.y)      /         PI);
    outColor = texture(tx_env, eq);
}