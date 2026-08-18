#ifndef MPG123MIN_H
#define MPG123MIN_H
/* minimal libmpg123 surface: system ships .so.0 but no dev header */
#include <stddef.h>

#define MPG123_OK            0
#define MPG123_DONE        -12
#define MPG123_ENC_SIGNED_16 0xD0

typedef struct mpg123_handle_struct mpg123_handle;

int            mpg123_init(void);
mpg123_handle* mpg123_new(const char* decoder, int* error);
int            mpg123_open(mpg123_handle* mh, const char* path);
int            mpg123_scan(mpg123_handle* mh);
int            mpg123_getformat(mpg123_handle* mh, long* rate, int* channels, int* encoding);
int            mpg123_format_none(mpg123_handle* mh);
int            mpg123_format(mpg123_handle* mh, long rate, int channels, int encodings);
long           mpg123_length(mpg123_handle* mh);
int            mpg123_read(mpg123_handle* mh, void* outmemory, size_t outmemsize, size_t* done);
int            mpg123_close(mpg123_handle* mh);
void           mpg123_delete(mpg123_handle* mh);
#endif
