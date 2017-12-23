#include <obj-poly2tri/poly2tri.h>
#include <assert.h>

void Sweep_init(Sweep self) {
    self->nodes = new(List);
}

void Sweep_free(Sweep self) {
    release(self->nodes);
}

void Sweep_sweep_points(Sweep self, SweepContext tcx) {
	int i = 0;
    Point point;
	each(tcx->points, point) {
		if (i == 0) {
			i++;
			continue;
		}
		AFNode node = Sweep_point_event(self, tcx, point);
        Edge edge;
		each(point->edge_list, edge)
			Sweep_edge_event_en(self, tcx, edge, node);
	}
}

void Sweep_triangulate(Sweep self, SweepContext tcx) {
  SweepContext_init_triangulation(tcx);
  SweepContext_create_advancing_front(tcx, &self->nodes);
  Sweep_sweep_points(self, tcx);
  Sweep_finalization_polygon(self, tcx);
}

void Sweep_finalization_polygon(Sweep self, SweepContext tcx) {
  Tri  t = tcx->front->head->next->triangle;
  Point p = tcx->front->head->next->point;
  while (!Tri_get_constrained_edge_cw(t, p)) {
    t = Tri_neighbor_ccw(t, p);
  }
  SweepContext_mesh_clean(tcx, t);
}

AFNode Sweep_point_event(Sweep self, SweepContext tcx, Point point) {
  AFNode node = SweepContext_locate_node(tcx, point);
  AFNode new_node = Sweep_new_front_triangle(self, tcx, point, node);

  if (point->x <= node->point->x + EPSILON)
    Sweep_fill(self, tcx, node);
  Sweep_fill_advancing_front(self, tcx, new_node);
  return new_node;
}

void Sweep_edge_event_en(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  tcx->edge_event->constrained_edge = edge;
  tcx->edge_event->right = edge->p->x > edge->q->x;

  if (Sweep_is_edge_side_of_triangle(self, node->triangle, edge->p, edge->q))
    return;

  Sweep_fill_edge_event(self, tcx, edge, node);
  Sweep_edge_event_pptp(self, tcx, edge->p, edge->q, node->triangle, edge->q);
}

void Sweep_edge_event_pptp(Sweep self, SweepContext tcx, Point ep, Point eq, Tri  triangle, Point point) {
  if (Sweep_is_edge_side_of_triangle(self, triangle, ep, eq))
    return;

  Point p1 = Tri_point_ccw(triangle, point);
  enum Orientation o1 = orient_2d(eq, p1, ep);
  if (o1 == COLLINEAR) {
    if (Tri_contains_points(triangle, eq, p1)) {
      Tri_mark_constrained_edge_pq(triangle, eq, p1);
      tcx->edge_event->constrained_edge->q = p1;
      triangle = Tri_neighbor_across(triangle, point);
      Sweep_edge_event_pptp(self, tcx, ep, p1, triangle, p1);
    } else {
        fprintf(stderr, "EdgeEvent - collinear points not supported\n");
      assert(0);
    }
    return;
  }

  Point p2 = Tri_point_cw(triangle, point);
  enum Orientation o2 = orient_2d(eq, p2, ep);
  if (o2 == COLLINEAR) {
    if( Tri_contains_points(triangle, eq, p2)) {
      Tri_mark_constrained_edge_pq(triangle, eq, p2);
      tcx->edge_event->constrained_edge->q = p2;
      triangle = Tri_neighbor_across(triangle, point);
      Sweep_edge_event_pptp(self, tcx, ep, p2, triangle, p2);
    } else {
        fprintf(stderr, "EdgeEvent - collinear points not supported\n");
      assert(0);
    }
    return;
  }

  if (o1 == o2) {
    if (o1 == CW)
      triangle = Tri_neighbor_ccw(triangle, point);
    else
      triangle = Tri_neighbor_cw(triangle, point);
    Sweep_edge_event_pptp(self, tcx, ep, eq, triangle, point);
  } else
    Sweep_flip_edge_event(self, tcx, ep, eq, triangle, point);
}

bool Sweep_is_edge_side_of_triangle(Sweep self, Tri triangle, Point ep, Point eq) {
  int index = Tri_edge_index(triangle, ep, eq);

  if (index != -1) {
    Tri_mark_constrained_edge_index(triangle, index);
    Tri  t = Tri_get_neighbor(triangle, index);
    if (t)
      Tri_mark_constrained_edge_pq(t, ep, eq);
    return true;
  }
  return false;
}

