#ifndef _DML_H_
#define _DML_H_

#include "table.h"

typedef struct Session Session;
typedef struct Datum Datum;

int insert(Session *session, const char *schema, const char *table, const char **names, const Datum *values);
int dml_select(Session *session, const char *schema, const char *table, const char **names, Titor *out);

#endif /* _DML_H_ */

