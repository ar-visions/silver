#include <obj-gfx/gfx.h>
#include <obj-gfx/shaders.h>

#include "gfx.h"
#include "util.h"
#include "shaders-build.h"

static const char *vertex_shaders[SHADER_COUNT] = {
    __shaders_new_vert,
	__shaders_rect_vert,
    __shaders_planar_vert,
    __shaders_text_vert,
    __shaders_linear_vert,
    __shaders_gaussianx_vert,
    __shaders_gaussiany_vert,
    __shaders_resample_vert,
};

static const char *fragment_shaders[SHADER_COUNT] = {
    __shaders_new_frag,
	__shaders_rect_frag,
    __shaders_planar_frag,
    __shaders_text_frag,
    __shaders_linear_frag,
    __shaders_gaussianx_frag,
    __shaders_gaussiany_frag,
    __shaders_resample_frag,
};

void Gfx_shaders_attribs(Gfx *gfx, int id, Shader *shader) {
	char a[32];
	char b[32];
	switch (id) {
		case SHADER_NEW:
			glBindAttribLocation(shader->program, SN_POSITION, "position");
			glBindAttribLocation(shader->program, SN_EDGE_COUNT, "edge_count");
			for (int i = 0; i < 6; i++) {
				sprintf(a, "edge%d_a", i);
				sprintf(b, "edge%d_b", i);
				glBindAttribLocation(shader->program, SN_EDGE0_A + (i * 2), a);
				glBindAttribLocation(shader->program, SN_EDGE0_A + (i * 2) + 1, b);
			}
			break;
		case SHADER_RECT:
			glBindAttribLocation(shader->program, 0, "position");
			glBindAttribLocation(shader->program, 1, "center");
			glBindAttribLocation(shader->program, 2, "radius");
			glBindAttribLocation(shader->program, 3, "distance");
			break;
		case SHADER_PLANAR:
			glBindAttribLocation(shader->program, 0, "position");
			glBindAttribLocation(shader->program, 1, "uv");
			break;
		case SHADER_TEXT:
		case SHADER_GAUSSIAN_X:
		case SHADER_GAUSSIAN_Y:
		case SHADER_LINEAR:
		case SHADER_RESAMPLE:
			glBindAttribLocation(shader->program, 0, "position");
			glBindAttribLocation(shader->program, 1, "uv");
			if (id == SHADER_TEXT)
				glBindAttribLocation(shader->program, 2, "gcolor");
			break;
	}
}

#define BUFFER_OFFSET(i) ((char *)NULL + (i))

void Gfx_clip_surface(Gfx *gfx, enum ShaderType type, GfxClip *clip) {
	Shader *shader = &gfx->shaders[type];
	BOOL textured_clip = clip && clip->last_surface;
	glUniform1i(shader->uniforms[U_CLIP_TEXTURE], 0);
	if (clip) {
		glUniform2f(shader->uniforms[U_CLIP_SIZE], 1.0, textured_clip ? 1.0 : 0.0);
		glUniform4fv(shader->uniforms[U_CLIP_RECT], 1, clip->u);
	} else {
		glUniform2f(shader->uniforms[U_CLIP_SIZE], 0.0, 0.0);
	}
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textured_clip ? clip->last_surface->tx : 0);
}

void Gfx_debug(Gfx *gfx, BOOL b) {
	gfx->debug = b;
}

Shader *Gfx_shaders_lookup(Gfx *gfx, enum ShaderType type) {
	return &gfx->shaders[type];
}

float Gfx_scaled_feather(Gfx *gfx) {
	float f = gfx->state.feather;
	float sx, sy;
	call(gfx, get_scales, &sx, &sy);
	return ((f / sx) + (f / sy)) / 2;
}

