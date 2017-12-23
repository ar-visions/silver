#include <obj-math/math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifndef M_PI
#    define M_PI 3.14159265358979323846
#endif

#define M(row, col) m[(col << 2) + row]
#define A(row, col) a[(col << 2) + row]
#define B(row, col) b[(col << 2) + row]

Matrix44 Matrix44_mul(Matrix44 self, Matrix44 mat_b) {
    Matrix44 product = auto(Matrix44);
	float *m = product->m;
	float *a = self->m;
	float *b = mat_b->m;
	for (int i = 0; i < 4; i++) {
		const float ai0=A(i,0),  ai1=A(i,1),  ai2=A(i,2),  ai3=A(i,3);
		M(i,0) = ai0 * B(0,0) + ai1 * B(1,0) + ai2 * B(2,0) + ai3 * B(3,0);
		M(i,1) = ai0 * B(0,1) + ai1 * B(1,1) + ai2 * B(2,1) + ai3 * B(3,1);
		M(i,2) = ai0 * B(0,2) + ai1 * B(1,2) + ai2 * B(2,2) + ai3 * B(3,2);
		M(i,3) = ai0 * B(0,3) + ai1 * B(1,3) + ai2 * B(2,3) + ai3 * B(3,3);
	}
    return product;
}

Matrix44 Matrix44_with_v4(Vec4 v4) {
    Matrix44 self = auto(Matrix44);
	float *m = self->m;
	for (int i = 0; i < 4; i++) {
		M(i,0) = v4->x;
		M(i,1) = v4->y;
		M(i,2) = v4->z;
		M(i,3) = v4->w;
	}
    return self;
}

Vec4 Matrix44_mul_v4(Matrix44 self, Vec4 v) {
    Vec4 product = auto(Vec4);
	float *a = self->m;
	product->x = a[0]*v->x + a[4]*v->y + a[8]*v->z + a[12]*v->w;
	product->y = a[1]*v->x + a[5]*v->y + a[9]*v->z + a[13]*v->w;
	product->z = a[2]*v->x + a[6]*v->y + a[10]*v->z + a[14]*v->w;
	product->w = a[3]*v->x + a[7]*v->y + a[11]*v->z + a[15]*v->w;
    return product;
}

Matrix44 Matrix44_translate(Matrix44 self, Vec3 v) {
    Matrix44 product = cp(self);
	float *m = product->m;
	m[12] = m[0] * v->x + m[4] * v->y + m[8] * v->z + m[12];
	m[13] = m[1] * v->x + m[5] * v->y + m[9] * v->z + m[13];
	m[14] = m[2] * v->x + m[6] * v->y + m[10] * v->z + m[14];
	m[15] = m[3] * v->x + m[7] * v->y + m[11] * v->z + m[15];
    return autorelease(product);
}

Matrix44 Matrix44_scale(Matrix44 self, Vec3 v) {
    Matrix44 product = cp(self);
	float *m = product->m;
	m[0] *= v->x;	m[4] *= v->y;	m[8]  *= v->z;
	m[1] *= v->x;	m[5] *= v->y;	m[9]  *= v->z;
	m[2] *= v->x;	m[6] *= v->y;	m[10] *= v->z;
	m[3] *= v->x;	m[7] *= v->y;	m[11] *= v->z;
    return product;
}

float Matrix44_get_xscale(Matrix44 mat) {
	float *m = mat->m;
	return sqrt(sqr(m[0]) + sqr(m[1]) + sqr(m[2]));
}

float Matrix44_get_yscale(Matrix44 mat) {
	float *m = mat->m;
	return sqrt(sqr(m[4]) + sqr(m[5]) + sqr(m[6]));
}

Matrix44 Matrix44_ident() {
    Matrix44 self = auto(Matrix44);
	float* m = self->m;
	static const float id[16] = {
		1, 0, 0, 0,
		0, 1, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	};
	memcpy(m, id, sizeof(float) * 16);
    return self;
}

Matrix44 Matrix44_ortho(float l, float r, float b, float t, float n, float f) {
    Matrix44 self = auto(Matrix44);
	float *m = self->m;

	M(0,0) = 2.f/(r-l);
	M(1,0) = M(2,0) = M(3,0) = 0.f;

	M(1,1) = 2.f/(t-b);
	M(0,1) = M(2,1) = M(3,1) = 0.f;

	M(2,2) = -2.f/(f-n);
	M(0,2) = M(1,2) = M(3,2) = 0.f;

	M(0,3) = -(r+l)/(r-l);
	M(1,3) = -(t+b)/(t-b);
	M(2,3) = -(f+n)/(f-n);
	M(3,3) = 1.f;
    return self;
}

