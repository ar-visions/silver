#include <obj-math/math.h>
#include <stdio.h>

#define SGN(x)   	((x) >= 0 ? 1 : -1)
#define MAXDEPTH 	64	
#define	EPSILON		(ldexp(1.0,-MAXDEPTH-1)) 
#define	DEGREE		3
#define	W_DEGREE 	5

implement(Bezier)

Bezier Bezier_new_bezier(float2 p1, float2 h1, float2 h2, float2 p2) {
    Bezier self = new(Bezier);
    self->p1 = p1;
    self->h1 = h1;
    self->h2 = h2;
    self->p2 = p2;
    return self;
}

String Bezier_to_string(Bezier self) {
    return class_call(String, format, "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f",
        self->p1.x, self->p1.y,
        self->h1.x, self->h1.y,
        self->h2.x, self->h2.y,
        self->p2.x, self->p2.y);
}

float2 Bezier_degree(float2 *V, int degree, float t, float2 *Left, float2 *Right) {
    int 	i, j;
    float2 	Vtemp[W_DEGREE+1][W_DEGREE+1];

    for (j =0; j <= degree; j++) {
        Vtemp[0][j] = V[j];
    }
    for (i = 1; i <= degree; i++) {	
        for (j =0 ; j <= degree - i; j++) {
            Vtemp[i][j].x = (1.0 - t) * Vtemp[i-1][j].x + t * Vtemp[i-1][j+1].x;
            Vtemp[i][j].y = (1.0 - t) * Vtemp[i-1][j].y + t * Vtemp[i-1][j+1].y;
			//Vtemp[i][j].z = (1.0 - t) * Vtemp[i-1][j].z + t * Vtemp[i-1][j+1].z;
        }
    }
    if (Left != NULL) {
        for (j = 0; j <= degree; j++) {
            Left[j]  = Vtemp[j][0];
        }
    }
    if (Right != NULL) {
        for (j = 0; j <= degree; j++) {
            Right[j] = Vtemp[degree-j][j];
        }
    }
    return Vtemp[degree][0];
}

int Bezier_crossing_count(float2 *V, int degree) {
    int 	i;	
    int 	n_crossings = 0;
    int		sign, old_sign;

    sign = old_sign = SGN(V[0].y);
    for (i = 1; i <= degree; i++) {
        sign = SGN(V[i].y);
        if (sign != old_sign) n_crossings++;
        old_sign = sign;
    }
    return n_crossings;
}

int Bezier_flat_enough(float2 *V, int degree) {
    int     i;
    float  value;
    float  max_distance_above = 0.0;
    float  max_distance_below = 0.0;
    float  error; 
    float  intercept_1,
            intercept_2,
            left_intercept,
            right_intercept;
    float  a, b, c;
    float  det, dInv;
    float  a1, b1, c1, a2, b2, c2;

    a = V[0].y - V[degree].y;
    b = V[degree].x - V[0].x;
    c = V[0].x * V[degree].y - V[degree].x * V[0].y;

    for (i = 1; i < degree; i++) {
        value = a * V[i].x + b * V[i].y + c;
       
        if (value > max_distance_above)
            max_distance_above = value;
        else if (value < max_distance_below)
            max_distance_below = value;
    }

    a1 = 0.0;
    b1 = 1.0;
    c1 = 0.0;

    a2 = a;
    b2 = b;
    c2 = c - max_distance_above;

    det = a1 * b2 - a2 * b1;
    dInv = 1.0/det;

    intercept_1 = (b1 * c2 - b2 * c1) * dInv;

    a2 = a;
    b2 = b;
    c2 = c - max_distance_below;
    det = a1 * b2 - a2 * b1;
    dInv = 1.0/det;

    intercept_2 = (b1 * c2 - b2 * c1) * dInv;

    left_intercept = min(intercept_1, intercept_2);
    right_intercept = max(intercept_1, intercept_2);

    error = right_intercept - left_intercept;
    return (error < EPSILON)? 1 : 0;
}

