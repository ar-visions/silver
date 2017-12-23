#include "gfx.h"
#include <poly2tri/cdt.h>

static void outline_init(Outline *outline, int n_alloc) {
	outline->holes = (LL *)malloc(sizeof(LL) * n_alloc);
	memset(outline->holes, 0, sizeof(LL) * n_alloc);
	ll(&outline->outline, 0, 10);
	outline->init = TRUE;
}

BOOL poly_contains_point(LL *list, Point *point) {
	Point *j = (Point *)ll_last(list);
    float px = point->x, py = point->y;
    BOOL contains = FALSE;
	
	ll_each(list, Point, p) {
        if( (p->y > py) != (j->y > py)
            && px < (j->x - p->x) * (py - p->y) / (j->y - p->y) + p->x )
            contains = !contains;
		j = p;
	}
    return contains;
}

void shape_free(Shape *shape) {
	for (int i = 0; i < shape->n_outlines; i++) {
		Outline *o = &shape->outlines[i];
		for (int h = 0; h < o->n_holes; h++) {
			ll_clear(&o->holes[h], FALSE);
		}
		free(o->holes);
		cdt_free(o->cdt);
		//ll_clear(&o->outline, FALSE);
	}
	ll_clear(&shape->edges, TRUE);
	free(shape->outlines);
	for (int i = 0; i < shape->n_points; i++) {
		ll_clear(&shape->points[i].edge_list, FALSE);
	}
	free(shape->points);
	free(shape);
}

void gfx_path_bbox(Gfx *gfx, LL *in, GfxRect *r) {
	BOOL first = TRUE;
	float _min_x=0, _max_x=0;
	float _min_y=0, _max_y=0;
	ll_each(in, Segment, s) {
		float min_x=0, max_x=0;
		float min_y=0, max_y=0;
		switch (s->type) {
			case SEGMENT_ARC: {
				min_x = s->center.x - s->radius;
				max_x = s->center.x + s->radius;
				min_y = s->center.y - s->radius;
				max_y = s->center.y + s->radius;
				break;
			}
			case SEGMENT_BEZIER:
			case SEGMENT_RECT:
			case SEGMENT_LINE: {
				min_x = min(s->a.x, s->b.x);
				max_x = max(s->a.x, s->b.x);
				min_y = min(s->a.y, s->b.y);
				max_y = max(s->a.y, s->b.y);
				break;
			}
		}
		if (first) {
			_min_x = min_x;
			_max_x = max_x;
			_min_y = min_y;
			_max_y = max_y;
			first = FALSE;
		} else {
			if (_min_x > min_x) _min_x = min_x;
			if (_max_x < max_x) _max_x = max_x;
			if (_min_y > min_y) _min_y = min_y;
			if (_max_y < max_y) _max_y = max_y;
		}
	}
	*r = (GfxRect) { _min_x, _min_y, _max_x - _min_x, _max_y - _min_y };
}

BOOL is_rect_path(Gfx *gfx, LL *in, float *radius_x, float *radius_y, GfxRect *r) {
	if (in->count != 1)
		return FALSE;
	ll_each(in, Segment, s) {
		switch (s->type) {
			case SEGMENT_ARC: {
				if (radius_x && fabs(s->rads - (M_PI * 2)) < 0.0001) {
					r->x = s->center.x - s->radius;
					r->y = s->center.y - s->radius;
					r->w = (s->center.x + s->radius) - r->x;
					r->h = (s->center.y + s->radius) - r->y;
					*radius_x = *radius_y = s->radius;
					return TRUE;
				}
				break;
			}
			case SEGMENT_RECT: {
				if (radius_x || s->radius == 0.0) {
					r->x = s->a.x;
					r->y = s->a.y;
					r->w = s->b.x - s->a.x;
					r->h = s->b.y - s->a.y;
					if (radius_x)
						*radius_x = *radius_y = s->radius;
					return TRUE;
				}
				break;
			}
			default:
				break;
		}
	}
	return FALSE;
}

