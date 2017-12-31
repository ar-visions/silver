#include <obj/obj.h>

implement(Data)

String Data_to_string(Data self) {
    const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const char pad = '=';
	String str = auto(String);
    int divis = self->length / 3 * 3;

    for (int x = 0; x < divis; x += 3) {
        char b0 = self->bytes[x], b1 = self->bytes[x + 1], b2 = self->bytes[x + 2];
        char buf[4];
        buf[0] = b64[b0 >> 2];
        buf[1] = b64[((b0 & 0x03) << 4) | ((b1 & 0xF0) >> 4)];
        buf[2] = b64[((b1 & 0x0F) << 2) | ((b2 & 0x70) >> 6)];
        buf[3] = b64[b2 & 0x3F];
        call(str, concat_chars, buf, 4);
    }
    int remaining = self->length - divis;
    if (remaining > 0) {
        char buf[4];
        char b0 = self->bytes[divis];
        if (remaining == 2) {
            char b1 = self->bytes[divis + 1];
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
        call(str, concat_chars, buf, 4);
    }
    return str;
}

Data Data_from_string(String value) {
    if (!value || value->length % 4)
        return NULL;
    size_t alloc_len = (size_t)value->length / 4 * 3;
    Data self = auto(Data);
    self->bytes = alloc_bytes(alloc_len);
    int cursor = 0;
    int pfound = 0;

    const static unsigned char map[] = {
        0, 0, 0, 62, 0, 0, 0, 63, 52, 53,       // 50
        54, 55, 56, 57, 58, 59, 60, 61, 0, 0,   // 60
        0, 0, 0, 0, 0, 0, 1, 2, 3, 4,           // 70
        5, 6, 7, 8, 9, 10, 11, 12, 13, 14,      // 80
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, // 90
        25, 0, 0, 0, 0, 0, 0, 26, 27, 28,       // 100
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38, // 110
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48, // 120
        49, 50, 51, 0, 0, 0, 0, 0, 0, 0         // 130
    };

    for (int i = 0; i < value->length; i += 4) {
        char x[4];
        for (int ii = 0; ii < 4; ii++) {
            char b = value->buffer[i + ii];
            if (b < 40 || b >= 130)
                return NULL;
            char v;
            if (b == '=') {
                v = 0;
                pfound++;
            } else {
                v = map[b - 40];
                if (v == 0)
                    return NULL;
            }
            x[ii] = v;
        }
        char bytes[3];
        bytes[0] = (x[0] << 2) | (x[1] >> 4);
        bytes[1] = ((x[1] & 0xF) << 4) | (x[2] >> 2);
        bytes[2] = (x[2] << 6) | (x[3] & 0x3F);

        memcpy(&self->bytes[cursor], bytes, 3);
        cursor += 3;
    }
    int pad = 0;
    self->length = alloc_len;
    if (value->length >= 4) {
        if (value->buffer[value->length - 1] == '=') {
            pad++;
            if (value->buffer[value->length - 2] == '=')
                pad++;
        }
        self->length -= pad;
    }
    if (pfound != pad)
        return NULL;
    return self;
}

void Data_free(Data self) {
    free_ptr(self->bytes);
}