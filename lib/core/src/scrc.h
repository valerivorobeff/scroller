#ifndef _SCRC_H_
#define _SCRC_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @file scrc.h
 * @brief Scroller client library
 *
 * Server-client response protocol:
 *
 * 1. Header
 *      Header consists of lines which consist of name and value end with '\n'.
 *      Name and value are separated with ':'.
 *      Name must be string.
 *      Value can consist of any printable characters. All the spaces at the
 *      beginning and end of value are trimmed.
 *      Value can be absent.
 *      example: "Status: 0\n".
 *      Header ends with string "$$\n".
 * 2. Binary response
 *      name                    size                   description
 * -----------------------------------------------------------------------------
 * 2.1. Table header
 *      SCRC_CMD_TABHEADER      ScrcCmd              - table header start
 *          SCRC_CMD_ROW        ScrcCmd              - column description start
 *          size                size_t               - column size
 *              Column struct   size mentioned above - struct Column
 *          SCRC_CMD_ROW        ScrcCmd              - column description start
 *          size                size_t               - next column description
 *              Column struct   size mentioned above - struct Column
 *          ....
 *      SCRC_CMD_END            ScrcCmd              - table header finish
 * 2.2. Table data
 *      SCRC_CMD_TABDATA        ScrcCmd              - table data start
 *          SCRC_CMD_ROW        ScrcCmd              - row data start
 *          size                size_t               - row data size
 *              row binary      size mentioned above - row binary data
 *          SCRC_CMD_ROW        ScrcCmd              - next row data start
 *          size                size_t               - next row data size
 *              row binary      size mentioned above - next row binary data
 *          ....
 *      SCRC_CMD_END            ScrcCmd              - table data finish
 * 2.3. Response end
 *      SCRC_CMD_END            ScrcCmd              - response end
 *
 */

typedef struct Column Column;

/**
 * @enum ScrcStatus
 * @brief Status codes returned by client functions
 *
 * Client errors: 0..99
 * Server errors: 100..199 (passed through from server)
 */
typedef enum ScrcStatus {
    SCRC_OK = 0,
    SCRC_END,
    SCRC_BAD_ALLOC,
    SCRC_NO_HOST,
    SCRC_UNKNOWN_HOST,
    SCRC_INCORRECT_PORT,
    SCRC_NO_USER,
    SCRC_SOCKET_ERROR,
    SCRC_CONNECTION_ERROR,
    SCRC_CONNECTION_CLOSED,
    SCRC_SEND_ERROR,
    SCRC_RECV_ERROR,
    SCRC_PROTOCOL_ERROR,
    SCRC_HEADER_ERROR,
    SCRC_HEADER_TOO_LARGE,
    SCRC_UNKNOWN_COMMAND,
    SCRC_BUFFER_OVERFLOW,
    SCRC_INCORRECT_PARAM,
    SCRC_OUT_OF_RANGE
} ScrcStatus;

/**
 * @enum ScrcCmd
 * @brief Binary protocol commands
 */
typedef enum ScrcCmd : uint32_t {
    SCRC_CMD_TABHEADER = 1, /**< Start of table header (columns) */
    SCRC_CMD_TABDATA,       /**< Start of table data (rows) */
    SCRC_CMD_ROW,           /**< Start of row */
    SCRC_CMD_END            /**< End of tabheader or tabdata */
} ScrcCmd;

/**
 * @struct ScrcConnection
 * @brief Client connection state
 *
 * Holds all state for a single connection: socket, parameters, column
 * metadata, and receive buffer.
 */
typedef struct ScrcConnection {
    ScrcStatus status;  /**<  Last operation status*/
    char *host;         /**< Server hostname or IP */
    int port;           /**< Server port */
    char *user;         /**< Username */
    char *catalog;      /**< Catalog name */
    int sockfd;         /**< Socket file descriptor */

    Column *columns;    /**< Array of column definitions */
    size_t columnsz;    /**< Number of columns in current result */
    size_t columncap;   /**< Allocated capacity of columns array */

    /* Receive buffer */
    char   *rbuf;       /**< Receive buffer (FULLSZ bytes) */
    size_t  rlen;       /**< Valid bytes in buffer */
    size_t  rpos;       /**< Read position in buffer */
} ScrcConnection;

/**
 * @typedef ScrcRow
 * @brief Opaque pointer to a row's binary data
 *
 * Valid until next call to scrc_fetch_row() or scrc_query().
 */
typedef void *ScrcRow;

/**
 * @struct ScrcCell
 * @brief A single cell (column value) within a row
 *
 */
typedef struct ScrcCell {
    const void *data;   /**< Pointer to cell data (not null-terminated) */
    size_t size;        /**< Size of cell data in bytes */
} ScrcCell;

