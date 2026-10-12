#ifndef _TRAN_H_
#define _TRAN_H_

#include <stdint.h>
#include <sys/types.h>
#include <stdbool.h>

#ifndef T_TRANID
#define T_TRANID    T_BIGINT
#define make_tranid make_bigint
#endif

typedef ssize_t tran_t;

typedef struct Mitor Mitor;

typedef struct Tran {
    tran_t key;         /**< Transaction id
                         (named as key to ihash requirements) */
} Tran;

static const tran_t TRAN_INVALID_ID = 0;

#define tran_init(t)    (Tran) { .key = 0 }

bool tran_row_is_visible(const Tran *tran, tran_t tmin, tran_t tmax);

#endif /* _TRAN_H_ */

