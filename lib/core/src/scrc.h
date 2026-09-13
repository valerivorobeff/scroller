#ifndef _SCRC_H_
#define _SCRC_H_

typedef enum ScrcStatus {
    SCRC_OK = 0,
    SCRC_BAD_ALLOC,
    SCRC_NO_HOST,
    SCRC_UNKNOWN_HOST,
    SCRC_NO_PORT,
    SCRC_INCORRECT_PORT,
    SCRC_NO_USER,
    SCRC_SOCKET_ERROR,
    SCRC_CONNECTION_ERROR,
    SCRC_SEND_ERROR
} ScrcStatus;

typedef struct ScrcConnection {
    ScrcStatus status;
    char *host;          /**< Server hostname or IP */
    int port;            /**< Server port */
    char *user;          /**< Username */
    char *catalog;       /**< Catalog name */
    int sockfd;          /**< Socket file descriptor */
} ScrcConnection;

ScrcConnection *scrc_connect(const char *host, int port, const char *user,
        const char *catalog);

ScrcConnection *scrc_reconnect(ScrcConnection *conn, const char *host,
        int port, const char *user, const char *catalog);

void scrc_close(ScrcConnection *conn);

ScrcStatus scrc_query(ScrcConnection *conn, const char *query);

#endif /* _SCRC_H_ */

