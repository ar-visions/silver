#include <obj-gfx/gfx.h>

float angle_diff(float a, float b) {
	float phi = fabs(b - a);
	if (phi > (M_PI * 2))
		phi -= floor(phi / (M_PI * 2)) * (M_PI * 2);
	float distance = phi > M_PI ? (M_PI * 2) - phi : phi;
	float sign = (a - b >= 0 && a - b <= M_PI) || (a - b <= -M_PI && a - b >= -(M_PI * 2)) ? 1 : -1;
	return distance * sign;
}

static inline void
push_edge(VertexNew *v, LineSegment s) {
	if (v->edge_count < 6)
		v->edge[(int)(v->edge_count++)] = s;
}

static int
push_stroke_cap(Gfx self, VertexNew *v, StrokePoly *p, int steps, bool start_cap, LineSegment *p_left_seg, LineSegment *p_right_seg) {
	if (self->state->stroke_cap == STROKE_CAP_NONE)
		return 0;
	float sw = self->state->stroke_width * self->state->stroke_scale;
	float ssw = start_cap ? sw : -sw;
	float2 left_s = start_cap ? p->left.a : p->left.b;
	float2 right_s = start_cap ? p->right.a : p->right.b;
	LineSegment left_seg = p->left;
	LineSegment right_seg = p->right;
	int base_verts = 3;
	VertexNew *start = v;
	left_seg = p->left;

	if (self->state->stroke_cap == STROKE_CAP_BLUNT) {
		float2 left_a_extrude   = float2_sub(left_s, float2_scale(p->dir, ssw / 2));
		float2 right_a_extrude  = float2_sub(right_s, float2_scale(p->dir, ssw / 2));
		float2 mid		      = float2_interpolate(left_a_extrude, right_a_extrude, 0.5);
		left_seg  = (LineSegment) { left_a_extrude, left_s };
		right_seg = (LineSegment) { right_a_extrude, right_s };
		base_verts = 9;
		
		(v++)->pos = left_a_extrude;
		(v++)->pos = left_s;
		(v++)->pos = mid;

		(v++)->pos = right_a_extrude;
		(v++)->pos = right_s;
		(v++)->pos = mid;

		(v++)->pos = start_cap ? p->left.a : p->left.b;
		(v++)->pos = mid;
		(v++)->pos = right_s;

		for (int i = 0; i < 9; i++) {
			start[i].edge[0] = left_seg;
			start[i].edge[1] = right_seg;
			if (self->state->stroke_cap == STROKE_CAP_BLUNT) {
				start[i].edge[2] = (LineSegment){ left_a_extrude, right_a_extrude };
				start[i].edge_count = 3;
			} else {
				start[i].edge_count = 2;
			}
			if (!start_cap) {
				push_edge(&start[i], *p_left_seg);
				push_edge(&start[i], *p_right_seg);
			}
		}
	} else {
		float2 mid		      	= float2_interpolate(left_s, right_s, 0.5);
		float2 mid_out			= float2_sub(mid, float2_scale(p->dir, ssw / 2));
		float2 mid_out_c			= float2_sub(mid, float2_scale(p->dir, ssw / 4));
		float2 mid_left_offset 	= float2_add(mid_out, float2_scale(p->normal, sw / 4));
		float2 left_offset 		= float2_sub(left_s, float2_scale(p->dir, ssw / 4));
		float2 mid_right_offset 	= float2_sub(mid_out, float2_scale(p->normal, sw / 4));
		float2 right_offset 		= float2_sub(right_s, float2_scale(p->dir, ssw / 4));
		Bezier b_left 			= bezier(left_s, left_offset, mid_left_offset, mid_out);
		Bezier b_right 			= bezier(right_s, right_offset, mid_right_offset, mid_out);
		float2 left_p 			= left_s;
		LineSegment first_seg;
		
		if (steps > 1) {
			for (int i = 1; i <= steps; i++) {
				float f = (float)i / (float)steps;
				Bezier b;
				if (f < 0.5) {
					f *= 2.0;
					b = b_left;
				} else {
					f = 1.0 - ((f - 0.5) * 2.0);
					b = b_right;
				}
				float2 right_p = call(b, point_at, f);
				LineSegment this_seg = { left_p, right_p };
				v[0].pos = mid_out_c;
				v[1].pos = left_p;
				v[2].pos = right_p;
				for (int e = 0; e < 3; e++) {
					v[e].edge[0] = this_seg;
					v[e].edge[1] = left_seg;
					for (int ee = 2; ee < 6; ee++)
						v[ee].edge[e] = this_seg;
				}
				if (i > 1) {
					for (int e = -3; e < 0; e++)
						v[e].edge[2] = this_seg;
				} else {
					first_seg = this_seg;
				}
				v += 3;
				left_seg = this_seg;
				left_p = right_p;
			}
			left_seg = first_seg;
			right_seg = left_seg;
			for (int e = -3; e < 0; e++)
				v[e].edge[2] = p->right;
		} else {
			steps = 0;
		}
		(v++)->pos = mid_out_c;
		(v++)->pos = left_s;
		(v++)->pos = right_s;
	}

	*p_left_seg = left_seg;
	*p_right_seg = right_seg;
	if (false)
	for (int i = 0; i < base_verts + steps * 3; i++) {
		VertexNew *v = &start[i];
		for (int e = 0; e < 6; e++) {
			v->edge[e] = (LineSegment){ { 0, 0 }, { 0, 0 } };
		}
	}
	return base_verts + steps * 3;
}

