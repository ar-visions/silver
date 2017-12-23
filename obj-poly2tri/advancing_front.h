#ifndef _ADVANCING_FRONT_H_
#define _ADVANCING_FRONT_H_

#include "primitives.h"
#include <stdlib.h>

#define _AFNode(D,T,C) _Base(spr,T,C)           \
    method(D,T,C,C,with_point,(Point))          \
    method(D,T,C,C,with_tri,(Point, Tri))       \
    private_var(D,T,C,Point,point)              \
    private_var(D,T,C,Tri,triangle)             \
    private_var(D,T,C,struct _object_AFNode *,next) \
    private_var(D,T,C,struct _object_AFNode *,prev) \
    private_var(D,T,C,float,value)
declare(AFNode, Base)

#define _AdvancingFront(D,T,C) _Base(spr,T,C)   \
    method(D,T,C,C,with_nodes,(AFNode, AFNode)) \
    method(D,T,C,AFNode,locate_node,(C, const float)) \
    method(D,T,C,AFNode,locate_point,(C, const Point)) \
    private_var(D,T,C,AFNode,head)              \
    private_var(D,T,C,AFNode,tail)              \
    private_var(D,T,C,AFNode,search_node)
declare(AdvancingFront, Base)

#endif