#ifndef _WORKER_H_
#define _WORKER_H_

typedef struct Session {
    int client_fd;
    const char *user;
    const char *catalog;
} Session;

int worker_main(Session *session);

#endif /* _WORKER_H_ */

