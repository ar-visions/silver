#include <obj-gfx/gfx.h>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include <obj-gfx/stb_image.h>
#include <obj-gfx/stb_image_write.h>

implement(Surface)

String Surface_to_string(Surface self) {
	
}

int Surface_divisible(int n, int d) {
	if (n % d == 0)
		return n;
	return n + (d - n % d);
}

uint Surface_framebuffer(Surface self) {
	if (self) {
		if (self->framebuffer == 0) {
			GLuint framebuffer;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer);
			glGenFramebuffers(1, &self->framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, self->framebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, self->tx, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		}
		return self->framebuffer;
	}
	return 0;
}

Surface Surface_cache_fetch(Gfx gfx, int w, int h, enum SurfaceType type) {
	for (int i = 0; i < gfx->surface_cache_size; i++) {
		GfxSurface *s = gfx->surface_cache[i];
		if (s && s->w == w && s->h == h && s->type == type) {
			gfx->surface_cache[i] = NULL;
			s->image = FALSE;
			s->refs = 1;
			return s;
		}
	}
	return NULL;
}

void Surface_cache_update(Surface self) {
    Gfx gfx = self->gfx;
	long oldest = -1;
	int index = -1;
	for (int i = 0; i < gfx->surface_cache_size; i++) {
		Surface s = gfx->surface_cache[i];
		if (!s) {
			index = i;
			break;
		}
		if (index == -1 || s->cached_at < oldest) {
			oldest = s->cached_at;
			index = i;
		}
	}
	if (index != -1) {
		Surface surf_free = gfx->surface_cache[index];
		if (surf_free) {
			if (surf_free->tx)
				glDeleteTextures(1, &surf_free->tx);
			if (surf_free->framebuffer)
				glDeleteFramebuffers(1, &surf_free->framebuffer);
			free(surf_free);
		}
		self->cached_at = gfx_millis();
		gfx->surface_cache[index] = self;
	}
}

Surface Surface_alloc(Gfx gfx, int w, int h, int stride, enum SurfaceType type) {
	Surface self = new(Surface);
	self->w = w;
	self->h = h;
	self->stride = stride;
	self->type = type;
	self->refs = 1;
	glGenTextures(1, &self->tx);
	return self;
}

void Surface_texture_clamp(Surface self, bool c) {
	glBindTexture(GL_TEXTURE_2D, self->tx);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, c ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, c ? GL_CLAMP_TO_EDGE : GL_REPEAT);
}

Surface Surface_resample(Surface self, int w, int h, bool flip) {
    call(gfx, push);
	gfx->state->mat = class_call(Matrix44, ident);

	uint framebuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, (uint *)&framebuffer);

	Surface surface = class_call(Surface, cache_fetch, gfx, w, h, SURFACE_RGBA);
	if (!surface) {
		surface = class_call(Surface, alloc, gfx, w, h, 0, SURFACE_RGBA);
		call(surface, linear, true);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, (u_char *)NULL);
		call(surface, framebuffer);
		surface->type = SURFACE_RGBA;
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, call(surface, framebuffer));
	}
	VertexTexture *v = (VertexTexture *)gfx->vbuffer;
	float top = flip ? 0 : 1, left = 0, bot = flip ? 1 : 0, right = 1;
	v->pos = (Vec2) { 0, 0 };
	v->u = left; v->v = top;
	v++;
	v->pos = (Vec2) { surface->w, 0 };
	v->u = right; v->v = top;
	v++;
	v->pos = (Vec2) { surface->w, surface->h };
	v->u = right; v->v = bot;
	v++;
	v->pos = (Vec2) { 0, 0 };
	v->u = left; v->v = top;
	v++;
	v->pos = (Vec2) { surface->w, surface->h };
	v->u = right; v->v = bot;
	v++;
	v->pos = (Vec2) { 0, surface->h };
	v->u = left; v->v = bot;
	gfx->state->surface_dst = surface;
	gfx->state->surface_src = surf;
	glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
	glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(VertexTexture), gfx->vbuffer, GL_STATIC_DRAW);
	call(gfx, shaders_use, SHADER_RESAMPLE, NULL, FALSE);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	call(gfx, pop);
	return surface;
}

Surface Surface_image(Gfx gfx, char *filename, bool store) {
	int w = 0, h = 0, n = 0;
	u_char *data = stbi_load(filename, &w, &h, &n, 4);
	if (!data)
		return NULL;
	Surface self = class_call(Surface, new_rgba, gfx, w, h, (RGBA *)data, w * 4, store);
	if (self)
		self->image = TRUE;
	if (!store)
		free(data);
	return self;
}

