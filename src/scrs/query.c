/**
 * @file query.c 
 * @brief query functions
 */

#include "query.h"

Query *query_init(Query *query);
int query_run(Query *query);
int query_drop(Query *query);


Query *
query_init(Query *query) {
    return query;
}

int
query_run(Query *query) {
    (void)query;

    return 0;
}

int
query_drop(Query *query) {
    (void)query;

    return 0;
}


