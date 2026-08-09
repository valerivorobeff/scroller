/**
 * @file session.h
 * @brief session functions
 */

#ifndef _SESSION_H_
#define _SESSION_H_

/**
 * @brief Session struct
 */
typedef struct Session {
    int client_fd;          /**< Client socket descriptor */
    const char *user;       /**< Client user name */
    const char *catalog;    /**< Catalog name, can be NULL for some commands,
                                e.g. create catalog */
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

#endif /* _SESSION_H_ */

