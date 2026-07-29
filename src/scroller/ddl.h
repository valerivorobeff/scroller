#ifndef _DDL_H_
#define _DDL_H_

typedef struct Server Server;

int create_user(Server *server, const char *user);

#endif /* _DDL_H_ */