Surface Surface_image_with_bytes(Gfx gfx, uchar *bytes, int length, bool store) {
	int w = 0, h = 0, comp = 4;
	unsigned char *data = stbi_load_from_memory(bytes, length, &w, &h, &comp, 0);
	if (!data)
		return NULL;
	Surface self = gfx_surface_create_rgba(gfx, w, h, (RGBA *)data, w, store);
	if (self)
		self->image = TRUE;
	if (!store)
		free(data);
	return self;
}

bool Surface_write(Surface self, char *filename) {
	if (!self)
		return FALSE;
	if (!self->bytes) {
		call(self, read);
		if (!self->bytes)
			return FALSE;
	}
	return stbi_write_png((char const *)filename, self->w, self->h, 4, self->bytes,
		call(self, stride)) >= 0;
}

bool Surface_write_callback(Surface self, void(*callback_func)(void *, void *, int), void *context) {
	if (!self)
		return FALSE;
	if (!self->bytes) {
		call(self, read);
		if (!self->bytes)
			return FALSE;
	}
	return stbi_write_png_to_func(callback_func, context, self->w, self->h, 4, self->bytes,
		call(self, stride)) >= 0;
}

int Surface_channels(Surface self) {
	switch (self->type) {
		case SURFACE_GRAY: return 1;
		case SURFACE_RGBA: return 4;
		case SURFACE_YUVP: return 4;
		default:
			return 0;
	}
	return 0;
}

int Surface_stride(Surface self) {
	int chans = call(self, channels);
	return self->stride > 0 ? self->stride : self->w * chans;
}

void Surface_update_rgba(Surface self, int w, int h, RGBA *rgba, int stride, bool store) {
	glBindTexture(GL_TEXTURE_2D, self->tx);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	self->w = w;
	self->h = h;
	if (store) {
		if (self->bytes)
			free(self->bytes);
		self->bytes = (u_char *)rgba;
	}
	self->stride = stride > 0 ? stride : gfx_surface_stride(gfx, self);
}

void Surface_yuvp(Gfx gfx, int w, int h, uchar *y_plane, int y_stride,
        uchar *u_plane, int u_stride, uchar *v_plane, int v_stride) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	Surface tx[3];
	int texture_defs[3] = { GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2 };
	int strides[3] = { y_stride, u_stride, v_stride };
	u_char *bufs[3] = { y_plane, u_plane, v_plane };
	for (int i = 0; i < 3; i++) {
		int tx_w = i == 0 ? w : w / 2;
		int tx_h = i == 0 ? h : h / 2;
		tx[i] = class_call(Surface, cache_fetch, gfx, tx_w, tx_h, SURFACE_GRAY);
		if (!tx[i]) {
			tx[i] = class_call(Surface, alloc, gfx, tx_w, tx_h, strides[i], SURFACE_GRAY);
		}
		glActiveTexture(texture_defs[i]);
		glBindTexture(GL_TEXTURE_2D, tx[i]->tx);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, strides[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE,
			tx_w, tx_h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, bufs[i]);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	}
	glActiveTexture(GL_TEXTURE0);
	VertexTexture *v = (VertexTexture *)gfx->vbuffer;
	v->pos = (Vec2) { 0, 0 };
	v->u = 0; v->v = 0.0;
	v++;
	v->pos = (Vec2) { w, 0 };
	v->u = 1; v->v = 0.0;
	v++;
	v->pos = (Vec2) { w, h };
	v->u = 1; v->v = 1.0;
	v++;
	v->pos = (Vec2) { 0, 0 };
	v->u = 0; v->v = 0.0;
	v++;
	v->pos = (Vec2) { w, h };
	v->u = 1; v->v = 1.0;
	v++;
	v->pos = (Vec2) { 0, h };
	v->u = 0; v->v = 1.0;
	v++;
	
	glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
	glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(VertexTexture), gfx->vbuffer, GL_STATIC_DRAW);
	call(gfx, shaders_use, SHADER_PLANAR, NULL, FALSE);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	for (int i = 0; i < 3; i++)
		release(tx[i]);
}

void Surface_update_yuvp(Surface self, int w, int h,
		uchar *y_plane, int y_stride, uchar *u_plane, int u_stride, uchar *v_plane, int v_stride) {
	uint framebuffer;
	self->w = w;
	self->h = h; // resize texture if changed
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, call(self, framebuffer));
	glBindTexture(GL_TEXTURE_2D, self->tx);

	call(gfx, push);
	call(gfx, surface_dst, self);
	gfx->state.mat = class_call(Mat44, ident);
	class_call(Surface, yuvp, gfx, w, h, y_plane, y_stride, u_plane, u_stride, v_plane, v_stride);
	call(gfx, pop);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
}

