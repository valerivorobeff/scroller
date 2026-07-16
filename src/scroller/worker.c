#include "worker.h"
#include "flog.h"
#include "memory.h"
#include "hquery.y.h"
#include "hquery.l.h"
#include <sys/socket.h>

int worker_main(Server *server);

int
worker_main(Server *server) {
    int ret = 0;
    const int client_fd = server->client_fd;
    FILE *fstream;
    Context *session_context = linear_context_create(MEMORY_PAGESZ *16);
    context_add_child(context_get_current(), session_context);
    context_switch(session_context);

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

        yylex_init(&scanner);
        yyset_in(fstream, scanner);

        ret = yyparse(scanner);

        flog("ret: %i", ret);

        switch(ret) {
            case 0:
                flog("Query parsed successfully");
                send(client_fd, "Good\n", 5, 0);
                break;

            case 1:
                ferr("Parser error");
                send(client_fd, "Error\n", 6, 0);
                break;

            case 2:
                ferr("Parser memory exhaustion");
                send(client_fd, "Memory error!\n", 14, 0);
                break;

            default:
                ferr("Unknown parser error code: %i", ret);
                send(client_fd, "Unknown error code!\n", 20, 0);
        }

        yylex_destroy(scanner);
    }

    context_drop(session_context);

    /* Drop worker */
    close(client_fd);

    return ret;
}