int push_stroke_join(Gfx self, VertexNew *v, int steps, float2 end, float2 s0, float2 s1, float2 intersect,
		LineSegment *left_seg, LineSegment *right_seg, bool right_bend) {
	steps = max(1, steps);
	float2 s0_mid = float2_interpolate(s0, intersect, 0.5);
	float2 s1_mid = float2_interpolate(s1, intersect, 0.5);
	LineSegment seg = right_bend ? *left_seg : *right_seg;
	LineSegment seg_other = right_bend ? *right_seg : *left_seg;
	Bezier b = bezier(s0, s0_mid, s1_mid, s1);
	float ratio = (float)1.0 / (float)steps;
	float pos = 0.0;
	for (int i = 0; i < steps; i++) {
		LineSegment s = { call(b, point_at, pos), call(b, point_at, pos + ratio) };
		(v++)->pos = end;
		(v++)->pos = s.a;
		(v++)->pos = s.b;
		for (int ii = -3; ii < 0; ii++) {
			push_edge(&v[ii], s);
			push_edge(&v[ii], seg);
			push_edge(&v[ii], seg_other);
		}
		if (i > 0)
			for (int ii = -6; ii < -3; ii++) {
				push_edge(&v[ii], s);
			}
		seg = s;
		if (i == steps - 1) {
			LineSegment *seg_p = right_bend ? left_seg : right_seg;
			*seg_p = seg;
		}
		pos += ratio;
	}
	return steps * 3;
}

void Gfx_stroke_scale(Gfx self, float sc) {
	self->state->stroke_scale = sc;
}

