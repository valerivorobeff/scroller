#include "type.h"
#include <string.h>
#include <assert.h>
/*
struct {
    TypeGroup group;
    Type base_type;
}
*/
STypeGroup g_type_groups[TG_MAX] = {
    { TG_UNKNOWN, T_UNKNOWN },
    { TG_INTEGER, T_BIGINT },
    { TG_CHARACTER, T_VARCHAR },
};

/*
struct {
    Type type;
    TypeGroup group;
    size_t size;
    convert_fn to_base_type;
}
*/
SType g_types[T_MAX] = {
    { T_UNKNOWN, TG_UNKNOWN, 0, NULL },
    { T_SMALLINT, TG_INTEGER, sizeof(int16_t), smallint2bigint },
    { T_INTEGER, TG_INTEGER, sizeof(int32_t), integer2bigint },
    { T_BIGINT, TG_INTEGER, sizeof(int64_t), NULL },
    { T_CHAR, TG_CHARACTER, 0, char2varchar },
    { T_VARCHAR, TG_CHARACTER, 0, NULL },
};

Datum
smallint2bigint(Datum src) {
    assert(src.type == T_SMALLINT);

    src.type = T_BIGINT;
    src.size = sizeof(int64_t);
    src.value.bigint = src.value.smallint;

    return src;
}

Datum
integer2bigint(Datum src) {
    assert(src.type == T_INTEGER);

    src.type = T_BIGINT;
    src.size = sizeof(int64_t);
    src.value.bigint = src.value.integer;

    return src;
}

Datum
char2varchar(Datum src) {
    assert(src.type == T_CHAR);

    src.type = T_VARCHAR;
    while (src.size && src.value.character[src.size - 1] == ' ')
        --src.size;

    return src;
}

Datum
to_base_type(Datum src) {
    convert_fn convert = g_types[src.type].to_base_type;

    if (convert)
        src = convert(src);

    return src;
}

ssize_t
cmp_integer(Datum d1, Datum d2) {
    assert(g_types[d1.type].group == TG_INTEGER);
    assert(g_types[d2.type].group == TG_INTEGER);

    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    return d1.value.bigint - d2.value.bigint;
}

ssize_t
cmp_character(Datum d1, Datum d2) {
    ssize_t ret;

    assert(g_types[d1.type].group == TG_CHARACTER);
    assert(g_types[d2.type].group == TG_CHARACTER);

    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    ret = memcmp(d1.value.character, d2.value.character,
        d1.size < d2.size ? d1.size : d2.size);

    return ret == 0 ? (ssize_t)d1.size - (ssize_t)d2.size : ret;
}
/*
#include <stdio.h>

int main() {
    Datum d1 = make_smallint(9);
    Datum d2 = make_bigint(10);
    Datum c1 = make_char("Hello");
    Datum c2 = make_varchar("Hello");

    printf("%i\n", le_integer(d1, d2));
    printf("%i\n", eq_character(c1, c2));

    return 0;
}
*/

