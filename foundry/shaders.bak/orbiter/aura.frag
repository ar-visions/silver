#include <aura>

layout(location = 0) out vec4 outColor;
layout(location = 0) in  vec2 v_uv;


mat3 rotateY(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat3(
        vec3(c, 0.0, -s),
        vec3(0.0, 1.0, 0.0),
        vec3(s, 0.0, c)
    );
}

float sdf_sphere(vec3 p, float radius, float squash) {
    // squash factor < 1.0 flattens vertically
    vec3 q = vec3(p.x, p.y / squash, p.z);
    return length(q) - radius;
}

float sdf_cylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, vec2(0.0)));
}

// permutation function
vec4 permute4(vec4 x) {
    return mod((x * 34.0 + 1.0) * x, vec4(289.0));
}

// fast inverse sqrt
vec4 taylorInvSqrt4(vec4 r) {
    return 1.79284291400159 - 0.85373472095314 * r;
}

// smootherstep fade curve
vec3 fade(vec3 t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

// basic perlin 3D noise
float perlin3D(vec3 P) {
    vec3 Pi0 = mod(floor(P), vec3(289.0));
    vec3 Pi1 = mod(Pi0 + vec3(1.0), vec3(289.0));
    vec3 Pf0 = fract(P);
    vec3 Pf1 = Pf0 - vec3(1.0);

    vec4 ix = vec4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
    vec4 iy = vec4(Pi0.y, Pi0.y, Pi1.y, Pi1.y);
    vec4 iz0 = vec4(Pi0.z);
    vec4 iz1 = vec4(Pi1.z);

    vec4 ixy = permute4(permute4(ix) + iy);
    vec4 ixy0 = permute4(ixy + iz0);
    vec4 ixy1 = permute4(ixy + iz1);

    vec4 gx0 = fract(ixy0 / 7.0) * 2.0 - 1.0;
    vec4 gy0 = fract(floor(ixy0 / 7.0) / 7.0) * 2.0 - 1.0;
    vec4 gz0 = 1.0 - abs(gx0) - abs(gy0);

    vec4 gx1 = fract(ixy1 / 7.0) * 2.0 - 1.0;
    vec4 gy1 = fract(floor(ixy1 / 7.0) / 7.0) * 2.0 - 1.0;
    vec4 gz1 = 1.0 - abs(gx1) - abs(gy1);

    vec4 sz0 = step(gz0, vec4(0.0));
    gx0 -= sz0 * (step(0.0, gx0) - 0.5);
    gy0 -= sz0 * (step(0.0, gy0) - 0.5);

    vec4 sz1 = step(gz1, vec4(0.0));
    gx1 -= sz1 * (step(0.0, gx1) - 0.5);
    gy1 -= sz1 * (step(0.0, gy1) - 0.5);

    vec3 g000 = vec3(gx0.x, gy0.x, gz0.x);
    vec3 g100 = vec3(gx0.y, gy0.y, gz0.y);
    vec3 g010 = vec3(gx0.z, gy0.z, gz0.z);
    vec3 g110 = vec3(gx0.w, gy0.w, gz0.w);
    vec3 g001 = vec3(gx1.x, gy1.x, gz1.x);
    vec3 g101 = vec3(gx1.y, gy1.y, gz1.y);
    vec3 g011 = vec3(gx1.z, gy1.z, gz1.z);
    vec3 g111 = vec3(gx1.w, gy1.w, gz1.w);

    vec4 norm0 = taylorInvSqrt4(vec4(dot(g000, g000), dot(g010, g010), dot(g100, g100), dot(g110, g110)));
    g000 *= norm0.x;
    g010 *= norm0.y;
    g100 *= norm0.z;
    g110 *= norm0.w;
    vec4 norm1 = taylorInvSqrt4(vec4(dot(g001, g001), dot(g011, g011), dot(g101, g101), dot(g111, g111)));
    g001 *= norm1.x;
    g011 *= norm1.y;
    g101 *= norm1.z;
    g111 *= norm1.w;

    float n000 = dot(g000, Pf0);
    float n100 = dot(g100, vec3(Pf1.x, Pf0.y, Pf0.z));
    float n010 = dot(g010, vec3(Pf0.x, Pf1.y, Pf0.z));
    float n110 = dot(g110, vec3(Pf1.xy, Pf0.z));
    float n001 = dot(g001, vec3(Pf0.xy, Pf1.z));
    float n101 = dot(g101, vec3(Pf1.x, Pf0.y, Pf1.z));
    float n011 = dot(g011, vec3(Pf0.x, Pf1.yz));
    float n111 = dot(g111, Pf1);

    vec3 fade_xyz = fade(Pf0);
    vec4 n_z = mix(vec4(n000, n100, n010, n110), vec4(n001, n101, n011, n111), fade_xyz.z);
    vec2 n_yz = mix(n_z.xy, n_z.zw, fade_xyz.y);
    float n_xyz = mix(n_yz.x, n_yz.y, fade_xyz.x);

    return n_xyz * 0.5 + 0.5;
}

// fractal brownian motion (fbm) using perlin noise
float fbm(vec3 pos, float y_translate) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 0.1;
    for (int i = 0; i < 6; ++i) {
        value += amplitude * perlin3D(pos * frequency + vec3(0.0, y_translate * 11.0, 0.0));
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

#define M_PI 3.1415926535897932384626433832795

void main() {
    vec2 screen = v_uv * 2.0 - 1.0;
    float beam_radius = 2.2 / 2.2 + 0.04;
    float beam_height = 8.0;
    
    // Inverse projection * view to get world ray
    mat4 inv_vp = inverse(au.v_proj * au.v_view);
    vec4 near = inv_vp * vec4(screen, 0.0, 1.0);
    vec4 far  = inv_vp * vec4(screen, 1.0, 1.0);

    vec3 ray_origin = near.xyz / near.w;
    vec3 ray_target = far.xyz / far.w;
    vec3 ray_dir    = normalize(ray_target - ray_origin);

    // Define beam origin and up-direction in world space
    vec3 beam_origin = (au.v_model * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
    vec3 beam_up     = normalize((au.model * vec4(0.0, 1.0, 0.0, 0.0)).xyz);

    // Transform ray into beam space
    vec3 ro = ray_origin - beam_origin;
    vec3 rd = ray_dir;

    // Build local beam space basis (up is y)
    vec3 bz = normalize(beam_up);
    vec3 bx = normalize(cross(vec3(0.0, 0.0, 1.0), bz));
    vec3 by = cross(bz, bx);
    mat3 beam_basis = transpose(mat3(bx, by, bz));

    ro = beam_basis * ro;
    rd = beam_basis * rd;
    //float front_facing = clamp(dot(rd, vec3(0.0, 0.0, 1.0)) * 2.0, 0.0, 1.0);

    // Raymarching
    float t                = 0.0;
    float entry_t          = 0.0;
    float exit_t           = 0.0;
    bool  inside           = false;
    float y_translate      = au.time * -0.48 * 0.88;
    mat3  noise_rotation   = rotateY(au.time * 0.88);
    float max_density      = 0.0;
    const vec3 beam_offset = vec3(0.0, -beam_height / 2.0, 0.0);
    int   num_steps        = 188;

    for (int  i = 0; i < num_steps; ++i) {
        vec3  p = beam_offset + ro + rd * t;
        float d = sdf_cylinder(p, beam_height / 2.0, beam_radius);
        if (!inside && d < 0.001) {
            entry_t = t;
            inside  = true;
        } else if (inside && d > 0.001) {
            exit_t  = t;
            break;
        }
        t += max(abs(d), 0.01);
    }

    vec3 accumulated_color   = vec3(0.0);
    float accumulated_alpha  = 0.0;
    float avg_dist_to_center = 0.0;
    float avg_py             = 0.0;

    if (inside) {
        bool  inside_core  = false;
        float core_entry_t = 0.0;
        float core_exit_t  = 0.0;
        float et           = 0.0;
        for (int  i = 0; i < num_steps; ++i) {
            vec3  p = beam_offset + ro + rd * t;
            float d = sdf_sphere(p, beam_radius, 0.5);
            if (!inside && d < 0.001) {
                core_entry_t = et;
                inside_core  = true;
            } else if (inside && d > 0.001) {
                core_exit_t  = et;
                break;
            }
            et += max(abs(d), 0.01);
        }
        vec4 au_core = vec4(0.0);
        if (inside_core) {
            float core_dist = core_exit_t - core_entry_t;
            float step_size = core_dist / float(num_steps);
            t = entry_t;
            float avg_dist_to_center = 0.0;
            for (int i = 0; i < num_steps; ++i) {
                vec3 p = ro + rd * et;
                float inner_factor = 1.0 - length(p.yz) / beam_radius;
                avg_py += p.y;

                float twist_amount = length(p.xy) * 2.0;
                mat3  twist        = mat3(
                    vec3(cos(twist_amount), 0.0, sin(twist_amount)),
                    vec3(0.0,             1.0, 0.0),
                    vec3(-sin(twist_amount), 0.0, cos(twist_amount)));
                
                vec3  twisted_pos    = twist * p;
                vec3  rotated_pos    = 4.0 * noise_rotation * twisted_pos;
                float dist_to_wall   = sdf_cylinder(p, 1.0, beam_radius); /// this should effectively control rotation acceleration on perlin
                float dist_to_center = clamp(length(p.xz), 0.0, beam_radius);
                float center_f       = max(0.0, 1.0 - (dist_to_center / beam_radius) * 1.0);
                avg_dist_to_center  += dist_to_center;
                float bowl_factor    = length(p.xy);
                float raw_density    = fbm(rotated_pos, inner_factor);
                float base_fade      = smoothstep(0.0, 2.0, p.y + (beam_height / 2.0)); // fades in from base
                float threshold      = 0.1;
                float u2             = 0.5 + atan(p.x, p.z) / (2.0 * M_PI);
                float v2             = (p.y + beam_radius * 0.5) / beam_radius;
                vec2  core_uv        = vec2(u2, v2);
                vec4  core_cv        = texture(tx_canvases[1], core_uv) * raw_density * base_fade;
                vec4  core_color     = vec4(core_cv.xyzw) * bowl_factor * raw_density;
                accumulated_color   +=  core_color.xyz;
                accumulated_alpha   += (core_color.w * center_f * 0.1);
                et                  += step_size;
            }
            
            avg_dist_to_center      /= num_steps;
            float center_final       = 1.0 - (avg_dist_to_center / beam_radius);
            //au_core                  = vec4(accumulated_color / (num_steps * 0.5), accumulated_alpha * center_final);
            
        }

        float dist      = exit_t - entry_t;
        float step_size = dist / float(num_steps);
        t = entry_t;
        float avg_dist_to_center2 = 0.0;
        vec3  accumulated_color2 = vec3(0.0);
        float accumulated_alpha2 = 0.0;
        for (int i = 0; i < num_steps; ++i) {
            vec3 p = beam_offset + ro + rd * t;
            float height_factor = 1.0 - (p.y + 4.0) / beam_height;
            avg_py += p.y;
            float twist_amount = length(p.xy) * height_factor * 4.0;
            mat3 twist = mat3(
                vec3(cos(twist_amount), 0.0, sin(twist_amount)),
                vec3(0.0,             1.0, 0.0),
                vec3(-sin(twist_amount), 0.0, cos(twist_amount)));

            vec3 twisted_pos = twist * p;
            vec3  rotated_pos    = 4.0 * noise_rotation * twisted_pos;
            float dist_to_wall   = sdf_cylinder(p, 1.0, beam_radius);
            float dist_to_center = clamp(length(p.xz), 0.0, beam_radius);
            float center_f       = max(0.0, 1.0 - (dist_to_center / beam_radius) * 1.0);
            float center_void    = (dist_to_center / beam_radius);
            if (center_void < 0.2)
                center_void = max(center_f, center_void);
            avg_dist_to_center2 += dist_to_center;
            float y_factor      = max(0.0, (p.y + (beam_height / 2.0)) / beam_height);
            float raw_density   = fbm(rotated_pos, y_translate);
            float base_fade     = smoothstep(0.0, 2.0, p.y + (beam_height / 2.0)); // fades in from base
            float threshold     = 0.2;
            float density       = max(0.0, raw_density - threshold);

            // i need to reference texture by uv, and that uv must be our xy placement in this cylinder, is that p.x and p.y ?
            float   u           = 0.5 + atan(p.x, p.z) / (2.0 * M_PI);          // angular
            float   v           = clamp(length(p.xz) / beam_radius, 0.0, 1.0);  // radial
            vec2    beam_uv     = vec2(0.6, 0.5);
            vec4    beam_cv     = texture(tx_canvases[0], beam_uv) * base_fade;
            vec4    beam_color  = vec4(beam_cv.xyzw) * (raw_density - threshold);
            accumulated_color2 += beam_color.xyz * 4.0;
            accumulated_alpha2 += (beam_color.w * center_f * 0.1);
            t += step_size;
        }
        
        /// perform a final alpha blend on the edges
        avg_dist_to_center2 /= num_steps;
        float center_final = 1.0 - (avg_dist_to_center2 / beam_radius);
        vec4 au  = vec4(accumulated_color2 / num_steps, accumulated_alpha2 * center_final);
        outColor = au + au_core; //mix(cv, au, au.a);
        return;
    } else {
        outColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }
}