LL *lines_from_path(Gfx *gfx, LL *in, BOOL close_paths) {
	LL *out = (LL *)malloc(sizeof(LL));
	memset(out, 0, sizeof(LL));
	ll(out, sizeof(Segment), 10);
	V2 a;
	V2 b;
	Segment *start = NULL;
	Segment *last_seg = NULL;
	const float EPS = 0.001;
	float sx, sy;
	gfx_get_scales(gfx, &sx, &sy);
	ll_each(in, Segment, s_) {
		if (!start || s_->moved) {
			a = s_->a;
			start = NULL;
		}
		b = s_->b;
		Segment stack[4];
		Segment *subs[4];
		int n_subs = 1;
		subs[0] = s_;

		if (s_->type == SEGMENT_RECT && s_->radius > 0.0) {
			float r = s_->radius;
			float x0 = s_->a.x, y0 = s_->a.y;
			float x1 = s_->b.x, y1 = s_->b.y;
			if (fabsf(x0-x1) <= EPS || fabsf(y0-y1) <= EPS)
				continue;
			if (x0 > x1) {
				float t = x1;
				x1 = x0;
				x0 = t;
			}
			if (y0 > y1) {
				float t = y1;
				y1 = y0;
				y0 = t;
			}
			if (x1 - r < x0 + r)
				r = fabs(x1 - x0) / 2;
			if (y1 - r < y0 + r)
				r = fabs(y1 - y0) / 2;
			// if rounded rect, make 4 arcs out of this (with moved start, and closed end)
			V2 b0 = gfx_arc_seg(gfx, &stack[0], &s_->a, x1 - r, y0 + r, r, radians(-90), radians(90.0));
			V2 b1 = gfx_arc_seg(gfx, &stack[1], &b0, x1 - r, y1 - r, r, radians(0), radians(90.0));
			V2 b2 = gfx_arc_seg(gfx, &stack[2], &b1, x0 + r, y1 - r, r, radians(90), radians(90.0));
			gfx_arc_seg(gfx, &stack[3], &b2, x0 + r, y0 + r, r, radians(180), radians(90.0));
			for (int i = 0; i < 4; i++)
				subs[i] = &stack[i];
			subs[0]->moved = TRUE;
			subs[3]->close = TRUE;
			n_subs = 4;
		}
		for (int ii = 0; ii < n_subs; ii++) {
			Segment *s = subs[ii];
			b = s->b;
			switch (s->type) {
				case SEGMENT_RECT: {
					Segment *ss = ll_push(out, NULL);
					ss->type = SEGMENT_LINE;
					ss->a = a;
					ss->b = (V2) { b.x, a.y };
					ss->no_feather = TRUE;
					ss->moved = s->moved;
					ss->close = FALSE;
					if (!start) start = ss;

					ss = ll_push(out, NULL);
					ss->type = SEGMENT_LINE;
					ss->a = (V2) { b.x, a.y };
					ss->b = (V2) { b.x, b.y };
					ss->no_feather = TRUE;
					ss->moved = FALSE;
					ss->close = FALSE;

					ss = ll_push(out, NULL);
					ss->type = SEGMENT_LINE;
					ss->a = b;
					ss->b = (V2) { a.x, b.y };
					ss->no_feather = TRUE;
					ss->moved = FALSE;
					ss->close = TRUE;
					last_seg = ss;
					break;
				}
				case SEGMENT_LINE: {
					Segment *ss = ll_push(out, NULL);
					if (!start) start = ss;
					ss->type = SEGMENT_LINE;
					ss->a = a;
					ss->b = b;
					ss->moved = s->moved;
					ss->close = s->close;
					last_seg = ss;
					break;
				}
				case SEGMENT_ARC: {
					float cir = (s->radius + (gfx->state.stroke_width * gfx->state.stroke_scale) / 2) * 2.0 * M_PI;
					float pixel_threshold = 0.10 * sx; // scale higher for stroke ops?
					float d = fabs(s->rads);
					float amount = min(1.0, d / (M_PI * 2.0));
					float ct = cir * pixel_threshold;
					ct = ct + (max(0.0, 1.0 - (ct / 15.0)) * ct);
					int steps = min(100, max(4, ct * amount));
					for (int i = 0; i < steps + 1; i++) {
						float f = ((float)i / (float)steps);
						float rads = s->rads_from + s->rads * f;
						float ox = cos(rads), oy = sin(rads);
						V2 p = { s->center.x + s->radius * ox, s->center.y + s->radius * oy };
						if (i > 0) {
							BOOL added_move = FALSE;
							if (i == 1) {
								if (!s->moved && vec2_dist(a, b) > EPS) {
									Segment *ss = ll_push(out, NULL);
									if (!start) start = ss;
									ss->type = SEGMENT_LINE;
									ss->a = a;
									ss->b = b;
									ss->moved = FALSE;
									added_move = TRUE;
									last_seg = ss;
								}
							}
							Segment *ss = ll_push(out, NULL);
							if (!start) start = ss;
							ss->type = SEGMENT_LINE;
							ss->a = b;
							ss->b = p;
							ss->moved = !added_move && (i == 1 && s->moved);
							last_seg = ss;
						}
						b = p;
					}
					if (last_seg)
						last_seg->close = s->close || (amount > 1.0 - EPS);
					break;
				}
				case SEGMENT_BEZIER: {
					Bezier bz = { .p1 = a, .h1 = s->cp1, .h2 = s->cp2, .p2 = b };
					float len = bezier_approx_length(&bz, 8);
					float pixel_threshold = 0.10;
					int steps = max(4, fabs(sx) * len * pixel_threshold);
					for (int i = 0; i < steps; i++) {
						float f = (float)i / (float)(steps - 1);
						V2 p = bezier_point_at(&bz, f);
						if (i > 0) {
							Segment *ss = ll_push(out, NULL);
							if (!start) start = ss;
							ss->type = SEGMENT_LINE;
							ss->a = b;
							ss->b = p;
							ss->moved = FALSE;
							last_seg = ss;
						}
						b = p;
					}
					if (last_seg)
						last_seg->close = s->close;
					break;
				}
			}
			a = b;
			if (close_paths && last_seg && start && last_seg->close) {
				float dist = vec2_dist(last_seg->b, start->a);
				if (dist > EPS) {
					Segment *ss = ll_push(out, NULL);
					ss->type = SEGMENT_LINE;
					ss->a = last_seg->b;
					ss->b = start->a;
					ss->moved = last_seg->moved;
					ss->close = last_seg->close;
				}
				start = NULL;
			}
		}
	}
	return out;
}

