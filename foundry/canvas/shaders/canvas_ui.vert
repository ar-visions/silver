#version 450

// unit quad: 4 vertices, triangle strip
layout(location = 0) in vec2 a_pos;  // [0,0] [1,0] [0,1] [1,1]

// uniforms from canvas_ui shader class (reflected via Au member walk)
layout(set = 0, binding = 0) uniform UBO {
    vec2 viewport;    vec2 _pad0;
    vec4 rect;        // x, y, w, h (pixels)
    vec4 radii;       // tl, tr, br, bl corner radii
    vec4 fill_color;  // rgba [0..1]
    vec4 stroke_clr;  // rgba [0..1]
    vec4 params;      // stroke_width, opacity, blur, mode
    vec4 grad0;       // gradient endpoints or tex rect
    vec4 grad_c0;     // gradient stop color 0
    vec4 grad_c1;     // gradient stop color 1
};

layout(location = 0) out vec2 v_pos;  // pixel position for SDF

void main() {
    float pad = 1.0 + params.z; // blur expands bounds
    vec2  lo  = rect.xy - vec2(pad);
    vec2  hi  = rect.xy + rect.zw + vec2(pad);
    vec2  px  = mix(lo, hi, a_pos);

    v_pos = px;

    vec2 ndc = (px / viewport) * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
