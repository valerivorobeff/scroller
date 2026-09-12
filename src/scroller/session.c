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
int session_send(Session *session, const void *buf, size_t len);
int session_send_header_str(Session *session, const char *name, const char *value);
int session_send_header_int(Session *session, const char *name, long long int value);
int session_finish_header(Session *session);
int session_flush(Session *session);
static int send_block(int fd, const void *buf, size_t len);


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
            flog("Query parsed successfully");
            send(client_fd, "Status: Session finished!\n\n", 27, 0);
            break;

        case 1:
            ferr("Parser error");
            send(client_fd, "Status: Parse error\n\n", 21, 0);
            break;

        case 2:
            ferr("Parser memory exhaustion");
            send(client_fd, "Status: Memory error\n\n", 22, 0);
            break;

        default:
            ferr("Unknown parser error code: %i", ret);
            send(client_fd, "Status: Unknown error\n\n", 23, 0);
    }

    cmd_drop(&cmd);
    query_drop(&query);
    context_pop();
    context_drop(session_context);

    return ret;
}

int
session_send(Session *session, const void *buf, size_t len) {
    if (session->send_buf_idx + len >= SENDBUFSZ) {
        /* data length is more than left buffer size */
        const size_t head = SENDBUFSZ - session->send_buf_idx;
        const size_t remaining = len - head;
        const size_t full_blocks = remaining / SENDBUFSZ;
        const size_t tail = remaining % SENDBUFSZ;

        /* Buffer has some data already, append the new data to SENDBUFSZ and send */
        if (session->send_buf_idx > 0) {
            memcpy(session->send_buf + session->send_buf_idx, buf, head);
            if (send_block(session->client_fd, session->send_buf, SENDBUFSZ) != 0) {
                ferr("send() error: %s", strerror(errno));
                return 1;
            }

            buf += head;
        }

        /* Send other data blocks directly without copying */
        if (full_blocks) {
            const size_t block_bytes = full_blocks * SENDBUFSZ;
            if (send_block(session->client_fd, buf, block_bytes) != 0) {
                ferr("send() error: %s", strerror(errno));
                return 1;
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

    return 0;
}

int
session_send_header_str(Session *session, const char *name, const char *value) {
    int ret;
    ret = session_send(session, name, strlen(name));
    if (ret != 0)
        return ret;

    ret = session_send(session, ": ", 2);
    if (ret != 0)
        return ret;

    ret = session_send(session, value, strlen(value));
    if (ret != 0)
        return ret;

    return session_send(session, "\n", 1);
}

int
session_send_header_int(Session *session, const char *name, long long int value) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%lli", value);

    return session_send_header_str(session, name, buf);
}

int
session_finish_header(Session *session) {
    return session_send(session, "$$\n", 3);
}

int
session_flush(Session *session) {
    if (session->send_buf_idx) {
        if (send_block(session->client_fd, session->send_buf, session->send_buf_idx) != 0) {
            ferr("flush error: %s", strerror(errno));
            return 1;
        }

        session->send_buf_idx = 0;
    }

    return 0;
}

/**
 * @brief Send all data (handles partial sends)
 * @param fd Socket descriptor
 * @param buf Data to send
 * @param len Data length
 * @return 0 on success, 1 on error
 */
static int
send_block(int fd, const void *buf, size_t len) {
    size_t total = 0;

    while (total < len) {
        ssize_t n = send(fd, buf + total, len - total, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 1;
        }
        if (n == 0) {
            /* Connection is closed */
            errno = EPIPE;
            return 1;
        }
        total += n;
    }

    return 0;
}

