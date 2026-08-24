/**
 * @file client.h
 * @brief Scroller console client
 */

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/**
 * @brief Client configuration
 */
typedef struct Client {
    char *host;          /**< Server hostname or IP */
    int port;            /**< Server port */
    char *user;          /**< Username */
    char *catalog;       /**< Catalog name */
    int sockfd;          /**< Socket file descriptor */
    int interactive;     /**< 1 if interactive mode, 0 if script mode */
} Client;

/**
 * @brief Initialize client
 * @param client Client structure to initialize
 * @param host Server hostname
 * @param port Server port
 * @param user Username
 * @param catalog Catalog name
 * @return 0 on success, -1 on error
 */
int client_create(Client *client, const char *host, int port,
                const char *user, const char *catalog);

/**
 * @brief Connect to server
 * @param client Client structure
 * @return 0 on success, -1 on error
 */
int client_connect(Client *client);

/**
 * @brief Send query to server
 * @param client Client structure
 * @param query Query string to send
 * @return 0 on success, -1 on error
 */
int client_send_query(Client *client, const char *query);

/**
 * @brief Receive response from server
 * @param client Client structure
 * @param response Buffer to store response
 * @param size Buffer size
 * @return Number of bytes received, -1 on error
 */
int client_receive_response(Client *client, char *response, size_t size);

/**
 * @brief Send header to server (user and catalog)
 * @param client Client structure
 * @return 0 on success, -1 on error
 */
int client_send_header(Client *client);

/**
 * @brief Run interactive mode
 * @param client Client structure
 * @return 0 on success, -1 on error
 */
int client_run_interactive(Client *client);

/**
 * @brief Run script mode (read from stdin)
 * @param client Client structure
 * @return 0 on success, -1 on error
 */
int client_run_script(Client *client);

/**
 * @brief Close client connection
 * @param client Client structure
 */
void client_free(Client *client);

/**
 * @brief Print usage help
 * @param program Program name
 */
void print_usage(const char *program);

#endif /* _CLIENT_H_ */

