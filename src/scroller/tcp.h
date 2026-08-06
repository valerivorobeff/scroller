/**
 * @file tcp.h
 * @brief scroller server file
 */

#ifndef _TCP_H_
#define _TCP_H_

#include "server.h"

/**
 * @brief Initializes tcp
 * @note it user g_server struct to store server information
 * @return 0 - if succeed, error code otherwise
 */
int tcp_init(void);
/**
 * @brief Runs tcp
 * @note it user g_server struct to store server information
 * @return 0 - if succeed, error code otherwise
 */
int tcp_run(void);
/**
 * @brief Drops tcp
 * @note it user g_server struct to store server information
 * @return 0 - if succeed, error code otherwise
 */
int tcp_drop(void);

#endif /* _TCP_H_ */

