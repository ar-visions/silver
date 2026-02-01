#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <import>
#include <math.h>

#undef element

static const real PI = 3.1415926535897932384; // M_PI;
static const real c1 = 1.70158;
static const real c2 = c1 * 1.525;
static const real c3 = c1 + 1;
static const real c4 = (2 * PI) / 3;
static const real c5 = (2 * PI) / 4.5;


xcoord xcoord_with_string(string sx) {
    string f = string(first(sx));
    xcoord a = (xcoord) { .x_type = e_val(xalign, f->chars) };
    string sx_offset = mid(sx, 1, len(sx) - 1);

    if (last(sx_offset) == '%') {
        sx_offset = mid(sx_offset, 0, len(sx_offset) - 1);
        a.percent = 1.0f;
    }
    if (sx_offset)
        if (last(sx_offset) == '+') {
            sx_offset = mid(sx_offset, 0, len(sx_offset) - 1);
            a.relative = 1.0f;
        }

    a.offset = real_value(sx_offset);
    return a;
}

ycoord ycoord_with_string(string sy) {
    string f = string(first(sy));
    ycoord a = (ycoord) { .y_type = e_val(yalign, f->chars) };
    string sy_offset = mid(sy, 1, len(sy) - 1);

    if (last(sy_offset) == '%') {
        sy_offset = mid(sy_offset, 0, len(sy_offset) - 1);
        a.percent = 1.0f;
    }
    if (sy_offset)
        if (last(sy_offset) == '+') {
            sy_offset = mid(sy_offset, 0, len(sy_offset) - 1);
            a.relative = 1.0f;
        }

    a.offset = real_value(sy_offset);
    return a;
}

// needs a relative input for each case that is non-width
f32 xcoord_plot(xcoord* a, f32 from, f32 to, xcoord* left_operand, f32* left_value, rect rel) {
    f32 val = f32_mix(a->offset, a->offset * (to - from), a->percent);
    /// if this is relative, its a sub-routine
    if (a->x_type > xalign_right) {
        verify(left_operand, "using width for left-operand");
        bool is_valid = left_operand && left_value && left_operand->x_type <= xalign_right;
        verify(is_valid, "invalid left-operand (xcoord)");
        return *left_value + val;
    }
    f32 r = (rel && a->relative) ? rel->w * a->relative : 0;
    f32 hsz = (to - from) / 2.0; // half size
    if (a->x_type < xalign_middle) { // if we are transitioning to middle from left
        f32 M = (a->x_type * 2.0f);
        return from + hsz * M + val + r;
    }
    // in this case, middle has offset that goes positive, transitioning to a negative for R
    f32 R = ((a->x_type - xalign_middle) * 2.0f);
    return from + ((hsz + val) * (1.0f - R)) + ((hsz * 2.0f - val) * R) + r;
}

f32 ycoord_plot(ycoord* a, f32 from, f32 to, ycoord* top_operand, f32* top_value, rect rel) {
    f32 val = f32_mix(a->offset, a->offset * (to - from), a->percent);
    /// if this is relative, its a sub-routine
    if (a->y_type > yalign_bottom) {
        verify(top_operand, "using height for left-operand");
        bool is_valid = top_operand && top_value && top_operand->y_type <= yalign_bottom;
        verify(is_valid, "invalid top-operand (ycoord)");
        return *top_value + val;
    }
    f32 r = (rel && a->relative) ? rel->h * a->relative : 0;
    f32 hsz = (to - from) / 2.0; // half size
    if (a->y_type < yalign_middle) { // if we are transitioning to middle from left
        f32 M = (a->y_type * 2.0f);
        return from + hsz * M + val + r;
    }
    // in this case, middle has offset that goes positive, transitioning to a negative for R (bottom)
    f32 R = ((a->y_type - yalign_middle) * 2.0f);
    return from + ((hsz + val) * (1.0f - R)) + ((hsz * 2.0f - val) * R) + r;
}


rotation rotation_with_string(rotation a, string s) {
    array sp = split(s, " ");
    verify(len(sp) == 4, "expected x y z Ndeg");
    verify(ends_with((string)sp->origin[3], "deg"), "expected 1deg (for example)");

    a->axis.x = real_value((string)sp->origin[0]);
    a->axis.y = real_value((string)sp->origin[1]);
    a->axis.z = real_value((string)sp->origin[2]);
    a->degs   = real_value((string)sp->origin[3]);
    return a;
}

rotation rotation_with_vec4f(rotation a, vec4f f) {
    a->axis.x = f.x;
    a->axis.y = f.y;
    a->axis.z = f.z;
    a->degs   = f.w;
    return a;
}

rotation rotation_mix(rotation a, rotation b, f32 f) {
    return rotation(
        axis, vec3f_mix(&a->axis, &b->axis, f),
        degs,   f32_mix( a->degs,  b->degs, f));
}



translation translation_with_string(translation a, string s) {
    array sp = split(s, " ");
    verify(len(sp) == 3, "expected x y z");
    a->value.x = real_value((string)sp->origin[0]);
    a->value.y = real_value((string)sp->origin[1]);
    a->value.z = real_value((string)sp->origin[2]);
    return a;
}

translation translation_with_vec3f(translation a, vec3f f) {
    a->value.x = f.x;
    a->value.y = f.y;
    a->value.z = f.z;
    return a;
}

translation translation_mix(translation a, translation b, f32 f) {
    return translation(
        value, vec3f_mix(&a->value, &b->value, f));
}



scaling scaling_with_string(scaling a, string s) {
    array sp = split(s, " ");
    verify(len(sp) == 3, "expected x y z");
    a->value.x = real_value((string)sp->origin[0]);
    a->value.y = real_value((string)sp->origin[1]);
    a->value.z = real_value((string)sp->origin[2]);
    return a;
}

scaling scaling_with_vec3f(scaling a, vec3f f) {
    a->value.x = f.x;
    a->value.y = f.y;
    a->value.z = f.z;
    return a;
}

scaling scaling_mix(scaling a, scaling b, f32 f) {
    return scaling(
        value, vec3f_mix(&a->value, &b->value, f));
}

define_class(rotation,    Au)
define_class(scaling,     Au)
define_class(translation, Au)












region region_with_f32(region reg, f32 f) {
    reg->l = (xcoord) { .x_type = xalign_left,   .offset = f };
    reg->t = (ycoord) { .y_type = yalign_top,    .offset = f };
    reg->r = (xcoord) { .x_type = xalign_right,  .offset = f };
    reg->b = (ycoord) { .y_type = yalign_bottom, .offset = f };
    reg->set = true;
    return reg;
}

region region_with_rect(region reg, rect r) {
    reg->l = (xcoord) { .x_type = xalign_left,   .offset = r->x };
    reg->t = (ycoord) { .y_type = yalign_top,    .offset = r->y };
    reg->r = (xcoord) { .x_type = xalign_width,  .offset = r->w };
    reg->b = (ycoord) { .y_type = yalign_height, .offset = r->h };
    reg->set = true;
    return reg;
}

region region_with_string(region reg, string s) {
    array a = split(s, " ");
    if (len(a) == 4) {
        reg->l = xcoord_with_string(a->origin[0]);
        reg->t = ycoord_with_string(a->origin[1]);
        reg->r = xcoord_with_string(a->origin[2]);
        reg->b = ycoord_with_string(a->origin[3]);

    } else if (len(a) == 1) {
        reg->l = xcoord_with_string(a->origin[0]);
        reg->t = ycoord_with_string(a->origin[0]);
        reg->r = xcoord_with_string(a->origin[0]);
        reg->b = ycoord_with_string(a->origin[0]);
    }
    reg->set = true;
    return reg;
}