Matrix44 Matrix44_rotate(Matrix44 self, float a, Vec3 v) {
	float xx, yy, zz, xy, yz, zx, xs, ys, zs, one_c, s, c;
	Matrix44 rotation = class_call(Matrix44, ident);
	bool opt = false;
	float x = v->x, y = v->y, z = v->z;
	
	s = sin(a);
	c = cos(a);
	float *m = rotation->m;
	if (x == 0.0f) {
		if (y == 0.0f) {
			if (z != 0.0f) {
				/* Rotate around Z Axis */
				opt = true;
				M(0,0) = c;
				M(1,1) = c;
				if (z < 0.0f) {
					M(0,1) = s;
					M(1,0) = -s;
				} else {
					M(0,1) = -s;
					M(1,0) = s;
				}
			}
			else if (z == 0.0f) {
				/* Rotate around Y Axis */
				opt = true;
				M(0,0) = c;
				M(2,2) = c;
				if (y < 0.0f) {
					M(0,2) = -s;
					M(2,0) = s;
				} else {
					M(0,2) = s;
					M(2,0) = -s;
				}
			}
		} else if (y == 0.0f) {
			if (z == 0.0f) {
				/* Rotate around X Axis */
				opt = true;
				M(1,1) = c;
				M(2,2) = c;
				if (x < 0.0f) {
					M(1,2) = s;
					M(2,1) = -s;
				} else {
					M(1,2) = -s;
					M(2,1) = s;
				}
			}
		}
	}
	
	if (!opt) {
		const float mag = sqrtf(x * x + y * y + z * z);
	
		if (mag <= 1.0e-4)
			return autorelease(cp(self));
	
		x /= mag;
		y /= mag;
		z /= mag;
		
		xx = x * x;
		yy = y * y;
		zz = z * z;
		xy = x * y;
		yz = y * z;
		zx = z * x;
		xs = x * s;
		ys = y * s;
		zs = z * s;
		one_c = 1.0F - c;
		
		/* We already hold the identity-matrix so we can skip some statements */
		M(0,0) = (one_c * xx) + c;
		M(0,1) = (one_c * xy) - zs;
		M(0,2) = (one_c * zx) + ys;
		/*    M(0,3) = 0.0F; */
		
		M(1,0) = (one_c * xy) + zs;
		M(1,1) = (one_c * yy) + c;
		M(1,2) = (one_c * yz) - xs;
		/*    M(1,3) = 0.0F; */
		
		M(2,0) = (one_c * zx) - ys;
		M(2,1) = (one_c * yz) + xs;
		M(2,2) = (one_c * zz) + c;
		/*    M(2,3) = 0.0F; */
	}
	return call(self, mul, rotation);
}

#define SWAP_ROWS(a, b) { float *_tmp = a; (a)=(b); (b)=_tmp; }
#define MAT(m,r,c) (m)[(c)*4+(r)]

