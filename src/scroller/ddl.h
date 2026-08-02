#ifndef _DDL_H_
#define _DDL_H_

typedef struct Server Server;

typedef struct Decl {
    const char *name;
    const char *type;
} Decl;

int create_user(Server *server, const char *user);
int create_catalog(Server *server, const char *catalog);
int create_schema(Server *server, const char *schema);
int create_table(Server *server, const char *schema, const char *tname, const Decl *decls);

#endif /* _DDL_H_ */

