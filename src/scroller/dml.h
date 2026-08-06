#ifndef _DML_H_
#define _DML_H_

typedef struct Session Session;

int insert(Session *session, const char *schema, const char *table, const char **names, const char **values);

#endif /* _DML_H_ */

