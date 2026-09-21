/**
 * @file scrc.c
 * @brief Scroller client library implementation
 *
 * @see scrc.h for protocol description and API documentation.
 */

#include "scrc.h"
#include "grid.h"
#include <malloc.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

/**
 * @brief Main buffer size (64 KB)
 */
#define BUFSZ   65536

/**
 * @brief Reserve size for reads without memmove
 * @note RESERVESZ must be >= BUFSZ
 */
#define RESERVESZ BUFSZ

/**
 * @brief Full buffer size (BUFSZ + RESERVESZ)
 */
#define FULLSZ (BUFSZ + RESERVESZ)

/**
 * @struct HeaderLine
 * @brief Parsed header line (name and value)
 */
typedef struct HeaderLine {
    char *name;     /**< Pointer to name (in buffer) */
    char *value;    /**< Pointer to value (in buffer), or NULL */
} HeaderLine;

static ScrcStatus send_header(ScrcConnection *conn);
static ScrcStatus send_query(ScrcConnection *conn, const char *query);
static ScrcStatus send_block(int sockfd, const char *data, size_t len);

static ScrcStatus recv_header(ScrcConnection *conn);
static ScrcStatus recv_header_line(ScrcConnection *conn, HeaderLine *hl);
static ScrcStatus recv_row(ScrcConnection *conn, ScrcRow *row);
static ScrcStatus recv_cmd(ScrcConnection *conn, ScrcCmd *cmd);
static ScrcStatus recv_block(ScrcConnection *conn, size_t size, char **p);
static ScrcStatus recv_refill(ScrcConnection *conn);
static char *trim(char *src);

/**
 * @brief Connect to server and perform handshake
 * @see scrc.h for full documentation
 */
ScrcConnection *
scrc_connect(const char *host, int port, const char *user,
        const char *catalog) {
    struct sockaddr_in server_addr;
    struct hostent *host_info;

    ScrcConnection *conn = malloc(sizeof(ScrcConnection));
    if (conn == NULL)
        return NULL;

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

    if (port < 0 || port > UINT16_MAX) {
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
        goto sock;

    conn->status = recv_header(conn);
    if (conn->status != SCRC_OK)
        goto sock;

    return conn;

    /* Error handlings */
sock: close(conn->sockfd);
rbuf: free(conn->rbuf);
catalog: if (conn->catalog)
             free(conn->catalog);
user: free(conn->user);
host: free(conn->host);
err:
      ScrcStatus tmp = conn->status;
      memset(conn, 0, sizeof(ScrcConnection));
      conn->sockfd = -1;
      conn->status = tmp;

    return conn;
}

/**
 * @brief Close connection and free resources
 * @see scrc.h for full documentation
 */
void
scrc_close(ScrcConnection *conn) {
    if (conn) {
        if (conn->sockfd >= 0)
            close(conn->sockfd);
        free(conn->host);
        free(conn->user);
        if (conn->catalog)
            free(conn->catalog);
        if (conn->columns)
            free(conn->columns);
        free(conn->rbuf);
        free(conn);
    }
}

const char *
scrc_error(ScrcConnection *conn) {
    if (conn == NULL)
        return NULL;

    switch (conn->status) {
        case SCRC_OK:                       return "";
        case SCRC_END:                      return "";
        case SCRC_BAD_ALLOC:                return "Client bad alloc";
        case SCRC_NO_HOST:                  return "No host";
        case SCRC_UNKNOWN_HOST:             return "Unknown host";
        case SCRC_INCORRECT_PORT:           return "Incorrect port";
        case SCRC_NO_USER:                  return "No user";
        case SCRC_SOCKET_ERROR:             return "Socket error";
        case SCRC_CONNECTION_ERROR:         return "Connection error";
        case SCRC_CONNECTION_CLOSED:        return "Server closed connection";
        case SCRC_SEND_ERROR:               return "Client send error";
        case SCRC_RECV_ERROR:               return "Client receive error";
        case SCRC_PROTOCOL_ERROR:           return "Protocol error";
        case SCRC_HEADER_ERROR:             return "Header error";
        case SCRC_HEADER_TOO_LARGE:         return "Header too large";
        case SCRC_UNKNOWN_COMMAND:          return "Unknown command";
        case SCRC_BUFFER_OVERFLOW:          return "Buffer overflow";
        case SCRC_INCORRECT_PARAM:          return "Incorrect param";
        case SCRC_OUT_OF_RANGE:             return "Out of range";

        case SCRS_NO_USER:                  return "No user";
        case SCRS_PARSER_ERROR:             return "Parser error";
        case SCRS_PARSER_MEMORY_EXHAUSTION: return "Parser memory exhaustion";
        case SCRS_UNKNOWN_PARSER_ERROR:     return "Unknown parser error";
        case SCRS_SEND_ERROR:               return "Server send error";
        case SCRS_SESSION_CLOSED:           return "Client closed connection";
        case SCRS_UNKNOWN_RELATION:         return "Unknown relation";
        case SCRS_UNKNOWN_COLUMN:           return "Unknown column";
        case SCRS_BAD_ALLOC:                return "Server bad alloc";
        case SCRS_DATUM_TYPE_MISMATCH:      return "Datum type mismatch";
        case SCRS_SEQUENCE_OVERFLOW:        return "Sequence overflow";
    }

    return "Unknown error";
}

/**
 * @brief Send query and receive response header
 * @see scrc.h for full documentation
 */
ScrcStatus
scrc_query(ScrcConnection *conn, const char *query) {
    static const size_t column_blocksz = 16;
    ScrcCmd cmd;
    ScrcStatus ret;

    if (conn->status != SCRC_OK)
        return conn->status;

    if (query == NULL)
        return SCRC_INCORRECT_PARAM;

    /* Reset columns, we don't free memory but just reuse it */
    conn->columnsz = 0;

    ret = send_query(conn, query);

    if (ret != SCRC_OK)
        return conn->status = ret;

    ret = recv_header(conn);
    if (ret != SCRC_OK)
        return conn->status = ret;

    if (conn->body == false)
        return ret;

    ret = recv_cmd(conn, &cmd);
    if (ret != SCRC_OK)
        return conn->status = ret;

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
        ScrcRow row;
        ret = recv_row(conn, &row);
        if (ret == SCRC_END) {
            conn->status = SCRC_OK;
            break;
        }

        if (ret != SCRC_OK)
            return conn->status = ret;

        if (conn->columnsz >= conn->columncap) {
            const size_t new_cap = conn->columncap + column_blocksz;
            Column *new_cols = realloc(conn->columns, new_cap * sizeof(Column));
            if (new_cols == NULL)
                return conn->status = SCRC_BAD_ALLOC;
            conn->columns = new_cols;
            conn->columncap = new_cap;
        }

        memcpy(conn->columns + conn->columnsz++, row, sizeof(Column));
    }

    /* Tab data command */
    ret = recv_cmd(conn, &cmd);
    if (ret != SCRC_OK)
        return conn->status = ret;

    switch (cmd) {
        case SCRC_CMD_TABDATA: break;
        case SCRC_CMD_TABHEADER:
        case SCRC_CMD_ROW:
        case SCRC_CMD_END: return conn->status = SCRC_PROTOCOL_ERROR;
        default: return conn->status = SCRC_UNKNOWN_COMMAND;
    }

    return conn->status = SCRC_OK;
}