region region_with_cstr(region a, cstr s) {
    return region_with_string(new(region), string(s));
}

rect region_rectangle(region a, rect win, rect rel) {
    f32 l = xcoord_plot(&a->l, win->x, win->x + win->w,  null, null, rel);
    f32 r = xcoord_plot(&a->r, win->x, win->x + win->w, &a->l, &l,   rel);
    f32 t = ycoord_plot(&a->t, win->y, win->y + win->h,  null, null, rel);
    f32 b = ycoord_plot(&a->b, win->y, win->y + win->h, &a->t, &t,   rel);
    return rect(x, l, y, t, w, r - l, h, b - t);
}

xcoord xcoord_mix(xcoord* a, xcoord* b, f32 f) {
    return (xcoord) {
        .x_type   = a->x_type   * (1.0f - f) + b->x_type   * f,
        .offset   = a->offset   * (1.0f - f) + b->offset   * f,
        .percent  = a->percent  * (1.0f - f) + b->percent  * f,
        .relative = a->relative * (1.0f - f) + b->relative * f
    };
}

ycoord ycoord_mix(ycoord* a, ycoord* b, f32 f) {
    return (ycoord) {
        .y_type   = a->y_type   * (1.0f - f) + b->y_type   * f,
        .offset   = a->offset   * (1.0f - f) + b->offset   * f,
        .percent  = a->percent  * (1.0f - f) + b->percent  * f,
        .relative = a->relative * (1.0f - f) + b->relative * f
    };
}

region region_mix(region a, region b, f32 f) {
    region reg = region(set, true);
    reg->l = xcoord_mix(&a->l, &b->l, f);
    reg->t = ycoord_mix(&a->t, &b->t, f);
    reg->r = xcoord_mix(&a->r, &b->r, f);
    reg->b = ycoord_mix(&a->b, &b->b, f);
    return reg;
}

bool style_qualifier_cast_bool(style_qualifier q) {
    return len(q->type) || q->id || q->state;
}

bool style_transition_cast_bool(style_transition a) {
    return a->duration->scale_v > 0;
}

style_transition style_transition_with_string(style_transition a, string s) {
    print("transition: %o", s);
    array sp = split(s, " ");
    sz    ln = len(sp);
    /// syntax:
    /// 500ms [ease [out]]
    /// 0.2s -- will be linear with in (argument meaningless for linear but applies to all others)
    string dur_string = sp->origin[0];
    a->duration = unit_with_string(new(tcoord), dur_string);
    a->easing = ln > 1 ? e_val(Ease,      sp->origin[1]) : Ease_linear;
    a->dir    = ln > 2 ? e_val(Direction, sp->origin[2]) : Direction_in;
    return a;
}

static real ease_linear        (real x) { return x; }
static real ease_in_quad       (real x) { return x * x; }
static real ease_out_quad      (real x) { return 1 - (1 - x) * (1 - x); }
static real ease_in_out_quad   (real x) { return x < 0.5 ? 2 * x * x : 1 - pow(-2 * x + 2, 2) / 2; }
static real ease_in_cubic      (real x) { return x * x * x; }
static real ease_out_cubic     (real x) { return 1 - pow(1 - x, 3); }
static real ease_in_out_cubic  (real x) { return x < 0.5 ? 4 * x * x * x : 1 - pow(-2 * x + 2, 3) / 2; }
static real ease_in_quart      (real x) { return x * x * x * x; }
static real ease_out_quart     (real x) { return 1 - pow(1 - x, 4); }
static real ease_in_out_quart  (real x) { return x < 0.5 ? 8 * x * x * x * x : 1 - pow(-2 * x + 2, 4) / 2; }
static real ease_in_quint      (real x) { return x * x * x * x * x; }
static real ease_out_quint     (real x) { return 1 - pow(1 - x, 5); }
static real ease_in_out_quint  (real x) { return x < 0.5 ? 16 * x * x * x * x * x : 1 - pow(-2 * x + 2, 5) / 2; }
static real ease_in_sine       (real x) { return 1 - cos((x * PI) / 2); }
static real ease_out_sine      (real x) { return sin((x * PI) / 2); }
static real ease_in_out_sine   (real x) { return -(cos(PI * x) - 1) / 2; }
static real ease_in_expo       (real x) { return x == 0 ? 0 : pow(2, 10 * x - 10); }
static real ease_out_expo      (real x) { return x == 1 ? 1 : 1 - pow(2, -10 * x); }
static real ease_in_out_expo   (real x) {
    return x == 0
        ? 0
        : x == 1
        ? 1
        : x < 0.5
        ? pow(2, 20 * x - 10) / 2
        : (2 - pow(2, -20 * x + 10)) / 2;
}
static real ease_in_circ       (real x) { return 1 - sqrt(1 - pow(x, 2)); }
static real ease_out_circ      (real x) { return sqrt(1 - pow(x - 1, 2)); }
static real ease_in_out_circ   (real x) {
    return x < 0.5
        ? (1 - sqrt(1 - pow(2 * x, 2))) / 2
        : (sqrt(1 - pow(-2 * x + 2, 2)) + 1) / 2;
}
static real ease_in_back       (real x) { return c3 * x * x * x - c1 * x * x; }
static real ease_out_back      (real x) { return 1 + c3 * pow(x - 1, 3) + c1 * pow(x - 1, 2); }
static real ease_in_out_back   (real x) {
    return x < 0.5
        ? (pow(2 * x, 2) * ((c2 + 1) * 2 * x - c2)) / 2
        : (pow(2 * x - 2, 2) * ((c2 + 1) * (x * 2 - 2) + c2) + 2) / 2;
}
static real ease_in_elastic    (real x) {
    return x == 0
        ? 0
        : x == 1
        ? 1
        : -pow(2, 10 * x - 10) * sin((x * 10 - 10.75) * c4);
}
static real ease_out_elastic   (real x) {
    return x == 0
        ? 0
        : x == 1
        ? 1
        : pow(2, -10 * x) * sin((x * 10 - 0.75) * c4) + 1;
}
static real ease_in_out_elastic(real x) {
    return x == 0
        ? 0
        : x == 1
        ? 1
        : x < 0.5
        ? -(pow(2, 20 * x - 10) * sin((20 * x - 11.125) * c5)) / 2
        : (pow(2, -20 * x + 10) * sin((20 * x - 11.125) * c5)) / 2 + 1;
}
static real bounce_out(real x) {
    const real n1 = 7.5625;
    const real d1 = 2.75;
    if (x < 1 / d1) {
        return n1 * x * x;
    } else if (x < 2 / d1) {
        return n1 * (x - 1.5 / d1) * x + 0.75;
    } else if (x < 2.5 / d1) {
        return n1 * (x - 2.25 / d1) * x + 0.9375;
    } else {
        return n1 * (x - 2.625 / d1) * x + 0.984375;
    }
}
static real ease_in_bounce     (real x) {
    return 1 - bounce_out(1 - x);
}
static real ease_out_bounce    (real x) { return bounce_out(x); }
static real ease_in_out_bounce (real x) {
    return x < 0.5
        ? (1 - bounce_out(1 - 2 * x)) / 2
        : (1 + bounce_out(2 * x - 1)) / 2;
}

static i64 distance(cstr s0, cstr s1) {
    i64 r = (i64)s1 - (i64)s0;
    return r < 0 ? -r : r;
}

