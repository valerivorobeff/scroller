#ifndef _SERVER_H_
#define _SERVER_H_

typedef struct Server {
    int server_fd;
    int client_fd;
} Server;

int server_init(const char *path, Server *server);
int server_run(Server *server);
int server_drop(Server *server);

#endif /* _SERVER_H_ */
