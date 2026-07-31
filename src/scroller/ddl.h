#ifndef _DDL_H_
#define _DDL_H_

typedef struct Server Server;

int create_user(Server *server, const char *user);
int create_catalog(Server *server, const char *catalog);
int create_schema(Server *server, const char *schema);

#endif /* _DDL_H_ */