/**
 * @brief Fetch next row
 * @see scrc.h for full documentation
 */
ScrcStatus
scrc_fetch_row(ScrcConnection *conn, ScrcRow *row) {
    ScrcStatus ret;

    if (row == NULL)
        return conn->status = SCRC_INCORRECT_PARAM;

    if (conn->body == false)
        return conn->status = SCRC_OK;

    ret = recv_row(conn, row);

    if (ret == SCRC_END) {
        *row = NULL;
        return conn->status = SCRC_OK;
    }

    if (ret != SCRC_OK) {
        *row = NULL;
        return conn->status = ret;
    }

    return conn->status = SCRC_OK;
}

/**
 * @brief Extract cell from row
 * @see scrc.h for full documentation
 */
ScrcStatus
scrc_fetch_cell(ScrcConnection *conn, const ScrcRow row, size_t n, ScrcCell *cell) {
    const Column *c;

    if (row == NULL || cell == NULL)
        return conn->status = SCRC_INCORRECT_PARAM;

    if (n >= conn->columnsz) {
        *cell = (ScrcCell){ .data = NULL, .size = 0 };
        return conn->status = SCRC_OUT_OF_RANGE;
    }

    c = conn->columns + n;
    *cell = (ScrcCell){ .data = (const char *)row + c->offs, .size = c->size };

    return conn->status = SCRC_OK;
}

/**
 * @brief Send header to server
 *
 * Builds and sends header with user, optional catalog, and end marker "$$".
 *
 * @param conn Connection
 * @return SCRC_OK on success, SCRC_SEND_ERROR on failure
 *
 * @note Called only from scrc_connect()
 */
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

    return send_block(conn->sockfd, header, len);
}