Surface Surface_new_yuvp(Gfx gfx, int w, int h,
		uchar *y_plane, int y_stride, uchar *u_plane, int u_stride, uchar *v_plane, int v_stride) {
	uint framebuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer);
	Surface self = class_call(Surface, cache_fetch, gfx, w, h, SURFACE_RGBA);
	if (!self)
		self = class_call(Surface, new_rgba, gfx, w, h, NULL, gfx_divisible(w, 16), FALSE);
	self->type = SURFACE_YUVP;
	self->image = TRUE;
	if (y_plane && u_plane && v_plane)
		call(self, update_yuvp, w, h, y_plane, y_stride, u_plane, u_stride, v_plane, v_stride);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	return self;
}

GLenum Surface_gl_type(Surface self) {
	switch (self->type) {
		case SURFACE_GRAY: return GL_LUMINANCE;
		case SURFACE_RGBA: return GL_RGBA;
		default:
			return 0;
	}
}

void Surface_readable(Surface self, bool readable) {
	if ((self->ppo_len != 0) == readable)
		return;
	if (readable) {
		GLenum type = call(self, gl_type);
		int channels = type == GL_RGBA ? 4 : 1;
		self->stride = call(self, divisible, self->w * channels, 16);
		self->ppo_len = self->stride * self->h;
		self->bytes = (u_char *)malloc(self->ppo_len);
		glGenBuffers(1, &self->ppo);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, self->ppo);
		glPixelStorei(GL_PACK_ROW_LENGTH, self->stride); // is this stored in state of ppo?
		glBufferData(GL_PIXEL_PACK_BUFFER, self->ppo_len, 0, GL_DYNAMIC_READ);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	} else {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glDeleteBuffers(1, &self->ppo);
		free(self->bytes);
		self->bytes = 0;
		self->ppo_len = 0;
		self->ppo = 0;
	}
}

void Surface_free(Surface self) {
    call(self, readable, false);
	call(self, cache_update);
}

int Surface_read(Surface self) {
	call(self, readable, TRUE);
	GLenum format = call(self, gl_type);
	if (surface->ppo_len == 0)
		return 0;
	GLuint framebuffer;
	int stride = cal(self, stride);
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, call(self, framebuffer));
	glBindBuffer(GL_PIXEL_PACK_BUFFER, surface->ppo);
	glPixelStorei(GL_PACK_ROW_LENGTH, stride / call(self, channels));
	glReadPixels(0, 0, self->w, self->h, format, GL_UNSIGNED_BYTE, 0);
	u_char *b = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, self->ppo_len, GL_MAP_READ_BIT);
	int len = 0;
	if (b) {
		len = self->ppo_len;
		if (self->image) {
			memcpy(self->bytes, b, self->ppo_len);
		} else {
			u_char *p = self->bytes;
			for (int y = self->h - 1; y >= 0; y--, p += stride)
				memcpy(p, &b[stride * y], stride);
		}
	}
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	return len;
}

bool Surface_resize(Surface self, int w, int h) {
	if (self && self->w == w && self->h == h)
		return TRUE;
	self->w = w;
	self->h = h;
	GLenum type = call(self, gl_type);
	glBindTexture(GL_TEXTURE_2D, self->tx);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, type, GL_UNSIGNED_BYTE, NULL);
	if (self->bytes) {
		free(surface->bytes);
		self->bytes = NULL;
	}
	surface->stride = w;
	if (surface->ppo_len) {
		call(self, readable, FALSE);
		call(self, readable, TRUE);
	}
	return TRUE;
}

void Surface_linear(Surface self, bool linear) {
	glBindTexture(GL_TEXTURE_2D, self->tx);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, linear ? GL_LINEAR : GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, linear ? GL_LINEAR : GL_NEAREST);
}

Surface Surface_new_gray(Gfx gfx, int w, int h, uchar *bytes, int stride, bool store) {
	Surface self = class_call(Surface, alloc, gfx, w, h, stride, SURFACE_GRAY);
	call(self, linear, TRUE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, (u_char *)bytes);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	if (store)
		self->bytes = (u_char *)bytes;
	self->stride = stride > 0 ? stride : call(self, stride);
	return self;
}

Surface Surface_new_rgba(Gfx gfx, int w, int h, RGBA *bytes, int stride, bool store) {
	Surface self = class_call(Surface, alloc, gfx, w, h, stride, SURFACE_RGBA);
	call(self, linear, TRUE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, stride / 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, (u_char *)bytes);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	if (store) {
		self->bytes = (u_char *)bytes;
		self->stride = stride > 0 ? stride : call(self, stride);
	}
	return surface;
}

Surface Surface_new_rgba_empty(Gfx gfx, int w, int h) {
	Surface self = class_call(Surface, new_rgba, gfx, w, h, NULL, call(self, divisible, w * 4, 16), FALSE);
	return self;
}
