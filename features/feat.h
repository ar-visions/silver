/* features' own C header: macros and types the tests read */
#define FEAT_CAST_I ((int)3.7)
#define FEAT_BIG 40000UL
#define FEAT_GREET "hel" "lo"
#define FEAT_SUM(x, y) \
	((x) + (y))
typedef int feat_trio[3];
typedef int feat_count;
int feat_add(int a, int b);
#define feat_plus feat_add
#ifdef FEAT_OS
#define FEAT_OS_ON 1
#else
#define FEAT_OS_ON 0
#endif
#ifdef FEAT_NEVER
#define FEAT_NEVER_ON 1
#else
#define FEAT_NEVER_ON 0
#endif
#ifndef FEAT_LEVEL
#define FEAT_LEVEL 0
#endif
