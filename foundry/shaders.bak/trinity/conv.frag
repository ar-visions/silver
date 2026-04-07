#include <conv>

layout(set = 1, binding = 0) uniform samplerCube tx_environment;

layout(location = 0) in vec3 dir;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;
const float TwoPI = 2.0 * PI;
const float Epsilon = 0.00001;

// Compute Van der Corput radical inverse
float radicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

// Sample i-th point from Hammersley point set
vec2 sampleHammersley(uint i, int numSamples) {
    float invNumSamples = 1.0 / float(numSamples);
    return vec2(float(i) * invNumSamples, radicalInverse_VdC(i));
}

// Sample GGX normal distribution function
vec3 sampleGGX(float u1, float u2, float roughness) {
    float alpha = roughness * roughness;

    float cosTheta = sqrt((1.0 - u2) / (1.0 + (alpha*alpha - 1.0) * u2));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
    float phi = TwoPI * u1;

    // Convert to Cartesian
    return vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}

// GGX/Towbridge-Reitz normal distribution function
float ndfGGX(float cosLh, float roughness) {
    float alpha = roughness * roughness;
    float alphaSq = alpha * alpha;

    float denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom);
}

// Compute orthonormal basis
void computeBasisVectors(const vec3 N, out vec3 S, out vec3 T) {
    T = cross(N, vec3(0.0, 1.0, 0.0));
    T = mix(cross(N, vec3(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));

    T = normalize(T);
    S = normalize(cross(N, T));
}

// Convert from tangent space to world space
vec3 tangentToWorld(const vec3 v, const vec3 N, const vec3 S, const vec3 T) {
    return S * v.x + T * v.y + N * v.z;
}

vec3 prefilterEnvMap(vec3 R, float roughness, int numSamples) {
    // Using R as the normal direction
    vec3 N = R;
    vec3 Lo = N; // View direction equals normal direction (isotropic reflection)
    
    // Compute basis vectors for tangent space
    vec3 S, T;
    computeBasisVectors(N, S, T);
    
    vec3 color = vec3(0.0);
    float weight = 0.0;
    
    // Get approximate size of the environment map for mip selection
    vec2 envMapSize = vec2(textureSize(tx_environment, 0));
    float wt = 4.0 * PI / (6.0 * envMapSize.x * envMapSize.y);
    
    // Convolve environment map using GGX NDF importance sampling
    for (uint i = 0; i < uint(numSamples); ++i) {
        vec2 u = sampleHammersley(i, numSamples);
        vec3 Lh = tangentToWorld(sampleGGX(u.x, u.y, roughness), N, S, T);
        
        // Compute incident direction by reflecting viewing direction around half-vector
        vec3 Li = 2.0 * dot(Lo, Lh) * Lh - Lo;
        
        float cosLi = dot(N, Li);
        if (cosLi > 0.0) {
            float cosLh = max(dot(N, Lh), 0.0);
            
            // PDF for GGX NDF sampling
            float pdf = ndfGGX(cosLh, roughness) * cosLh / max(4.0 * dot(Lh, Lo), 0.001);
            
            // Solid angle of current sample
            float ws = 1.0 / (float(numSamples) * pdf);
            
            // Mip level to sample from
            //float mipLevel =   max(0.5 * log2(ws / wt), 0.0);
            float mipLevel = clamp(0.5 * log2(ws / wt), 0.0, 7);

            color += textureLod(tx_environment, Li, mipLevel).rgb * cosLi;
            weight += cosLi;
        }
    }
    
    // Normalize by weight
    //return (weight > 0.0) ? (color / weight) : color;
    return (weight > Epsilon) ? (color / weight) : vec3(0.0);
}

void main() {
    vec3 R = normalize(dir);
    float roughness = conv.roughness_samples.x * conv.roughness_samples.x; // 0...1
    int numSamples = int(conv.roughness_samples.y); // we use 1024

    outColor = (roughness < 0.01)
        ? texture(tx_environment, R)
        : vec4(prefilterEnvMap(R, roughness, numSamples), 1.0);
}