#include "scrc.h"
#include "grid.h"
#include <malloc.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define BUFSZ   65536
/* RESERVESZ must be >= BUFSZ */
#define RESERVESZ BUFSZ
#define FULLSZ (BUFSZ + RESERVESZ)

/*
 * @brief helper structure to pass header line parameters
 */
typedef struct HeaderLine {
    char *name;
    char *value;
} HeaderLine;

static ScrcStatus send_header(ScrcConnection *conn);
static ScrcStatus send_query(ScrcConnection *conn, const char *query);
static ScrcStatus send_block(int sockfd, const char *data, size_t len);

static ScrcStatus recv_header(ScrcConnection *conn);
static ScrcStatus recv_header_line(ScrcConnection *conn, HeaderLine *hl);
static ScrcStatus recv_row(ScrcConnection *conn, ScrcRow *row);
static ScrcStatus recv_cmd(ScrcConnection *conn, ScrcCmd *cmd);
static ScrcStatus recv_block(ScrcConnection *conn, size_t size);
static ScrcStatus recv_refill(ScrcConnection *conn);

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

    conn->rbuf = malloc(FULLSZ);
    if (conn->rbuf == NULL) {
        conn->status = SCRC_BAD_ALLOC;
        goto catalog;
    }

    /* Create socket */
    conn->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (conn->sockfd < 0) {
        conn->status = SCRC_SOCKET_ERROR;
        goto rbuf;
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

    /* Handshake */
    conn->status = send_header(conn);
    if (conn->status != SCRC_OK)
        return conn;

    conn->status = recv_header(conn);
    if (conn->status == SCRC_OK)
        conn->status = SCRC_CONNECTED;

    return conn;

    /* Error handlings */
sock: close(conn->sockfd);
rbuf: free(conn->rbuf);
catalog: if (conn->catalog)
             free(conn->catalog);
user: free(conn->user);
host: free(conn->host);
err:  memset(conn, 0, sizeof(ScrcConnection));
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
        free(conn->rbuf);
        free(conn);
    }
}

ScrcStatus
scrc_query(ScrcConnection *conn, const char *query) {
    static const size_t column_blocksz = 16;
    ScrcCmd cmd;
    ScrcRow row;
    ScrcStatus ret = send_query(conn, query);

    if (ret != SCRC_OK)
        return ret;

    ret = recv_header(conn);
    if (ret != SCRC_OK)
        return ret;

    ret = recv_cmd(conn, &cmd);
    if (ret != SCRC_OK)
        return ret;

    /* Tab header */
    switch (cmd) {
        case SCRC_CMD_TABHEADER: break;
        case SCRC_CMD_TABDATA:
        case SCRC_CMD_ROW:
        case SCRC_CMD_END: return conn->status = SCRC_PROTOCOL_ERROR;
        default: return conn->status = SCRC_UNKNOWN_COMMAND;
    }

    /* Tab columns */
    for (;;) {
        ret = recv_row(conn, &row);
        if (ret == SCRC_END) {
            conn->status = SCRC_OK;
            break;
        }

        if (ret != SCRC_OK)
            return ret;

        if (conn->columnsz % column_blocksz == 0) {
            conn->columns = realloc(conn->columns, conn->columncap += column_blocksz);
            if (conn->columns == NULL)
                return conn->status = SCRC_BAD_ALLOC;
        }

        memcpy(conn->columns + conn->columnsz++, conn->rbuf, sizeof(Column));
    }

    /* Tab data command */
    ret = recv_cmd(conn, &cmd);
    if (ret != SCRC_OK)
        return ret;

    switch (cmd) {
        case SCRC_CMD_TABDATA: break;
        case SCRC_CMD_TABHEADER:
        case SCRC_CMD_ROW:
        case SCRC_CMD_END: return conn->status = SCRC_PROTOCOL_ERROR;
        default: return conn->status = SCRC_UNKNOWN_COMMAND;
    }

    return SCRC_OK;
}

ScrcStatus
scrc_fetch_row(ScrcConnection *conn, ScrcRow *row) {
    ScrcStatus ret = recv_row(conn, row);

    if (ret == SCRC_END) {
        *row = NULL;
        return conn->status = SCRC_OK;
    }

    if (ret != SCRC_OK) {
        *row = NULL;
        return ret;
    }

    *row = conn->rbuf;
    return SCRC_OK;
}

ScrcStatus
scrc_fetch_cell(ScrcConnection *conn, const ScrcRow row, size_t n, ScrcCell **cell) {
    const Column *c;

    if (n >= conn->columnsz) {
        *cell = NULL;
        return SCRC_OUT_OF_RANGE;
    }

    c = conn->columns + n;
    **cell = (ScrcCell){ .data = (const char *)row + c->offs, .size = c->size };

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

    return conn->status = send_block(conn->sockfd, header, len);
}

static ScrcStatus
send_query(ScrcConnection *conn, const char *query) {
    char buffer[BUFSZ];
    int len;

    if (!query || !*query) {
        return SCRC_OK;
    }

    /* Build query with request end marker */
    len = snprintf(buffer, sizeof(buffer), "%s$$\n", query);

    return conn->status = send_block(conn->sockfd, buffer, len);
}

