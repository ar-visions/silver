#include <obj-gfx/gfx.h>
#include <unistd.h>

void Gfx_color(Gfx gfx, Color c) {
    set(gfx->state, color, c);
}

void Gfx_linear_color_from(Gfx gfx, Color c) {
    set(gfx->state, linear_color_from, c);
}

void Gfx_linear_color_to(Gfx gfx, Color c) {
	set(gfx->state, linear_color_to, c);
}

void Gfx_linear_from(Gfx gfx, float x, float y) {
	gfx->state->linear_from = (float2) { x, y };
}

void Gfx_linear_to(Gfx gfx, float x, float y) {
	gfx->state->linear_to = (float2) { x, y };
}

void Gfx_linear_blend(Gfx gfx, float blend) {
	gfx->state->linear_blend = blend;
}

void Gfx_linear_multiply(Gfx gfx, float a) {
	gfx->state->linear_multiply = a;
}

void Gfx_stroke_width(Gfx gfx, float w) {
	gfx->state->stroke_width = w + (gfx->state->feather * gfx->state->feather_stroke_factor);
}

void Gfx_new_path(Gfx gfx) {
	call(gfx->path, clear);
	gfx->state->moved = false;
	gfx->state->new_path = true;
	gfx->state->cursor = (float2) { 0, 0 };
}

void Gfx_new_sub_path(Gfx gfx) {
	//ll_clear(&gfx->path, true);
	gfx->state->moved = false;
	gfx->state->new_path = true;
}

static inline
float2 Gfx_arc_seg(Gfx gfx, Segment *seg, float2 from, float cx, float cy, float radius, float rads_from, float rads) {
	float2 to = (float2) {
		cx + radius * cos(rads_from + rads),
		cy + radius * sin(rads_from + rads) };
	*seg = (Segment){.type = SEGMENT_ARC, .center = (float2){ cx, cy }, .a = *p_from, .b = to,
		.rads_from = rads_from, .rads = rads, .radius = radius, .moved = false};
	return to;
}

void Gfx_arc_to(Gfx gfx, float cx, float cy, float radius, float rads_from, float rads) {
	const float EPS = 0.0001;
	Segment seg;
	float2 from;
	if (gfx->state->new_path) {
		from = (float2) {
			cx + radius * cos(rads_from),
			cy + radius * sin(rads_from) };
	} else {
		from = gfx->state->cursor;
	}
	gfx_arc_seg(gfx, &seg, &from, cx, cy, radius, rads_from, rads);
	float dist = vec2_dist(from, gfx->state->cursor);
	seg.moved = gfx->state->new_path || (gfx->state->moved && dist < EPS);
	ll_push(&gfx->path, &seg);
	gfx->state->cursor = seg.b;
	gfx->state->moved = false;
	gfx->state->new_path = false;
}

void Gfx_bezier_to(Gfx gfx, float cp1_x, float cp1_y, float cp2_x, float cp2_y, float to_x, float to_y) {
	float2 from = gfx->state->cursor;
	gfx->state->cursor = (float2){ to_x, to_y };
	Segment seg = {.type = SEGMENT_BEZIER, .a = from, .b = gfx->state->cursor, .cp1 = (float2){ cp1_x, cp1_y }, .cp2 = (float2){ cp2_x, cp2_y }, .moved = gfx->state->moved};
	ll_push(&gfx->path, &seg);
	gfx->state->moved = false;
	gfx->state->new_path = false;
}

void Gfx_rect_to(Gfx gfx, float x0, float y0, float x1, float y1, float rads) {
	gfx->state->cursor = (float2){ x0, y0 };
	Segment seg = {.type = SEGMENT_RECT, .a = gfx->state->cursor, .b = (float2){ x1, y1 }, .radius = rads, .moved = true};
	ll_push(&gfx->path, &seg);
	gfx->state->moved = false;
	gfx->state->new_path = false;
}

void Gfx_rounded_rect(Gfx gfx, float x, float y, float w, float h, float r) {
	gfx_rect_to(gfx, x, y, x + w, y + h, r);
}

void Gfx_rect(Gfx gfx, float x, float y, float w, float h) {
	gfx_rect_to(gfx, x, y, x + w, y + h, 0.0);
}

void Gfx_line_to(Gfx gfx, float x, float y) {
	float2 from = gfx->state->cursor;
	gfx->state->cursor = (float2){ x, y };
	Segment seg = {.type = SEGMENT_LINE, .a = from, .b = gfx->state->cursor, .moved = gfx->state->moved};
	ll_push(&gfx->path, &seg);
	gfx->state->moved = false;
	gfx->state->new_path = false;
}

void Gfx_move_to(Gfx gfx, float x, float y) {
	gfx->state->cursor = (float2) { x, y };
	gfx->state->moved = true;
}

void Gfx_close_path(Gfx gfx) {
	Segment *last = ll_last(&gfx->path);
	if (last)
		last->close = true;
	gfx->state->moved = false;
	gfx->state->new_path = true;
}