/**
 * @brief Send query string to server
 *
 * Appends "$$" end marker and sends to server.
 *
 * @param conn  Connection
 * @param query SQL query string
 * @return      SCRC_OK on success, SCRC_SEND_ERROR on failure
 *
 * @note Empty query (NULL or "") is silently ignored (returns SCRC_OK).
 */
static ScrcStatus
send_query(ScrcConnection *conn, const char *query) {
    char buffer[BUFSZ];
    int len;

    if (!query || !*query) {
        return SCRC_OK;
    }

    /* Build query with request end marker */
    len = snprintf(buffer, sizeof(buffer), "%s$$\n", query);

    return send_block(conn->sockfd, buffer, len);
}

/**
 * @brief Send all data, handling partial sends
 *
 * Loops until all `len` bytes are sent. Handles EINTR.
 * Uses MSG_NOSIGNAL to avoid SIGPIPE.
 *
 * @param sockfd Socket descriptor
 * @param data   Data to send
 * @param len    Data length
 * @return       SCRC_OK on success, SCRC_SEND_ERROR on failure
 *
 * @note This function blocks until all data is sent.
 */
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
 * Reads lines until "$$" end marker. Parses "Status: <code>" lines
 * and returns server status if non-zero. Ignores unknown headers.
 *
 * @param conn Connection
 * @return SCRC_OK on success, error code otherwise
 *
 * @note Server status is returned as-is (see ScrcStatus for server range).
 * @warning Header lines are modified in-place (':' and '\n' replaced with '\0').
 */
static ScrcStatus
recv_header(ScrcConnection *conn) {
    conn->body = false;

    for (;;) {
        HeaderLine hl;
        ScrcStatus ret = recv_header_line(conn, &hl);
        if (ret != SCRC_OK)
            return ret;

        if (hl.name == NULL || *hl.name == '\0')
            return SCRC_PROTOCOL_ERROR;

        /* Check for end of header */
        if (strcmp(hl.name, "$$") == 0) {
            return SCRC_OK;
        } else if (strcmp(hl.name, "Status") == 0) {
            char *end;
            long int status;
            errno = 0;

            if (hl.value == NULL)
                return SCRC_HEADER_ERROR;

            status = strtol(hl.value, &end, 10);

            if (errno == ERANGE)
                return SCRC_OUT_OF_RANGE;

            if (*end != '\0')
                return SCRC_PROTOCOL_ERROR;

            if (status != 0)
                return status;
        } else if (strcmp(hl.name, "Body") == 0) {
            if (hl.value == NULL)
                return SCRC_HEADER_ERROR;

            if (strcmp(hl.value, "Yes") == 0) {
                conn->body = true;
            } else if (strcmp(hl.value, "No") == 0) {

            } else {
                return SCRC_PROTOCOL_ERROR;
            }
        }
        /* Ignore unknown headers */
    }
}

/**
 * @brief Read one header line from socket (up to '\n')
 *
 * Reads up to HEADER_MAX bytes, finds ':' and '\n'. Modifies buffer
 * in-place: replaces ':' and '\n' with '\0'. Adjusts rpos to point
 * right after the line.
 *
 * @param conn Connection
 * @param hl   Output HeaderLine structure
 * @return     SCRC_OK on success, SCRC_HEADER_ERROR if line too long
 *
 * @note The line must fit in HEADER_MAX bytes.
 * @note hl->name and hl->value point into conn->rbuf. Valid until
 *       next refill.
 * @warning On error, rpos is restored to original position.
 */
static ScrcStatus
recv_header_line(ScrcConnection *conn, HeaderLine *hl) {
    static const size_t HEADER_MAX = BUFSZ / 2;
    char *begin;
    bool delim = false;
    ScrcStatus ret = recv_block(conn, 1, &begin);
    size_t len = 1;

    if (ret != SCRC_OK)
        return ret;

    if (*begin == ':' || *begin == '\n')
        return SCRC_HEADER_ERROR;

    for (;;) {
        char *p;

        if (++len >= HEADER_MAX)
            return SCRC_HEADER_TOO_LARGE;

        ret = recv_block(conn, 1, &p);
        if (ret != SCRC_OK)
            return ret;

        /* Find ':' in buffer */
        if (*p == ':') {
            *p = '\0';
            hl->name = trim(begin);
            begin = p + 1;
            delim = true;
        /* Find '\n' in buffer */
        } else if (*p == '\n') {
            *p = '\0';
            if (delim)
                hl->value = trim(begin);
            else
                hl->name = trim(begin), hl->value = NULL;
            break;
        }
    }

    return SCRC_OK;
}