static ScrcStatus 
send_block(int sockfd, const char *data, size_t len) {
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

/**
 * @brief Receive response header
 *
 * Reads lines until "$$\n". Parses name:value pairs.
 * Stores status, column count, etc.
 *
 * @param conn Connection
 * @return SCRC_OK on success
 */
static ScrcStatus
recv_header(ScrcConnection *conn) {
    for (;;) {
        HeaderLine hl;
        ScrcStatus ret = recv_header_line(conn, &hl);
        if (ret != SCRC_OK)
            return ret;

        if (hl.name == NULL || *hl.name == '\0')
            return conn->status = SCRC_PROTOCOL_ERROR;

        /* Check for end of header */
        if (strcmp(hl.name, "$$") == 0) {
            return SCRC_OK;
        } else if (strcmp(hl.name, "Status") == 0) {
            char *res;
            int status = strtol(hl.value, &res, 10);

            if (res)
                return conn->status = SCRC_PROTOCOL_ERROR;

            if (status != 0)
                return conn->status = status;
        }
        /* Ignore unknown headers */
    }
}

/**
 * @brief Read one header line from socket (up to \n)
 *
 * @param conn Connection
 * @param hl HeaderLine structure with found variables
 * @return SCRC_OK on success
 */
static ScrcStatus
recv_header_line(ScrcConnection *conn, HeaderLine *hl) {
    for (;;) {
        char *begin;
        char *find;
        bool delim;

        ScrcStatus ret = recv_block(conn, 1);
        if (ret != SCRC_OK)
            return ret;

        /* Find ':' in buffer */
        begin = conn->rbuf + conn->rpos;
        find = memchr(begin, ':', conn->rlen - conn->rpos);
        if (find) {
            *find = '\0';
            conn->rpos += find - begin + 1;
            hl->name = begin; /* @todo: trim */
            begin = conn->rbuf + conn->rpos;
            delim = true;
        } else
            delim = false;

        /* Find '\n' in buffer */
        find = memchr(begin, '\n', conn->rlen - conn->rpos);
        if (find == NULL)
            continue;

        *find = '\0';
        conn->rpos += find - begin + 1;
        if (delim)
            hl->value = begin;
        else
            hl->name = begin, hl->value = NULL; /* @todo: trum */

        return SCRC_OK;
    }
}

static ScrcStatus
recv_row(ScrcConnection *conn, ScrcRow *row) {
    ScrcCmd cmd;
    size_t size;
    ScrcStatus ret = recv_cmd(conn, &cmd);

    switch (cmd) {
        case SCRC_CMD_ROW: break;
        case SCRC_CMD_TABHEADER:
        case SCRC_CMD_TABDATA: return conn->status = SCRC_PROTOCOL_ERROR;
        case SCRC_CMD_END: return conn->status = SCRC_END;
        default: return conn->status = SCRC_UNKNOWN_COMMAND;
    }

    ret = recv_block(conn, sizeof(size_t));
    if (ret != SCRC_OK)
        return ret;

    size = *conn->rbuf;

    if (size != sizeof(Column))
        return conn->status = SCRC_INCORRECT_COLUMNSZ;

    ret = recv_block(conn, size);
    if (ret != SCRC_OK)
        return ret;

    *row = conn->rbuf;

    return SCRC_OK;
}

static ScrcStatus
recv_cmd(ScrcConnection *conn, ScrcCmd *cmd) {
    ScrcStatus ret = recv_block(conn, sizeof(ScrcCmd));
    if (ret != SCRC_OK)
        return ret;

    *cmd = *(conn->rbuf + conn->rpos);

    return SCRC_OK;
}

/**
 * @brief Receive al least `size` at most BUFSZ bytes
 *
 * Uses the connection's buffer. If data is available in buffer,
 * returns pointer to it. Otherwise refills buffer.
 *
 * @param conn Connection
 * @param size Number of bytes to receive
 * @return SCRC_OK on success
 */
static ScrcStatus
recv_block(ScrcConnection *conn, size_t size) {
    size_t total = 0; /* Bytes read */

    while (total < size) {
        /* How many bytes available in buffer? */
        const size_t available = conn->rlen - conn->rpos;

        if (available == 0) {
            /* Refill and retry */
            ScrcStatus ret = recv_refill(conn);
            if (ret == SCRC_CONNECTION_CLOSED) {
                return conn->status = (total == 0) ? SCRC_CONNECTION_CLOSED
                                    : SCRC_RECV_ERROR;
            }
            if (ret != SCRC_OK) {
                return ret;
            }
            continue;
        }

        /* Copy min(available, size - total) bytes */
        const size_t chunk = (available < size - total) ? available : size - total;
        conn->rpos += chunk;
        total += chunk;
    }

    return SCRC_OK;
}

/**
 * @brief Refill the receive buffer
 *
 * Reads up to RESERVESZ bytes into the buffer. If the buffer is
 * full (or nearly full), compacts it first by moving valid data
 * to the beginning.
 *
 * @param conn Connection
 * @return SCRC_OK on success, SCRC_RECV_ERROR on error,
 *         SCRC_CONNECTION_CLOSED on EOF
 */
static ScrcStatus
recv_refill(ScrcConnection *conn) {
    /* Space available after valid data */
    size_t space = FULLSZ - conn->rlen;

    /* If we can't read RESERVESZ bytes, compact first */
    if (space < RESERVESZ) {
        const size_t valid = conn->rlen - conn->rpos;

        if (valid > 0 && conn->rpos > 0) {
            memmove(conn->rbuf, conn->rbuf + conn->rpos, valid);
        }

        conn->rlen = valid;
        conn->rpos = 0;
        space = FULLSZ - conn->rlen;
    }

    /* Read into buffer */
    const ssize_t n = recv(conn->sockfd, conn->rbuf + conn->rlen, space, 0);
    if (n < 0) {
        if (errno == EINTR) {
            return SCRC_OK;  /* Caller retries */
        }
        return conn->status = SCRC_RECV_ERROR;
    }
    if (n == 0) {
        return conn->status = SCRC_CONNECTION_CLOSED;
    }

    conn->rlen += n;

    return SCRC_OK;
}

