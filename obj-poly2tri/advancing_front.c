#include <obj-poly2tri/poly2tri.h>
#include <assert.h>

implement(AFNode)

AFNode AFNode_with_point(Point p) {
	AFNode self = auto(AFNode);
	self->point = p;
	self->value = p->x;
	return self;
}

AFNode AFNode_with_tri(Point p, Tri t) {
	AFNode self = auto(AFNode);
	self->point = p;
    self->triangle = t;
	self->value = p->x;
	return self;
}

implement(AdvancingFront)

AdvancingFront AdvancingFront_with_nodes(AFNode head, AFNode tail) {
	AdvancingFront self = auto(AdvancingFront);
	self->head = head;
	self->tail = tail;
	self->search_node = head;
	return self;
}

AFNode AdvancingFront_locate_node(AdvancingFront self, const float x) {
  AFNode node = self->search_node;

  if (x < node->value) {
    while ((node = node->prev) != NULL) {
      if (x >= node->value) {
        self->search_node = node;
        return node;
      }
    }
  } else {
    while ((node = node->next) != NULL) {
      if (x < node->value) {
        self->search_node = node->prev;
        return node->prev;
      }
    }
  }
  return NULL;
}

AFNode AdvancingFront_locate_point(AdvancingFront self, const Point point) {
  const float px = point->x;
  AFNode node = self->search_node;
  const float nx = node->point->x;

  if (px == nx) {
    if (point != node->point) {
      if (point == node->prev->point) {
        node = node->prev;
      } else if (point == node->next->point) {
        node = node->next;
      } else {
        assert(0); // [todo] replace with std error logging
      }
    }
  } else if (px < nx) {
    while ((node = node->prev) != NULL) {
      if (point == node->point) {
        break;
      }
    }
  } else {
    while ((node = node->next) != NULL) {
      if (point == node->point)
        break;
    }
  }
  if (node)
  	self->search_node = node;
  return node;
}