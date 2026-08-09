/**
 * @file query.h
 * @brief query functions
 */


#ifndef _QUERY_H_
#define _QUERY_H_

/**
 * @brief Query struct
 */
typedef struct Query {

} Query;

/**
 * @brief Initializes query
 * @param query query struct to initialize
 * @return pointer to initialized Session of NULL if error
 */
Query *query_init(Query *query);

/**
 * @brief Runs query
 * @param query struct
 * @return 0 - if succeed, error code otherwise
 */
int query_run(Query *query);

/**
 * @brief Drops query
 * @param query struct
 * @return 0 - if succeed, error code otherwise
 */
int query_drop(Query *query);

#endif /* _QUERY_H_ */

