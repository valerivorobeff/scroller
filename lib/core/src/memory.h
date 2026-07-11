#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <malloc.h>

#define salloc(s) malloc(s)
#define srealloc(s) realloc(s)
#define sfree(s) free(s)

#endif /* _MEMORY_H_ */

