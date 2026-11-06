#define _GNU_SOURCE
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

/* Android's ASharedMemory: a memfd, as Android's own is */

int ASharedMemory_create(const char *name, size_t size)
{
	int fd = memfd_create(name ? name : "ASharedMemory", MFD_CLOEXEC);
	if (fd < 0)
		return fd;
	if (ftruncate(fd, size) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

size_t ASharedMemory_getSize(int fd)
{
	struct stat st;
	if (fstat(fd, &st) < 0)
		return 0;
	return st.st_size;
}

int ASharedMemory_setProt(int fd, int prot)
{
	return 0;
}
