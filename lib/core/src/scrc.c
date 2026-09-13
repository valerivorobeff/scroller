#include "scrc.h"
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSZ   65536

static ScrcStatus send_header(ScrcConnection *conn);
static ScrcStatus send_query(ScrcConnection *conn, const char *query);
static ScrcStatus send_binary(int sockfd, const char *data, size_t len);

ScrcConnection *
scrc_connect(const char *host, int port, const char *user,
        const char *catalog) {
    ScrcConnection *conn = malloc(sizeof(ScrcConnection));
    if (conn == NULL)
        return NULL;

    return scrc_reconnect(conn, host, port, user, catalog);
}

ScrcConnection *
scrc_reconnect(ScrcConnection *conn, const char *host, int port,
        const char *user, const char *catalog) {
    struct sockaddr_in server_addr;
    struct hostent *host_info;

    memset(conn, 0, sizeof(ScrcConnection));

    /* Initialize parameters */

    if (!host) {
        conn->status = SCRC_NO_HOST;
        goto err;
    } else {
        conn->host = strdup(host);
        if (conn->host == NULL) {
            conn->status = SCRC_BAD_ALLOC;
            goto err;
        }
    }

    if (!port) {
        conn->status = SCRC_NO_PORT;
        goto host;
    } else if (port < 0 || port > UINT16_MAX) {
        conn->status = SCRC_INCORRECT_PORT;
        goto host;
    } else
        conn->port = port;

    if (!user) {
        conn->status = SCRC_NO_USER;
        goto host;
    } else {
        conn->user = strdup(user);
        if (conn->user == NULL) {
            conn->status = SCRC_BAD_ALLOC;
            goto host;
        }
    }

    if (catalog) {
        conn->catalog = strdup(catalog);
        if (conn->catalog == NULL) {
            conn->status = SCRC_BAD_ALLOC;
            goto user;
        }
    }

    /* Create socket */
    conn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->sockfd < 0) {
        conn->status = SCRC_SOCKET_ERROR;
        goto user;
    }

    /* Resolve hostname */
    host_info = gethostbyname(conn->host);
    if (!host_info) {
        conn->status = SCRC_UNKNOWN_HOST;
        goto sock;
    }

    /* Set up server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(conn->port);
    memcpy(&server_addr.sin_addr, host_info->h_addr, host_info->h_length);

    /* Connect to server */
    if (connect(conn->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        conn->status = SCRC_CONNECTION_ERROR;
        goto sock;
    }

    conn->status = send_header(conn);

    return conn;

    /* Error handlings */
sock: close(conn->sockfd);
user: free(conn->user);
host: free(conn->host);
err:  conn->host = conn->user = conn->catalog = NULL;
      conn->port = 0;
      conn->sockfd = -1;

    return conn;
}

void
scrc_close(ScrcConnection *conn) {
    if (conn) {
        free(conn->host);
        free(conn->user);
        if (conn->catalog)
            free(conn->catalog);
        free(conn);
    }
}

ScrcStatus
scrc_query(ScrcConnection *conn, const char *query) {
    ScrcStatus ret = send_query(conn, query);

    if (ret != SCRC_OK)
        return ret;

    return SCRC_OK;
}

static ScrcStatus
send_header(ScrcConnection *conn) {
    char header[BUFSZ];
    int len = 0;

    /* Build header: user: <username>\ncatalog: <catalog>\n */
    len += snprintf(header + len, sizeof(header) - len, "user: %s\n", conn->user);

    if (conn->catalog) {
        len += snprintf(header + len, sizeof(header) - len, "catalog: %s\n", conn->catalog);
    }

    /* Add header end marker */
    len += snprintf(header + len, sizeof(header) - len, "$$\n");

    return send_binary(conn->sockfd, header, len);
}

static ScrcStatus
send_query(ScrcConnection *conn, const char *query) {
    char buffer[BUFSZ];
    int len;

    if (!query || !*query) {
        return 0;
    }

    /* Build query with request end marker */
    len = snprintf(buffer, sizeof(buffer), "%s$$\n", query);

    return send_binary(conn->sockfd, buffer, len);
}

static ScrcStatus 
send_binary(int sockfd, const char *data, size_t len) {
    ssize_t sent = 0;
    ssize_t total = 0;

    while (total < (ssize_t)len) {
        sent = send(sockfd, data + total, len - total, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return SCRC_SEND_ERROR;
        }
        total += sent;
    }

    return SCRC_OK;
}

