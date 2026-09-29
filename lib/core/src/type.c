/**
 * @file type.c
 * @brief Type system implementation
 */

#include "type.h"
#include "memory.h"
#include <string.h>
#include <assert.h>

static bool like(const char *src, const char *pattern);
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
    { T_NAME, TG_UNKNOWN, SM_TYPESZ, 0, NULL },
    { T_SMALLINT, TG_INTEGER, SM_TYPESZ, sizeof(int16_t), smallint2bigint },
    { T_INTEGER, TG_INTEGER, SM_TYPESZ, sizeof(int32_t), integer2bigint },
    { T_BIGINT, TG_INTEGER, SM_TYPESZ, sizeof(int64_t), NULL },
    { T_CHAR, TG_CHARACTER, SM_COLUMNSZ, 0, char2varchar },
    { T_VARCHAR, TG_CHARACTER, SM_TYPESZ, sizeof(int16_t), NULL },
};

/**
 * @brief Shows Datum as a null-terminated string
 * @note: allocates memory for string in current memory context
 * @param src Source datum to print
 * @return Null-terminated string representing the datum
 */
/* @todo: show all the types */
const char *datum_sdup(Datum src) {
    switch (src.type) {
        case T_UNKNOWN:     return sdup("<Unknown type>");
        case T_SMALLINT:
        case T_INTEGER:
        case T_BIGINT:  {
                            char *ret;
                            int64_t v;
                            int cnt = 1;

                            src = to_base_type(src);
                            v = src.value.bigint;
                            while (v /= 10)
                                ++cnt;

                            ret = salloc(cnt + 1);
                            v = src.value.bigint;
                            ret[cnt] = '\0';
                            while (cnt--) {
                                ret[cnt] = '0' + v % 10;
                                v /= 10;
                            }

                            return ret;
                        }
        case T_CHAR:    {
                            char *ret;
                            src = to_base_type(src);
                            ret = salloc(src.size + 1);
                            memcpy(ret, src.value.character, src.size);
                            ret[src.size] = '\0';
                            return ret;
                        }
        case T_VARCHAR:     return sdup("<varchar type not implemented>");
        case T_MAX:         return sdup("<Out of range type>");
        default:            return sdup("<Not implemented type>");
    }
}

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
 * brief - Answers if datum has a non-zero value (not NULL, 0, empty string)
 * @param d datum
 * @return true if zeroed, false otherwise
 */
bool
datum_zeroed(Datum d) {
    switch (d.type) {
        case T_UNKNOWN:     assert(0 && "Unknown data type"); return false;
        case T_SMALLINT:    return d.value.smallint == 0;
        case T_INTEGER:     return d.value.integer == 0;
        case T_BIGINT:      return d.value.bigint == 0;
        case T_NAME:
        case T_CHAR:
        case T_VARCHAR:     return d.value.character == NULL ||
                                *d.value.character == '\0';
        case T_MAX:         assert(0 && "Incorrect data type"); return false;
    }

    return false;
}

/**
 * @brief Checks comparability of two data
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return true if data comparable, false otherwise
 */
bool
data_comparable(Datum d1, Datum d2) {
    return get_type_group(d1.type) == get_type_group(d2.type) &&
        get_type_group(d1.type) != TG_UNKNOWN;
}

/**
 * @brief Compare two integer values
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return Negative if d1 < d2, zero if equal, positive if d1 > d2
 */
ssize_t
cmp_integer(Datum d1, Datum d2) {
    assert(get_type_group(d1.type) == TG_INTEGER);
    assert(get_type_group(d2.type) == TG_INTEGER);

    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    return d1.value.bigint - d2.value.bigint;
}

/** @brief Adds two integers
 * @note you should check data_arithmetical(d1, d2) before using this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
add_integer(Datum d1, Datum d2) {
    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    d1.value.bigint += d2.value.bigint;

    return d1;
}

/** @brief Substitutes two integers
 * @note you should check data_arithmetical(d1, d2) before using this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
sub_integer(Datum d1, Datum d2) {
    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    d1.value.bigint -= d2.value.bigint;

    return d1;
}

/** @brief Multiplies two integers
 * @note you should check data_arithmetical(d1, d2) before using this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
mul_integer(Datum d1, Datum d2) {
    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    d1.value.bigint *= d2.value.bigint;

    return d1;
}

/** @brief Divides two integers
 * @note you should check data_arithmetical(d1, d2) before using this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
div_integer(Datum d1, Datum d2) {
    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    d1.value.bigint /= d2.value.bigint;

    return d1;
}

/** @brief Modulos two integers
 * @note you should check data_arithmetical(d1, d2) before using this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
mod_integer(Datum d1, Datum d2) {
    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    d1.value.bigint %= d2.value.bigint;

    return d1;
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

    assert(get_type_group(d1.type) == TG_CHARACTER);
    assert(get_type_group(d2.type) == TG_CHARACTER);

    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    ret = memcmp(d1.value.character, d2.value.character,
        d1.size < d2.size ? d1.size : d2.size);

    return ret == 0 ? (d1.size < d2.size ? -1 : (d1.size > d2.size ? 1 : 0)) : ret;
}

/**
 * @brief Compare two data values
 * @param d1 First character datum
 * @param d2 Second character datum
 * note if data not comparable it asserts and returns 0, you should
 *      check data comparability before using this function by calling data_comparable!
 * @return Negative if d1 < d2, zero if equal, positive if d1 > d2
 */