/// functions are courtesy of easings.net; just organized them into 2 enumerables compatible with web
real style_transition_pos(style_transition a, real tf) {
    real x = clampf(tf, 0.0, 1.0);
    switch (a->easing) {
        case Ease_linear:
            switch (a->dir) {
                case Direction_in:      return ease_linear(x);
                case Direction_out:     return ease_linear(x);
                case Direction_in_out:  return ease_linear(x);
            }
            break;
        case Ease_quad:
            switch (a->dir) {
                case Direction_in:      return ease_in_quad(x);
                case Direction_out:     return ease_out_quad(x);
                case Direction_in_out:  return ease_in_out_quad(x);
            }
            break;
        case Ease_cubic:
            switch (a->dir) {
                case Direction_in:      return ease_in_cubic(x);
                case Direction_out:     return ease_out_cubic(x);
                case Direction_in_out:  return ease_in_out_cubic(x);
            }
            break;
        case Ease_quart:
            switch (a->dir) {
                case Direction_in:      return ease_in_quart(x);
                case Direction_out:     return ease_out_quart(x);
                case Direction_in_out:  return ease_in_out_quart(x);
            }
            break;
        case Ease_quint:
            switch (a->dir) {
                case Direction_in:      return ease_in_quint(x);
                case Direction_out:     return ease_out_quint(x);
                case Direction_in_out:  return ease_in_out_quint(x);
            }
            break;
        case Ease_sine:
            switch (a->dir) {
                case Direction_in:      return ease_in_sine(x);
                case Direction_out:     return ease_out_sine(x);
                case Direction_in_out:  return ease_in_out_sine(x);
            }
            break;
        case Ease_expo:
            switch (a->dir) {
                case Direction_in:      return ease_in_expo(x);
                case Direction_out:     return ease_out_expo(x);
                case Direction_in_out:  return ease_in_out_expo(x);
            }
            break;
        case Ease_circ:
            switch (a->dir) {
                case Direction_in:      return ease_in_circ(x);
                case Direction_out:     return ease_out_circ(x);
                case Direction_in_out:  return ease_in_out_circ(x);
            }
            break;
        case Ease_back:
            switch (a->dir) {
                case Direction_in:      return ease_in_back(x);
                case Direction_out:     return ease_out_back(x);
                case Direction_in_out:  return ease_in_out_back(x);
            }
            break;
        case Ease_elastic:
            switch (a->dir) {
                case Direction_in:      return ease_in_elastic(x);
                case Direction_out:     return ease_out_elastic(x);
                case Direction_in_out:  return ease_in_out_elastic(x);
            }
            break;
        case Ease_bounce:
            switch (a->dir) {
                case Direction_in:      return ease_in_bounce(x);
                case Direction_out:     return ease_out_bounce(x);
                case Direction_in_out:  return ease_in_out_bounce(x);
            }
            break;
    };
    return x;
}

/// to debug style, place conditional breakpoint on member->s_key == "your-prop" (used in a style block) and n->data->id == "id-you-are-debugging"
/// hopefully we dont have to do this anymore.  its simple and it works.  we may be doing our own style across service component and elemental component but having one system for all is preferred,
/// and brings a sense of orthogonality to the react-like pattern, adds type-based contextual grabs and field lookups with prop accessors

style_entry style_best_match(style a, element n, string prop_name, array entries) {
  //array       blocks     = get(members, prop_name);
    style_entry match      = null; /// key is always a symbol, and maps are keyed by symbol
    real        best_score = 0;
    Au_t       type       = isa(n);
    each (entries, style_entry, e) {
        style_block  bl = e->bl;
        real   sc = score(bl, n, true);
        if (sc > 0 && sc >= best_score) {
            match = get(bl->entries, prop_name);
            verify(match, "should have matched");
            best_score = sc;
        }
    }
    return match;
}

void style_block_init(style_block a) {
    if (!a->entries) a->entries = map(hsize, 16);
    if (!a->blocks)  a->blocks  = array(alloc, 16);
}

f32 style_block_score(style_block a, element n, bool score_state) {
    f32 best_sc = 0;
    element   cur     = n;

    each (a->quals, style_qualifier, q) {
        f32 best_this = 0;
        for (;;) {
            bool    id_match  = q->id &&  eq(q->id, cur->id->chars);
            bool   id_reject  = q->id && !id_match;
            bool  type_match  = q->ty &&  A_inherits(isa(cur), q->ty);
            bool type_reject  = q->ty && !type_match;
            bool state_match  = score_state && q->state;
            if (state_match) {
                verify(len(q->state) > 0, "null state");
                object addr = A_get_property(cur, cstring(q->state));
                if (addr)
                    state_match = A_header(addr)->type->cast_bool(addr);
            }

            bool state_reject = score_state && q->state && !state_match;
            if (!id_reject && !type_reject && !state_reject) {
                f64 sc = (sz)(   id_match) << 1 |
                         (sz)( type_match) << 0 |
                         (sz)(state_match) << 2;
                best_this = max(sc, best_this);
            } else
                best_this = 0;

            if (q->parent && best_this > 0) {
                q   = q->parent; // parent qualifier
                cur = cur->parent ? cur->parent : new(element); // parent element
            } else
                break;
        }
        best_sc = max(best_sc, best_this);
    }
    return best_sc > 0 ? 1.0 : 0.0;
};

f64 Duration_base_millis(Duration duration) {
    switch (duration) {
        case Duration_ns: return 1.0 / 1000.0;
        case Duration_ms: return 1.0;
        case Duration_s:  return 1000.0;
    }
    return 0.0;
}

i64 tcoord_get_millis(tcoord a) {
    Duration u = (Duration)(i32)a->enum_v;
    f64 base = Duration_base_millis(u);
    return base * a->scale_v;
}

bool style_applicable(style s, element n, string prop_name, array result) {
    array blocks = get(s->members, prop_name); // style free'd?
    Au_t type   = isa(n);
    bool  ret    = false;

    clear(result);
    if (blocks)
        each (blocks, style_block, block) {
            if (!len(block->types) || index_of(block->types, type) >= 0) {
                item f = lookup(block->entries, prop_name); // this returns the wrong kind of item reference
                if (f && score(block, n, false) > 0) {
                    Au_t ftype = isa(f->value);
                    push(result, f->value);
                    ret = true;
                }
            }
        }
    return ret;
}

void  event_prevent_default (event e) {         e->prevent_default = true;  }
bool  event_is_default      (event e) { return !e->prevent_default;         }
bool  event_should_propagate(event e) { return !e->stop_propagation;        }
bool  event_stop_propagation(event e) { return  e->stop_propagation = true; }
none  event_clear           (event e) {
    //drop(e->key.text);
    memset(e, 0, sizeof(struct _event));
    e->mouse.state  = -1;
    e->mouse.button = -1;
}

none element_draw(element a, window w) {
}

int element_compare(element a, element b) {
    Au_t type = isa(a);
    if (type != isa(b))
        return -1;
    if (a == b)
        return 0;
    for (int m = 0; m < type->member_count; m++) {
        type_member_t* mem = &type->members[m];
        bool is_prop = mem->member_type & A_MEMBER_PROP;
        if (!is_prop || strcmp(mem->name, "elements") == 0) continue;
        if (A_is_inlay(mem)) { // works for structs and primitives
            ARef cur = (ARef)((cstr)a + mem->offset);
            ARef nxt = (ARef)((cstr)b + mem->offset);
            bool is_same = (cur == nxt || 
                memcmp(cur, nxt, mem->type->size) == 0);
            if (!is_same)
                return -1;
        } else {
            object* cur = (object*)((cstr)a + mem->offset);
            object* nxt = (object*)((cstr)b + mem->offset);
            if (*cur != *nxt) {
                bool is_same = (*cur && *nxt) ? 
                    compare(*cur, *nxt) == 0 : false;
                if (!is_same)
                    return -1;
            }
        }
    }
    return 0;
}


map element_render(element a, array changed) {
    return a->origin; /// elements is not allocated for non-container elements, so default behavior is to not host components
}


