#ifndef _TCP_H_
#define _TCP_H_

#include "server.h"

int tcp_init(Server *server);
int tcp_run(Server *server);
int tcp_destroy(Server *server);

#endif /* _TCP_H_ */

