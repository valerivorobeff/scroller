#ifndef _DML_H_
#define _DML_H_

typedef struct Server Server;

int insert(Server *server, const char *schema, const char *table, const char **names, const char **values);

#endif /* _DML_H_ */