style style_with_path(style a, path css_path) {
    verify(exists(css_path), "css path does not exist");
    string style_str = read(css_path, typeid(string), null);
    a->base    = array(alloc, 32);
    if (css_path != a->css_path) {
        a->css_path = css_path;
    }
    a->mod_time = modified_time(css_path);
    a->members = map(hsize, 32);
    process(a, style_str);
    cache_members(a);
    a->loaded   = true;
    a->reloaded = true; /// cache validation for composer user
    return a;
}

bool style_check_reload(style a) {
    verify(a->css_path, "style not loaded with path");
    i64 m = modified_time(a->css_path);
    if (a->mod_time != m) {
        a->mod_time  = m;
        //drop(a->members);
        //drop(a->base);
        style_with_path(a, a->css_path);
        A_hold_members(a);
        return true;
    }
    return false;
}

none style_watch_reload(style a, array css, ARef arg) {
    style_with_path(a, css);
}

style style_with_object(style a, object app) {
    Au_t  app_type  = isa(app);
    string root_type = string(app_type->name);
    path   css_path  = form(path, "style/%o.css", root_type);
    /*a->reloader = watch(
        res,        css_path,
        callback,   style_watch_reload); -- todo: implement watch [perfectly good one] */
    return style_with_path(a, css_path);
}

bool is_cmt(symbol c) {
    return c[0] == '/' && c[1] == '*';
}

bool ws(cstr* p_cursor) {
    cstr cursor = *p_cursor;
    while (isspace(*cursor) || is_cmt(cursor)) {
        while (isspace(*cursor))
            cursor++;
        if (is_cmt(cursor)) {
            cstr f = strstr(cursor, "*/");
            cursor = f ? &f[2] : &cursor[strlen(cursor) - 1];
        }
    }
    *p_cursor = cursor;
    return *cursor != 0;
}

static bool scan_to(cstr* p_cursor, string chars) {
    bool sl  = false;
    bool qt  = false;
    bool qt2 = false;
    cstr cursor = *p_cursor;
    for (; *cursor; cursor++) {
        if (!sl) {
            if (*cursor == '"')
                qt = !qt;
            else if (*cursor == '\'')
                qt2 = !qt2;
        }
        sl = *cursor == '\\';
        if (!qt && !qt2) {
            char cur[2] = { *cursor, 0 };
            if (index_of(chars, cur) >= 0) {
                 *p_cursor = cursor;
                 return true;
            }
        }
    }
    *p_cursor = null;
    return false;
}

static array parse_qualifiers(style_block bl, cstr *p) {
    string   qstr;
    cstr start = *p;
    cstr end   = null;
    cstr scan  =  start;

    /// read ahead to {
    do {
        if (!*scan || *scan == '{') {
            end  = scan;
            qstr = string(chars, start, ref_length, distance(start, scan));
            break;
        }
    } while (*++scan);
    
    ///
    if (!qstr) {
        end = scan;
        *p  = end;
        return null;
    }
    
    ///
    array quals = split(qstr, ",");
    array result = array(alloc, 32);
    
    array ops = array_of_cstr("!=", ">=", "<=", ">", "<", "=", null);
    ///
    each (quals, string, qs) {
        string  qq = trim(qs);
        if (!len(qq)) continue;
        style_qualifier v = style_qualifier(); /// push new qualifier
        push(result, v);

        /// we need to scan by >
        array parent_to_child = split(qq, "/"); /// we choose not to use > because it interferes with ops
        style_qualifier processed = null;

        /// iterate through reverse
        for (int i = len(parent_to_child) - 1; i >= 0; i--) {
            string q = trim((string)parent_to_child->origin[i]);
            if (processed) {
                v->parent = hold(style_qualifier()); //opaque, must hold
                v = v->parent; /// dont need to cast this
            }
            num idot = index_of(q, ".");
            num icol = index_of(q, ":");

            string tail = null;
            ///
            if (idot >= 0) {
                array sp = split(q, ".");
                bool no_type = q->chars[0] == '.';
                v->type   = no_type ? string("element") : trim((string)first(sp));
                array sp2 = split((string)sp->origin[no_type ? 0 : 1], ":");
                v->id     = first(sp2);
                if (icol >= 0)
                    tail  = trim(mid(q, icol + 1, len(q) - (icol + 1))); /// likely fine to use the [1] component of the split
            } else {
                if (icol  >= 0) {
                    v->type = trim(mid(q, 0, icol));
                    tail   = trim(mid(q, icol + 1, len(q) - (icol + 1)));
                } else
                    v->type = trim(q);
            }
            if (v->type) { /// todo: verify idata is correctly registered and looked up
                v->ty = A_find_type(v->type->chars, null);
                verify(v->ty, "type must exist: %o", v->type);
                if (index_of(bl->types, v->ty) == -1)
                    push(bl->types, v->ty);
            }
            if (tail) {
                // check for ops
                bool is_op = false;
                each (ops, string, op) {
                    if (index_of(tail, cstring(op)) >= 0) {
                        is_op   = true;
                        array sp = split(tail, cstring(op));
                        string f = first(sp);
                        v->state = trim(f);
                        v->oper  = op;
                        int istart = len(f) + len(op);
                        v->value = trim(mid(tail, istart, len(tail) - istart));
                        break;
                    }
                }
                if (!is_op)
                    v->state = tail;
            }
            processed = v;
            A_hold_members(v);
        }
    }
    //drop(ops);
    *p = end;
    return result;
}

/// compute available entries for props on a Element
map style_compute(style a, element n) {
    map avail = map(hsize, 16);
    Au_t ty = isa(n);
    verify(instanceof(n, element), "must inherit element");
    while (ty != typeid(Au)) {
        array all = array(alloc, 32);
        for (int m = 0; m < ty->member_count; m++) {
            type_member_t* mem = &ty->members[m];
            if (mem->member_type != A_MEMBER_PROP)
                continue;
            Au_t sname_type = isa(mem->sname);
            A sname_header = A_header(mem->sname);
            string name = mem->sname;
            if (applicable(a, n, name, all)) {
                set(avail, name, all);
                all = array(alloc, 32);
            }
        }
        ty = ty->parent_type;
    }
    return avail;
}

static void cache_b(style a, style_block bl) {
    pairs (bl->entries, i) {
        string      key = i->key;
        style_entry e   = i->value;
        bool  found = false;
        array cache = get(a->members, e->member);
        if (!cache) {
             cache = array();
             set(a->members, e->member, cache);
        }
        each (cache, style_block, cb)
            found |= cb == bl;
        ///
        if (!found)
            push(cache, bl);
    }
    each (bl->blocks, style_block, s)
        cache_b(a, s);
}

void style_cache_members(style a) {
    if (a->base)
        each (a->base, style_block, b)
            cache_b(a, b);
}

/// \\ = \ ... \x = \x
static string parse_quoted(cstr *cursor, size_t max_len) {
    cstr first = *cursor;
    if (*first != '"')
        return string("");
    ///
    bool last_slash = false;
    cstr start     = ++(*cursor);
    cstr s         = start;
    ///
    for (; *s != 0; ++s) {
        if (*s == '\\')
            last_slash = true;
        else if (*s == '"' && !last_slash)
            break;
        else
            last_slash = false;
    }
    if (*s == 0)
        return string("");
    ///
    size_t len = (size_t)(s - start);
    if (max_len > 0 && len > max_len)
        return string("");
    ///
    *cursor = &s[1];
    return string(chars, start, ref_length, len);
}