static int
gfx_fill_rect(Gfx gfx, Rect r, float rx, float ry) {
	rx = min((float)r.w / 2.0, rx);
	ry = min((float)r.h / 2.0, ry);
	rx = min(rx, ry);
	float x0 = r.x, x1 = r.x + r.w;
	float y0 = r.y, y1 = r.y + r.h;
	VertexRect *v = (VertexRect *)gfx->vbuffer;
	float sx, sy;
	gfx_get_scales(gfx, &sx, &sy);
	float EPS = 0.001;
	float rt, yb;
	float n_verts = 6 * 9;
	// TL
	if (rx > EPS && ry > EPS) {
		float2 tl[6] = { { x0, y0 },     { x0 + rx, y0 },       { x0 + rx, y0 + ry },
					   { x0, y0 },     { x0 + rx, y0 + ry },  { x0, y0 + ry } };
		for (int i = 0; i < 6; i++) {
			v->pos = tl[i];
			v->center = (float2) { x0 + rx, y0 + ry };
			v->radius = rx;
			v->distance = 0.0;
			v++;
		}
		// TR
		rt = x1 - rx;
		float2 tr[6] = { { rt, y0 },     { rt + rx, y0 },       { rt + rx, y0 + ry },
					   { rt, y0 },     { rt + rx, y0 + ry },  { rt, y0 + ry } };
		for (int i = 0; i < 6; i++) {
			v->pos = tr[i];
			v->center = (float2) { rt, y0 + ry };
			v->radius = rx;
			v->distance = 0.0;
			v++;
		}
		// BL
		yb = y1 - ry;
		float2 bl[6] = { { x0, yb },     { x0 + rx, yb },       { x0 + rx, yb + ry },
					{ x0, yb },     { x0 + rx, yb + ry },  { x0, yb + ry } };
		for (int i = 0; i < 6; i++) {
			v->pos = bl[i];
			v->center = (float2) { x0 + rx, yb };
			v->radius = rx;
			v->distance = 0.0;
			v++;
		}
		// BR
		float2 br[6] = { { rt, yb },     { rt + rx, yb },       { rt + rx, yb + ry },
					{ rt, yb },     { rt + rx, yb + ry },  { rt, yb + ry } };
		for (int i = 0; i < 6; i++) {
			v->pos = br[i];
			v->center = (float2) { rt, yb };
			v->radius = rx;
			v->distance = 0.0;
			v++;
		}

		// Top
		float2 top[6] = { { x0 + rx, y0 },  { rt, y0 },      { rt, y0 + ry },
						{ x0 + rx, y0 },  { rt, y0 + ry }, { x0 + rx, y0 + ry } };
		float top_dist[6] = { 0.0001, 0.0001, ry, 0.0001, ry, ry, };
		for (int i = 0; i < 6; i++) {
			v->pos = top[i];
			v->center = (float2) { x0, y0 };
			v->radius = rx;
			v->distance = top_dist[i];
			v++;
		}

		// Bottom
		float2 bot[6] = { { x0 + rx, yb },  { rt, yb },      { rt, yb + ry },
						{ x0 + rx, yb },  { rt, yb + ry }, { x0 + rx, yb + ry } };
		float bot_dist[6] = { ry, ry, 0.0001, ry, 0.0001, 0.0001 };
		for (int i = 0; i < 6; i++) {
			v->pos = bot[i];
			v->center = (float2) { x0, y0 };
			v->radius = rx;
			v->distance = bot_dist[i];
			v++;
		}

		// Left
		float2 left[6] = { { x0, y0 + ry },  { x0 + rx, y0 + ry },  { x0 + rx, yb },
						{ x0, y0 + ry },  { x0 + rx, yb }, { x0, yb } };
		float left_dist[6] = { 0.0001, rx, rx, 0.0001, rx, 0.0001 };
		for (int i = 0; i < 6; i++) {
			v->pos = left[i];
			v->center = (float2) { x0, y0 };
			v->radius = rx;
			v->distance = left_dist[i];
			v++;
		}

		// Right
		float2 right[6] = { { rt, y0 + ry },  { x1, y0 + ry },  { x1, yb },
						{ rt, y0 + ry },  { x1, yb }, { rt, yb } };
		float right_dist[6] = { rx, 0.0001, 0.0001, rx, 0.0001, rx };
		for (int i = 0; i < 6; i++) {
			v->pos = right[i];
			v->center = (float2) { x0, y0 };
			v->radius = rx;
			v->distance = right_dist[i];
			v++;
		}
	} else {
		rx = ry = 0.0;
		rt = x1;
		yb = y1;
		n_verts = 6;
	}

	// Middle
	float2 middle[6] = { { x0 + rx, y0 + ry },  { rt, y0 + ry },  { rt, yb },
					   { x0 + rx, y0 + ry },  { rt, yb }, 		{ x0 + rx, yb } };
	float f = Gfx_scaled_feather(gfx);
	for (int i = 0; i < 6; i++) {
		v->pos = middle[i];
		v->center = (float2) { x0, y0 };
		v->radius = 0.0;
		v->distance = 100.0 * f;
		v++;
	}
	return n_verts;
}

