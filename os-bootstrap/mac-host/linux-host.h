/* Linux-only names the kernel's host tools use, on macOS */
#ifndef LINUX_HOST_H
#define LINUX_HOST_H
#include <sys/types.h>
#include <errno.h>
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif
/* never copies: callers fall back to read and write */
static inline ssize_t copy_file_range(int fi, long long *oi, int fo,
                                      long long *oo, size_t n,
                                      unsigned int fl)
{
    (void)fi; (void)oi; (void)fo; (void)oo; (void)n; (void)fl;
    errno = ENOSYS;
    return -1;
}
#endif