none parse_block(style_block bl, cstr* p_sc) {
    cstr sc = *p_sc;
    ws(&sc);
    verify(*sc == '.' || isalpha(*sc), "expected Type[.id], or .id");
    bl->quals = hold(parse_qualifiers(bl, &sc));
    sc++;
    ws(&sc);
    ///
    while (*sc && *sc != '}') {
        /// read up to ;, {, or }
        ws(&sc);
        cstr start = sc;
        verify(scan_to(&sc, string(";{}")), "expected member expression or qualifier");
        if (*sc == '{') {
            ///
            style_block bl_n = hold(style_block(types, array()));
            push(bl->blocks, bl_n);
            bl_n->parent = bl;
            /// parse sub-block
            sc = start;
            parse_block(bl_n, sc);
            verify(*sc == '}', "expected }");
            sc++;
            ws(&sc);
            ///
        } else if (*sc == ';') {
            /// read member
            cstr cur = start;
            verify(scan_to(&cur, string(":")) && (cur < sc), "expected [member:]value;");
            string  member = string(chars, start, ref_length, distance(start, cur));
            for (int i = 0; i < member->len; i++)
                if (member->chars[i] == '-') (*(cstr)&member->chars[i]) = '_';
            cur++;
            ws(&cur);

            /// read value
            cstr vstart = cur;
            verify(scan_to(&cur, string(";")), "expected member:value[, transition];");
            
            /// needs escape sequencing?
            size_t len      = distance(vstart, cur);
            string cb_value = trim(string(chars, vstart, ref_length, len));
            string end      = mid(cb_value, -1, 1);
            bool   qs       = eq(mid(cb_value,  0, 1), "\"");
            bool   qe       = eq(mid(cb_value, -1, 1), "\"");

            if (qs && qe) {
                cstr   cs = cstring(cb_value);
                cb_value  = parse_quoted(&cs, len(cb_value));
            }
            string  param = null;
            object  value = null;
            num         a = index_of(cb_value, "[");
            num         i = index_of(cb_value, ",");

            if (a != -1 && (i == -1 || a < i)) {
                num ae = index_of(cb_value, "]");
                verify(ae != -1, "expected ']' character");
                string items = trim(mid(cb_value, a + 1, ae - 1));
                int ln = len(cb_value) - ae;
                param = trim(mid(cb_value, ae + 1, ln));
                if (first(param) == ',') {
                    param = trim(mid(param, 1, len(param) - 1));
                } else {
                    param = null;
                }
                array sp = split(items, ",");
                value = array((i32)len(sp));
                each(sp, string, raw) {
                    push((array)value, trim(raw));
                }

                items = items;
            } else {
                param = i >= 0 ? trim(mid(cb_value, i + 1, len(cb_value) - (i + 1))) : null;
                value = i >= 0 ? trim(mid(cb_value, 0, i))  : cb_value;
            }

            style_transition trans = param ? style_transition(param) : null;
            
            /// check
            verify(member, "member cannot be blank");
            verify(value,  "value cannot be blank");
            style_entry e = style_entry(
                member, member, value, value, trans, trans, bl, bl);
            set(bl->entries, member, e);
            /// 
            sc++;
            ws(&sc);
        }
    }
    verify(!*sc || *sc == '}', "expected closed-brace");
    if (*sc == '}')
        sc++;
    *p_sc = sc;
}

void style_process(style a, string code) {
    a->base = array(alloc, 32);
    for (cstr sc = cstring(code); sc && *sc; ws(&sc)) {
        style_block n_block = style_block(types, array());
        push(a->base, n_block);
        parse_block(n_block, &sc);
    }
}

array composer_apply_args(composer ux, element i, element e) {
    Au_t type    = isa(e);
    array changed = array(alloc, 32);
    u64   f_user  = AF_bits(e);

    // check the difference between members (not elements within)
    while (type != typeid(Au)) { 
        for (int m = 0; m < type->member_count; m++) {
            type_member_t* mem = &type->members[m];
            bool is_prop = mem->member_type & A_MEMBER_PROP;
            if (!is_prop || strcmp(mem->name, "elements") == 0) continue;
            bool is_set = ((1 << mem->id) & f_user) != 0;
            if (A_is_inlay(mem)) { // works for structs and primitives
                ARef cur = (ARef)((cstr)i + mem->offset);
                ARef nxt = (ARef)((cstr)e + mem->offset);
                bool is_same = (cur == nxt || 
                    memcmp(cur, nxt, mem->type->size) == 0);
                
                /// primitive memory of zero is effectively unset for args
                if (!is_same && is_set) {
                    memcpy(cur, nxt, mem->type->size);
                    push(changed, mem->sname);
                }
            } else {
                object* cur = (object*)((cstr)i + mem->offset);
                object* nxt = (object*)((cstr)e + mem->offset);
                if (*nxt && *cur != *nxt) {
                    bool is_same = (*cur && *nxt) ? compare(*cur, *nxt) == 0 : false;
                    if (!is_same && is_set) {
                        if (*cur != *nxt) {
                            print("prop different: %s, prev value: %o", mem->name, *cur);
                            //drop(*cur);
                            *cur = hold(*nxt); // hold required here, because member dealloc happens on the other object
                            push(changed, mem->sname);
                        }
                    }
                }
            }
        }
        type = type->parent_type;
    }
    return changed;
}

array composer_apply_style(composer ux, element i, map style_avail, array exceptions) {
    Au_t type = isa(i);
    array changed = array(alloc, 32);

    while (type != typeid(Au)) {
        for (int m = 0; m < type->member_count; m++) {
            type_member_t* mem = &type->members[m];
            bool is_prop = mem->member_type & A_MEMBER_PROP;
            if (!is_prop || strcmp(mem->name, "elements") == 0)
                continue;
            
            A info = head(mem->sname);
            string prop    = string(mem->name);
            array entries = get(style_avail, prop);
            if (!entries)
                continue;
            // dont apply over these exceptional args
            if (exceptions && index_of(exceptions, prop) >= 0)
                continue;
            
            /// compute best match for this prop against style_entries
            style_entry best = best_match(ux->style, i, prop, entries);
            if (!best)
                continue;

            // lazily create instance value from string on style entry
            if (!best->instance) {
                if (mem->type == typeid(object))
                    best->instance = hold(copy(best->value));
                else {
                    array a = instanceof(best->value, array);
                    if (a || mem->type == typeid(array)) {
                        best->instance = array((i32)(a ? len(a) : 1));
                        if (!a) a = a((string)best->value);
                        each(a, string, sv) {
                            Au_t of_type = mem->args.meta_0;
                            verify(of_type, "expected type of array member described: %s", mem->name);
                            object v = A_formatter(
                                of_type, null, (object)false,
                                (symbol)"%s", sv->chars);
                            push((array)best->instance, v);
                        }
                    } else {
                        best->instance = hold(A_formatter(
                            mem->type, null, (object)false,
                            (symbol)"%s", best->value->chars));
                    }
                }
            }
            verify(best->instance, "instance must be initialized");

            push(changed, prop);
            object* cur = (object*)((cstr)i + mem->offset);

            style_transition t  = best->trans;
            style_transition ct = null;
            bool should_trans = false;
            if (t) {
                if (t && !i->transitions)
                    i->transitions = hold(map(hsize, 16));
                ct = i->transitions ? get(i->transitions, prop) : null;
                if (!ct) {
                    ct = copy(t);
                    ct->reference = t; // mark as weak, or intern
                    should_trans = true;
                    set(i->transitions, prop, ct);
                } else {
                    should_trans = ct->reference != t;
                }
            } else if (i->transitions) {
                // check if there is an existing transition
                rm(i->transitions, prop); // This Should Happen when we release the mouse for the 'pressed' state, but the transition stays
            }
            
            // we know this is a different transition assigned
            if (ct && should_trans) {
                // save the value where it is now
                if (A_is_inlay(mem)) {
                    ct->from = A_alloc(mem->type, 1);
                    memcpy(ct->from, cur, mem->type->size);
                } else {
                    ct->from = *cur ? *cur : best->instance;
                }
                ct->type     = isa(best->instance);
                ct->location = cur; /// hold onto pointer location
                if (ct->to != best->instance)
                    ct->to  = best->instance;
                ct->start    = epoch_millis();
                ct->is_inlay = A_is_inlay(mem);
            } else if (!ct) {
                if (A_is_inlay(mem)) {
                    //print("%s cur = %p", mem->name, cur);
                    memcpy(cur, best->instance, mem->type->size);
                } else if (*cur != best->instance) {
                    //drop(*cur);
                    *cur = best->instance;
                }
            }
        }
        type = type->parent_type;
    }
    return changed;
}

