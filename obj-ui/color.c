#include <obj-ui.h>

implement(Color)

void Color_init(Color self) {
    self->a = 1.0;
}

void hextobin(const char *str, int len, uint8 *bytes, size_t blen) {
    uint8  idx0;
    uint8  idx1;

    if (len <= 0)
        len = strlen(str);

    const uint8_t hashmap[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, // 01234567
        0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 89:;<=>?
        0x00, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x00, // @ABCDEFG
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // HIJKLMNO
    };
    memset(bytes, 0, blen);
    for (uint8 pos = 0; ((pos < (blen*2)) && (pos < len)); pos += 2) {
        idx0 = ((uint8)str[pos+0] & 0x1F) ^ 0x10;
        idx1 = ((uint8)str[pos+1] & 0x1F) ^ 0x10;
        bytes[pos/2] = (uint8)(hashmap[idx0] << 4) | hashmap[idx1];
    };
}

Color Color_from_cstring(const char *str) {
    Color self = auto(Color);
    if (strncmp(str, "rgba(", 5) == 0) {
        int i = sscanf(str, "rgba(%f,%f,%f", &self->r, &self->g, &self->a);
        if (i > 0)
            sscanf(&str[i], "%f", &self->a);
        self->r /= 255.0;
        self->g /= 255.0;
        self->b /= 255.0;
    } else {
        int len = strlen(str);
        if (len <= 8) {
            uint8 bytes[8];
            if (len == 6 || len == 8) {
                hextobin(str, len, bytes, len / 2);
                self->r = (double)bytes[0] / 255.0;
                self->g = (double)bytes[1] / 255.0;
                self->b = (double)bytes[2] / 255.0;
                if (len == 8)
                    self->a = (double)bytes[3] / 255.0;
            } else if (len == 3 || len == 4) {
                char expand[9];
                expand[0] = str[0];
                expand[1] = str[0];
                expand[2] = str[1];
                expand[3] = str[1];
                expand[4] = str[2];
                expand[5] = str[2];
                if (len == 4) {
                    expand[6] = str[3];
                    expand[7] = str[3];
                    expand[8] = 0;
                } else
                    expand[6] = 0;
                hextobin((const char *)expand, len * 2, bytes, len);
            } else
                return NULL;
        } else
            return NULL;
    }
    return self;
}

String Color_to_string(Color self) {
    return class_call(String, format, "rgba(%d,%d,%d,%.2f)",
        (int)(self->r * 255.0), (int)(self->g * 255.0), (int)(self->b * 255.0), self->a);
}

Color Color_mix(Color self, Color b, double amount) {
    Color ret = auto(Color);
    ret->r = mix(self->r, b->r, amount);
    ret->g = mix(self->g, b->g, amount);
    ret->b = mix(self->b, b->b, amount);
    ret->a = mix(self->a, b->a, amount);
    return ret;
}