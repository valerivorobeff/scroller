/**
 * @file type.c
 * @brief Type system implementation
 */

#include "type.h"
#include <string.h>
#include <assert.h>

/**
 * @cond INTERNAL
 * Type group descriptors
 * @endcond
 */
const STypeGroup g_type_groups[TG_MAX] = {
    { TG_UNKNOWN, T_UNKNOWN },
    { TG_INTEGER, T_BIGINT },
    { TG_CHARACTER, T_VARCHAR },
};

/**
 * @cond INTERNAL
 * Type descriptors with conversion functions
 * @endcond
 */
const SType g_types[T_MAX] = {
    { T_UNKNOWN, TG_UNKNOWN, SM_TYPESZ, 0, NULL },
    { T_SMALLINT, TG_INTEGER, SM_TYPESZ, sizeof(int16_t), smallint2bigint },
    { T_INTEGER, TG_INTEGER, SM_TYPESZ, sizeof(int32_t), integer2bigint },
    { T_BIGINT, TG_INTEGER, SM_TYPESZ, sizeof(int64_t), NULL },
    { T_CHAR, TG_CHARACTER, SM_COLUMNSZ, 0, char2varchar },
    { T_VARCHAR, TG_CHARACTER, SM_TYPESZ, sizeof(int16_t), NULL },
};

/**
 * @brief Convert smallint to bigint
 * @param src Source datum (must be T_SMALLINT)
 * @return Converted T_BIGINT datum
 */
Datum
smallint2bigint(Datum src) {
    assert(src.type == T_SMALLINT);

    src.type = T_BIGINT;
    src.size = sizeof(int64_t);
    src.value.bigint = src.value.smallint;

    return src;
}

/**
 * @brief Convert integer to bigint
 * @param src Source datum (must be T_INTEGER)
 * @return Converted T_BIGINT datum
 */
Datum
integer2bigint(Datum src) {
    assert(src.type == T_INTEGER);

    src.type = T_BIGINT;
    src.size = sizeof(int64_t);
    src.value.bigint = src.value.integer;

    return src;
}

/**
 * @brief Convert char to varchar (trims trailing spaces)
 * @param src Source datum (must be T_CHAR)
 * @return Converted T_VARCHAR datum
 */
Datum
char2varchar(Datum src) {
    assert(src.type == T_CHAR);

    src.type = T_VARCHAR;
    while (src.size && src.value.character[src.size - 1] == ' ')
        --src.size;

    return src;
}

/**
 * @brief Convert to base type (bigint for integers, varchar for chars)
 * @param src Source datum
 * @return Converted datum, or src if already base type
 */
Datum
to_base_type(Datum src) {
    convert_fn convert = g_types[src.type].to_base_type;

    if (convert)
        src = convert(src);

    return src;
}

/**
 * @brief Compare two integer values
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return Negative if d1 < d2, zero if equal, positive if d1 > d2
 */
ssize_t
cmp_integer(Datum d1, Datum d2) {
    assert(g_types[d1.type].group == TG_INTEGER);
    assert(g_types[d2.type].group == TG_INTEGER);

    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    return d1.value.bigint - d2.value.bigint;
}

/**
 * @brief Compare two character values
 * @param d1 First character datum
 * @param d2 Second character datum
 * @return Negative if d1 < d2, zero if equal, positive if d1 > d2
 */
ssize_t
cmp_character(Datum d1, Datum d2) {
    ssize_t ret;

    assert(g_types[d1.type].group == TG_CHARACTER);
    assert(g_types[d2.type].group == TG_CHARACTER);

    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    ret = memcmp(d1.value.character, d2.value.character,
        d1.size < d2.size ? d1.size : d2.size);

    return ret == 0 ? (d1.size < d2.size ? -1 : (d1.size > d2.size ? 1 : 0)) : ret;
}