AFNode Sweep_new_front_triangle(Sweep self, SweepContext tcx, Point point, AFNode node) {
  Tri  triangle = tri(point, node->point, node->next->point);

  Tri_mark_neighbor_tri(triangle, node->triangle);
  list_push(tcx->map, triangle);

  AFNode new_node = node_with_point(point);
	list_push(self->nodes, new_node);

  new_node->next = node->next;
  new_node->prev = node;
  node->next->prev = new_node;
  node->next = new_node;

  if (!Sweep_legalize(self, tcx, triangle))
    SweepContext_map_triangle_to_nodes(tcx, triangle);
  return new_node;
}

void Sweep_fill(Sweep self, SweepContext tcx, AFNode node) {
  Tri  triangle = tri(node->prev->point, node->point, node->next->point);

  Tri_mark_neighbor_tri(triangle, node->prev->triangle);
  Tri_mark_neighbor_tri(triangle, node->triangle);

  list_push(tcx->map, triangle);

  node->prev->next = node->next;
  node->next->prev = node->prev;

  if (!Sweep_legalize(self, tcx, triangle))
    SweepContext_map_triangle_to_nodes(tcx, triangle);
}

void Sweep_fill_advancing_front(Sweep self, SweepContext tcx, AFNode n) {
  AFNode node = n->next;

  while (node->next) {
    if (Sweep_large_hole(self, node)) break;
    Sweep_fill(self, tcx, node);
    node = node->next;
  }

  node = n->prev;

  while (node->prev) {
    if (Sweep_large_hole(self, node))
		break;
    Sweep_fill(self, tcx, node);
    node = node->prev;
  }

  if (n->next && n->next->next) {
    float angle = Sweep_basin_angle(self, n);
    if (angle < PI_3div4) {
      Sweep_fill_basin(self, tcx, n);
    }
  }
}

bool Sweep_large_hole(Sweep self, AFNode node) {
	AFNode nextNode = node->next;
	AFNode prevNode = node->prev;
	if (!Sweep_angle_exceeds_90(self, node->point, nextNode->point, prevNode->point))
		return false;

	AFNode next2Node = nextNode->next;
	if ((next2Node != NULL) && !Sweep_angle_invalid(self, node->point, next2Node->point, prevNode->point))
		return false;

	AFNode prev2Node = prevNode->prev;
	if ((prev2Node != NULL) && !Sweep_angle_invalid(self, node->point, nextNode->point, prev2Node->point))
		return false;
	return true;
}

bool Sweep_angle_exceeds_90(Sweep self, Point origin, Point pa, Point pb) {
	float angle = Sweep_angle(self, origin, pa, pb);
	return (angle > PI_div2) || (angle < -PI_div2);
}

bool Sweep_angle_invalid(Sweep self, Point origin, Point pa, Point pb) {
	float angle = Sweep_angle(self, origin, pa, pb);
	return (angle > PI_div2) || (angle < 0);
}

float Sweep_angle(Sweep self, Point origin, Point pa, Point pb) {
	float px = origin->x;
	float py = origin->y;
	float ax = pa->x - px;
	float ay = pa->y - py;
	float bx = pb->x - px;
	float by = pb->y - py;
	float x = ax * by - ay * bx;
	float y = ax * bx + ay * by;
	float angle = atan2(x, y);
	return angle;
}

float Sweep_basin_angle(Sweep self, AFNode node) {
	float ax = node->point->x - node->next->next->point->x;
	float ay = node->point->y - node->next->next->point->y;
	return atan2(ay, ax);
}

float Sweep_hole_angle(Sweep self, AFNode node) {
  float ax = node->next->point->x - node->point->x;
  float ay = node->next->point->y - node->point->y;
  float bx = node->prev->point->x - node->point->x;
  float by = node->prev->point->y - node->point->y;
  return atan2(ax * by - ay * bx, ax * bx + ay * by);
}