static int
gfx_fill_shape(Gfx gfx, bool debug_interior) {
	Shape *shape = NULL;
	if (!shape_from_path(gfx, &gfx->path, &shape))
		return 0;
	int n_verts_alloc = 0;
	for (int o = 0; o < shape->n_outlines; o++)
		 n_verts_alloc += shape->outlines[o].tris->count * 3;
	if (gfx->vbuffer_size < n_verts_alloc)
		gfx_realloc_buffer(gfx, n_verts_alloc + (gfx->vbuffer_size >> 1));
	int n_verts = 0;
	int dupe = 1;
	VertexNew *v = (VertexNew *)gfx->vbuffer;
	for (int o = 0; o < shape->n_outlines; o++) {
		Outline *outline = &shape->outlines[o];
		ll_each(outline->tris, Tri, t) {
			Edge *edges[6];
			int n_edges = 0;
			dupe++;
			if (!outline->no_feather) {
				for (int i = 0; i < 3; i++) {
					Point *p = t->points[i];
					for (int e = 0; e < p->n_edges; e++) {
						if (p->edges[e]->user_data != dupe) {
							int ie = n_edges++;
							edges[ie] = p->edges[e];
							edges[ie]->user_data = dupe;
						}
					}
				}
			}
			for (int i = 0; i < 3; i++, v++, n_verts++) {
				Point *p = t->points[i];
				v->pos = (float2) { p->x, p->y };
				for (int e = 0; e < n_edges; e++) {
					Edge *edge = edges[e];
					v->edge[e].a = (float2) { edge->p->x, edge->p->y };
					v->edge[e].b = (float2) { edge->q->x, edge->q->y };
				}
				if (n_edges < 6)
					memset(&v->edge[n_edges], 0, (6 - n_edges) * sizeof(LineSegment));
			}
		}
	}
	shape_free(shape);
	return n_verts;
}

void Gfx_render_pass(Gfx gfx, int n_verts, enum ShaderType shader, int vsize, bool blend) {
	glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
	glBufferData(GL_ARRAY_BUFFER, n_verts * vsize, gfx->vbuffer, GL_STATIC_DRAW);
	gfx_shaders_use(gfx, shader, NULL, false);
	GfxClip *clip = ll_last(&gfx->clips);
	gfx_clip_surface(gfx, shader, clip);
	if (blend || gfx->state->surface_src)
		glEnable(GL_BLEND);
	else
		glDisable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDrawArrays(GL_TRIANGLES, 0, n_verts);
	if (!blend)
		glEnable(GL_BLEND);
}

void gv_debug_lines(Gfx gfx, int n_verts) {
	gfx_debug(gfx, true);
	gfx_color(gfx, 0.3, 0.3, 0.3, 1.0);
	glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
	glBufferData(GL_ARRAY_BUFFER, n_verts * sizeof(VertexNew), gfx->vbuffer, GL_STATIC_DRAW);
	gfx_shaders_use(gfx, SHADER_NEW, NULL, false);
	GfxClip *clip = ll_last(&gfx->clips);
	gfx_clip_surface(gfx, SHADER_NEW, clip);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDrawArrays(GL_LINES, 0, n_verts);
	gfx_debug(gfx, false);
}

void Gfx_get_scales(Gfx gfx, float *sx, float *sy) {
	*sx = mat44_get_xscale(&gfx->state->mat);
	*sy = mat44_get_yscale(&gfx->state->mat);
}

int Gfx_fill_verts(Gfx gfx, int *vsize, enum ShaderType *shader, bool *blend) {
	*shader = SHADER_NEW;
	int n_verts;
	Rect r;
	float rx = 0, ry = 0;
	*vsize = sizeof(VertexNew);
	*blend = true;
	bool is_rect = is_rect_path(gfx, &gfx->path, &rx, &ry, &r);
	if (is_rect) {
		*shader = SHADER_RECT;
		n_verts = Gfx_fill_rect(gfx, r, rx, ry);
		*vsize = sizeof(VertexRect);
		if ((rx == 0.0 && ry == 0.0 && !gfx->state->clip) && ((gfx->state->color.a * gfx->state->opacity) == 1.0))
			*blend = false;
	} else
		n_verts = Gfx_fill_shape(gfx, false);
	return n_verts;
}

void Gfx_fill(Gfx gfx, bool clear) {
	enum ShaderType shader;
	int vsize;
	bool blend = true;
	int n_verts = Gfx_fill_verts(gfx, &vsize, &shader, &blend);
	gfx_render_pass(gfx, n_verts, shader, vsize, blend);
	if (clear)
		gfx_new_path(gfx);
}