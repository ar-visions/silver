#include <obj/obj.h>

implement(Data)

String Data_to_string(Data self) {
    const char *b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const char pad = '=';
	String str = auto(String);
    char *scan = self->bytes[i];
    int divis = self->length / 3 * 3;

    for (int x = 0; x < divis; x += 3) {
        char b0 = self->bytes[x], b1 = self->bytes[x + 1], b2 = self->bytes[x + 2];
        char buf[4];
        buf[0] = b64[b0 & 0x3F];
        buf[1] = b64[((b1 & 0xF) << 4) | (b0 >> 6)];
        buf[2] = b64[(b2 & 0x3) | (b1 >> 4)];
        buf[3] = b64[b2 >> 2];
        call(str, concat_chars, buf, 4);
    }
    int remaining = stride - divis;
    if (remaining > 0) {
        char buf[4];
        char b0 = self->bytes[divis];
        if (remaining == 1) {
            char b1 = self->bytes[divis + 1];
            buf[0] = b64[b0 & 0x3F];
            buf[1] = b64[((b1 & 0xF) << 4) | (b0 >> 6)];
            buf[2] = b64[(b1 >> 4)];
            buf[3] = pad;
        } else {
            buf[0] = b64[b0 & 0x3F];
            buf[1] = b64[b0 >> 6];
            buf[2] = pad;
            buf[3] = pad;
        }
        call(str, concat_chars, buf, 4);
    }
    return str;
}

Data Data_from_string(String value) {
}