/**
 * @brief Receive one row (or column) from server
 *
 * Reads command, size, and data. Data pointer is returned in *row.
 *
 * @param conn Connection
 * @param row  Output pointer to row data
 * @return     SCRC_OK on success, SCRC_END on end of rows,
 *             error code otherwise
 *
 * @pre row must not be NULL (not checked for performance reasons)
 *
 * @note The returned row pointer is valid until next refill.
 */
static ScrcStatus
recv_row(ScrcConnection *conn, ScrcRow *row) {
    ScrcCmd cmd;
    size_t size;
    char *p;
    ScrcStatus ret = recv_cmd(conn, &cmd);

    switch (cmd) {
        case SCRC_CMD_ROW: break;
        case SCRC_CMD_TABHEADER:
        case SCRC_CMD_TABDATA: return SCRC_PROTOCOL_ERROR;
        case SCRC_CMD_END: return SCRC_END;
        default: return SCRC_UNKNOWN_COMMAND;
    }

    ret = recv_block(conn, sizeof(size_t), &p);
    if (ret != SCRC_OK)
        return ret;

    memcpy(&size, p, sizeof(size_t));

    ret = recv_block(conn, size, &p);
    if (ret != SCRC_OK)
        return ret;

    *row = p;

    return SCRC_OK;
}

/**
 * @brief Receive binary command from server
 *
 * @param conn Connection
 * @param cmd  Output command
 * @return     SCRC_OK on success, error code otherwise
 *
 * @note Uses memcpy to handle unaligned access.
 */
static ScrcStatus
recv_cmd(ScrcConnection *conn, ScrcCmd *cmd) {
    char *p;

    ScrcStatus ret = recv_block(conn, sizeof(ScrcCmd), &p);
    if (ret != SCRC_OK)
        return ret;

    memcpy(cmd, p, sizeof(ScrcCmd));

    return SCRC_OK;
}

/**
 * @brief Receive at least `size` bytes and return pointer to them
 *
 * Ensures that `size` bytes are available in the buffer simultaneously.
 * If not, calls recv_refill() to read more data. Returns pointer to
 * the beginning of the block in the internal buffer.
 *
 * @param conn Connection
 * @param size Number of bytes to receive (must be <= BUFSZ)
 * @param p    Output pointer to received block
 * @return     SCRC_OK on success, SCRC_BUFFER_OVERFLOW if size > BUFSZ,
 *             error code otherwise
 *
 * @warning p must not be NULL (not checked for performance reasons).
 * @note The returned pointer is valid until next refill.
 * @note Advances rpos by `size`.
 */
static ScrcStatus
recv_block(ScrcConnection *conn, size_t size, char **p) {
    if (size > BUFSZ) {
        *p = NULL;
        return SCRC_BUFFER_OVERFLOW;
    }

    /* Ensure we have `size` bytes available */
    while (conn->rlen - conn->rpos < size) {
        ScrcStatus ret = recv_refill(conn);
        if (ret != SCRC_OK) {
            *p = NULL;
            return ret;
        }
    }

    *p = conn->rbuf + conn->rpos;
    conn->rpos += size;

    return SCRC_OK;
}

/**
 * @brief Refill the receive buffer
 *
 * Reads up to RESERVESZ bytes into the buffer. If the buffer is
 * nearly full, compacts it first by moving valid data to the beginning.
 *
 * @param conn Connection
 * @return     SCRC_OK on success,
 *             SCRC_RECV_ERROR on recv error,
 *             SCRC_CONNECTION_CLOSED on EOF
 *
 * @note Handles EINTR internally.
 * @note After compaction, rpos is reset to 0 and rlen is set to valid bytes.
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
    for (;;) {
        const ssize_t n = recv(conn->sockfd, conn->rbuf + conn->rlen, space, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;  /* Caller retries */

            return SCRC_RECV_ERROR;
        }
        if (n == 0) {
            return SCRC_CONNECTION_CLOSED;
        }

        conn->rlen += n;

        return SCRC_OK;
    }
}

/**
 * @brief Trims line
 *
 * Trims spaces in beginning and end of line. It considers end of line
 * '\n' or '\0' characters
 *
 * @note it edits line
 *
 * @param src Socuse string
 * @return Edited string
 */
static char *
trim(char *src) {
    char *c;

    if (src == NULL || *src == '\0')
        return src;

    /* Ignore beginning spaces */
    while (*src && isspace(*src))
        ++src;

    c = src;

    /* Roll till the end of line */
    while (*c && *c != '\n')
        ++c;

    /* The final character */
    --c;

    /* Roll till the first not space character */
    while (c != src && isspace(*c))
        --c;

    /* Ignore ending spaces */
    *(c + 1) = '\0';

    return src;
}

