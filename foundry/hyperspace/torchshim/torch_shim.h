#ifndef TORCH_SHIM_H
#define TORCH_SHIM_H
/* extern-C surface over libtorch TorchScript inference.
 * ts_forward1: single image input (1,1,S,S)         -> out floats
 * ts_forward4: 3 images (1,1,S,S) + aux (1,7)       -> out floats */
#ifdef __cplusplus
extern "C" {
#endif

void* ts_load(const char* path);
int   ts_forward1(void* model, const float* img, int S,
                  float* out, int out_cap);
int   ts_forward4(void* model, const float* l, const float* r,
                  const float* f, const float* aux, int S, int aux_n,
                  float* out, int out_cap);
int   ts_forward2i(void* model, const float* a, const float* b,
                   const float* aux, int S, int aux_n,
                   float* out, int out_cap);
int   ts_forward6i(void* model, const float* a, const float* b,
                   const float* c, const float* d, const float* e,
                   const float* f, const float* aux, int S, int aux_n,
                   float* out, int out_cap);
int   ts_read_cols(void* model, const float* img, int h, int w,
                   float* out, int out_cap);
int   ts_align(void* model, const float* img, int h, int w,
               const int* toks, int n_toks, int* out_spans);
int   ts_forward5(void* model, const float* l, const float* r,
                  const float* f, const float* w, const float* aux,
                  int S, int aux_n, float* out, int out_cap);

#ifdef __cplusplus
}
#endif
#endif