bool Sweep_legalize(Sweep self, SweepContext tcx, Tri t) {
  for (int i = 0; i < 3; i++) {
    if (t->delaunay[i])
      continue;

    Tri  ot = t->neighbors[i];

    if (ot) {
      Point p = t->points[i];
      Point op = Tri_opposite_point(ot, t, p);
      int oi = Tri_index(ot, op);

      if (ot->constrained[oi] || ot->delaunay[oi]) {
        t->constrained[i] = ot->constrained[oi];
        continue;
      }

      bool inside = Sweep_incircle(self, p, Tri_point_ccw(t, p), Tri_point_cw(t, p), op);

      if (inside) {
        t->delaunay[i] = true;
        ot->delaunay[oi] = true;

        Sweep_rotate_triangle_pair(self, t, p, ot, op);

        bool not_legalized = !Sweep_legalize(self, tcx, t);
        if (not_legalized)
          SweepContext_map_triangle_to_nodes(tcx, t);

        not_legalized = !Sweep_legalize(self, tcx, ot);
        if (not_legalized)
					SweepContext_map_triangle_to_nodes(tcx, ot);

        t->delaunay[i] = false;
        ot->delaunay[oi] = false;
        return true;
      }
    }
  }
  return false;
}

bool Sweep_incircle(Sweep self, Point pa, Point pb, Point pc, Point pd) {
  float adx = pa->x - pd->x;
  float ady = pa->y - pd->y;
  float bdx = pb->x - pd->x;
  float bdy = pb->y - pd->y;

  float adxbdy = adx * bdy;
  float bdxady = bdx * ady;
  float oabd = adxbdy - bdxady;

  if (oabd <= 0)
    return false;

  float cdx = pc->x - pd->x;
  float cdy = pc->y - pd->y;

  float cdxady = cdx * ady;
  float adxcdy = adx * cdy;
  float ocad = cdxady - adxcdy;

  if (ocad <= 0)
    return false;

  float bdxcdy = bdx * cdy;
  float cdxbdy = cdx * bdy;

  float alift = adx * adx + ady * ady;
  float blift = bdx * bdx + bdy * bdy;
  float clift = cdx * cdx + cdy * cdy;

  float det = alift * (bdxcdy - cdxbdy) + blift * ocad + clift * oabd;

  return det > 0;
}

void Sweep_rotate_triangle_pair(Sweep self, Tri t, Point p, Tri ot, Point op) {
  Tri  n1, *n2, *n3, *n4;
  n1 = Tri_neighbor_ccw(t, p);
  n2 = Tri_neighbor_cw(t, p);
  n3 = Tri_neighbor_ccw(ot, op);
  n4 = Tri_neighbor_cw(ot, op);

  bool ce1, ce2, ce3, ce4;
  ce1 = Tri_get_constrained_edge_ccw(t, p);
  ce2 = Tri_get_constrained_edge_cw(t, p);
  ce3 = Tri_get_constrained_edge_ccw(ot, op);
  ce4 = Tri_get_constrained_edge_cw(ot, op);

  bool de1, de2, de3, de4;
  de1 = Tri_get_delunay_edge_ccw(t, p);
  de2 = Tri_get_delunay_edge_cw(t, p);
  de3 = Tri_get_delunay_edge_ccw(ot, op);
  de4 = Tri_get_delunay_edge_cw(ot, op);

  Tri_legalize_opoint(t, p, op);
  Tri_legalize_opoint(ot, op, p);

  Tri_set_delunay_edge_ccw(ot, p, de1);
  Tri_set_delunay_edge_cw(t, p, de2);
  Tri_set_delunay_edge_ccw(t, op, de3);
  Tri_set_delunay_edge_cw(ot, op, de4);

  Tri_set_constrained_edge_ccw(ot, p, ce1);
  Tri_set_constrained_edge_cw(t, p, ce2);
  Tri_set_constrained_edge_ccw(t, op, ce3);
  Tri_set_constrained_edge_cw(ot, op, ce4);

  Tri_clear_neighbors(t);
  Tri_clear_neighbors(ot);

  if (n1) Tri_mark_neighbor_tri(ot, n1);
  if (n2) Tri_mark_neighbor_tri(t, n2);
  if (n3) Tri_mark_neighbor_tri(t, n3);
  if (n4) Tri_mark_neighbor_tri(ot, n4);
  Tri_mark_neighbor_tri(t, ot);
}

