#ifndef AI_H
#define AI_H
#include <stdint.h>

// a pytorch checkpoint (.pth / .pt / .bin, the zip format): every
// tensor it holds, named by its path through the nested dicts
typedef struct PthFile PthFile;
PthFile*    pthf_open  (const char* path);
void        pthf_close (PthFile* p);
int         pthf_count (PthFile* p);
const char* pthf_name  (PthFile* p, int i);
int         pthf_find  (PthFile* p, const char* name);
// 0 f32, 1 f16, 2 bf16, 3 i64, 4 i32, 5 u8/bool, 6 f64, -1 unknown
int         pthf_dtype (PthFile* p, int i);
int         pthf_ndim  (PthFile* p, int i);
int64_t     pthf_dim   (PthFile* p, int i, int k);
int64_t     pthf_numel (PthFile* p, int i);
// the values as f32 into out (numel of them); 0 when it cannot
int         pthf_read_f32 (PthFile* p, int i, float* out);
// c[m, n] = a[m, k] . b[n, k] + bias[n] (bias may be null), on
// every core with avx2 and fma
void ai_gemm_nt(const float* a, const float* b, const float* bias, float* c, int m, int n, int k);
#endif
