#version 450

layout(location = 0) in vec2 v_pos;

// same UBO as vertex shader
layout(set = 0, binding = 0) uniform UBO {
    vec2 viewport;    vec2 _pad0;
    vec4 rect;
    vec4 radii;
    vec4 fill_color;
    vec4 stroke_clr;
    vec4 params;      // stroke_width, opacity, blur, mode
    vec4 grad0;
    vec4 grad_c0;
    vec4 grad_c1;
};

// glyph atlas (R8) and image texture
layout(set = 0, binding = 1) uniform sampler2D u_atlas;
layout(set = 0, binding = 2) uniform sampler2D u_image;

layout(location = 0) out vec4 o_color;

// ---- per-corner rounded rect SDF ----
float sdf_rounded_rect(vec2 p, vec2 center, vec2 half_size, vec4 r) {
    vec2 q = p - center;
    float cr = (q.x >= 0.0)
        ? ((q.y >= 0.0) ? r.z : r.y)   // br : tr
        : ((q.y >= 0.0) ? r.w : r.x);  // bl : tl
    cr = min(cr, min(half_size.x, half_size.y));
    vec2 d = abs(q) - half_size + vec2(cr);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - cr;
}

// ---- gradient evaluation ----
vec4 eval_gradient(vec2 p, float mode) {
    vec2 g0 = grad0.xy;
    vec2 g1 = grad0.zw;
    float t;

    if (mode == 1.0) {
        // linear
        vec2  d      = g1 - g0;
        float len_sq = dot(d, d);
        t = (len_sq > 0.0001) ? clamp(dot(p - g0, d) / len_sq, 0.0, 1.0) : 0.0;
    } else if (mode == 2.0) {
        // radial: g0 = center, g1.x = inner_r, g1.y = outer_r
        float dist = length(p - g0);
        float span = g1.y - g1.x;
        t = (span > 0.0001) ? clamp((dist - g1.x) / span, 0.0, 1.0) : 0.0;
    } else {
        // conic: g0 = center, g1.x = start_angle
        vec2  d     = p - g0;
        float angle = atan(d.y, d.x) - g1.x;
        if (angle < 0.0) angle += 6.2831853;
        t = clamp(angle / 6.2831853, 0.0, 1.0);
    }

    return mix(grad_c0, grad_c1, t);
}

void main() {
    float mode         = params.w;
    float stroke_width = params.x;
    float opacity      = params.y;
    float blur         = params.z;

    vec2  center    = rect.xy + rect.zw * 0.5;
    vec2  half_size = rect.zw * 0.5;
    float dist      = sdf_rounded_rect(v_pos, center, half_size, radii);

    // ---- text: sample glyph atlas ----
    if (mode == 4.0) {
        vec2 uv    = (v_pos - rect.xy) / rect.zw;
        uv         = mix(grad0.xy, grad0.zw, uv);
        float a    = texture(u_atlas, uv).r;
        o_color    = fill_color * vec4(1.0, 1.0, 1.0, a) * opacity;
        return;
    }

    // ---- image: sample texture, clip to rounded rect ----
    if (mode == 5.0) {
        vec2 uv    = (v_pos - rect.xy) / rect.zw;
        uv         = mix(grad0.xy, grad0.zw, uv);
        vec4 texel = texture(u_image, uv);
        float aa   = 1.0 - smoothstep(-0.5, 0.5, dist);
        o_color    = texel * aa * opacity;
        return;
    }

    // ---- SDF path: sample distance field texture ----
    if (mode == 6.0) {
        vec2 uv     = (v_pos - rect.xy) / rect.zw;
        uv          = mix(grad0.xy, grad0.zw, uv);
        float sd    = texture(u_image, uv).r;
        // R8 texture: 0.5 = boundary, >0.5 = inside, <0.5 = outside
        float d_sdf = (0.5 - sd) * 2.0;   // remap to signed: negative inside
        float path_aa;
        if (stroke_width > 0.0) {
            // stroke: ring around boundary
            float half_w = stroke_width * 0.5;
            float outer  = 1.0 - smoothstep(-0.5, 0.5, abs(d_sdf) - half_w);
            o_color      = fill_color * vec4(1.0, 1.0, 1.0, outer) * opacity;
        } else {
            // fill: inside boundary
            path_aa = 1.0 - smoothstep(-0.5, 0.5, d_sdf);
            o_color = fill_color * vec4(1.0, 1.0, 1.0, path_aa) * opacity;
        }
        return;
    }

    // ---- fill: solid or gradient ----
    vec4 fill = (mode >= 1.0 && mode <= 3.0)
        ? eval_gradient(v_pos, mode)
        : fill_color;

    // ---- AA at shape boundary ----
    float aa = (blur > 0.0)
        ? 1.0 - smoothstep(-blur, blur, dist)
        : 1.0 - smoothstep(-0.5, 0.5, dist);

    // ---- stroke: SDF ring between outer and inner boundary ----
    vec4 color;
    if (stroke_width > 0.0) {
        float fill_aa = 1.0 - smoothstep(-0.5, 0.5, dist + stroke_width);
        float ring    = max(aa - fill_aa, 0.0);
        color = fill * fill_aa + stroke_clr * ring;
    } else {
        color = fill * aa;
    }

    o_color = color * opacity;
}
