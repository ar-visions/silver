#include <obj-gfx/gfx.h>
#include <unistd.h>

unsigned int power_2(unsigned int v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
	return v;
}

void Gfx_clip_perform(Gfx gfx, bool clear, bool stroke) {
	if (gfx->path.count >= 1) {
		Clip *last_clip = call(gfx->clips, last);
		Rect r;
		bool is_rect = call(gfx, is_rect_path, gfx->path, NULL, NULL, &r);
        int clip_w = gfx->w, clip_h = gfx->h;
        Vec2 scale = { 1, 1 };
        float clip_L, clip_R, clip_T, clip_B;
		if (!is_rect) {
			call(gfx, path_bbox, gfx->path, &r);
			float L = r.x, T = r.y;
			float R = r.x + r.w, B = r.y + r.h;
			call(gfx, to_screen, L, T, &clip_L, &clip_T);
			call(gfx, to_screen, R, B, &clip_R, &clip_B);
			clip_T = clamp(clip_T, 0, gfx->h);
			clip_B = clamp(clip_B, 0, gfx->h);
			clip_L = clamp(clip_L, 0, gfx->w);
			clip_R = clamp(clip_R, 0, gfx->w);
		}
		Surface surface_clip = !is_rect ? class_call(Surface, cache_fetch, gfx, clip_w, clip_h, SURFACE_RGBA) : NULL;
		bool new = FALSE;
		if (!is_rect && !surface_clip) {
			surface_clip = class_call(Surface, new_rgba, gfx, clip_w, clip_h, NULL, 0, FALSE);
			new = TRUE;
		}
        Clip *clip = list_push(gfx->clips, NULL);
		clip->scale = scale;
		clip->surface = surface_clip;
		clip->last_surface = surface_clip ? surface_clip : (last_clip ? last_clip->last_surface : NULL);
		clip->u[0] = 0;
		clip->u[1] = 0;
		clip->u[2] = 1;
		clip->u[3] = 1;
		bool copy_coords = FALSE;
		if (is_rect) {
			new = TRUE;
			float L = r.x, T = r.y;
			float R = r.x + r.w, B = r.y + r.h;
			call(gfx, to_screen, L, T, &clip_L, &clip_T);
			call(gfx, to_screen, R, B, &clip_R, &clip_B);
			clip->u[0] = clip_L / (float)gfx->w;
			clip->u[1] = clip_T / (float)gfx->h;
			clip->u[2] = clip_R / (float)gfx->w;
			clip->u[3] = clip_B / (float)gfx->h;
		} else if (last_clip) {
			memcpy(clip->u, last_clip->u, sizeof(clip->u));
			copy_coords = TRUE;
		}
		if (last_clip && !copy_coords) {
			clip->u[0] = max(clip->u[0], last_clip->u[0]);
			clip->u[1] = max(clip->u[1], last_clip->u[1]);
			clip->u[2] = min(clip->u[2], last_clip->u[2]);
			clip->u[3] = min(clip->u[3], last_clip->u[3]);
		}
		if (clip->last_surface) {
			uint framebuffer;
			glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&framebuffer);
			glBindFramebuffer(GL_FRAMEBUFFER, gfx_surface_framebuffer(clip->last_surface));
			if (!new) {
				float cc[4];
				glGetFloatv(GL_COLOR_CLEAR_VALUE, cc);
				glClearColor(1.0, 1.0, 1.0, 0.0);
				glClear(GL_COLOR_BUFFER_BIT);
				glClearColor(cc[0], cc[1], cc[2], cc[3]);
			}
			enum ShaderType shader = SHADER_NEW;
			bool blend = FALSE;
			int vsize = sizeof(VertexNew), n_verts = stroke ?
                call(gfx, stroke_shape) : call(gfx, fill_verts, &vsize, &shader, &blend);
			glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
			glBufferData(GL_ARRAY_BUFFER, n_verts * vsize, gfx->vbuffer, GL_STATIC_DRAW);
			glEnable(GL_BLEND);
			glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			call(gfx, shaders_use, shader, NULL, TRUE);
			gfx_clip_surface(gfx, shader, last_clip);
			glDrawArrays(GL_TRIANGLES, 0, n_verts);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		}
	}
	if (clear)
		call(gfx, new_path);
}

void Gfx_clip_stroke(Gfx gfx, bool clear) {
	gfx_clip_perform(gfx, clear, TRUE);
}

void Gfx_clip(Gfx gfx, bool clear) {
	gfx_clip_perform(gfx, clear, FALSE);
}

bool gfx_unclip(Gfx gfx) {
	Clip *clip = list_last(gfx->clips);
	if (clip) {
		release(clip);
        list_pop(gfx->clips);
	}
	Clip *last = list_last(gfx->clips);
	glBindTexture(GL_TEXTURE_2D, (last && last->last_surface) ? last->last_surface->tx : 0);
	return TRUE;
}