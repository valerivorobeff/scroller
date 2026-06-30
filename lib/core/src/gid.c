#include "gid.h"

inline static uint8_t char2hex(uint8_t c);

gid_hex_t
gid2hex(Gid gid) {
    gid_hex_t hex;
    const uint8_t *c = (const uint8_t *)&gid.parts;
    int j = 0;

    for (int i = (GID_FILEIDBITS / GID_OCTETBITS) - 1; i >= 0; --i, j += 2) {
        hex.value[j] = char2hex(c[i] >> 4);
        hex.value[j + 1] = char2hex(c[i]);
    }

    hex.value[j] = '\0';

    return hex;
}

uint8_t
char2hex(uint8_t c) {
    c &= 0xf;
    return c > 9 ? 'A' + c - 10 : '0' + c;
}