ssize_t
cmp_data(Datum d1, Datum d2) {
    if (data_comparable(d1, d2)) {
        switch (get_type_group(d1.type)) {
            case TG_UNKNOWN: assert(0 && "Cannot copmare TG_UNKNOWN"); break;
            case TG_INTEGER: return cmp_integer(d1, d2);
            case TG_CHARACTER: return cmp_character(d1, d2);
            case TG_MAX: assert(0 && "Cannot copmare TG_MAX"); break;
        }
    }

    return 0;
}

/** @brief Adds two data
 * @note you should check data_arithmetical(d1, d2) before using this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
add_data(Datum d1, Datum d2) {
    if (data_comparable(d1, d2)) {
        switch (get_type_group(d1.type)) {
            case TG_INTEGER: return add_integer(d1, d2);
            default: assert(0 && "Cannot add not arithmetical data"); break;
        }
    } else
        assert(0 && "Cannot add non comparable data");
}

/** @brief Substitutes two data
 * @note you should check data_arithmetical(d1, d2) before using this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
sub_data(Datum d1, Datum d2) {
    if (data_comparable(d1, d2)) {
        switch (get_type_group(d1.type)) {
            case TG_INTEGER: return sub_integer(d1, d2);
            default: assert(0 && "Cannot substitute not arithmetical data"); break;
        }
    } else
        assert(0 && "Cannot substitute non comparable data");
}

/** @brief Multiplies two data
 * @note you should check data_arithmetical(d1, d2) before using this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
mul_data(Datum d1, Datum d2) {
    if (data_comparable(d1, d2)) {
        switch (get_type_group(d1.type)) {
            case TG_INTEGER: return mul_integer(d1, d2);
            default: assert(0 && "Cannot multiply not arithmetical data"); break;
        }
    } else
        assert(0 && "Cannot multiply non comparable data");
}

/** @brief Divides two data
 * @note you should check data_arithmetical(d1, d2) before using this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
div_data(Datum d1, Datum d2) {
    if (data_comparable(d1, d2)) {
        switch (get_type_group(d1.type)) {
            case TG_INTEGER: return div_integer(d1, d2);
            default: assert(0 && "Cannot divide not arithmetical data"); break;
        }
    } else
        assert(0 && "Cannot divide non comparable data");
}

/** @brief Modulos two data
 * @note you should check data type == TG_INTEGER for both data before using
 *      this function
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return new datum of type T_BIGINT containing the result
 */
Datum
mod_data(Datum d1, Datum d2) {
    if (data_comparable(d1, d2)) {
        switch (get_type_group(d1.type)) {
            case TG_INTEGER: return mod_integer(d1, d2);
            default: assert(0 && "Cannot modulo not integer data"); break;
        }
    } else
        assert(0 && "Cannot modulo non comparable data");
}

/**
 * @brief Concatenates two lexixal values
 * @note: allocates memory for string in current memory context
 * @param d1 datum (must be T_CHAR or T_VARCHAR)
 * @param d2 datum (must be T_CHAR or T_VARCHAR)
 * @return Concatenated character data
 */
Datum
cat_data(Datum d1, Datum d2) {
    if (data_lexical(d1, d2)) {
        const size_t l1 = strlen(d1.value.character);
        const size_t l2 = strlen(d2.value.character);
        char *str = salloc(l1 + l2 + 1);

        strcpy(str, d1.value.character);
        strcpy(str + l1, d2.value.character);
        str[l1 + l2] = '\0';

        return make_char(str);
    } else
        assert(0 && "Cannot concatenate non lexical data");
}

/**
 * @brief SQL LIKE pattern matching
 * @param d1 Source datum (must be T_CHAR or T_VARCHAR)
 * @param d2 Pattern datum (must be T_CHAR or T_VARCHAR)
 * @return true if d1 matches pattern d2
 */
bool
like_data(Datum d1, Datum d2) {
    assert(data_lexical(d1, d2));

    d1 = to_base_type(d1);
    d2 = to_base_type(d2);

    return like(d1.value.character, d2.value.character);
}

static bool
like(const char *src, const char *pattern) {
    const char *s = src;
    const char *p = pattern;

    while (*p) {
        if (*p == '%') {
            /* Skip multiple % */
            while (*p == '%') p++;

            if (*p == '\0')
                return true;  /* % in the end matches all */

            /* Find next part (till next '%'') */
            const char *part = p;
            while (*p && *p != '%') p++;
            size_t part_len = p - part;

            /* Find part in src */
            const char *found = memmem(s, strlen(s), part, part_len);
            if (!found)
                return false;

            s = found + part_len;
        } else if (*p == '_') {
            if (*s == '\0') return false;
            s++;
            p++;
        } else {
            if (*p != *s) return false;
            s++;
            p++;
        }
    }

    return *s == '\0';
}