Matrix44 Matrix44_invert(Matrix44 self) {
    Matrix44 product = auto(Matrix44);
	float *out = product->m;
	float *m = self->m;
	float wtmp[4][8];
	float m0, m1, m2, m3, s;
	float *r0, *r1, *r2, *r3;
	r0 = wtmp[0], r1 = wtmp[1], r2 = wtmp[2], r3 = wtmp[3];
	r0[0] = MAT(m, 0, 0), r0[1] = MAT(m, 0, 1),
	r0[2] = MAT(m, 0, 2), r0[3] = MAT(m, 0, 3),
	r0[4] = 1.0, r0[5] = r0[6] = r0[7] = 0.0,
	r1[0] = MAT(m, 1, 0), r1[1] = MAT(m, 1, 1),
	r1[2] = MAT(m, 1, 2), r1[3] = MAT(m, 1, 3),
	r1[5] = 1.0, r1[4] = r1[6] = r1[7] = 0.0,
	r2[0] = MAT(m, 2, 0), r2[1] = MAT(m, 2, 1),
	r2[2] = MAT(m, 2, 2), r2[3] = MAT(m, 2, 3),
	r2[6] = 1.0, r2[4] = r2[5] = r2[7] = 0.0,
	r3[0] = MAT(m, 3, 0), r3[1] = MAT(m, 3, 1),
	r3[2] = MAT(m, 3, 2), r3[3] = MAT(m, 3, 3),
	r3[7] = 1.0, r3[4] = r3[5] = r3[6] = 0.0;

	/* choose pivot - or die */
	if (fabsf(r3[0]) > fabsf(r2[0]))
		SWAP_ROWS(r3, r2);
	if (fabsf(r2[0]) > fabsf(r1[0]))
		SWAP_ROWS(r2, r1);
	if (fabsf(r1[0]) > fabsf(r0[0]))
		SWAP_ROWS(r1, r0);
	if (0.0 == r0[0])
		return NULL;
   /* eliminate first variable     */
	m1 = r1[0] / r0[0];
	m2 = r2[0] / r0[0];
	m3 = r3[0] / r0[0];
	s = r0[1];
	r1[1] -= m1 * s;
	r2[1] -= m2 * s;
	r3[1] -= m3 * s;
	s = r0[2];
	r1[2] -= m1 * s;
	r2[2] -= m2 * s;
	r3[2] -= m3 * s;
	s = r0[3];
	r1[3] -= m1 * s;
	r2[3] -= m2 * s;
	r3[3] -= m3 * s;
	s = r0[4];
	if (s != 0.0) {
		r1[4] -= m1 * s;
		r2[4] -= m2 * s;
		r3[4] -= m3 * s;
	}
	s = r0[5];
	if (s != 0.0) {
		r1[5] -= m1 * s;
		r2[5] -= m2 * s;
		r3[5] -= m3 * s;
	}
	s = r0[6];
	if (s != 0.0) {
		r1[6] -= m1 * s;
		r2[6] -= m2 * s;
		r3[6] -= m3 * s;
	}
	s = r0[7];
	if (s != 0.0) {
		r1[7] -= m1 * s;
		r2[7] -= m2 * s;
		r3[7] -= m3 * s;
	}
	/* choose pivot - or die */
	if (fabsf(r3[1]) > fabsf(r2[1]))
		SWAP_ROWS(r3, r2);
	if (fabsf(r2[1]) > fabsf(r1[1]))
		SWAP_ROWS(r2, r1);
	if (0.0 == r1[1])
		return NULL;
	/* eliminate second variable */
	m2 = r2[1] / r1[1];
	m3 = r3[1] / r1[1];
	r2[2] -= m2 * r1[2];
	r3[2] -= m3 * r1[2];
	r2[3] -= m2 * r1[3];
	r3[3] -= m3 * r1[3];
	s = r1[4];
	if (0.0 != s) {
		r2[4] -= m2 * s;
		r3[4] -= m3 * s;
	}
	s = r1[5];
	if (0.0 != s) {
		r2[5] -= m2 * s;
		r3[5] -= m3 * s;
	}
	s = r1[6];
	if (0.0 != s) {
		r2[6] -= m2 * s;
		r3[6] -= m3 * s;
	}
	s = r1[7];
	if (0.0 != s) {
		r2[7] -= m2 * s;
		r3[7] -= m3 * s;
	}
	/* choose pivot - or die */
	if (fabsf(r3[2]) > fabsf(r2[2]))
		SWAP_ROWS(r3, r2);
	if (0.0 == r2[2])
		return NULL;
	/* eliminate third variable */
	m3 = r3[2] / r2[2];
	r3[3] -= m3 * r2[3], r3[4] -= m3 * r2[4],
		r3[5] -= m3 * r2[5], r3[6] -= m3 * r2[6], r3[7] -= m3 * r2[7];
	/* last check */
	if (0.0 == r3[3])
		return NULL;
	s = 1.0 / r3[3];		/* now back substitute row 3 */
	r3[4] *= s;
	r3[5] *= s;
	r3[6] *= s;
	r3[7] *= s;
	m2 = r2[3];			/* now back substitute row 2 */
	s = 1.0 / r2[2];
	r2[4] = s * (r2[4] - r3[4] * m2), r2[5] = s * (r2[5] - r3[5] * m2),
		r2[6] = s * (r2[6] - r3[6] * m2), r2[7] = s * (r2[7] - r3[7] * m2);
	m1 = r1[3];
	r1[4] -= r3[4] * m1, r1[5] -= r3[5] * m1,
		r1[6] -= r3[6] * m1, r1[7] -= r3[7] * m1;
	m0 = r0[3];
	r0[4] -= r3[4] * m0, r0[5] -= r3[5] * m0,
		r0[6] -= r3[6] * m0, r0[7] -= r3[7] * m0;
	m1 = r1[2];			/* now back substitute row 1 */
	s = 1.0 / r1[1];
	r1[4] = s * (r1[4] - r2[4] * m1), r1[5] = s * (r1[5] - r2[5] * m1),
		r1[6] = s * (r1[6] - r2[6] * m1), r1[7] = s * (r1[7] - r2[7] * m1);
	m0 = r0[2];
	r0[4] -= r2[4] * m0, r0[5] -= r2[5] * m0,
		r0[6] -= r2[6] * m0, r0[7] -= r2[7] * m0;
	m0 = r0[1];			/* now back substitute row 0 */
	s = 1.0 / r0[0];
	r0[4] = s * (r0[4] - r1[4] * m0), r0[5] = s * (r0[5] - r1[5] * m0),
		r0[6] = s * (r0[6] - r1[6] * m0), r0[7] = s * (r0[7] - r1[7] * m0);
	MAT(out, 0, 0) = r0[4];
	MAT(out, 0, 1) = r0[5], MAT(out, 0, 2) = r0[6];
	MAT(out, 0, 3) = r0[7], MAT(out, 1, 0) = r1[4];
	MAT(out, 1, 1) = r1[5], MAT(out, 1, 2) = r1[6];
	MAT(out, 1, 3) = r1[7], MAT(out, 2, 0) = r2[4];
	MAT(out, 2, 1) = r2[5], MAT(out, 2, 2) = r2[6];
	MAT(out, 2, 3) = r2[7], MAT(out, 3, 0) = r3[4];
	MAT(out, 3, 1) = r3[5], MAT(out, 3, 2) = r3[6];
	MAT(out, 3, 3) = r3[7];
	return product;
}