void Sweep_fill_basin(Sweep self, SweepContext tcx, AFNode node) {
  if (orient_2d(node->point, node->next->point, node->next->next->point) == CCW)
    tcx->basin->left_node = node->next->next;
  else
    tcx->basin->left_node = node->next;

  tcx->basin->bottom_node = tcx->basin->left_node;
  while (tcx->basin->bottom_node->next
         && tcx->basin->bottom_node->point->y >= tcx->basin->bottom_node->next->point->y) {
    tcx->basin->bottom_node = tcx->basin->bottom_node->next;
  }
  if (tcx->basin->bottom_node == tcx->basin->left_node)
    return;

  tcx->basin->right_node = tcx->basin->bottom_node;
  while (tcx->basin->right_node->next
         && tcx->basin->right_node->point->y < tcx->basin->right_node->next->point->y) {
    tcx->basin->right_node = tcx->basin->right_node->next;
  }
  if (tcx->basin->right_node == tcx->basin->bottom_node)
    return;

  tcx->basin->width = tcx->basin->right_node->point->x - tcx->basin->left_node->point->x;
  tcx->basin->left_highest = tcx->basin->left_node->point->y > tcx->basin->right_node->point->y;

  Sweep_fill_basin_req(self, tcx, tcx->basin->bottom_node);
}

void Sweep_fill_basin_req(Sweep self, SweepContext tcx, AFNode node) {
  if (Sweep_is_shallow(self, tcx, node))
    return;

  Sweep_fill(self, tcx, node);

  if (node->prev == tcx->basin->left_node && node->next == tcx->basin->right_node) {
    return;
  } else if (node->prev == tcx->basin->left_node) {
    enum Orientation o = orient_2d(node->point, node->next->point, node->next->next->point);
    if (o == CW)
      return;
    node = node->next;
  } else if (node->next == tcx->basin->right_node) {
    enum Orientation o = orient_2d(node->point, node->prev->point, node->prev->prev->point);
    if (o == CCW)
      return;
    node = node->prev;
  } else {
    if (node->prev->point->y < node->next->point->y)
      node = node->prev;
    else
      node = node->next;
  }
  Sweep_fill_basin_req(self, tcx, node);
}

bool Sweep_is_shallow(Sweep self, SweepContext tcx, AFNode node) {
  float height;
  if (tcx->basin->left_highest)
    height = tcx->basin->left_node->point->y - node->point->y;
  else
    height = tcx->basin->right_node->point->y - node->point->y;

  if (tcx->basin->width > height)
    return true;
  return false;
}

void Sweep_fill_edge_event(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  if (tcx->edge_event->right) {
    Sweep_fill_right_above_edge_event(self, tcx, edge, node);
  } else {
    Sweep_fill_left_above_edge_event(self, tcx, edge, node);
  }
}

void Sweep_fill_right_above_edge_event(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  while (node->next->point->x < edge->p->x) {
    if (orient_2d(edge->q, node->next->point, edge->p) == CCW)
      Sweep_fill_right_below_edge_event(self, tcx, edge, node);
    else
      node = node->next;
  }
}

void Sweep_fill_right_below_edge_event(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  if (node->point->x < edge->p->x) {
    if (orient_2d(node->point, node->next->point, node->next->next->point) == CCW) {
      Sweep_fill_right_concave_edge_event(self, tcx, edge, node);
    } else {
      Sweep_fill_right_convex_edge_event(self, tcx, edge, node);
      Sweep_fill_right_below_edge_event(self, tcx, edge, node);
    }
  }
}

void Sweep_fill_right_concave_edge_event(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  Sweep_fill(self, tcx, node->next);
  if (node->next->point != edge->p)
    if (orient_2d(edge->q, node->next->point, edge->p) == CCW)
      if (orient_2d(node->point, node->next->point, node->next->next->point) == CCW)
        Sweep_fill_right_concave_edge_event(self, tcx, edge, node);
}

void Sweep_fill_right_convex_edge_event(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  if (orient_2d(node->next->point, node->next->next->point, node->next->next->next->point) == CCW)
    Sweep_fill_right_concave_edge_event(self, tcx, edge, node->next);
  else if (orient_2d(edge->q, node->next->next->point, edge->p) == CCW)
      Sweep_fill_right_convex_edge_event(self, tcx, edge, node->next);
}

void Sweep_fill_left_above_edge_event(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  while (node->prev->point->x > edge->p->x) {
    if (orient_2d(edge->q, node->prev->point, edge->p) == CW)
      Sweep_fill_left_below_edge_event(self, tcx, edge, node);
    else
      node = node->prev;
  }
}

void Sweep_fill_left_below_edge_event(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  if (node->point->x > edge->p->x) {
    if (orient_2d(node->point, node->prev->point, node->prev->prev->point) == CW) {
      Sweep_fill_left_concave_edge_event(self, tcx, edge, node);
    } else {
      Sweep_fill_left_convex_edge_event(self, tcx, edge, node);
      Sweep_fill_left_below_edge_event(self, tcx, edge, node);
    }
  }
}

