#ifndef _DDL_H_
#define _DDL_H_

typedef struct Session Session;

typedef struct Decl {
    const char *name;
    const char *type;
} Decl;

int create_user(const char *user);
int create_catalog(const char *catalog);
int create_schema(Session *session, const char *schema);
int create_table(Session *session, const char *schema, const char *tname, const Decl *decls);

#endif /* _DDL_H_ */

