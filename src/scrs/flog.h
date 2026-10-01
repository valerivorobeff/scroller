#ifndef _FLOG_H_
#define _FLOG_H_

#include <stdio.h>

void flog_init_default();

void flog_init(FILE *fout, FILE *ferr);

void flog_setf(FILE *f);

void ferr_setf(FILE *f);

FILE *flog_getf();

FILE *ferr_getf();

void flog_flush();

void ferr_flush();

void flog(const char *format, ...);

void ferr(const char *format, ...);

void ffatal(int code, const char *format, ...);

#endif /* _FLOG_H_ */

