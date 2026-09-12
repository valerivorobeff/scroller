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
int session_send(Session *session, const void *buf, size_t len);

/**
 * @brief Sends header to client with value of type const char *
 * @param session session struct
 * @param name
 * @param value
 * @return 0 - if succeed, error code otherwise
 */
int session_send_header_str(Session *session, const char *name, const char *value);

/**
 * @brief Alias macro session_send_header_str
 */
#define session_send_header(s, n, v) session_send_header_str(s, n, v)

/**
 * @brief Sends header to client with value of type long long int
 * @param session session struct
 * @param name
 * @param value
 * @return 0 - if succeed, error code otherwise
 */
int session_send_header_int(Session *session, const char *name, long long int value);

/**
 * @brief Sends finish header mark to client
 * @param session session struct
 * @return 0 - if succeed, error code otherwise
 */
int session_finish_header(Session *session);

/**
 * @brief Flushes buffer to client
 * @param session session struct
 * @return 0 - if succeed, error code otherwise
 */
int session_flush(Session *session);

#endif /* _SESSION_H_ */

