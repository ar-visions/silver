#include <obj/obj.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_PNM
#include <obj/stb_image.h>
#include <obj/stb_image_write.h>

implement(Data)

String Data_to_string(Data self) {
    const uint8 *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const uint8 pad = '=';
	String str = auto(String);
    int clength = 0;
    uint8 *cbytes = stbi_zlib_compress(self->bytes, self->length, &clength, 0);
    int divis = clength / 3 * 3;
    
    for (int x = 0; x < divis; x += 3) {
        uint8 b0 = cbytes[x], b1 = cbytes[x + 1], b2 = cbytes[x + 2];
        uint8 buf[4];
        buf[0] = b64[b0 >> 2];
        buf[1] = b64[((b0 & 0x03) << 4) | ((b1 & 0xF0) >> 4)];
        buf[2] = b64[((b1 & 0x0F) << 2) | (b2 >> 6)];
        buf[3] = b64[b2 & 0x3F];
        call(str, concat_chars, buf, 4);
    }
    int remaining = clength - divis;
    if (remaining > 0) {
        uint8 buf[4];
        uint8 b0 = cbytes[divis];
        if (remaining == 2) {
            uint8 b1 = cbytes[divis + 1];
            buf[0] = b64[b0 >> 2];
            buf[1] = b64[((b0 & 0x03) << 4) | ((b1 & 0xF0) >> 4)];
            buf[2] = b64[((b1 & 0x0F) << 2)];
            buf[3] = pad;
        } else {
            buf[0] = b64[b0 >> 2];
            buf[1] = b64[((b0 & 0x03) << 4)];
            buf[2] = pad;
            buf[3] = pad;
        }
        call(str, concat_chars, (const char *)buf, 4);
    }
    return str;
}

Data Data_from_string(String value) {
    if (!value || value->length % 4)
        return NULL;
    size_t alloc_len = (size_t)value->length / 4 * 3;
    Data self = auto(Data);
    uint8 *cbytes = alloc_bytes(alloc_len);
    int cursor = 0;
    int pfound = 0;

    const static int16 map[] = {
        -1, -1, -1, 62, -1, -1, -1, 63, 52, 53,       // 50
        54, 55, 56, 57, 58, 59, 60, 61, -1, -1,   // 60
        -1, -1, -1, -1, -1, 0, 1, 2, 3, 4,           // 70
        5, 6, 7, 8, 9, 10, 11, 12, 13, 14,      // 80
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 90
        25, -1, -1, -1, -1, -1, -1, 26, 27, 28,       // 100
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38, // 110
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48, // 120
        49, 50, 51, -1, -1, -1, -1, -1, -1, -1         // 130
    };

    for (int i = 0; i < value->length; i += 4) {
        uint8 x[4];
        for (int ii = 0; ii < 4; ii++) {
            uint8 b = value->buffer[i + ii];
            if (b < 40 || b >= 130)
                return NULL;
            int16 v;
            if (b == '=') {
                v = 0;
                pfound++;
            } else {
                v = map[b - 40];
                if (v == -1)
                    return NULL;
            }
            x[ii] = (uint8)v;
        }
        uint8 bytes[3];
        bytes[0] = (x[0] << 2) | (x[1] >> 4);
        bytes[1] = ((x[1] & 0xF) << 4) | (x[2] >> 2);
        bytes[2] = (x[2] << 6) | (x[3] & 0x3F);

        memcpy(&cbytes[cursor], bytes, 3);
        cursor += 3;
    }
    int pad = 0;
    int clength = alloc_len;
    if (value->length >= 4) {
        if (value->buffer[value->length - 1] == '=') {
            pad++;
            if (value->buffer[value->length - 2] == '=')
                pad++;
        }
        clength -= pad;
    }

    self->bytes = stbi_zlib_decode_malloc(cbytes, clength, (int *)&self->length);
    free_ptr(cbytes);

    if (!self->bytes || pfound != pad)
        return NULL;
    return self;
}

void Data_free(Data self) {
    free_ptr(self->bytes);
}