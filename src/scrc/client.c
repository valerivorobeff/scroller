/**
 * @file client.c
 * @brief Scroller console client implementation
 */

#include "client.h"
#include "../scroller/server.h"
#include <ctype.h>

#define BUFFER_SIZE 65536
#define PROMPT "scroller> "

/* Forward declarations of helper functions */
static int send_all(int sockfd, const char *data, size_t len);
static int recv_all(int sockfd, char *buffer, size_t size);
static char *trim(char *str);

/**
 * @brief Initialize client
 */
int
client_create(Client *client, const char *host, int port, 
                const char *user, const char *catalog) {
    if (!client || !host || !user) {
        fprintf(stderr, "Error: Invalid parameters\n");
        return -1;
    }

    memset(client, 0, sizeof(Client));
    
    client->host = strdup(host);
    if (!client->host) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return -1;
    }

    client->port = port;
    client->user = strdup(user);
    if (!client->user) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        free(client->host);
        return -1;
    }

    if (catalog) {
        client->catalog = strdup(catalog);
        if (!client->catalog) {
            fprintf(stderr, "Error: Memory allocation failed\n");
            free(client->host);
            free(client->user);
            return -1;
        }
    }

    client->sockfd = -1;
    client->interactive = isatty(STDIN_FILENO);

    return 0;
}

/**
 * @brief Connect to server
 */
int
client_connect(Client *client) {
    struct sockaddr_in server_addr;
    struct hostent *host_info;

    /* Create socket */
    client->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->sockfd < 0) {
        perror("socket");
        return -1;
    }

    /* Resolve hostname */
    host_info = gethostbyname(client->host);
    if (!host_info) {
        fprintf(stderr, "Error: Unknown host '%s'\n", client->host);
        close(client->sockfd);
        client->sockfd = -1;
        return -1;
    }

    /* Set up server address */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(client->port);
    memcpy(&server_addr.sin_addr, host_info->h_addr, host_info->h_length);

    /* Connect to server */
    if (connect(client->sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(client->sockfd);
        client->sockfd = -1;
        return -1;
    }

    printf("Connected to %s:%d\n", client->host, client->port);
    return 0;
}

/**
 * @brief Send header (user and catalog) to server
 */
int
client_send_header(Client *client) {
    char header[BUFFER_SIZE];
    int len = 0;

    /* Build header: user: <username>\ncatalog: <catalog>\n */
    len += snprintf(header + len, sizeof(header) - len, "user: %s\n", client->user);

    if (client->catalog) {
        len += snprintf(header + len, sizeof(header) - len, "catalog: %s\n", client->catalog);
    }

    /* Add header end marker */
    len += snprintf(header + len, sizeof(header) - len, "$$\n");

    if (send_all(client->sockfd, header, len) < 0) {
        fprintf(stderr, "Error: Failed to send header\n");
        return -1;
    }

    return 0;
}

/**
 * @brief Send query to server
 */
int
client_send_query(Client *client, const char *query) {
    char buffer[BUFFER_SIZE];
    int len;

    if (!query || !*query) {
        return 0;
    }

    /* Build query with request end marker */
    len = snprintf(buffer, sizeof(buffer), "%s\n@\n", query);

    if (send_all(client->sockfd, buffer, len) < 0) {
        fprintf(stderr, "Error: Failed to send query\n");
        return -1;
    }

    return 0;
}

/**
 * @brief Receive response from server
 */
int
client_receive_response(Client *client, char *response, size_t size) {
    return recv_all(client->sockfd, response, size);
}

/**
 * @brief Run interactive mode
 */
int
client_run_interactive(Client *client) {
    char input[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    int ret;
    size_t len;

    printf("Scroller client (type 'exit' or 'quit' to quit)\n");

    /* Send header first */
    if (client_send_header(client) < 0) {
        return -1;
    }

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

        /* Remove trailing newline */
        len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
            len--;
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
        if (client_send_query(client, cmd) < 0) {
            break;
        }

        /* Receive response */
        ret = client_receive_response(client, response, sizeof(response));
        if (ret < 0) {
            break;
        }

        /* Print response */
        if (ret > 0) {
            printf("%s", response);
        }
    }

    return 0;
}

/**
 * @brief Run script mode (read from stdin)
 */
int
client_run_script(Client *client) {
    char input[BUFFER_SIZE];
    char response[BUFFER_SIZE];
    char query[BUFFER_SIZE] = "";
    int ret;
    int line_num = 0;
    int in_multiline = 0;

    /* Send header first */
    if (client_send_header(client) < 0) {
        return -1;
    }

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
        if (client_send_query(client, query) < 0) {
            fprintf(stderr, "Error sending query at line %d\n", line_num);
            return -1;
        }

        /* Receive response */
        ret = client_receive_response(client, response, sizeof(response));
        if (ret < 0) {
            fprintf(stderr, "Error receiving response at line %d\n", line_num);
            return -1;
        }

        /* Print response */
        if (ret > 0) {
            printf("%s", response);
        }

        query[0] = '\0';
    }

    return 0;
}

/**
 * @brief Close client connection
 */
void
client_free(Client *client) {
    if (client->sockfd >= 0) {
        close(client->sockfd);
        client->sockfd = -1;
    }

    free(client->host);
    free(client->user);
    free(client->catalog);
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
    Client client;
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
    if (client_create(&client, host, port, user, catalog) < 0) {
        return 1;
    }

    /* Redirect stdin if file specified */
    if (file) {
        if (freopen(file, "r", stdin) == NULL) {
            perror("freopen");
            client_free(&client);
            return 1;
        }
        client.interactive = 0;
    }

    /* Connect to server */
    if (client_connect(&client) < 0) {
        client_free(&client);
        return 1;
    }

    /* Run client */
    if (client.interactive) {
        ret = client_run_interactive(&client);
    } else {
        ret = client_run_script(&client);
    }

    /* Clean up */
    client_free(&client);

    return ret;
}

/**
 * @brief Send all data (wrapper for send)
 */
static int
send_all(int sockfd, const char *data, size_t len) {
    ssize_t sent = 0;
    ssize_t total = 0;

    while (total < (ssize_t)len) {
        sent = send(sockfd, data + total, len - total, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("send");
            return -1;
        }
        total += sent;
    }

    return 0;
}

/**
 * @brief Receive all data until server closes or buffer full
 */
static int
recv_all(int sockfd, char *buffer, size_t size) {
    ssize_t received;
    ssize_t total = 0;

    while (total < (ssize_t)(size - 1)) {
        received = recv(sockfd, buffer + total, size - 1 - total, 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("recv");
            return -1;
        }
        if (received == 0) {
            /* Server closed connection */
            break;
        }
        total += received;

        /* Check if we have a complete response (ends with \n\n) */
        if (total >= 2 && buffer[total - 1] == '\n' && buffer[total - 2] == '\n') {
            break;
        }
    }

    buffer[total] = '\0';
    return total;
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