void Gfx_shaders_use(Gfx *gfx, enum ShaderType type, void *shader_args, BOOL clipping_op) {
	Shader *shader = &gfx->shaders[type];
	GfxState *state = &gfx->state;
	GfxSurface *surf_src = gfx->state.surface_src;
	GfxSurface *surf_dst = gfx->state.surface_dst;
	if (!clipping_op) {
		if (surf_dst) {
			glBindFramebuffer(GL_FRAMEBUFFER, call(surf_dst, framebuffer));
		} else
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	glUseProgram(shader->program);
	int p = 0;
	switch (type) {
		case SHADER_NEW:
		case SHADER_TEXT:
		case SHADER_RECT:
			switch (type) {
				case SHADER_NEW:
					glEnableVertexAttribArray(SN_POSITION);
					glVertexAttribPointer(SN_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(VertexNew), BUFFER_OFFSET(p)); 	p += sizeof(float2);
					glVertexAttribPointer(SN_EDGE_COUNT, 1, GL_FLOAT, GL_FALSE, sizeof(VertexNew), BUFFER_OFFSET(p)); 	p += sizeof(float);
					for (int i = 0; i < 6; i++) {
						glEnableVertexAttribArray(SN_EDGE0_A + (i * 2));
						glEnableVertexAttribArray(SN_EDGE0_A + (i * 2) + 1);
						glVertexAttribPointer(SN_EDGE0_A + (i * 2), 2, GL_FLOAT, GL_FALSE, sizeof(VertexNew), BUFFER_OFFSET(p)); 	 p += sizeof(float2);
						glVertexAttribPointer(SN_EDGE0_A + (i * 2) + 1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexNew), BUFFER_OFFSET(p)); p += sizeof(float2);
					}
					glUniform1f(shader->uniforms[U_FEATHER], call(gfx, scaled_feather));
					break;
				case SHADER_TEXT:
					glEnableVertexAttribArray(ST_POSITION);
					glVertexAttribPointer(ST_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(VertexText), BUFFER_OFFSET(p)); p += sizeof(float2);
					glEnableVertexAttribArray(ST_UV);
					glVertexAttribPointer(ST_UV, 2, GL_FLOAT, GL_FALSE, sizeof(VertexText), BUFFER_OFFSET(p)); p += sizeof(float2);
					glEnableVertexAttribArray(ST_COLOR);
					glVertexAttribPointer(ST_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(VertexText), BUFFER_OFFSET(p));

					glUniform1i(shader->uniforms[U_FONT_TEXTURE], 2);
					if (state->font) {
						glActiveTexture(GL_TEXTURE2);
						glBindTexture(GL_TEXTURE_2D, state->font->surface->tx);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						glActiveTexture(GL_TEXTURE0);
						glUniform2f(shader->uniforms[U_FONT_SURFACE_SIZE], state->font->surface->w, state->font->surface->h);
						glUniform1f(shader->uniforms[U_CUBIC_SAMPLING], state->arb_rotation ? 1.0 : 0.0);
					}
					break;
				case SHADER_RECT:
					glEnableVertexAttribArray(SRT_POSITION);
					glVertexAttribPointer(SRT_POSITION, 2, GL_FLOAT, GL_FALSE, sizeof(VertexRect), BUFFER_OFFSET(p)); p += sizeof(float2);
					glEnableVertexAttribArray(SRT_CENTER);
					glVertexAttribPointer(SRT_CENTER, 2, GL_FLOAT, GL_FALSE, sizeof(VertexRect), BUFFER_OFFSET(p)); p += sizeof(float2);
					glEnableVertexAttribArray(SRT_RADIUS);
					glVertexAttribPointer(SRT_RADIUS, 1, GL_FLOAT, GL_FALSE, sizeof(VertexRect), BUFFER_OFFSET(p)); p += sizeof(float);
					glEnableVertexAttribArray(SRT_DISTANCE);
					glVertexAttribPointer(SRT_DISTANCE, 1, GL_FLOAT, GL_FALSE, sizeof(VertexRect), BUFFER_OFFSET(p));
					glUniform1f(shader->uniforms[U_FEATHER], call(gfx, scaled_feather));
					glUniform1f(shader->uniforms[U_LINEAR_MULTIPLY], state->linear_multiply);
					break;
				default:
					break;
			}
			glUniform1i(shader->uniforms[U_CLIP_TEXTURE], 0);
			glUniform1i(shader->uniforms[U_SURFACE_TEXTURE], 1);
			glUniform2f(shader->uniforms[U_CLIP_SIZE], 0.0, 0.0);
			if (surf_src) {
				glActiveTexture(GL_TEXTURE1);
				glBindTexture(GL_TEXTURE_2D, surf_src->tx);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); 
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glActiveTexture(GL_TEXTURE0);
				glUniform2f(shader->uniforms[U_SURFACE_SIZE], surf_src->w, surf_src->h);
			} else
				glUniform2f(shader->uniforms[U_SURFACE_SIZE], 0.0, 0.0);
			glUniform2f(shader->uniforms[U_VIEWPORT_SIZE], gfx->w, gfx->h);
			glUniform2f(shader->uniforms[U_SURFACE_V00], state->surface_v00.x, state->surface_v00.y);
			glUniform2f(shader->uniforms[U_SURFACE_V10], state->surface_v10.x, state->surface_v10.y);
			glUniform2f(shader->uniforms[U_SURFACE_V11], state->surface_v11.x, state->surface_v11.y);
			glUniform2f(shader->uniforms[U_SURFACE_V01], state->surface_v01.x, state->surface_v01.y);
			Color color = gfx->state.color;
			color.a *= gfx->state.opacity;
			glUniform4fv(shader->uniforms[U_COLOR], 1, (GLvoid *)&color);
			glUniform1f(shader->uniforms[U_LINEAR_BLEND], state->linear_blend);
			glUniform4fv(shader->uniforms[U_LINEAR_COLOR_FROM], 1, (float *)&state->linear_color_from);
			glUniform4fv(shader->uniforms[U_LINEAR_COLOR_TO], 1, (float *)&state->linear_color_to);
			glUniform2fv(shader->uniforms[U_LINEAR_FROM], 1, (float *)&state->linear_from);
			glUniform2fv(shader->uniforms[U_LINEAR_TO], 1, (float *)&state->linear_to);
			break;
		case SHADER_PLANAR:
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexTexture), BUFFER_OFFSET(p)); p += sizeof(float2);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexTexture), BUFFER_OFFSET(p));
			glUniform1i(shader->uniforms[U_PLANAR_Y], 0);
			glUniform1i(shader->uniforms[U_PLANAR_U], 1);
			glUniform1i(shader->uniforms[U_PLANAR_V], 2);
			break;
		case SHADER_LINEAR: {
			GfxLinearArgs *args = shader_args;
			glBindTexture(GL_TEXTURE_2D, args->src ? args->src->tx : 0);
			glUniform1i(shader->uniforms[U_SURFACE_TEXTURE], 0);
			glUniform1f(shader->uniforms[U_SURFACE_BLEND], args->src ? args->src_blend : 0);
			glUniform4fv(shader->uniforms[U_LINEAR_COLOR_FROM], 1, (float *)&args->color_from);
			glUniform4fv(shader->uniforms[U_LINEAR_COLOR_TO], 1, (float *)&args->color_to);
			glUniform2fv(shader->uniforms[U_LINEAR_FROM], 1, (float *)&args->from);
			glUniform2fv(shader->uniforms[U_LINEAR_TO], 1, (float *)&args->to);
			break;
		}
		case SHADER_RESAMPLE:
		case SHADER_GAUSSIAN_X:
		case SHADER_GAUSSIAN_Y: {
			GfxGaussianArgs *args = shader_args;
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexTexture), BUFFER_OFFSET(p)); p += sizeof(float2);
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexTexture), BUFFER_OFFSET(p));
			
			if (type != SHADER_RESAMPLE) {
				glUniform2f(shader->uniforms[U_REDUCTION_SIZE], surf_src->w, surf_src->h);
				glUniform1fv(shader->uniforms[U_WEIGHTS], 9, args->weights); // 0.134535 0.130417 0.118804 0.101701 0.081812
				glUniform1fv(shader->uniforms[U_OFFSETS], 8, args->offsets); // 1, 2, 3, 4
			} else {
				glUniform2f(shader->uniforms[U_SURFACE_SIZE], surf_src->w, surf_src->h);
			}
			if (surf_src) {
				glActiveTexture(GL_TEXTURE0);
				glUniform1i(shader->uniforms[U_SURFACE_TEXTURE], 0);
				glBindTexture(GL_TEXTURE_2D, surf_src->tx);
			}
			break;
		}
		default:
			break;
	}
	Mat44 mat;
	mat44_identity(&mat);
	mat44_mul(&mat, &gfx->state.proj, &gfx->state.mat);
	glUniformMatrix4fv(shader->uniforms[U_MATRIX], 1, GL_FALSE, mat.m);
}

