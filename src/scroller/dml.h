#ifndef _DML_H_
#define _DML_H_

typedef struct Session Session;
typedef struct Datum Datum;

int insert(Session *session, const char *schema, const char *table, const char **names, const Datum *values);

#endif /* _DML_H_ */