void animate_element(composer ux, element e) {
    if (e->transitions) {
        i64 cur_millis = epoch_millis();

        pairs(e->transitions, i) {
            string prop = i->key;
            style_transition ct = i->value;
            i64 dur = tcoord_get_millis(ct->duration);
            i64 millis = cur_millis - ct->start;
            f64 cur_pos = style_transition_pos(ct, (f64)millis / (f64)dur);
            if (ct->type->traits & AU_TRAIT_PRIMITIVE) {
                verify(ct->is_inlay, "unsupported member type (primitive in object form)");
                /// mix these primitives; lets support i32 / enum, i64, f32, f64
                Au_t ct_type = ct->type;
                if (ct->type == typeid(f32)) {
                    *(f32*)ct->location = *(f32*)ct->from * (1.0f - cur_pos) + 
                                          *(f32*)ct->to   * cur_pos;
                }
            } else {
                type_member_t* fmix = A_member(ct->type, A_MEMBER_IMETHOD, "mix", false);
                verify(fmix, "animate: implement mix for type %s", ct->type->name);
                
                //drop(*ct->location);
                Au_t atype = ct->type;
                *ct->location = hold(((mix_fn)fmix->ptr)(ct->from, ct->to, cur_pos));
                //print("color = %o (millis: %i / %i, pos: %.2f)", *ct->location, (int)millis, (int)dur, cur_pos);
            }
        }
    }
    pairs(e->origin, i) { // todo: do we need mounts as separate map?
        element ee = i->value;
        animate_element(ux, ee);
    }
}

void composer_animate(composer ux) {
    if (!ux->root)
        return;
    invalidate(ux);
    animate_element(ux, ux->root);
    remove_invalid(ux);
}

void composer_remove_invalid_transitions(composer ux, element e) {
    bool found = true;
    while (found) {
        found = false;
        pairs(e->transitions, i) {
            style_transition t = i->value;
            if (t->invalidate) {
                rm(e->transitions, i->key);
                found = true;
                break;
            }
        }
    }
}

void composer_remove_invalid(composer ux) {
    composer_remove_invalid_transitions(ux, ux->root);
}

void composer_invalidate_element(composer ux, element e) {
    if (e && e->transitions)
        pairs(e->transitions, i) {
            style_transition t = i->value;
            t->invalidate = true;
        }
}

void composer_invalidate(composer ux) {
    composer_invalidate_element(ux, ux->root);
}

element composer_find_target(composer ux, element e, event ev, element must) {
    if (!e || e->disabled || e->ghost)
        return null;

    rect  b       = e->bounds;
    float x       = ev->mouse.pos.x;
    float y       = ev->mouse.pos.y;
    float local_x = x - b->x;
    float local_y = y - b->y;
    if (!must && (local_x < 0 || local_y < 0 || local_x >= b->w || local_y >= b->h))
        return null;

    ev->mouse.pos.x = local_x + e->scroll.x;
    ev->mouse.pos.y = local_y + e->scroll.y;

    // Check children first (from topmost to bottommost if needed)
    pairs(e->origin, i) {
        element child = i->value;
        element hit   = find_target(ux, child, ev, must);
        if     (hit) return hit;
    }

    // restore
    ev->mouse.pos.x = x;
    ev->mouse.pos.y = y;
    return (!must || must == e) ? e : null;
}

static none ux_event(element e, subs esubs, event ev) {
    if  (!esubs) return;
    each (esubs->entries, subscriber, sub) {
        ev->target = e;
        sub->method(sub->target, ev);
        if (ev->stop_propagation)
            break;
    }
}

none composer_wheel_event(composer ux, event ev) {
    // first element from the top level mouse over control to handle scroll
    for (element f = find_target(ux, ux->root, ev, ux->capture); f; f = f->parent) {
        ux_event(f, f->wheel, ev);
        if (ev->stop_propagation)
            break;
        else if (f->parent) {
            ev->mouse.pos.x -= f->bounds->x + f->scroll.x;
            ev->mouse.pos.y -= f->bounds->y + f->scroll.y;
        }
    }
}

none composer_text_event(composer ux, event ev) {
    element f = ux->focus;
    if     (f)  ux_event   (f, f->text, ev);
}

none composer_key_event(composer ux, event ev) {
    element f = ux->focus;
    if     (f)  ux_event   (f, f->move, ev);
}
 
none composer_move_event(composer ux, event ev) {
    element f = find_target(ux, ux->root, ev, null);
    element a = ux->capture ? 
        find_target(ux, ux->root, ev, ux->capture) : f;
    if (ux->hovered != f) {
        if (ux->hovered) {
            ux->hovered->restyle = true;
            ux->hovered->hovered = false;
            ux_event(ux->hovered, ux->hovered->leave, ev);
        }
        ux->hovered = f;
        if (f) {
            ux->hovered->restyle = true;
            ux->hovered->hovered = true;
            ux_event(f, f->hover, ev);
        }
    }
    if (a) ux_event(a, a->move, ev);
}
 
none composer_set_capture(composer ux, element e) {
    if (ux->capture != e) {
        if (ux->capture) {
            ux->capture->captured = false;
            ux->capture->restyle  = true;
            ux_event(ux->capture, ux->capture->lost_capture, null);
        }
        if (e && !e->ghost) {
            ux->capture = e;
            ux->capture->captured = true;
            ux->capture->restyle  = true;
            ux_event(ux->capture, e->got_capture, null);
        }
    }
}

none composer_release_capture(composer ux, element e) {
    if (ux->capture && ux->capture == e) {
        ux_event(ux->capture, ux->capture->lost_capture, null);
        ux->capture = null;
    }
}


none composer_set_focus(composer ux, element e) {
    if (ux->focus != e) {
        if (ux->focus) {
            ux->focus->focused = false;
            ux->focus->restyle = true;
            ux_event(ux->focus, ux->focus->blur, null);
        }
        if (e && !e->ghost) {
            ux->focus = e;
            ux->focus->focused = true;
            ux->focus->restyle = true;
            ux_event(ux->focus, e->focus, null);
        }
    }
}

none composer_release_focus(composer ux, element e) {
    if (ux->focus && ux->focus == e) {
        ux->focus->focused = false;
        ux->focus->restyle = true;
        ux_event(ux->focus, ux->focus->blur, null);
        ux->focus = null;
    }
}


none composer_press_event(composer ux, event ev) {
    release_capture(ux, ux->capture);

    element f = find_target(ux, ux->root,  ev, null);
    if     (f)  ux_event   (f,  f->press, ev);
    if (!ux->capture)
        set_capture(ux, f);

    if (ev->mouse.button == 0) {
        if (ux->action_pressed) {
            ux->action_pressed->restyle = true;
            ux->action_pressed->pressed = false;
        }
        if (f) {
            f->restyle = true;
            f->pressed = true;
        }
        ux->action_pressed  = f;
    } else if (ev->mouse.button == 1)
        ux->context_pressed = f;
}

