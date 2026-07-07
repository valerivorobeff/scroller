#ifndef _TYPE_H_
#define _TYPE_H_

#include <inttypes.h>
#include <stddef.h>
#include <sys/types.h>

typedef enum TypeGroup : int16_t {
    TG_UNKNOWN = 0,
    TG_INTEGER,
    TG_CHARACTER,
    TG_MAX
} TypeGroup;

typedef enum Type : int16_t {
    T_UNKNOWN = 0,
    T_SMALLINT,
    T_INTEGER,
    T_BIGINT,
    T_CHAR,
    T_VARCHAR,
    T_MAX
} Type;

typedef struct Datum {
    Type type;
    size_t size;
    union {
        int16_t smallint;
        int32_t integer;
        int64_t bigint;
        char *character;
        void *unknown;
    } value;
    
} Datum;

typedef Datum (*convert_fn)(Datum src);

typedef struct STypeGroup {
    TypeGroup group;
    Type base_type;
} STypeGroup;

typedef struct SType {
    Type type;
    TypeGroup group;
    size_t size;
    convert_fn to_base_type;
} SType;

extern STypeGroup g_type_groups[TG_MAX];
extern SType g_types[T_MAX];

Datum smallint2bigint(Datum src);
Datum integer2bigint(Datum src);
Datum char2varchar(Datum src);
Datum to_base_type(Datum src);

ssize_t cmp_integer(Datum d1, Datum d2);
ssize_t cmp_character(Datum d1, Datum d2);

#define eq_integer(d1, d2) (cmp_integer(d1, d2) == 0)
#define ne_integer(d1, d2) (cmp_integer(d1, d2) != 0)
#define lt_integer(d1, d2) (cmp_integer(d1, d2) < 0)
#define le_integer(d1, d2) (cmp_integer(d1, d2) <= 0)
#define gt_integer(d1, d2) (cmp_integer(d1, d2) > 0)
#define ge_integer(d1, d2) (cmp_integer(d1, d2) >= 0)

#define eq_character(d1, d2) (cmp_character(d1, d2) == 0)
#define ne_character(d1, d2) (cmp_character(d1, d2) != 0)
#define lt_character(d1, d2) (cmp_character(d1, d2) < 0)
#define le_character(d1, d2) (cmp_character(d1, d2) <= 0)
#define gt_character(d1, d2) (cmp_character(d1, d2) > 0)
#define ge_character(d1, d2) (cmp_character(d1, d2) >= 0)

#define make_smallint(v) (Datum){ T_SMALLINT, g_types[T_SMALLINT].size, .value.smallint = (v) }
#define make_integer(v) (Datum){ T_INTEGER, g_types[T_INTEGER].size, .value.integer = (v) }
#define make_bigint(v) (Datum){ T_BIGINT, g_types[T_BIGINT].size, .value.bigint = (v) }
#define make_char(v) (Datum){ T_CHAR, strlen(v), .value.character = (v) }
#define make_varchar(v) (Datum){ T_VARCHAR, strlen(v), .value.character = (v) }

#endif /* _TYPE_H_ */