float Bezier_x_intercept(float2 *V, int degree) {
    float	XLK, YLK, XNM, YNM, XMK, YMK;
    float	det, detInv;
    float	S;
    float	X;

    XLK = 1.0 - 0.0;
    YLK = 0.0 - 0.0;
    XNM = V[degree].x - V[0].x;
    YNM = V[degree].y - V[0].y;
    XMK = V[0].x - 0.0;
    YMK = V[0].y - 0.0;

    det = XNM*YLK - YNM*XLK;
    detInv = 1.0/det;

    S = (XNM*YMK - YNM*XMK) * detInv;
/*  T = (XLK*YMK - YLK*XMK) * detInv; */

    X = 0.0 + XLK * S;
/*  Y = 0.0 + YLK * S; */
    return X;
}

int Bezier_find_roots(float2 *w, int degree, float *t, int depth) {
    int 	i;
    float2 	Left[W_DEGREE+1],
    	  	Right[W_DEGREE+1];
    int 	left_count,right_count;
    float 	left_t[W_DEGREE+1],
	   		right_t[W_DEGREE+1];
    int     cc = class_call(Bezier, crossing_count, w, degree);

    switch (cc) {
       	case 0:
            return 0;	
        case 1:
            if (depth >= MAXDEPTH) {
                    t[0] = (w[0].x + w[W_DEGREE].x) / 2.0;
                    return 1;
            }
            if (class_call(Bezier, flat_enough, w, degree)) {
                    t[0] = class_call(Bezier, x_intercept, w, degree);
                    return 1;
            }
            break;
    }

    class_call(Bezier, degree, w, degree, 0.5, Left, Right);
    left_count  = class_call(Bezier, find_roots, Left,  degree, left_t, depth+1);
    right_count = class_call(Bezier, find_roots, Right, degree, right_t, depth+1);

    for (i = 0; i < left_count; i++) {
        t[i] = left_t[i];
    }
    for (i = 0; i < right_count; i++) {
        t[i+left_count] = right_t[i];
    }

    return left_count + right_count;
}

void Bezier_bezier_form(float2 P, float2 *V, float2 *w) {
    int 	i, j, k, m, n, ub, lb;	
    int 	row, column;
    float2 	c[DEGREE+1];
    float2 	d[DEGREE];
    float 	cdTable[3][4];
    static const float z[3][4] = {
        {1.0, 0.6, 0.3, 0.1},
        {0.4, 0.6, 0.6, 0.4},
        {0.1, 0.3, 0.6, 1.0},
    };

    for (i = 0; i <= DEGREE; i++)
        c[i] = float2_sub(V[i], P);

    for (i = 0; i <= DEGREE - 1; i++) 
        d[i] = float2_scale(float2_sub(V[i+1], V[i]), 3.0);

    for (row = 0; row <= DEGREE - 1; row++) {
        for (column = 0; column <= DEGREE; column++) {
            cdTable[row][column] = float2_dot(d[row], c[column]);
        }
    }

    for (i = 0; i <= W_DEGREE; i++) {
        w[i].y = 0.0;
        w[i].x = (float)(i) / W_DEGREE;
    }

    n = DEGREE;
    m = DEGREE-1;
    for (k = 0; k <= n + m; k++) {
        lb = max(0, k - m);
        ub = min(k, n);
        for (i = lb; i <= ub; i++) {
            j = k - i;
            w[i+j].y += cdTable[j][i] * z[j][i];
        }
    }
}

float2 Bezier_nearest_point(Bezier self, float2 P) {
    float2 V[4] = { self->p1, self->h1, self->h2, self->p2 };
    float 	t_candidate[W_DEGREE];  
    int 	n_solutions;
    float	t;
    float 	dist, new_dist;
    float2 	p;
    int		i;
    float2	w[W_DEGREE+1];
    
    class_call(Bezier, bezier_form, P, V, w);

    n_solutions = class_call(Bezier, find_roots, w, W_DEGREE, t_candidate, 0);
    dist = float2_len_sqr(float2_sub(P, V[0]));
    t = 0.0;

    for (i = 0; i < n_solutions; i++) {
        p = class_call(Bezier, degree, V, DEGREE, t_candidate[i], (float2 *)NULL, (float2 *)NULL);
        new_dist = float2_len_sqr(float2_sub(P, p));
        if (new_dist < dist) {
            dist = new_dist;
            t = t_candidate[i];
        }
    }

    new_dist = float2_len_sqr(float2_sub(P, V[DEGREE]));
    if (new_dist < dist) {
        dist = new_dist;
        t = 1.0;
    }

    return class_call(Bezier, degree, V, DEGREE, t, (float2 *)NULL, (float2 *)NULL);
}