Vec3 Matrix44_unproject(Matrix44 self, Vec3 window, Vec4 viewport) {
    Matrix44 inv = call(self, invert);
    if (!inv)
        return NULL;
    
    Vec4 in = vec4(
        (window->x-(float)viewport->x)/(float)viewport->z*2.0-1.0,
        (window->y-(float)viewport->y)/(float)viewport->w*2.0-1.0,
        2.0*window->z-1.0,
        1.0);

	Vec4 out = call(inv, mul_v4, in);
	if (!out || out->w == 0.0)
		return NULL;
	
	out->w = 1.0 / out->w;
	return vec3(out->x * out->w, out->y * out->w, out->z * out->w);
}

Vec3 Matrix44_project(Vec3 v, Matrix44 mv, Matrix44 proj, Vec4 viewport) {
    float objx = v->x, objy = v->y, objz = v->z;
	//Transformation vectors
	float fTempo[8];

    float *modelview = mv->m;
    float *projection = proj->m;
	//Modelview transform
	fTempo[0]=modelview[0]*objx+modelview[4]*objy+modelview[8]*objz+modelview[12];  //w is always 1
	fTempo[1]=modelview[1]*objx+modelview[5]*objy+modelview[9]*objz+modelview[13];
	fTempo[2]=modelview[2]*objx+modelview[6]*objy+modelview[10]*objz+modelview[14];
	fTempo[3]=modelview[3]*objx+modelview[7]*objy+modelview[11]*objz+modelview[15];
	//Projection transform, the final row of projection matrix is always [0 0 -1 0]
	//so we optimize for that.
	fTempo[4]=projection[0]*fTempo[0]+projection[4]*fTempo[1]+projection[8]*fTempo[2]+projection[12]*fTempo[3];
	fTempo[5]=projection[1]*fTempo[0]+projection[5]*fTempo[1]+projection[9]*fTempo[2]+projection[13]*fTempo[3];
	fTempo[6]=projection[2]*fTempo[0]+projection[6]*fTempo[1]+projection[10]*fTempo[2]+projection[14]*fTempo[3];
	fTempo[7]=-fTempo[2];
	//The result normalizes between -1 and 1
	if(fTempo[7]==0.0)	//The w value
		return NULL;
	fTempo[7]=1.0/fTempo[7];
	//Perspective division
	fTempo[4]*=fTempo[7];
	fTempo[5]*=fTempo[7];
	fTempo[6]*=fTempo[7];
	//Window coordinates
	//Map x, y to range 0-1
    return vec3(
        (fTempo[4]*0.5+0.5)*viewport->z+viewport->x,
        (fTempo[5]*0.5+0.5)*viewport->w+viewport->y,
        (1.0+fTempo[6])*0.5
    );
}
