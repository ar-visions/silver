#include "gfx.h"
#include "util.h"

static
Surface gaussian_reduce(Gfx gfx, GfxGaussianArgs *g_args, Surface surf, float amount, float min_factor, int base, float *size) {
	float s = fmax(surf->w, surf->h);
	float r_size = fmin(s, fmax(1.0, s / (amount / (float)base)));
	*size = r_size;
	r_size = fmax(s * min_factor, r_size);
	float fw = 1.0, fh = 1.0;
	if (surf->w > surf->h) {
		fh = ((float)surf->h / (float)surf->w);
		g_args->reduction_sx = r_size;
		g_args->reduction_sy = r_size * fh;
	} else {
		fw = ((float)surf->w / (float)surf->h);
		g_args->reduction_sx = r_size * fw;
		g_args->reduction_sy = r_size;
	}
	if (r_size == s)
		return surf;
	Surface reduction = surf;
	int ss = s;
	while (ss / 2 > r_size) {
		ss /= 2;
		Surface prev = reduction;
		//gfx_surface_clamp(gfx, reduction, TRUE);
		reduction = call(reduction, resample, ss * fw, ss * fh, !reduction->image);
		if (prev != surf)
			release(gfx, prev);
	}
	Surface prev = reduction;
	reduction = call(reduction, resample, r_size * fw, r_size * fh, !reduction->image);
	if (prev != surf)
		release(prev);
	call(reduction, texture_clamp, TRUE);
	return reduction;
}

static double
gaussian_distribution(double x, double mu, double sigma) {
	double d = x - mu;
	double n = 1.0 / (sqrt(2.0 * M_PI) * sigma);
	return exp(-d*d/(2 * sigma * sigma)) * n;
};

static void
sample_interval(double sigma, double min, double max, int samples_per_bin, List samples) {
	double step_size = (max - min) / (double)(samples_per_bin - 1);

	for(int s = 0; s < samples_per_bin; ++s) {
		double x = min + s * step_size;
		double y = gaussian_distribution(x, 0, sigma);
        float2 *v2 = list_push(samples, NULL);
        v2->x = x;
        v2->y = y;
	}
};

static double
integrate_simphson(List samples) {
	Vec2 *v0 = ll_first(samples);
	Vec2 *vL = ll_last(samples);
	double result = v0->y + vL->y;//samples[0][1] + samples[samples.length-1][1];
	int len = samples->count;
	int s = 0;
	ll_each(samples, Vec2, sample) {
		if (s == 0 || s == len - 1) {
			s++;
			continue;
		}
		double sample_weight = (s % 2 == 0) ? 2.0 : 4.0;
		result += sample_weight * sample->y;
		s++;
	}
	double h = (vL->x - v0->x) / (double)(len - 1);
	return result * h / 3.0;
};

static LList *
calc_samples_for_range(int samples_per_bin, double sigma, double min, double max) {
	List samples = list_with_item(float2);
	sample_interval(sigma, min, max, samples_per_bin, samples);
	return samples;
}

static void
compute_gaussian_kernel(double sigma, int kernel_size, double *r) {
	const double sample_count = 1000.0;
	int samples_per_bin = ceil(sample_count / (double)kernel_size);
	if (samples_per_bin % 2 == 0) // need an even number of intervals for simpson integration => odd number of samples
		samples_per_bin++;
	double weight_sum = 0;
	double kernel_left = -floor((double)kernel_size / 2.0);
	int i = 0;

	for(double tap = 0; tap < kernel_size; ++tap) {
		double left = kernel_left - 0.5 + tap;

		List tap_samples = calc_samples_for_range(samples_per_bin, sigma, left, left + 1);
		double tap_weight = integrate_simphson(tap_samples);
		ll_clear(tap_samples, FALSE);
		free(tap_samples);
		r[i++] = tap_weight;
		weight_sum += tap_weight;
	}
	for (int n = 0; n < kernel_size; n++)
		r[n] /= weight_sum;
}

