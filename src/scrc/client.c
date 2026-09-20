/**
 * @file client.c
 * @brief Scroller console client implementation
 */

#include "scrc.h"
#include "../scroller/server.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 65536
#define PROMPT "scroller> "

/* Forward declarations of helper functions */
static char *trim(char *str);

/**
 * @brief Run interactive mode
 */
int
client_run_interactive(ScrcConnection *conn) {
    char input[BUFFER_SIZE];

    printf("Scroller client (type 'exit' or 'quit' to quit)\n");

    while (1) {
        /* Print prompt */
        printf(PROMPT);
        fflush(stdout);

        /* Read input */
        if (!fgets(input, sizeof(input), stdin)) {
            if (feof(stdin)) {
                printf("\n");
                break;
            }
            break;
        }

        /* Trim whitespace */
        char *cmd = trim(input);

        /* Check for exit */
        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            break;
        }

        /* Skip empty lines */
        if (cmd[0] == '\0') {
            continue;
        }

        /* Send query */
        if (scrc_query(conn, input) != SCRC_OK) {
            fprintf(stderr, "Error sending query: %s\n", scrc_error(conn));
            return -1;
        } else {
            /* Receive response */

            size_t cnt = 0;
            ScrcRow row;
            ScrcStatus status;

            while ((status = scrc_fetch_row(conn, &row)) == SCRC_OK && row != NULL) {
                ScrcCell cell;

                for (size_t i = 0; i < conn->columnsz; i++) {
                    scrc_fetch_cell(conn, row, i, &cell);
                }

                ++cnt;
            }

            if (status != SCRC_OK)
                fprintf(stderr, "Error receiving response: %s\n", scrc_error(conn));
            else
                printf("%li lines received\n", cnt);
        }
    }

    return 0;
}

/**
 * @brief Run script mode (read from stdin)
 */
int
client_run_script(ScrcConnection *conn) {
    char input[BUFFER_SIZE];
    char query[BUFFER_SIZE] = "";
    int line_num = 0;
    int in_multiline = 0;

    while (fgets(input, sizeof(input), stdin)) {
        line_num++;

        /* Remove trailing newline */
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
            len--;
        }

        char *cmd = trim(input);

        /* Skip empty lines */
        if (cmd[0] == '\0') {
            if (in_multiline) {
                /* Empty line in multiline - keep it */
                strcat(query, "\n");
            }
            continue;
        }

        /* Check for multiline (ends with ';') */
        if (cmd[strlen(cmd) - 1] != ';' && !in_multiline) {
            /* Single line query without semicolon - add it */
            strcpy(query, cmd);
            in_multiline = 0;
        } else if (cmd[strlen(cmd) - 1] != ';' && in_multiline) {
            /* Continue multiline */
            strcat(query, "\n");
            strcat(query, cmd);
            continue;
        } else {
            /* Query ends with ';' */
            if (in_multiline) {
                strcat(query, "\n");
                strcat(query, cmd);
                in_multiline = 0;
            } else {
                strcpy(query, cmd);
                in_multiline = 0;
            }
        }

        /* Send query */
        if (scrc_query(conn, query) != SCRC_OK) {
            fprintf(stderr, "Error sending query at line %d: %s\n", line_num, scrc_error(conn));
            return -1;
        } else {
            printf("Ok\n");
        }

        /* Receive response */

        query[0] = '\0';
    }

    return 0;
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
 * @brief Trim whitespace from string
 */
static char *
trim(char *str) {
    char *end;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) {
        str++;
    }

    if (*str == 0) {
        return str;
    }

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }

    /* Write new null terminator */
    *(end + 1) = '\0';

    return str;
}