void Gfx_shaders_uniforms(Gfx *gfx, int id, Shader *shader) {
	switch (id) {
		case SHADER_NEW:
		case SHADER_RECT:
		case SHADER_TEXT:
			if (id == SHADER_NEW || id == SHADER_RECT) {
				shader->uniforms[U_FEATHER] = glGetUniformLocation(shader->program, "feather");
				shader->uniforms[U_LINEAR_MULTIPLY] = glGetUniformLocation(shader->program, "linear_multiply");
			} else {
				shader->uniforms[U_FONT_TEXTURE] = glGetUniformLocation(shader->program, "font_texture");
				shader->uniforms[U_FONT_SURFACE_SIZE] = glGetUniformLocation(shader->program, "font_surface_size");
				shader->uniforms[U_CUBIC_SAMPLING] = glGetUniformLocation(shader->program, "cubic_sampling");
			}
			shader->uniforms[U_MATRIX] = glGetUniformLocation(shader->program, "matrix");
			shader->uniforms[U_SURFACE_V00] = glGetUniformLocation(shader->program, "surface_v00");
			shader->uniforms[U_SURFACE_V10] = glGetUniformLocation(shader->program, "surface_v10");
			shader->uniforms[U_SURFACE_V11] = glGetUniformLocation(shader->program, "surface_v11");
			shader->uniforms[U_SURFACE_V01] = glGetUniformLocation(shader->program, "surface_v01");
			shader->uniforms[U_VIEWPORT_SIZE] = glGetUniformLocation(shader->program, "viewport_size");
			shader->uniforms[U_CLIP_SIZE] = glGetUniformLocation(shader->program, "clip_size");
			shader->uniforms[U_CLIP_RECT] = glGetUniformLocation(shader->program, "clip_rect");
			shader->uniforms[U_CLIP_TEXTURE] = glGetUniformLocation(shader->program, "clip_texture");
			shader->uniforms[U_SURFACE_SIZE] = glGetUniformLocation(shader->program, "surface_size");
			shader->uniforms[U_SURFACE_TEXTURE] = glGetUniformLocation(shader->program, "surface_texture");
			shader->uniforms[U_COLOR] = glGetUniformLocation(shader->program, "color");
			shader->uniforms[U_LINEAR_BLEND] = glGetUniformLocation(shader->program, "linear_blend");
			shader->uniforms[U_LINEAR_COLOR_FROM] = glGetUniformLocation(shader->program, "linear_color_from");
			shader->uniforms[U_LINEAR_COLOR_TO] = glGetUniformLocation(shader->program, "linear_color_to");
			shader->uniforms[U_LINEAR_FROM] = glGetUniformLocation(shader->program, "linear_from");
			shader->uniforms[U_LINEAR_TO] = glGetUniformLocation(shader->program, "linear_to");
			break;
		case SHADER_PLANAR:
			shader->uniforms[U_MATRIX] = glGetUniformLocation(shader->program, "matrix");
			shader->uniforms[U_PLANAR_Y] = glGetUniformLocation(shader->program, "texture0");
			shader->uniforms[U_PLANAR_U] = glGetUniformLocation(shader->program, "texture1");
			shader->uniforms[U_PLANAR_V] = glGetUniformLocation(shader->program, "texture2");
			break;
		case SHADER_LINEAR:
			shader->uniforms[U_MATRIX] = glGetUniformLocation(shader->program, "matrix");
			shader->uniforms[U_SURFACE_TEXTURE] = glGetUniformLocation(shader->program, "surface_texture");
			shader->uniforms[U_SURFACE_BLEND] = glGetUniformLocation(shader->program, "surface_blend");
			shader->uniforms[U_LINEAR_COLOR_FROM] = glGetUniformLocation(shader->program, "color_from");
			shader->uniforms[U_LINEAR_COLOR_TO] = glGetUniformLocation(shader->program, "color_to");
			shader->uniforms[U_LINEAR_FROM] = glGetUniformLocation(shader->program, "from");
			shader->uniforms[U_LINEAR_TO] = glGetUniformLocation(shader->program, "to");
			break;
		case SHADER_RESAMPLE:
		case SHADER_GAUSSIAN_X:
		case SHADER_GAUSSIAN_Y:
			shader->uniforms[U_MATRIX] = glGetUniformLocation(shader->program, "matrix");
			shader->uniforms[U_SURFACE_TEXTURE] = glGetUniformLocation(shader->program, "surface_texture");
			if (id != SHADER_RESAMPLE) {
				shader->uniforms[U_REDUCTION_SIZE] = glGetUniformLocation(shader->program, "reduction_size");
				shader->uniforms[U_WEIGHTS] = glGetUniformLocation(shader->program, "weights");
				shader->uniforms[U_OFFSETS] = glGetUniformLocation(shader->program, "offsets");
			} else {
				shader->uniforms[U_SURFACE_SIZE] = glGetUniformLocation(shader->program, "surface_size");
			}
			break;
	}
}

