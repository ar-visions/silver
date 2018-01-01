#ifndef _GFX_SHADERS_H_
#define _GFX_SHADERS_H_

#include <obj-math/vec.h>
#include <obj-math/float2.h>
#include <obj-gfx/color.h>

enum ShaderType {
	SHADER_NEW,
	SHADER_RECT,
	SHADER_PLANAR,
	SHADER_TEXT,
	SHADER_LINEAR,
	SHADER_GAUSSIAN_X,
	SHADER_GAUSSIAN_Y,
	SHADER_RESAMPLE,
	SHADER_COUNT
};
enum ShaderNew {
	SN_POSITION,
	SN_EDGE_COUNT,
	SN_EDGE0_A,
	SN_EDGE0_B,
	SN_EDGE1_A,
	SN_EDGE1_B,
	SN_EDGE2_A,
	SN_EDGE2_B,
	SN_EDGE3_A,
	SN_EDGE3_B,
	SN_EDGE4_A,
	SN_EDGE4_B,
	SN_EDGE5_A,
	SN_EDGE5_B
};
enum ShaderRect {
	SRT_POSITION,
	SRT_CENTER,
	SRT_RADIUS,
	SRT_DISTANCE
};
enum ShaderPlanar {
	SP_POSITION
};
enum ShaderText {
	ST_POSITION,
	ST_UV,
	ST_COLOR
};
enum ShaderLinear {
	SL_POSITION,
	SL_UV
};
enum ShaderGaussianX {
	SGX_POSITION,
	SGX_UV
};
enum ShaderGaussianY {
	SGY_POSITION,
	SGY_UV
};
enum ShaderResample {
	SR_POSITION,
	SR_UV
};
enum ShaderUniform {
	U_MATRIX,
	U_SURFACE_V00,
	U_SURFACE_V10,
	U_SURFACE_V11,
	U_SURFACE_V01,
	U_VIEWPORT_SIZE,
	U_CLIP_SIZE,
	U_CLIP_RECT,
	U_CLIP_TEXTURE,
	U_SURFACE_SIZE,
	U_SURFACE_TEXTURE,
	U_SURFACE_BLEND,
	U_FONT_TEXTURE,
	U_COLOR,
	U_FEATHER,
	U_PLANAR_Y,
	U_PLANAR_U,
	U_PLANAR_V,
	//U_PLANAR_SIZE,
	U_LINEAR_BLEND,
	U_LINEAR_MULTIPLY,
	U_LINEAR_FROM,
	U_LINEAR_TO,
	U_LINEAR_COLOR_FROM,
	U_LINEAR_COLOR_TO,
	U_REDUCTION_SIZE,
	U_WEIGHTS,
	U_OFFSETS,
	U_FONT_SURFACE_SIZE,
	U_CUBIC_SAMPLING,
	U_COUNT
};
typedef struct Shader {
	GLuint program;
	GLint uniforms[U_COUNT];
} Shader;

enum ShaderError {
	SHADER_ERROR_NONE,
	SHADER_ERROR_VERTEX,
	SHADER_ERROR_FRAGMENT,
	SHADER_ERROR_LINKING
};

typedef struct _VertexNew {
	float2 pos;
	float edge_count;
	LineSegment edge[6];
} VertexNew;

typedef struct _VertexRect {
	float2 pos;
	float2 center;
	float radius;
	float distance;
} VertexRect;

typedef struct _VertexText {
	float2 pos;
	float u, v;
	Color color;
} VertexText;

typedef struct _VertexTexture {
	float2 pos;
	float u, v;
} VertexTexture;

typedef struct _VertexClipTransfer {
	float2 pos;
} VertexClipTransfer;

#endif