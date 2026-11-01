#ifndef _TCL_H_
#define _TCL_H_

#include "scrc.h"

typedef struct Session Session;

ScrcStatus tcl_start(Session *session);
ScrcStatus tcl_commit(Session *session);
ScrcStatus tcl_rollback(Session *session);

#endif /* _DML_H_ */

