#include "flog.h"
#include <stdarg.h>
#include <stdlib.h>

static FILE *out = NULL;
static FILE *err = NULL;

#define check_out() if (out == NULL) out = stdout
#define check_err() if (err == NULL) err = stderr

void
flog_init_default() {
    out = stdout;
    err = stderr;
}

void
flog_init(FILE *fout, FILE *ferr) {
    out = fout;
    err = ferr;
}

void
flog_setf(FILE *f) {
    out = f;
}

void
ferr_setf(FILE *f) {
    err = f;
}

FILE *
flog_getf() {
    return out;
}

FILE *
ferr_getf() {
    return err;
}

void
flog_flush() {
    fflush(out);
}

void
ferr_flush() {
    fflush(err);
}

void
flog(const char *format, ...) {
    va_list args;
    check_out();

    va_start(args, format);

    vfprintf(out, format, args);
    fprintf(out, "\n");

    va_end(args);
}

void
ferr(const char *format, ...) {
    va_list args;
    check_err();

    va_start(args, format);

    vfprintf(err, format, args);
    fprintf(err, "\n");

    va_end(args);
}

void
ffatal(int code, const char *format, ...) {
    va_list args;
    check_err();

    va_start(args, format);

    vfprintf(err, format, args);
    fprintf(err, "\n");

    va_end(args);

    exit(code);
}

