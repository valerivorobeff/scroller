#ifndef _DML_H_
#define _DML_H_

#include "table.h"
#include "scrc.h"

typedef struct Session Session;
typedef struct Datum Datum;

ScrcStatus insert(Session *session, const char *schema, const char *table, const char **names, const Datum *values);
ScrcStatus dml_select(Session *session, const char *schema, const char *table, const char **names, Titor *out);

#endif /* _DML_H_ */