int Gfx_stroke_shape(Gfx self) {
	GfxState st = self->state;
	float sw = st->stroke_width * st->stroke_scale;
	List line_segments = class_call(Shape, lines_from_path, self, &self->path, true);
	int polys_len = sizeof(StrokePoly) * list_count(line_segments);
	StrokePoly *polys = (StrokePoly *)malloc(polys_len);
	memset(polys, 0, polys_len);
	float sx, sy;
	call(self, get_scales, &sx, &sy);
	int count = 0;

	StrokePoly *start = &polys[0];
	Segment *segment;
	each(line_segments, segment) {
		StrokePoly *p = &polys[count++];
		p->seg		= (LineSegment){ segment->a, segment->b };
		p->moved 	= segment->moved;
		p->close 	= segment->close;
		float2 diff 	= float2_sub(p->seg.b, p->seg.a);
		float2 cross 	= float2_cross(diff);
		p->dir		= float2_normalize(diff);
		p->normal 	= float2_normalize(cross);
		p->rads 	= float2_angle_between((float2){1, 0}, p->normal);
		float2 right 	= float2_scale(p->normal, sw / -2.0);
		p->right  	= (LineSegment){ float2_add(p->seg.a, right), float2_add(p->seg.b, right) };
		p->left  	= (LineSegment){ float2_sub(p->seg.a, right), float2_sub(p->seg.b, right) };
		if (p->close)
			start->loop = true;
		if (p->moved)
			start = p;
	}

	start = &polys[0];
	float miter_limit_sqr = sqr(st->miter_limit + sw / 2);
	int n_verts = 0;
	for (int i = 0; i < count; i++) {
		StrokePoly *p = &polys[i];
		StrokePoly *n = i == (count - 1) ? start : &polys[i + 1];
		if (n->moved && n != start)
			n = p->close ? start : NULL;
		if (n == start && !start->loop)
			n = NULL;
		if (p->moved)
			start = p;
		if (start == p && !start->loop && st->stroke_cap != STROKE_CAP_NONE) {
			// place end (a) cap
			if (st->stroke_cap == STROKE_CAP_ROUNDED) {
				p->start_cap = round(float2_dist(p->left.a, p->right.a) * sx / 3.0);
				if (p->start_cap == 1)
					p->start_cap = 0;
			}
			n_verts += (st->stroke_cap == STROKE_CAP_ROUNDED ? 3 : 9) + p->start_cap * 3;
		}
		if (n) {
			float angle = angle_diff(p->rads, n->rads);
			float2 i_left, i_right;
			p->wedge = 1;
			if (angle != 0.0 && float2_intersect_line(n->left, p->left, &i_left) &&
								float2_intersect_line(n->right, p->right, &i_right)) {
				if (angle > 0.0) {
					// stroke bends to right
					p->left_intersect = i_left;
					p->right.b = i_right;
					n->right.a = i_right;
					float miter = float2_dist_sqr(i_left, i_right);
					if (st->stroke_join != STROKE_JOIN_MITER || miter > miter_limit_sqr) {
						p->wedge = st->stroke_join != STROKE_JOIN_ROUNDED ? 1 : max(1, round(float2_dist(n->left.a, p->left.b) * sx / 3.0));
					} else if (st->stroke_join == STROKE_JOIN_MITER) {
						p->left.b = i_left;
						n->left.a = i_left;
						p->wedge = 0;
					}
				} else {
					// stroke bends to left
					p->right_intersect = i_right;
					p->left.b = i_left;
					n->left.a = i_left;
					float miter = float2_dist_sqr(i_left, i_right);
					if (st->stroke_join != STROKE_JOIN_MITER || miter > miter_limit_sqr)
						p->wedge = st->stroke_join != STROKE_JOIN_ROUNDED ? -1 : -max(1, round(float2_dist(n->right.a, p->right.b) * sx / 3.0));
					else if (st->stroke_join == STROKE_JOIN_MITER) {
						p->right.b = i_right;
						n->right.a = i_right;
						p->wedge = 0;
					}
				}
			} else {
				p->wedge = 0;
			}
		} else if (!p->close && st->stroke_cap != STROKE_CAP_NONE) {
			// place end (b) cap
			if (st->stroke_cap == STROKE_CAP_ROUNDED) {
				p->end_cap = round(float2_dist(p->left.b, p->right.b) * sx / 3.0);
				if (p->end_cap == 1)
					p->end_cap = 0;
			}
			n_verts += (st->stroke_cap == STROKE_CAP_ROUNDED ? 3 : 9) + p->end_cap * 3;
		}
		n_verts += 6 + abs(p->wedge) * 3;
	}
	if (n_verts > self->vbuffer_size)
		call(self, realloc_buffer, n_verts + (self->vbuffer_size >> 1));
	memset(self->vbuffer, 0, sizeof(VertexNew) * n_verts);
	VertexNew *v = self->vbuffer;
	LineSegment left_seg, right_seg;
	bool segs = false;
	start = &polys[0];
	for (int i = 0; i < count; i++) {
		StrokePoly *p = &polys[i];
		StrokePoly *n = i == (count - 1) ? start : &polys[i + 1];
		if (n->moved && n != start)
			n = p->close ? start : NULL;
		if (n == start && !start->loop)
			n = NULL;
		if (p->moved)
			start = p;
		if (start == p && !start->loop) {
			if (st->stroke_cap != STROKE_CAP_NONE) {
				v += push_stroke_cap(self, v, p, p->start_cap, true, &left_seg, &right_seg);
				segs = true;
			} else {
				left_seg = (LineSegment) { p->left.a, p->right.a };
				if (!n)
					right_seg = (LineSegment) { p->left.b, p->right.b };
				else
					right_seg = left_seg;
			}
		}
		VertexNew *start_v = v;
		(v++)->pos = p->left.a;
		(v++)->pos = p->left.b;
		(v++)->pos = p->right.b;
		(v++)->pos = p->right.b;
		(v++)->pos = p->right.a;
		(v++)->pos = p->left.a;
		
		for (int i = 0; i < 6; i++) {
			VertexNew *sv = &start_v[i];
			if (st->stroke_cap == STROKE_CAP_NONE) {
				if (!start->loop && start == p)
					push_edge(sv, (LineSegment) { p->left.a, p->right.a });
				else if (!n && !p->close)
					push_edge(sv, (LineSegment) { p->left.b, p->right.b });
			}
			push_edge(sv, p->left);
			push_edge(sv, p->right);
			if (segs) {
				push_edge(sv, left_seg);
				push_edge(sv, right_seg);
			}
			if (n) {
				push_edge(sv, n->left);
				push_edge(sv, n->right);
			}
		}
		left_seg = p->left;
		right_seg = p->right;
		segs = true;
		if (p->wedge != 0) {
			// negative = left bend = pivot on left side, positive = right
			if (p->wedge < 0) {
				int nv = push_stroke_join(self, v, -p->wedge, p->left.b, p->right.b, n->right.a,
						p->right_intersect, &left_seg, &right_seg, false);
				v += nv;
			} else {
				int nv = push_stroke_join(self, v, p->wedge, p->right.b, p->left.b, n->left.a,
						p->left_intersect, &left_seg, &right_seg, true);
				v += nv;
			}
		}
		if (!n && !p->close) {
			int nv = push_stroke_cap(self, v, p, p->end_cap, false, &left_seg, &right_seg);
			v += nv;
		}
	}
	// vertex count
	free(polys);
	return n_verts;
}

void Gfx_stroke(Gfx self, bool clear) {
	int n_verts = call(self, stroke_shape);
	if (n_verts > 0)
		call(self, render_pass, n_verts, SHADER_NEW, sizeof(VertexNew), true);
	if (clear)
		call(self, new_path);
}