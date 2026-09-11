/**
 * @file session.h
 * @brief session functions
 */

#ifndef _SESSION_H_
#define _SESSION_H_

#include <stddef.h>

#define SENDBUFSZ  4096    /* size of send buffer */

/**
 * @brief Session struct
 */
typedef struct Session {
    int client_fd;              /**< Client socket descriptor */
    const char *user;           /**< Client user name */
    const char *catalog;        /**< Catalog name, can be NULL for some commands,
                                    e.g. create catalog */
    char send_buf[SENDBUFSZ];   /**< Send buffer (data buffer for client) */
    size_t send_buf_idx;        /**< Send buffer index */
} Session;

/**
 * @brief Initializes session
 * @param session session struct to initialize
 * @return pointer to initialized Session of NULL if error
 */
Session *session_init(Session *session);

/**
 * @brief Runs session
 * @param session session struct
 * @return 0 - if succeed, error code otherwise
 */
int session_run(Session *session);

/**
 * @brief Drops session
 * @param session session struct
 * @return 0 - if succeed, error code otherwise
 */
int session_drop(Session *session);

/**
 * @brief Sends buffer to client
 * @param session session struct
 * @param buf buffer
 * @param len buffer length
 * @return 0 - if succeed, error code otherwise
 */
int session_send(Session *session, const char *buf, size_t len);

/**
 * @brief Flushes buffer to client
 * @param session session struct
 * @return 0 - if succeed, error code otherwise
 */
int session_flush(Session *session);

#endif /* _SESSION_H_ */