float2 Bezier_point_at(Bezier self, float p) {
    p = 1.0 - p;
	float2 point;
    point.x = self->p1.x*bezier_B1(p) + self->h1.x*bezier_B2(p) + self->h2.x*bezier_B3(p) + self->p2.x*bezier_B4(p);
    point.y = self->p1.y*bezier_B1(p) + self->h1.y*bezier_B2(p) + self->h2.y*bezier_B3(p) + self->p2.y*bezier_B4(p);
	//point.z = self->p1.z*bezier_B1(p) + self->h1.z*bezier_B2(p) + self->h2.z*bezier_B3(p) + self->p2.z*bezier_B4(p);
	return point;
}

float Bezier_percent_from_point(Bezier self, float2 point) {
    const unsigned int steps = 20;
    const unsigned int depth = 20;
    float per = 0.0;
    float per_before = 0.0;
    float per_after = 0.0;
    float dist_sm_sqr = float2_dist_sqr(point,  self->p1);
    float dist_sm_sqr_before = -1;
    float dist_sm_sqr_after = -1;
    float dist_sm_last = dist_sm_sqr;
    bool set_after = false;
    float2 p;

    /* Quick 20 step pass over bezier to find percentages between */
    for (int i = 1; i <= steps; i++) {
        float f = (float)i / (float)steps;
        p = call(self, point_at, f);
        float dist_sqr = float2_dist_sqr(point, p);
        
        if (dist_sm_sqr > dist_sqr) {
            dist_sm_sqr = dist_sqr;
            dist_sm_sqr_before = dist_sm_last;
            per = f;
            per_before = (float)(i-1) / (float)steps;
            per_after = (float)(i+1) / (float)steps;
            set_after = true;
        } else if (set_after) {
            dist_sm_sqr_after = dist_sqr;
        }
        dist_sm_last = dist_sqr;
    }

    /* 20 steps of sub divisions to narrow down the mid-point to a high degree of precision */
    if (dist_sm_sqr_before >= 0 && dist_sm_sqr_after >= 0) {
        for (int i = 0; i < depth; i++) {
            if (dist_sm_sqr_before < dist_sm_sqr_after) {
                per_after = per_before + (per_after - per_before) / 2.0;
                p = call(self, point_at, per_after);
                dist_sm_sqr_after = float2_dist_sqr(point, p);
                per = per_before + (per_after - per_before) / 2.0;
            } else if (dist_sm_sqr_before > dist_sm_sqr_after) {
                per_before = per_after - (per_after - per_before) / 2.0;
                p = call(self, point_at, per_before);
                dist_sm_sqr_before = float2_dist_sqr(point, p);
                per = per_before + (per_after - per_before) / 2.0;
            } else {
                break;
            }
        }
    }

    return per;
}

void Bezier_split(Bezier self, Bezier *left, Bezier *right, float f) {
    float2 p4 = float2_interpolate(self->p1, self->h1, f);
    float2 p5 = float2_interpolate(self->h1, self->h2, f);
    float2 p6 = float2_interpolate(self->h2, self->p2, f);
    float2 p7 = float2_interpolate(p4, p5, f);
    float2 p8 = float2_interpolate(p5, p6, f);
    float2 p9 = float2_interpolate(p7, p8, f);

    if (left)   *left = bezier(self->p1,p4,p7,p9);
    if (right) *right = bezier(p9,p8,p6,self->p2);
}

float Bezier_approx_length(Bezier self, int steps) {
    float2 last = self->p1;
    float len = 0;
    for (int i = 1; i <= steps; i++) {
        float f = (float)i / (float)steps;
        float2 p = call(self, point_at, f);
        len += float2_dist(last, p);
        last = p;
    }
    return len;
}

void Bezier_closest_point(Bezier self, float2 point, float2 *closest, float *dist) {   
    float2  p = call(self, nearest_point, point);
    if (dist) {
        *dist = float2_dist(point, p);
    }
    *closest = p;
}