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

Session *
session_init(Session *session) {
    memset(session, 0, sizeof(Session));
    session->client_fd = -1;

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
    yyscan_t scanner;
    Query query;
    Cmd cmd;

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

    if (y1lex_init(&scanner))
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