none composer_release_event(composer ux, event ev) {
    element f = find_target(ux, ux->root,  ev, ux->capture);
    if     (f)  ux_event   (f,  f->release, ev);

    if (ev->mouse.button == 0) {
        if (ux->action_pressed) {
            ux->action_pressed->restyle = true;
            ux->action_pressed->pressed = false;
        }
        if (f) {
            f->restyle = true;
            f->pressed = false;
        }
        if (f && ux->action_pressed == f && f->hovered)
            ux_event(f, f->action, ev);
        ux->action_pressed  = null;
        ux->context_pressed = null; // clear this state on left click
    }
    else if (ev->mouse.button == 1) {
        if (f && ux->action_pressed == f && f->hovered)
            ux_event(f, f->context, ev); // context == right click (or hold on mobile <-- trinity will abstract that)
        ux->action_pressed  = null; // clear this state on right click
        ux->context_pressed = null;
    }
    if (!ux->action_pressed && !ux->context_pressed)
        release_capture(ux, ux->capture);
}


none composer_bind_subs(composer ux, element instance, element parent) {
    object target = ux->app;
    string id     = instance->id;

    if (!id) return;
    Au_t type = isa(instance);
    while (type != typeid(Au)) {
        for (int m = 0; m < type->member_count; m++) {
            type_member_t* mem = &type->members[m];
            bool is_prop = mem->member_type & A_MEMBER_PROP;
            if (!is_prop || mem->type != typeid(subs))
                continue;

            subs* field = (subs*)((cstr)instance + mem->offset);
            /// bind to elements from inner to outter, to app
            callback f;
            object   bind_target = instance;
            while (bind_target) {
                f = bind(instance, bind_target, false,
                    null, typeid(event), id->chars, mem->name);
                if (f) {
                    if (!*field) *field = hold(subs(entries, array(8)));
                    add(*field, bind_target, f);
                }
                bind_target = ((element)bind_target)->parent;
            }
            f = bind(instance, target, false,
                    null, type == typeid(element) ? typeid(event) : null, id->chars, mem->name); // we cant check the type on the sub-classes of element binds
            if (f) {
                if (!*field) *field = hold(subs(entries, array(8)));
                add(*field, target, f);
            }
        }
        type = type->parent_type;
    }
}

none composer_update(composer ux, element parent, map rendered_elements) {
    object target = ux->app; // app not defined in element, but we need only care about the A-type bind api
    
    /// mark for umount
    pairs(parent->origin, i) {
        string id = i->key;
        element e = i->value;
        e->flags |= 1;
    }

    /// iterate through rendered elements
    pairs(rendered_elements, ir) {
        string  id       = ir->key;
        element e        = ir->value;
        element instance = parent->origin ? get(parent->origin, id) : null; // needs hook for free on a very specific object
        Au_t   type     = isa(e);
        bool    restyle  = ux->restyle || e->restyle;

        if (instance) {
            instance->flags &= ~1; // instance found (pandora tomorrow...)
            if (!instance->id) {
                A info = head(id);
                instance->id = hold(id);
                instance->ux = ux;
                instance->parent = parent;
                //instance->origin = hold(instance->origin);
                // this is where we bind events between
                // these components from component, to parent, 
                // all the way to app controller
                
                A_init_recur(instance, type, null);
                A_hold_members(instance);
                bind_subs(ux, instance, parent);
                restyle = true;
            }
        }

        array changed   = null;
        bool new_inst  = false;
        if (!instance) {
            new_inst   = true;
            restyle    = true;
            instance   = hold(e);
            instance->id     = id;
            instance->ux     = ux;
            instance->parent = parent; /// weak reference
            A m_header = null;
            map m = instance->origin;
            m_header = A_header(m);
            
            A_init_recur(instance, type, null);
            A_hold_members(instance);
            bind_subs(ux, instance, parent);
            
            Au_t itype = isa(instance);

            if (!parent->origin)
                 parent->origin = hold(map(hsize, 44));
            set (parent->origin, id, instance);
        } else if (!restyle) {
            changed = apply_args(ux, instance, e);
            restyle = index_of(changed, string("tags")) >= 0; // tags effects style application
        }
        if (restyle) {
            e->restyle  = false;
            map  avail  = compute(ux->style, instance);
            array styled = apply_style(ux, instance, avail, changed);
            element e_inst = instance;
            /// merge unique props changed from style
            if (styled && changed)
                each(styled, string, prop) {
                    int i = index_of(changed, prop);
                    if (i == -1)
                        push(changed, prop);
                }
        }
        map irender = render(instance, changed);     // first render has a null changed; clear way to perform init/mount logic
        //drop(changed);
        if (irender) {
            update(ux, instance, irender);
        }
    }

    /// perform umount on elements not updated in render
    bool retry = true;
    while (retry) {
        retry = false;
        pairs(parent->origin, i) {
            string id = i->key;
            element e = i->value;
            if (e->flags & 1) {
                e->flags = 0;
                retry = true;
                A info = head(e);
                verify(info->refs == 2, "expected 2 ref count on object %s, element id: %o", info->type->name, e->id);
                e->parent = null;
                A_drop_members(e);
                rm(parent->origin, (object)id); // verify that this ALWAYS results in a ref of -1, and an A_dealloc()
                break;
            }
        }
    }
}
 
void composer_update_all(composer ux, map render) {
    ux->restyle = false;
    if (!ux->root) {
         ux->root        = hold(new(element, id, string("root")));
         A info = head(ux->style->css_path);
         ux->root_styles = hold(compute(ux->style, ux->root));
         ux->restyle = true;
    }
    if (!ux->restyle) ux->restyle = check_reload(ux->style);
    if ( ux->restyle) apply_style(ux, ux->root, ux->root_styles, null);
    
    // then only apply tag-states here
    update(ux, ux->root, render); /// 'reloaded' is checked inside the update
    //ux->style->reloaded = false;
}

none element_set_capture(element e) {
    set_capture(e->ux, e);
}

none element_release_capture(element e) {
    release_capture(e->ux, e);
}

none element_set_focus(element e) {
    set_focus(e->ux, e);
}

none element_release_focus(element e) {
    release_focus(e->ux, e);
}

none scene_init(scene a) {
    verify(a->ux, "ux not set");
    window w = a->ux->w;
    if (a->render_scale == 0.0f) a->render_scale = 1.0f;
}

// test drawing orbiter model in window pane

none scene_load(scene a, window w) {
    if (!a->target) {
        int width  = (a->bounds ? a->bounds->w : w->width)  * a->render_scale;
        int height = (a->bounds ? a->bounds->h : w->height) * a->render_scale;
        a->target = hold(target (t, w->t, w, w,
            width,          width,
            height,         height,
            reduce,         a->render_scale == 4.0 ? true : false,
            clear_color,    a->clear_color,
            models,         a->models));
        a->target->color = hold(a->target->color); // todo: fill these with a color (clear color preferred, otherwise we can transfer)
        if (a->target->reduction)
            a->target->reduction = hold(a->target->reduction);
        set(w->element_targets, a->id, a->target);
    }
}

