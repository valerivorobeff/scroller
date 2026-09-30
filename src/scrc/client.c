/**
 * @file client.c
 * @brief Scroller console client implementation
 */

#include "scrc.h"
#include "grid.h"
#include "cell.h"
#include "../scroller/server.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 65536
#define PROMPT "scroller> "

/* Forward declarations of helper functions */
static ScrcStatus print_response_body(ScrcConnection *conn);
static bool line_has_semicolon(const char *line, int *in_comment, int *in_string, char *quote);
static inline bool is_exit_command(const char *line);

/**
 * @brief Run interactive mode
 *
 * Reads input line by line, accumulates until a semicolon is found,
 * then sends the query. Shows continuation prompt while accumulating.
 */
int
client_run_interactive(ScrcConnection *conn) {
    ScrcStatus status = SCRC_OK;
    char line[BUFFER_SIZE];
    char query[BUFFER_SIZE] = "";
    size_t query_len = 0;
    int in_comment = 0;
    int in_string = 0;
    char quote = 0;

    printf("Scroller client (type 'exit' or 'quit' to quit)\n");

    while (1) {
        /* Prompt: continuation or new */
        printf("%s", query_len == 0 ? PROMPT : "     -> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            break;
        }

        /* Check for exit only when no pending query */
        if (query_len == 0 && is_exit_command(line))
            break;

        size_t line_len = strlen(line);

        /* Append line to query buffer as-is */
        if (query_len + line_len >= sizeof(query)) {
            fprintf(stderr, "Query too large\n");
            query[0] = '\0';
            query_len = 0;
            continue;
        }
        memcpy(query + query_len, line, line_len);
        query_len += line_len;
        query[query_len] = '\0';

        /* Check if this line completes a query */
        if (line_has_semicolon(line, &in_comment, &in_string, &quote)) {
            /* Send accumulated query */
            status = scrc_query(conn, query);
            if (status != SCRC_OK) {
                fprintf(stderr, "Error: %s\n", scrc_error(conn));
                query[0] = '\0';
                query_len = 0;
                continue;
            }

            /* Print response */
            if (conn->body) {
                status = print_response_body(conn);
            } else {
                puts("Ok");
            }

            /* Reset buffer */
            query[0] = '\0';
            query_len = 0;
        }
    }

    return status;
}

/**
 * @brief Run script mode (read from stdin)
 *
 * Reads input line by line, accumulates until a semicolon is found
 * (outside comments and string literals), then sends the whole
 * accumulated query to the server.
 *
 * @param conn Connection
 * @return     SCRC_OK on success, error code otherwise
 */
int
client_run_script(ScrcConnection *conn) {
    ScrcStatus status = SCRC_OK;
    char line[BUFFER_SIZE];
    char query[BUFFER_SIZE] = "";
    size_t query_len = 0;
    int in_comment = 0;
    int in_string = 0;
    char quote = 0;

    while (fgets(line, sizeof(line), stdin)) {
        size_t line_len = strlen(line);

        /* Append line to query buffer as-is (preserve \n) */
        if (query_len + line_len >= sizeof(query)) {
            fprintf(stderr, "Query too large\n");
            return SCRC_BUFFER_OVERFLOW;
        }
        memcpy(query + query_len, line, line_len);
        query_len += line_len;
        query[query_len] = '\0';

        /* Check if this line completes a query */
        if (line_has_semicolon(line, &in_comment, &in_string, &quote)) {
            /* Send accumulated query */
            status = scrc_query(conn, query);
            if (status != SCRC_OK) {
                fprintf(stderr, "Error: %s\n", scrc_error(conn));
                return status;
            }

            /* Print response */
            if (conn->body) {
                status = print_response_body(conn);
            } else {
                puts("Ok");
            }

            /* Reset buffer */
            query[0] = '\0';
            query_len = 0;
        }
    }

    /* Send remaining buffer if not empty */
    if (query_len > 0) {
        /* Trim whitespace to check if anything is left */
        const char *p = query;
        while (*p && isspace((unsigned char)*p))
            p++;

        if (*p) {
            status = scrc_query(conn, query);
            if (status != SCRC_OK) {
                fprintf(stderr, "Error: %s\n", scrc_error(conn));
                return status;
            }

            if (conn->body) {
                status = print_response_body(conn);
            } else {
                puts("Ok");
            }
        }
    }

    return status;
}

/**
 * @brief Print usage help
 */
void
print_usage(const char *program) {
    printf("Usage: %s [options]\n\n", program);
    printf("Options:\n");
    printf("  -h <host>      Server hostname or IP (default: localhost)\n");
    printf("  -p <port>      Server port (default: 8080)\n");
    printf("  -u <user>      Username (required)\n");
    printf("  -c <catalog>   Catalog name\n");
    printf("  -f <file>      Read queries from file instead of stdin\n");
    printf("  --help         Show this help\n\n");
    printf("If no file is specified, runs in interactive mode.\n");
    printf("In interactive mode, type 'exit' or 'quit' to quit.\n");
    printf("Queries can be multiline, end with ';'.\n");
}

/**
 * @brief Main function
 */
