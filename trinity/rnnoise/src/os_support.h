/* rnnoise 0.2 includes this Opus header but never shipped it */
#ifndef OS_SUPPORT_H
#define OS_SUPPORT_H

#include <string.h>

#ifndef OPUS_INLINE
#define OPUS_INLINE inline
#endif

#define OPUS_CLEAR(dst, n) (memset((dst), 0, (n)*sizeof(*(dst))))

#endif
