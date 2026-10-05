#ifndef _TRAN_H_
#define _TRAN_H_

#include <stddef.h>

#ifndef T_TRANID
#define T_TRANID    T_BIGINT
#endif

typedef struct Tran {
    ssize_t key;        /**< Transaction id
                         (named as key to ihash requirements) */
} Tran;

#define tran_init(t)    (Tran) { .key = 0 }

#endif /* _TRAN_H_ */

