#include <obj/obj.h>
#include <obj/stb_zlib.h>

implement(Data)

Data Data_with_bytes(Class cl, uint8 *bytes, uint length) {
    Data self = auto(Data);
    self->bytes = bytes;
    self->length = length;
    return self;
}

Data Data_with_size(uint length) {
    Data self = auto(Data);
    self->bytes = (uint8 *)malloc(length);
    self->length = length;
    return self;
}

void Data_get_vector(Data self, void **buf, size_t type_size, uint *count) {
    *buf = (void *)self->bytes;
    *count = self->length / type_size;
}

String Data_base64_encode(Class cl, uint8 *input, int length, int primitive_size, bool compress) {
    const uint8 *b64 = (const uint8 *)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const uint8 pad = '=';
	String str = auto(String);
    int clength = 0;
    uint8 *cbytes = stbi_zlib_compress(input, length, &clength, 0);
    if (!cbytes)
        return NULL;
    int divis = clength / 3 * 3;
    
    if (compress)
        call(str, concat_char, '^');

    for (int x = 0; x < divis; x += 3) {
        uint8 b0 = cbytes[x], b1 = cbytes[x + 1], b2 = cbytes[x + 2];
        uint8 buf[4];
        buf[0] = b64[b0 >> 2];
        buf[1] = b64[((b0 & 0x03) << 4) | ((b1 & 0xF0) >> 4)];
        buf[2] = b64[((b1 & 0x0F) << 2) | (b2 >> 6)];
        buf[3] = b64[b2 & 0x3F];
        call(str, concat_chars, (const char *)buf, 4);
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

String Data_to_string(Data self) {
    return class_call(Data, base64_encode, self->bytes, self->length, 1, true);
}

uint8 *Data_base64_decode(Class cl, const char *input, int input_len,
        int *output_len, int primitive_size) {
    bool compressed = false;
    int input_len_ = input_len;
    if (input && input[0] == '^') {
        input_len--;
        input++;
        compressed = true;
    }
    if (!input || input_len % 4)
        return NULL;
    size_t alloc_len = (size_t)input_len / 4 * 3;
    uint8 *cbytes = class_alloc((class_Base)cl, alloc_len);
    int cursor = 0;
    int pfound = 0;

    const static int16 map[] = {
        -1, -1, -1, 62, -1, -1, -1, 63, 52, 53,   // 40
        54, 55, 56, 57, 58, 59, 60, 61, -1, -1,   // 50
        -1, -1, -1, -1, -1, 0, 1, 2, 3, 4,        // 60
        5, 6, 7, 8, 9, 10, 11, 12, 13, 14,        // 70
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24,   // 80
        25, -1, -1, -1, -1, -1, -1, 26, 27, 28,   // 90
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38,   // 100
        39, 40, 41, 42, 43, 44, 45, 46, 47, 48,   // 110
        49, 50, 51, -1, -1, -1, -1, -1, -1, -1    // 120
    };

    for (int i = 0; i < input_len; i += 4) {
        uint8 x[4];
        for (int ii = 0; ii < 4; ii++) {
            uint8 b = input[i + ii];
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
    if (input_len >= 4) {
        if (input[input_len - 1] == '=') {
            pad++;
            if (input[input_len - 2] == '=')
                pad++;
        }
        clength -= pad;
    }

    uint8 *output;
    if (compressed) {
        output = (uint8 *)stbi_zlib_decode_malloc((const char *)cbytes, clength, output_len);
        class_dealloc((class_Base)cl, cbytes);
    } else {
        output = (uint8 *)cbytes;
        *output_len = clength;
    }
    if (!output || pfound != pad)
        return NULL;
    return output;
}

Data Data_from_string(Class cl, String value) {
    if (!value || value->length % 4)
        return NULL;
    Data self = (Data)autorelease(new_obj((class_Base)cl, 0));
    class_Data data_class = (class_Data)cl;
    self->bytes = data_class->base64_decode(cl, value->buffer, value->length, &self->length, 1);
    return self->bytes ? self : NULL;
}

void Data_free(Data self) {
    object_dealloc(self, self->bytes);
}