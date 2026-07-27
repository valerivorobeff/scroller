#include "cell.h"

void
put_char(char *restrict dst, const char *restrict src, size_t dsize) {
    size_t  dlen = strnlen(src, dsize);
    memset(mempcpy(dst, src, dlen), ' ', dsize - dlen);
}

