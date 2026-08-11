/**
 * @file type.h
 * @brief SQL type system with conversions and comparisons
 */

#ifndef _TYPE_H_
#define _TYPE_H_

#include <inttypes.h>
#include <stddef.h>
#include <sys/types.h>

/**
 * @brief Type groups for categorization
 */
typedef enum TypeGroup : int16_t {
    TG_UNKNOWN = 0,     /**< Unknown type group */
    TG_INTEGER,         /**< Integer types (smallint, integer, bigint) */
    TG_CHARACTER,       /**< Character types (char, varchar) */
    TG_MAX              /**< Number of type groups */
} TypeGroup;

/**
 * @brief SQL data types
 */
typedef enum Type : int16_t {
    T_UNKNOWN = 0,      /**< Unknown type */
    T_SMALLINT,         /**< 16-bit integer */
    T_INTEGER,          /**< 32-bit integer */
    T_BIGINT,           /**< 64-bit integer */
    T_CHAR,             /**< Fixed-length character (padded with spaces) */
    T_VARCHAR,          /**< Variable-length character */
    T_MAX               /**< Number of types */
} Type;

/**
 * @brief Datum holding a value with its type
 */
typedef struct Datum {
    Type type;          /**< Data type */
    size_t size;        /**< Size in bytes (for strings: length) */
    union {
        int16_t smallint;   /**< T_SMALLINT value */
        int32_t integer;    /**< T_INTEGER value */
        int64_t bigint;     /**< T_BIGINT value */
        char *character;    /**< T_CHAR/T_VARCHAR value */
        void *unknown;      /**< T_UNKNOWN value */
    } value;            /**< Union of possible values */
} Datum;

/**
 * @brief Type conversion function signature
 */
typedef Datum (*convert_fn)(Datum src);

/**
 * @brief Type group descriptor
 */
typedef struct STypeGroup {
    TypeGroup group;    /**< Type group */
    Type base_type;     /**< Base type for this group */
} STypeGroup;

/**
 * @brief Type descriptor
 */
typedef struct SType {
    Type type;              /**< Type enum */
    TypeGroup group;        /**< Type group */
    enum {
        SM_TYPESZ,          /**< Size is defined in SType:size, e.g. T_INTEGER, T_VARCHAR */
        SM_COLUMNSZ         /**< Size is defined in Column:size, e.g. T_CHAR */
    } size_meaning;
    size_t size;            /**< Size in bytes (0 for variable-length) */
    convert_fn to_base_type;/**< Conversion to base type, or NULL */
} SType;

/** @brief Type group metadata */
extern const STypeGroup g_type_groups[TG_MAX];

/** @brief Type metadata */
extern const SType g_types[T_MAX];

/**
 * @brief Shows Datum as a null-terminated string
 * @note: allocates memory for string in current memory context
 * @param src Source datum to print
 * @return Null-terminated string representing the datum
 */
const char *datum_sdup(Datum src);

/**
 * @brief Convert smallint to bigint
 * @param src Source datum (must be T_SMALLINT)
 * @return Converted T_BIGINT datum
 */
Datum smallint2bigint(Datum src);

/**
 * @brief Convert integer to bigint
 * @param src Source datum (must be T_INTEGER)
 * @return Converted T_BIGINT datum
 */
Datum integer2bigint(Datum src);

/**
 * @brief Convert char to varchar (trims trailing spaces)
 * @param src Source datum (must be T_CHAR)
 * @return Converted T_VARCHAR datum
 */
Datum char2varchar(Datum src);

/**
 * @brief Convert to base type (bigint for integers, varchar for chars)
 * @param src Source datum
 * @return Converted datum, or src if already base type
 */
Datum to_base_type(Datum src);

/**
 * @brief Compare two integer values
 * @param d1 First integer datum
 * @param d2 Second integer datum
 * @return Negative if d1 < d2, zero if equal, positive if d1 > d2
 */
ssize_t cmp_integer(Datum d1, Datum d2);

/**
 * @brief Compare two character values
 * @param d1 First character datum
 * @param d2 Second character datum
 * @return Negative if d1 < d2, zero if equal, positive if d1 > d2
 */
ssize_t cmp_character(Datum d1, Datum d2);

/** @brief Check if two integers are equal */
#define eq_integer(d1, d2) (cmp_integer(d1, d2) == 0)
/** @brief Check if two integers are not equal */
#define ne_integer(d1, d2) (cmp_integer(d1, d2) != 0)
/** @brief Check if first integer is less than second */
#define lt_integer(d1, d2) (cmp_integer(d1, d2) < 0)
/** @brief Check if first integer is less than or equal to second */
#define le_integer(d1, d2) (cmp_integer(d1, d2) <= 0)
/** @brief Check if first integer is greater than second */
#define gt_integer(d1, d2) (cmp_integer(d1, d2) > 0)
/** @brief Check if first integer is greater than or equal to second */
#define ge_integer(d1, d2) (cmp_integer(d1, d2) >= 0)

/** @brief Check if two characters are equal */
#define eq_character(d1, d2) (cmp_character(d1, d2) == 0)
/** @brief Check if two characters are not equal */
#define ne_character(d1, d2) (cmp_character(d1, d2) != 0)
/** @brief Check if first character is less than second */
#define lt_character(d1, d2) (cmp_character(d1, d2) < 0)
/** @brief Check if first character is less than or equal to second */
#define le_character(d1, d2) (cmp_character(d1, d2) <= 0)
/** @brief Check if first character is greater than second */
#define gt_character(d1, d2) (cmp_character(d1, d2) > 0)
/** @brief Check if first character is greater than or equal to second */
#define ge_character(d1, d2) (cmp_character(d1, d2) >= 0)

/** @brief Create T_SMALLINT datum */
#define make_smallint(v) ((Datum){ T_SMALLINT, g_types[T_SMALLINT].size, .value.smallint = (v) })
/** @brief Create T_INTEGER datum */
#define make_integer(v) ((Datum){ T_INTEGER, g_types[T_INTEGER].size, .value.integer = (v) })
/** @brief Create T_BIGINT datum */
#define make_bigint(v) ((Datum){ T_BIGINT, g_types[T_BIGINT].size, .value.bigint = (v) })
/** @brief Create T_CHAR datum (string length is set) */
#define make_char(v) ((Datum){ T_CHAR, (v) ? strlen(v) : 0, .value.character = (v) })
/** @brief Create T_VARCHAR datum (string length is set) */
#define make_varchar(v) ((Datum){ T_VARCHAR, (v) ? strlen(v) : 0, .value.character = (v) })

#endif /* _TYPE_H_ */