void Gfx_gaussian(Gfx gfx, float amount) {
	float base = 8.0;
	float min_scale = 0.0625;
	GfxGaussianArgs g_args;
	Surface dst = gfx->state.surface_dst;
	Surface src = gfx->state.surface_src;
	float size = 1.0;
	Surface reduction0 = gaussian_reduce(gfx, &g_args, src, amount, min_scale, base, &size);
	
	call(gfx, push);
	gfx->state->mat = class_call(Mat44, ident);
	VertexTexture *v = (VertexTexture *)gfx->vbuffer;

	float top = 0, left = 0, bot = 1, right = 1;
	v->pos = (Vec2) { 0, 0 };
	v->u = left;
	v->v = top;
	v++;
	v->pos = (Vec2) { src->w, 0 };
	v->u = right;
	v->v = top;
	v++;
	v->pos = (Vec2) { src->w, src->h };
	v->u = right;
	v->v = bot;
	v++;
	v->pos = (Vec2) { 0, 0 };
	v->u = left;
	v->v = top;
	v++;
	v->pos = (Vec2) { src->w, src->h };
	v->u = right;
	v->v = bot;
	v++;
	v->pos = (Vec2) { 0, src->h };
	v->u = left;
	v->v = bot;

	Surface y_pass = gfx_surface_cache_fetch(gfx, src->w, src->h, SURFACE_RGBA);
	if (!y_pass)
		y_pass = gfx_surface_create(gfx, src->w, src->h);

	glBindBuffer(GL_ARRAY_BUFFER, gfx->vbo);
	glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(VertexTexture), gfx->vbuffer, GL_STATIC_DRAW);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	call(reduction0, texture_clamp, TRUE);

	call(gfx, surface_src, reduction0, 0, 0);
	call(gfx, surface_dst, y_pass);

	// sigma
	double sigma = (0.5 + min(2.0, ((float)src->w / (float)reduction0->w))) * (reduction0->w / size);
	double weights[17];

	if (amount < base)
		sigma = max(0.0001, amount / base);

	compute_gaussian_kernel(sigma, 17, weights);

	g_args.weights[0] = weights[8];
	g_args.weights[1] = weights[7];
	g_args.weights[2] = weights[6];
	g_args.weights[3] = weights[5];
	g_args.weights[4] = weights[4];
	g_args.weights[5] = weights[3];
	g_args.weights[6] = weights[2];
	g_args.weights[7] = weights[1];
	g_args.weights[8] = weights[0];

	g_args.offsets[0] = 1.0;
	g_args.offsets[1] = 2.0;
	g_args.offsets[2] = 3.0;
	g_args.offsets[3] = 4.0;
	g_args.offsets[4] = 5.0;
	g_args.offsets[5] = 6.0;
	g_args.offsets[6] = 7.0;
	g_args.offsets[7] = 8.0;

	call(gfx, shaders_use, SHADER_GAUSSIAN_Y, &g_args, FALSE);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	GfxSurface *reduction1 = gaussian_reduce(gfx, &g_args, y_pass, amount, min_scale, base, &size);
	call(reduction1, texture_clamp, TRUE);

	call(gfx, surface_dst, dst);
	call(gfx, surface_src, reduction1, 0, 0);
	call(gfx, scale, (float)src->w / (float)reduction1->w, (float)src->h / (float)reduction1->h);
	/*if (reduction0 == src) {
		gfx_translate(gfx, 0, src->h);
		gfx_scale(gfx, 1, -1);
	}*/
	call(gfx, shaders_use, SHADER_GAUSSIAN_X, &g_args, FALSE);
	glEnable(GL_BLEND);
	glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glDrawArrays(GL_TRIANGLES, 0, 6);

	call(gfx, surface_src, NULL, 0, 0);
	call(gfx, pop);

	if (reduction0 != src)
		release(reduction0);
	else
		call(reduction0, texture_clamp, FALSE);
	
	if (reduction1 != y_pass)
		release(gfx, reduction1);
	else
		call(reduction1, texture_clamp, FALSE);
	
	release(gfx, y_pass);
}