/**
 * @brief Connect to server and perform handshake
 *
 * Creates a new connection, resolves hostname, connects to the server,
 * and sends the initial header with user and catalog information.
 *
 * @param host    Server hostname or IP address (must not be NULL)
 * @param port    Server port (1-65535)
 * @param user    Username for authentication (must not be NULL)
 * @param catalog Catalog name (can be NULL)
 * @return        Pointer to initialized connection, or NULL on bad allocation
 *
 * @note Even on connection failure, the returned connection is valid
 *       (non-NULL) and its status field contains the error code.
 *       Even if connection failed but function returnd not null,
 *       ScrcConnection struct must be freed with scrc_close function.
 * @warning Always check conn->status after calling this function.
 *
 * @code
 * ScrcConnection *conn = scrc_connect("localhost", 8080, "user", "catalog");
 * if (conn->status != SCRC_OK) {
 *     fprintf(stderr, "Connection failed: %d\n", conn->status);
 *     scrc_close(conn);
 *     return 1;
 * }
 * @endcode
 *
 * @see scrc_close()
 * @see scrc_query()
 */
ScrcConnection *scrc_connect(const char *host, int port, const char *user,
        const char *catalog);

/**
 * @brief Close connection and free all associated resources
 *
 * Closes the socket (if open), frees host, user, catalog, columns,
 * and the receive buffer.
 *
 * @param conn Pointer to connection (can be NULL, in which case nothing happens)
 *
 * @note Safe to call with NULL.
 * @warning After this call, the pointer is invalid and must not be used.
 */
void scrc_close(ScrcConnection *conn);

/**
 * @brief Returns status description of connection status
 *
 * @param conn Pointer to connection (can be NULL, in which case NULL is returned)
 * @return c-string description of connection status
 */
const char *scrc_error(ScrcConnection *conn);
    
/**
 * @brief Send query to server and receive response header
 *
 * Sends the query, reads response header, reads table header (columns),
 * and prepares for row fetching. Must be called before scrc_fetch_row().
 *
 * @param conn  Pointer to connection
 * @param query SQL query string (must not be NULL)
 * @return      SCRC_OK on success, error code otherwise
 *
 * @pre conn->status == SCRC_OK
 * @pre Previous query must be fully read (all rows fetched)
 *
 * @note After success, use scrc_fetch_row() to read data rows.
 * @warning Query must not be NULL.
 *
 * @code
 * ScrcStatus ret = scrc_query(conn, "select * from users;");
 * if (ret != SCRC_OK) {
 *     fprintf(stderr, "Query failed: %d\n", ret);
 *     return 1;
 * }
 * @endcode
 *
 * @see scrc_fetch_row()
 * @see scrc_fetch_cell()
 */
ScrcStatus scrc_query(ScrcConnection *conn, const char *query);

/**
 * @brief Fetch next row from current result set
 *
 * Reads the next data row. Must be called repeatedly until it returns
 * SCRC_OK with *row == NULL (end of data).
 *
 * @param conn Pointer to connection
 * @param row  Output pointer to row data (set to NULL at end)
 * @return     SCRC_OK on success (including end of data),
 *             error code otherwise
 *
 * @pre scrc_query() must have been called successfully
 *
 * @note At end of data, returns SCRC_OK and sets *row = NULL.
 * @note The returned row pointer is valid until the next call to
 *       scrc_fetch_row() or scrc_query().
 *
 * @code
 * ScrcRow row;
 * while (scrc_fetch_row(conn, &row) == SCRC_OK && row != NULL) {
 *     ScrcCell cell;
 *     for (size_t i = 0; i < conn->columnsz; i++) {
 *         scrc_fetch_cell(conn, row, i, &cell);
 *         printf("%.*s ", (int)cell.size, (char *)cell.data);
 *     }
 *     printf("\n");
 * }
 * @endcode
 *
 * @see scrc_query()
 * @see scrc_fetch_cell()
 */
ScrcStatus scrc_fetch_row(ScrcConnection *conn, ScrcRow *row);

/**
 * @brief Extract cell from row by column index
 *
 * Returns a pointer to the cell data and its size. The data is valid
 * as long as the row is valid (until next scrc_fetch_row call).
 *
 * @param conn Pointer to connection
 * @param row  Row data (from scrc_fetch_row)
 * @param n    Column index (0-based)
 * @param cell Output cell structure
 * @return     SCRC_OK on success, SCRC_OUT_OF_RANGE if n >= columnsz
 *
 * @pre row != NULL
 * @pre n < conn->columnsz
 *
 * @note For column names, use conn->columns[n].name.
 * @note The returned data is not null-terminated — use cell.size.
 *
 * @code
 * ScrcCell cell;
 * if (scrc_fetch_cell(conn, row, 0, &cell) == SCRC_OK) {
 *     printf("Column 0: %.*s\n", (int)cell.size, (char *)cell.data);
 * }
 * @endcode
 *
 * @see scrc_fetch_row()
 */
ScrcStatus scrc_fetch_cell(ScrcConnection *conn, const ScrcRow row, size_t n,
        ScrcCell *cell);

#endif /* _SCRC_H_ */