none scene_draw(scene a, window w) {
    verify(a->update, "not set");

    scene_load(a, w);

    int width  = a->bounds->w * a->render_scale;
    int height = a->bounds->h * a->render_scale;
    a->target->width  = width; // verify placement of output as well as scaling quality
    a->target->height = height;
    a->target->wscale = 0.0f; // target needs to now support its own reduction copy; we cannot rely on skia to perform this action since it requires mipmaps

    update(a->target);

    /// it actually would be a nice thing to have subs as a primitive in silver, rather more meta declared (here first...) i_subs(X, Y, required, update)
    /// silver as an element api would work well, since A-type works quite well.  we just need to stop shifting things around, my God its good enough.
    /// now its just actual implementation of the best we can represent.  callback would be the name for a singleton version of this
    
    /// load Orbiter model
    verify(a->update, "'%o_update' sub not declared by the app", a->id);
    if (a->update)
        invoke(a->update, a); // verify this is initialized here

    array at = a->translate;
    array ar = a->rotate;
    array as = a->scale;

    if ((at || ar || as)) {

        each (a->target->models, model, m) {
            // check for invalidation here
            // no need to update if we have already processed and this is not in transition
            free(m->transforms);

            int ln0 = at ? len(at) : 0, ln1 = ar ? len(ar) : 0, ln2 = as ? len(as) : 0;
            int  ln = (ln0 > ln1 && ln0 > ln2) ? ln0 : 
                      (ln1 > ln0 && ln1 > ln2) ? ln1 : ln2;

            m->transforms      = (mat4f*)calloc(ln, sizeof(mat4f));
            m->transform_count = ln;

            each (a->target->models, model, m) {
                for (int i = 0; i < ln; i++) {
                    mat4f mat = mat4f_ident();

                    scaling s = as ? peek(as, i) : 0;
                    if (s)
                        mat = mat4f_scale(&mat, &s->value);

                    rotation r = ar ? peek(ar, i) : 0;
                    if (r) {
                        vec4f v = vec4f(
                            r->axis.x, r->axis.y, r->axis.z, radians(r->degs));
                        quatf q = quatf(&v);
                        mat     = mat4f_rotate(&mat, &q);
                    }
                    
                    translation t = at ? peek(at, i) : 0;
                    if (t)
                        mat = mat4f_translate(&mat, &t->value);

                    m->transforms[i] = mat;
                }
            }
        }
    }

    if (isa(a) == typeid(background))
        return; // already the first target in window's target list

    // draw onto canvas (somehow)
    set_texture(w->overlay, a->target->reduction ? a->target->reduction : a->target->color);
    draw_fill(w->overlay);
}

none scene_dealloc(scene a) {
    drop(a->target);
}

none sk_set_bs(texture tx);

none stage_init(stage a) {
    window  w = a->ux->w; // lets make this class messed up looking rather than window, ok?
    trinity t = w->t;

    scene_load(a, w); // the regular scene we want to load on first draw (where we have an idea of width/height), where as background should be loaded when the app loads

    if (a->frost) { // we must create the following in a 'background' object based on scene, or add a background boolean to scene
        sk_set_bs(a->target->color);

        w->m_reduce  = model (t, t, w, w, samplers, m("color", a->target->color));
        w->r_reduce  = target(t, t, w, w, wscale, 1.0f, models, a(w->m_reduce));

        w->m_reduce0 = model (t, t, w, w, samplers, m("color", w->r_reduce->color));
        w->r_reduce0 = target(t, t, w, w, wscale, 0.5f, models, a(w->m_reduce0));
        
        w->m_reduce1 = model (t, t, w, w, samplers, m("color", w->r_reduce0->color));
        w->r_reduce1 = target(t, t, w, w, wscale, 0.25f, models, a(w->m_reduce1));

        BlurV   fbv  = BlurV  (t, t, name, string("blur-v"), reduction_scale, 4.0f);
        w->m_blur_v  = model  (t, t, w, w, s, fbv, samplers, m("color", w->r_reduce1->color));
        w->r_blur_v  = target (t, t, w, w, wscale, 1.0, models, a(w->m_blur_v));
        
        Blur    fbl  = Blur   (t, t, name, string("blur"), reduction_scale, 4.0f);
        w->m_blur    = model  (t, t, w, w, s, fbl, samplers, m("color", w->r_blur_v->color));
        w->r_blur    = target (t, t, w, w, wscale, 1.0, models, a(w->m_blur));

        w->m_reduce2 = model (t, t, w, w, samplers, m("color", w->r_reduce1->color));
        w->r_reduce2 = target(t, t, w, w, wscale, 1.0f / 8.0f, models, a(w->m_reduce2));

        w->m_reduce3 = model  (t, t, w, w, samplers, m("color", w->r_reduce2->color));
        w->r_reduce3 = target (t, t, w, w, wscale, 1.0f / 16.0f, models, a(w->m_reduce3));

        BlurV   bv   = BlurV  (t, t, name, string("blur-v"), reduction_scale, 16.0f);
        w->m_frost_v = model  (t, t, w, w, s, bv, samplers, m("color", w->r_reduce3->color));
        w->r_frost_v = target (t, t, w, w, wscale, 1.0f, models, a(w->m_frost_v));

        Blur    bl   = Blur   (t, t, name, string("blur"), reduction_scale, 16.0f);
        w->m_frost   = model  (t, t, w, w, s, bl, samplers, m("color", w->r_frost_v->color));
        w->r_frost   = target (t, t, w, w, wscale, 1.0f, models, a(w->m_frost));

        w->m_view    = model  (t, t, w, w, s, UXCompose(t, t, name, string("ux")),
            samplers, m(
                "background", a->target->color,
                "frost",      w->r_frost->color,
                "blur",       w->r_blur->color,
                "compose",    w->compose->tx,
                "colorize",   w->colorize->tx,
                "overlay",    w->overlay->tx,
                "glyph",      w->glyph->tx));

        w->r_view    = target (t, t, w, w, wscale, 1.0f, clear_color, vec4f(1.0, 1.0, 1.0, 1.0),
            models, a(w->m_view));

        UXCompose  ux_shader = w->m_view->s;
        ux_shader->low_color  = vec4f(0.0, 0.1, 0.2, 1.0);
        ux_shader->high_color = vec4f(1.0, 0.8, 0.8, 1.0); // is this the high low bit?
    } else {
        w->m_view = model  (t, t, w, w, s, UXSimple(t, t, name, string("simple")),
            samplers, map_of(
                "background", a->target->color,
                "overlay",    w->overlay->tx, null));
        w->r_view = target (t, t, w, w, wscale, 1.0f, clear_color, vec4f(1.0, 1.0, 1.0, 1.0),
            models, a(w->m_view));
    }
    
    if (a->frost) {
        w->list = hold(a(
            a->target, w->r_reduce,  w->r_reduce0, w->r_reduce1, w->r_blur_v, w->r_blur,
            w->r_reduce2,    w->r_reduce3, w->r_frost_v, w->r_frost,
            w->r_view));
        w->last_target = w->r_view;
    } else {
        w->list = hold(a(
            a->target, w->r_view));
        w->last_target = w->r_view;
    }
}

none stage_dealloc(stage a) {
}


/*
define_enum(Pattern)
define_enum(Clip)

define_class(mixable,    Au) // mix
define_class(shadow,     mixable)
define_class(glow,       mixable)

define_class(background, Au) // mix, cstr 

define_class(radius,   array, f32) // array can be mixed already (add primitives)
define_class(layer,    Au)
define_class(text,     layer)
define_class(border,   layer)
define_class(fill,     layer)
define_class(children, layer)
*/










define_struct(xcoord, f32)
define_struct(ycoord, f32)

define_typed_enum(xalign, f32)
define_typed_enum(yalign, f32)

define_class(region,            Au)
define_struct(mouse_state,       i32)
define_struct(keyboard_state,    i32)
define_class(line_info,         Au)
define_class(text_sel,          Au)
define_class(text,              Au)
define_class(composer,          Au)
define_class(arg,               Au)
define_class(style,             Au)
define_class(style_block,       Au)
define_class(style_entry,       Au)
define_class(style_qualifier,   Au)
define_class(style_transition,  Au)
define_class(style_selection,   Au)

define_class(event,             Au)

define_any(element, A, sizeof(struct _element), AU_TRAIT_USER_INIT)

define_element(scene,      element)
define_element(background, scene)
define_element(button,     element)
define_element(pane,       element)


