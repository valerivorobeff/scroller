#include "worker.h"
#include "flog.h"
#include "memory.h"
#include "cmd.h"
#include "hquery.y.h"
#include "hquery.l.h"
#include <sys/socket.h>

int worker_main(Session *session);

int
worker_main(Session *session) {
    int ret = 0;
    const int client_fd = session->client_fd;
    FILE *fstream;
    Context *session_context = linear_context_create(MEMORY_PAGESZ *16);
    Cmd cmd;

    context_add_child(context_get_current(), session_context);
    context_switch(session_context);

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

    for (;;) {
        yyscan_t scanner;

        y1lex_init(&scanner);
        y1set_in(fstream, scanner);

        ret = y1parse(scanner, session, &cmd);

        cmd_reset(&cmd);
        y1lex_destroy(scanner);

        flog("ret: %i", ret);

        switch(ret) {
            case 0:
                flog("Query parsed successfully");
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
    }

    context_drop(session_context);

    /* Drop worker */
    close(client_fd);

    return ret;
}