int Gfx_shaders_init(Gfx *gfx) {
	for (int i = 0; i < SHADER_COUNT; i++) {
		Shader *shader = &gfx->shaders[i];
		char info_log[256];
		GLint success;
		GLint vertex_shader, fragment_shader;

		vertex_shader = glCreateShader(GL_VERTEX_SHADER);
		const GLchar *vert = vertex_shaders[i];
		const GLchar *frag = fragment_shaders[i];
		glShaderSource(vertex_shader, 1, &vert, NULL);
		glCompileShader(vertex_shader);
		glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(vertex_shader, sizeof(info_log), NULL, info_log);
			fprintf(stderr, "shaders_init Error (Vertex)\n%s\n", info_log);
			return SHADER_ERROR_VERTEX;
		}
		fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fragment_shader, 1, &frag, NULL);
		glCompileShader(fragment_shader);
		glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			glGetShaderInfoLog(fragment_shader, sizeof(info_log), NULL, info_log);
			fprintf(stderr, "shaders_init Error (Fragment)\n%s\n", info_log);
			return SHADER_ERROR_FRAGMENT;
		}
		shader->program = glCreateProgram();
		glAttachShader(shader->program, vertex_shader);
		glAttachShader(shader->program, fragment_shader);
		
		call(gfx, shaders_attribs, i, shader);
		glLinkProgram(shader->program);
		call(gfx, shaders_uniforms, i, shader);
		
		glGetProgramiv(shader->program, GL_LINK_STATUS, &success);
		if (!success) {
			glGetProgramInfoLog(shader->program, sizeof(info_log), NULL, info_log);
			fprintf(stderr, "shaders_init Error (Linking)\n%s\n", info_log);
			return SHADER_ERROR_LINKING;
		}
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
	}
	return SHADER_ERROR_NONE;
}