int
main(int argc, char **argv) {
    ScrcConnection *conn;
    int port = DEFAULT_PORT;
    const char *host = "localhost";
    const char *user = NULL;
    const char *catalog = NULL;
    const char *file = NULL;
    int ret = 0;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            host = argv[++i];
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-u") == 0 && i + 1 < argc) {
            user = argv[++i];
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            catalog = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc) {
            file = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Validate required arguments */
    if (!user) {
        fprintf(stderr, "Error: Username (-u) is required\n\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Initialize client */
    conn = scrc_connect(host, port, user, catalog);
    if (conn == NULL) {
        perror("Cannot create socket");
        return 1;
    } else if (conn->status != SCRC_OK) {
        perror(scrc_error(conn));
        return conn->status;
    }

    /* Redirect stdin if file specified */
    if (file) {
        if (freopen(file, "r", stdin) == NULL) {
            perror("error freopen");
            scrc_close(conn);
            return 1;
        }
        ret = client_run_script(conn);
    } else
        ret = client_run_interactive(conn);

    /* Clean up */
    scrc_close(conn);

    return ret;
}

/**
 * @brief Prints response body
 */
static ScrcStatus
print_response_body(ScrcConnection *conn) {
    /* Receive response */

    size_t cnt = 0;
    size_t width = 0;
    ScrcRow row;
    ScrcStatus status;

    /* Print columns */
    for (size_t i = 0; i != conn->columnsz; ++i) {
        const Column *col = &conn->columns[i];
        const char *c = col->name;
        size_t sz;

        /* Calculate column size */
        switch (get_type_group(col->type)) {
            case TG_CHARACTER:
                sz = col->size;
                break;

            case TG_INTEGER:
                switch (col->type) {
                    case T_SMALLINT: sz = 5; break;
                    case T_INTEGER: sz = 10; break;
                    case T_BIGINT: sz = 19; break;
                    default: sz = 0; break;
                }
                break;

            default:
                sz = 0;
                break;
        }

        putchar('|');
        ++width;

        while(sz && *c) {
            putchar(*c);
            ++width;
            --sz;
            ++c;
        }

        while(sz--) {
            putchar(' ');
            ++width;
        }
    }

    puts("|");
    ++width;

    /* Wide horizontal line */
    for (size_t i = 0; i != width; ++i)
        putchar('-');

    puts("");

    /* Print values */
    while ((status = scrc_fetch_row(conn, &row)) == SCRC_OK && row != NULL) {
        ScrcCell cell;

        for (size_t i = 0; i < conn->columnsz; i++) {
            const Column *col = &conn->columns[i];
            scrc_fetch_cell(conn, row, i, &cell);

            switch (get_type_group(col->type)) {
                case TG_CHARACTER:
                    const char *c = cell.data;
                    size_t sz = col->size;

                    putchar('|');

                    while (sz--)
                        putchar(*c++);

                    break;

                case TG_INTEGER:
                    switch (col->type) {
                        case T_SMALLINT:
                            printf("|% 5i", get_smallint(cell.data));
                            break;

                        case T_INTEGER:
                            printf("|% 10i", get_integer(cell.data));
                            break;

                        case T_BIGINT:
                            printf("|% 19li", get_bigint(cell.data));
                            break;

                        default:
                    }

                    break;

                default:
                    puts("|--Unknown type--");
                    break;
            }
        }

        puts("|");

        ++cnt;
    }

    /* Wide horizontal line */
    for (size_t i = 0; i != width; ++i)
        putchar('-');

    puts("");

    if (status != SCRC_OK)
        fprintf(stderr, "Error receiving response: %s\n", scrc_error(conn));
    else
        printf("%li lines received\n", cnt);

    return status;
}

/**
 * @brief Check if line contains a semicolon outside comments/strings
 *
 * @param line       Line to check
 * @param in_comment Pointer to comment state (carried between lines)
 * @param in_string  Pointer to string state (carried between lines)
 * @param quote      Pointer to string quote char (carried between lines)
 * @return true if line contains terminating semicolon
 */
static bool
line_has_semicolon(const char *line, int *in_comment, int *in_string, char *quote) {
    for (const char *p = line; *p; p++) {
        char c = *p;

        if (*in_comment) {
            if (c == '\n') {
                *in_comment = 0;
            }
            continue;
        }

        if (*in_string) {
            if (c == '\\' && *(p + 1)) {
                p++;  /* Skip escaped char */
                continue;
            }
            if (c == *quote) {
                *in_string = 0;
            }
            continue;
        }

        /* Not in comment or string */
        if (c == '\'') {
            *in_string = 1;
            *quote = '\'';
        } else if (c == '"') {
            *in_string = 1;
            *quote = '"';
        } else if (c == '-' && *(p + 1) == '-') {
            *in_comment = 1;
            p++;  /* Skip second '-' */
        } else if (c == ';') {
            return true;
        }
    }

    return false;
}

/*
 * @brief Checks if line id an exit command not modifying line
 * @param line to check
 * @return true if line is exit command
 */
static inline bool
is_exit_command(const char *line) {
    while (isspace(*line))
        ++line;

    return (memcmp(line, "exit", 4) == 0 || memcmp(line, "quit", 4) == 0) &&
        (line[4] == '\n' || line[4] == '\0');
}