BOOL shape_from_path(Gfx *gfx, LL *path, Shape **p_shape) {
	int alloc = 1;
	ll_each(path, Segment, _pe) {
		if (_pe->moved)
			alloc++;
	}
	Shape *shape = (Shape *)malloc(sizeof(Shape));
	memset(shape, 0, sizeof(Shape));
	shape->outlines = (Outline *)malloc(sizeof(Outline) * alloc);
	memset(shape->outlines, 0, sizeof(Outline) * alloc);
	shape->n_alloc = alloc;
	ll(&shape->edges, 0, 10);

	Outline *o = shape->outlines;
	BOOL first = TRUE;
	Point *p_last = NULL;
	Point *p_first = NULL;
	LL *list = NULL;
	float sx, sy;
	gfx_get_scales(gfx, &sx, &sy);
	float eps_sqr = sqr(0.001 * sx);
	LL *line_segments = lines_from_path(gfx, path, FALSE);
	Segment *pe_first = NULL;
	shape->points_alloc = line_segments->count * 2 + 1;
	shape->points = (Point *)malloc(sizeof(Point) * shape->points_alloc);
	memset(shape->points, 0, sizeof(Point) * shape->points_alloc);
	ll_each(line_segments, Segment, pe) {
		if (!pe_first)
			pe_first = pe;
		if (first || pe->moved) {
			if (p_last) {
				Edge *edge = edge_simple(p_last, p_first);
				ll_push(&shape->edges, edge);
				p_last->edges[p_last->n_edges++] = edge;
				p_first->edges[p_first->n_edges++] = edge;
				p_first = NULL;
				list = NULL;
			}
			Point *p = point_init(&shape->points[shape->n_points++], pe->a.x, pe->a.y);
			p_first = p;
			if (o->init) {
				if (poly_contains_point(&o->outline, p)) {
					BOOL inside_hole = FALSE;
					for (int i = 0; i < o->n_holes; i++) {
						inside_hole = poly_contains_point(&o->holes[i], p);
						if (inside_hole)
							break;
					}
					if (inside_hole) {
						shape->n_outlines++;
						outline_init(++o, shape->n_alloc);
						list = &o->outline;
					} else {
						list = &o->holes[o->n_holes++];
						ll(list, 0, 10);
					}
				} else {
					shape->n_outlines++;
					outline_init(++o, shape->n_alloc);
					list = &o->outline;
				}
			} else {
				shape->n_outlines++;
				outline_init(o, shape->n_alloc);
				list = &o->outline;
				o->no_feather = pe->no_feather;
			}
			ll_push(list, p);
			first = FALSE;
			p_last = p;
		}
		Point *p = point_init(&shape->points[shape->n_points++], pe->b.x, pe->b.y);
		Edge *edge = edge_simple(p_last, p);
		ll_push(&shape->edges, edge);

		if (p_first && vec2_dist_sqr(pe->b, (V2){p_first->x, p_first->y}) <= eps_sqr)
			continue;

		ll_push(list, p);
		p->edges[p->n_edges++] = edge;
		if (p_last && p_last->n_edges < 6) {
			p_last->edges[p_last->n_edges++] = edge;
		}
		p_last = p;
	}
	if (p_last) {
		Edge *edge = edge_simple(p_last, p_first);
		ll_push(&shape->edges, edge);
		p_last->edges[p_last->n_edges++] = edge;
		p_first->edges[p_first->n_edges++] = edge;
	}
	for (int i = 0; i < shape->n_outlines; i++) {
		Outline *o = &shape->outlines[i];
		o->cdt = cdt(&o->outline);
		for (int h = 0; h < o->n_holes; h++)
			cdt_add_hole(o->cdt, &o->holes[h]);
		cdt_triangulate(o->cdt);
		o->tris = cdt_get_triangles(o->cdt);
	}
	ll_clear(line_segments, FALSE);
	free(line_segments);
	*p_shape = shape;
	return TRUE;
}
