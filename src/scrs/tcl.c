#include "tcl.h"
#include "session.h"
#include "tran.h"
#include "ihash.h"

extern Tran *g_tran;

ScrcStatus tran_put(Session *, bool, Tran **);

ScrcStatus
tcl_start(Session *session) {
    if (session->tran != NULL) {
        if (session->tran->autocommit == true) {
            ihash_erase(g_tran, session->tran->key);
            session->tran = NULL;
        } else
            return SCRS_TRANSACTION_START_ERROR;
    }

    /* Create transaction with autocommit = false */
    return tran_put(session, false, &session->tran);
}

ScrcStatus
tcl_commit(Session *session) {
    if (session->tran != NULL) {
        if (session->tran->autocommit == true)
            return SCRS_NO_TRANSACTION;
        else {
            tran_flag_put(session->tran->key, TF_COMMITED);
            ihash_erase(g_tran, session->tran->key);
            session->tran = NULL;
            return SCRS_OK;
        }
    }

    return SCRS_NO_TRANSACTION;
}

ScrcStatus
tcl_rollback(Session *session) {
    if (session->tran != NULL) {
        if (session->tran->autocommit == true)
            return SCRS_NO_TRANSACTION;
        else {
            /* @note Actually the next line can be removed as transaction
             *      is aborted by default */
            tran_flag_put(session->tran->key, TF_ABORTED);
            ihash_erase(g_tran, session->tran->key);
            session->tran = NULL;
            return SCRS_OK;
        }
    }

    return SCRS_NO_TRANSACTION;
}

