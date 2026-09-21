/**
 * @file session.c
 * @brief session functions
 */

#include "session.h"
#include "flog.h"
#include "memory.h"
#include "query.h"
#include "cmd.h"
#include "hquery.y.h"
#include "hquery.l.h"
#include <sys/socket.h>

Session *session_init(Session *session);
int session_run(Session *session);
int session_drop(Session *session);
ScrcStatus session_send(Session *session, const void *buf, size_t len);
ScrcStatus session_send_header_str(Session *session, const char *name, const char *value);
ScrcStatus session_send_header_int(Session *session, const char *name, long long int value);
ScrcStatus session_finish_header(Session *session);
ScrcStatus session_flush(Session *session);
static ScrcStatus send_block(int fd, const void *buf, size_t len);


Session *
session_init(Session *session) {
    session->client_fd = -1;
    session->user = NULL;
    session->catalog = NULL;
    session->send_buf_idx = 0;

    return session;
}

int
session_drop(Session *session) {
    return close(session->client_fd);
}

int
session_run(Session *session) {
    int ret = 0;
    const int client_fd = session->client_fd;
    FILE *fstream;
    Context *session_context = linear_context_create(MEMORY_PAGESZ *16);
    Context *flex_context = bump_context_create(MEMORY_PAGESZ *16);

    yyscan_t scanner;
    Query query;
    Cmd cmd;

    context_add_child(session_context, flex_context);

    context_add_child(context_get_current(), session_context);
    context_push(session_context);

    query_init(&query);
    cmd_init(&cmd);

    flog("[Child %d] Client connected\n", getpid());

    /* @todo: we have to create file stream because flex requires it
     * for file reading. But there is another way, that we can redefine
     * YY_INPUT macro and pass socket fd somehow via global variable,
     * but this method seems to be faster because we avoid creating file
     * stream, we should think over which method is the bast one, now the
     * first methood works.
     */
    fstream = fdopen(client_fd, "r+b");
    if (fstream == NULL)
        ffatal(1, "Cannot create fstream");

    setbuf(fstream, NULL);

    if (y1lex_init_extra(flex_context, &scanner))
        ffatal(1, "Cannot initialize scanner");

    y1set_in(fstream, scanner);

    ret = y1parse(scanner, session, &query, &cmd);

    y1lex_destroy(scanner);

    flog("ret: %i", ret);

    switch(ret) {
        case 0:
            flog("Session finished successfully");
            session_send_status(session, SCRS_OK);
            break;

        case 1:
            ferr("Parser error");
            session_send_status(session, SCRS_PARSER_ERROR);
            break;

        case 2:
            ferr("Parser memory exhaustion");
            session_send_status(session, SCRS_PARSER_MEMORY_EXHAUSTION);
            break;

        default:
            ferr("Unknown parser error code: %i", ret);
            session_send_status(session, SCRS_UNKNOWN_PARSER_ERROR);
    }

    cmd_drop(&cmd);
    query_drop(&query);
    context_pop();
    context_drop(session_context);

    return ret;
}

ScrcStatus
session_send(Session *session, const void *buf, size_t len) {
    if (session->send_buf_idx + len >= SENDBUFSZ) {
        /* data length is more than left buffer size */
        const size_t head = SENDBUFSZ - session->send_buf_idx;
        const size_t remaining = len - head;
        const size_t full_blocks = remaining / SENDBUFSZ;
        const size_t tail = remaining % SENDBUFSZ;
        ScrcStatus status;

        /* Buffer has some data already, append the new data to SENDBUFSZ and send */
        if (session->send_buf_idx > 0) {
            memcpy(session->send_buf + session->send_buf_idx, buf, head);
            status = send_block(session->client_fd, session->send_buf, SENDBUFSZ);
            if (status != SCRS_OK) {
                ferr("send() error: %s", strerror(errno));
                return status;
            }

            buf += head;
        }

        /* Send other data blocks directly without copying */
        if (full_blocks) {
            const size_t block_bytes = full_blocks * SENDBUFSZ;
            status =send_block(session->client_fd, buf, block_bytes);
            if (status != 0) {
                ferr("send() error: %s", strerror(errno));
                return status;
            }

            buf += block_bytes;
        }

        /* Copy the tail into buffer */
        memcpy(session->send_buf, buf, tail);
        session->send_buf_idx = tail;
    } else {
        /* All the data are less than buffer size, just copy them there */
        memcpy(session->send_buf + session->send_buf_idx, buf, len);
        session->send_buf_idx += len;
    }

    return SCRS_OK;
}

ScrcStatus
session_send_header_str(Session *session, const char *name, const char *value) {
    ScrcStatus ret;
    ret = session_send(session, name, strlen(name));
    if (ret != SCRS_OK)
        return ret;

    ret = session_send(session, ": ", 2);
    if (ret != SCRS_OK)
        return ret;

    ret = session_send(session, value, strlen(value));
    if (ret != SCRS_OK)
        return ret;

    return session_send(session, "\n", 1);
}

ScrcStatus
session_send_header_int(Session *session, const char *name, long long int value) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%lli", value);

    return session_send_header_str(session, name, buf);
}

ScrcStatus
session_finish_header(Session *session) {
    return session_send(session, "$$\n", 3);
}

ScrcStatus
session_flush(Session *session) {
    if (session->send_buf_idx) {
        ScrcStatus status;
        status = send_block(session->client_fd, session->send_buf, session->send_buf_idx);
        if (status != SCRC_OK) {
            ferr("flush error: %s", strerror(errno));
            return status;
        }

        session->send_buf_idx = 0;
    }

    return SCRS_OK;
}

/**
 * @brief Send all data (handles partial sends)
 * @param fd Socket descriptor
 * @param buf Data to send
 * @param len Data length
 * @return Session status
 */
static ScrcStatus
send_block(int fd, const void *buf, size_t len) {
    size_t total = 0;

    while (total < len) {
        ssize_t n = send(fd, buf + total, len - total, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return SCRS_SEND_ERROR;
        }
        if (n == 0) {
            /* Connection is closed */
            errno = EPIPE;
            return SCRS_SESSION_CLOSED;
        }
        total += n;
    }

    return SCRS_OK;
}

