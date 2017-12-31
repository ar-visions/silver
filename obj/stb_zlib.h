#ifndef _STB_ZLIB_
#define _STB_ZLIB_

#include <obj/obj.h>
#include <assert.h>

#define STBI_UCHAR(x) (unsigned char) ((x) & 0xff)

#define STBI_MALLOC(sz)        malloc(sz)
#define STBI_REALLOC(p,newsz)  realloc(p,newsz)
#define STBI_FREE(p)           free(p)
#define STBI_REALLOC_SIZED(p,oldsz,newsz) STBI_REALLOC(p,newsz)
#define STBI_MEMMOVE(a,b,sz) memmove(a,b,sz)
#define STBI_ASSERT(x) assert(x)

#ifdef _MSC_VER
#define STBI_NOTUSED(v)  (void)(v)
#else
#define STBI_NOTUSED(v)  (void)sizeof(v)
#endif

#ifdef _MSC_VER
#define STBI_HAS_LROTL
#endif

#ifdef STBI_HAS_LROTL
   #define stbi_lrot(x,y)  _lrotl(x,y)
#else
   #define stbi_lrot(x,y)  (((x) << (y)) | ((x) >> (32 - (y))))
#endif

// stretchy buffer; stbiw__sbpush() == vector<>::push_back() -- stbiw__sbcount() == vector<>::size()
#define stbiw__sbraw(a) ((int *) (a) - 2)
#define stbiw__sbm(a)   stbiw__sbraw(a)[0]
#define stbiw__sbn(a)   stbiw__sbraw(a)[1]

#define stbiw__sbneedgrow(a,n)  ((a)==0 || stbiw__sbn(a)+n >= stbiw__sbm(a))
#define stbiw__sbmaybegrow(a,n) (stbiw__sbneedgrow(a,(n)) ? stbiw__sbgrow(a,n) : 0)
#define stbiw__sbgrow(a,n)  stbiw__sbgrowf((void **) &(a), (n), sizeof(*(a)))

#define stbiw__sbpush(a, v)      (stbiw__sbmaybegrow(a,1), (a)[stbiw__sbn(a)++] = (v))
#define stbiw__sbcount(a)        ((a) ? stbiw__sbn(a) : 0)
#define stbiw__sbfree(a)         ((a) ? STBI_FREE(stbiw__sbraw(a)),0 : 0)

#define stbiw__zlib_flush() (out = stbiw__zlib_flushf(out, &bitbuf, &bitcount))
#define stbiw__zlib_add(code,codebits) \
      (bitbuf |= (code) << bitcount, bitcount += (codebits), stbiw__zlib_flush())
#define stbiw__zlib_huffa(b,c)  stbiw__zlib_add(stbiw__zlib_bitrev(b,c),c)
// default huffman tables
#define stbiw__zlib_huff1(n)  stbiw__zlib_huffa(0x30 + (n), 8)
#define stbiw__zlib_huff2(n)  stbiw__zlib_huffa(0x190 + (n)-144, 9)
#define stbiw__zlib_huff3(n)  stbiw__zlib_huffa(0 + (n)-256,7)
#define stbiw__zlib_huff4(n)  stbiw__zlib_huffa(0xc0 + (n)-280,8)
#define stbiw__zlib_huff(n)  ((n) <= 143 ? stbiw__zlib_huff1(n) : (n) <= 255 ? stbiw__zlib_huff2(n) : (n) <= 279 ? stbiw__zlib_huff3(n) : stbiw__zlib_huff4(n))
#define stbiw__zlib_huffb(n) ((n) <= 143 ? stbiw__zlib_huff1(n) : stbiw__zlib_huff2(n))

#define stbiw__ZHASH   16384

extern char *stbi_zlib_decode_malloc_guesssize(const char *buffer, int len, int initial_size, int *outlen);
extern char *stbi_zlib_decode_malloc(char const *buffer, int len, int *outlen);
extern char *stbi_zlib_decode_malloc_guesssize_headerflag(const char *buffer, int len, int initial_size, int *outlen, int parse_header);
extern int stbi_zlib_decode_buffer(char *obuffer, int olen, char const *ibuffer, int ilen);
extern char *stbi_zlib_decode_noheader_malloc(char const *buffer, int len, int *outlen);
extern int stbi_zlib_decode_noheader_buffer(char *obuffer, int olen, const char *ibuffer, int ilen);
extern uint8* stbi_zlib_compress(uint8 *data, int data_len, int *out_len, int quality);



// this is not threadsafe
static const char *stbi__g_failure_reason;

extern const char *stbi_failure_reason(void);
extern int stbi__err(const char *str);

static void *stbiw__sbgrowf(void **arr, int increment, int itemsize)
{
   int m = *arr ? 2*stbiw__sbm(*arr)+increment : increment+1;
   void *p = STBI_REALLOC_SIZED(*arr ? stbiw__sbraw(*arr) : 0, *arr ? (stbiw__sbm(*arr)*itemsize + sizeof(int)*2) : 0, itemsize * m + sizeof(int)*2);
   STBI_ASSERT(p);
   if (p) {
      if (!*arr) ((int *) p)[1] = 0;
      *arr = (void *) ((int *) p + 2);
      stbiw__sbm(*arr) = m;
   }
   return *arr;
}

inline static uint8 *stbiw__zlib_flushf(uint8 *data, unsigned int *bitbuffer, int *bitcount)
{
   while (*bitcount >= 8) {
      stbiw__sbpush(data, STBI_UCHAR(*bitbuffer));
      *bitbuffer >>= 8;
      *bitcount -= 8;
   }
   return data;
}

inline static int stbiw__zlib_bitrev(int code, int codebits)
{
   int res=0;
   while (codebits--) {
      res = (res << 1) | (code & 1);
      code >>= 1;
   }
   return res;
}

inline static unsigned int stbiw__zlib_countm(uint8 *a, uint8 *b, int limit)
{
   int i;
   for (i=0; i < limit && i < 258; ++i)
      if (a[i] != b[i]) break;
   return i;
}

inline static unsigned int stbiw__zhash(uint8 *data)
{
    uint32 hash = data[0] + (data[1] << 8) + (data[2] << 16);
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;
    return hash;
}

#endif
