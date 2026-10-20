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
    bool autocommit;    /**< Every query is automatically commited */
} Tran;

typedef enum TranFlag {
    TF_COMMITED = 0,
    TF_ABORTED  = 1
} TranFlag;

/**
 * @brief tran page layout
 */
typedef struct TranPage {
    size_t tid_start;       /**< tid which datum bitset starts from */
    unsigned char datum[];  /**< tid bitset if TranFlag */
} TranPage;

static const tran_t TRAN_INVALID_ID = 0;

bool tran_row_is_visible(const Tran *tran, tran_t tmin, tran_t tmax);
TranFlag tran_flag_get(tran_t tid);
void tran_flag_put(tran_t tid, TranFlag flag);


#endif /* _TRAN_H_ */