void Sweep_fill_left_convex_edge_event(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  if (orient_2d(node->prev->point, node->prev->prev->point, node->prev->prev->prev->point) == CW) {
    Sweep_fill_left_concave_edge_event(self, tcx, edge, node->prev);
  } else if (orient_2d(edge->q, node->prev->prev->point, edge->p) == CW)
      Sweep_fill_left_convex_edge_event(self, tcx, edge, node->prev);
}

void Sweep_fill_left_concave_edge_event(Sweep self, SweepContext tcx, Edge edge, AFNode node) {
  Sweep_fill(self, tcx, node->prev);
  if (node->prev->point != edge->p)
    if (orient_2d(edge->q, node->prev->point, edge->p) == CW)
      if (orient_2d(node->point, node->prev->point, node->prev->prev->point) == CW)
        Sweep_fill_left_concave_edge_event(self, tcx, edge, node);
}

void Sweep_flip_edge_event(Sweep self, SweepContext tcx, Point ep, Point eq, Tri  t, Point p) {
  Tri ot = Tri_neighbor_across(t, p);
  Point op = Tri_opposite_point(ot, t, p);

  if (ot == NULL)
    assert(0);

  if (in_scan_area(p, Tri_point_ccw(t, p), Tri_point_cw(t, p), op)) {
    Sweep_rotate_triangle_pair(self, t, p, ot, op);
    SweepContext_map_triangle_to_nodes(tcx, t);
    SweepContext_map_triangle_to_nodes(tcx, ot);

    if (p == eq && op == ep) {
      if (eq == tcx->edge_event->constrained_edge->q && ep == tcx->edge_event->constrained_edge->p) {
				Tri_mark_constrained_edge_pq(t, ep, eq);
				Tri_mark_constrained_edge_pq(ot, ep, eq);
        Sweep_legalize(self, tcx, t);
        Sweep_legalize(self, tcx, ot);
      }
    } else {
      enum Orientation o = orient_2d(eq, op, ep);
      t = Sweep_next_flip_triangle(self, tcx, (int)o, t, ot, p, op);
      Sweep_flip_edge_event(self, tcx, ep, eq, t, p);
    }
  } else {
    Point newP = Sweep_next_flip_point(self, ep, eq, ot, op);
    Sweep_flip_scan_edge_event(self, tcx, ep, eq, t, ot, newP);
    Sweep_edge_event_pptp(self, tcx, ep, eq, t, p);
  }
}

Tri Sweep_next_flip_triangle(Sweep self, SweepContext tcx, int o, Tri t, Tri ot, Point p, Point op) {
  if (o == CCW) {
    int edge_index = Tri_edge_index(ot, p, op);// ot.EdgeIndex(&p, &op);
    ot->delaunay[edge_index] = true;
    Sweep_legalize(self, tcx, ot);
    Tri_clear_delunay(ot);
    return t;
  }
  int edge_index = Tri_edge_index(t, p, op);// t.EdgeIndex(&p, &op);
  t->delaunay[edge_index] = true;
  Sweep_legalize(self, tcx, t);
  Tri_clear_delunay(t);
  return ot;
}

Point Sweep_next_flip_point(Sweep self, Point ep, Point eq, Tri ot, Point op) {
  enum Orientation o2d = orient_2d(eq, op, ep);
  if (o2d == CW)
    return Tri_point_ccw(ot, op);
  else if (o2d == CCW)
    return Tri_point_cw(ot, op);
  assert(0);
}

void Sweep_flip_scan_edge_event(Sweep self, SweepContext tcx, Point ep, Point eq, Tri flip_triangle, Tri t, Point p) {
  Tri ot = Tri_neighbor_across(t, p); // t.NeighborAcross(p);
  Point op = Tri_opposite_point(ot, t, p); // ot.OppositePoint(t, p);

  if (Tri_neighbor_across(t, p) == NULL)
    assert(0);

  if (in_scan_area(eq, Tri_point_ccw(flip_triangle, eq), Tri_point_cw(flip_triangle, eq), op))
    Sweep_flip_edge_event(self, tcx, eq, op, ot, op);
  else {
    Point newP = Sweep_next_flip_point(self, ep, eq, ot, op);
    Sweep_flip_scan_edge_event(self, tcx, ep, eq, flip_triangle, ot, newP);
  }
}

