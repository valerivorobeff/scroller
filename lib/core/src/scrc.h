#ifndef _SCRC_H_
#define _SCRC_H_

#include <stddef.h>
#include <stdint.h>

/**
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

typedef enum ScrcStatus {
    SCRC_OK = 0,
    SCRC_CONNECTED,
    SCRC_END,
    SCRC_BAD_ALLOC,
    SCRC_NO_HOST,
    SCRC_UNKNOWN_HOST,
    SCRC_NO_PORT,
    SCRC_INCORRECT_PORT,
    SCRC_NO_USER,
    SCRC_SOCKET_ERROR,
    SCRC_CONNECTION_ERROR,
    SCRC_CONNECTION_CLOSED,
    SCRC_SEND_ERROR,
    SCRC_RECV_ERROR,
    SCRC_PROTOCOL_ERROR,
    SCRC_UNKNOWN_COMMAND,
    SCRC_INCORRECT_COLUMNSZ,
    SCRC_OUT_OF_RANGE
} ScrcStatus;

typedef enum ScrcCmd : uint32_t {
    SCRC_CMD_TABHEADER = 1,
    SCRC_CMD_TABDATA,
    SCRC_CMD_ROW,       /**< Start of row */
    SCRC_CMD_END        /**< End of tabheader or tabdata */
} ScrcCmd;

typedef struct ScrcConnection {
    ScrcStatus status;
    char *host;         /**< Server hostname or IP */
    int port;           /**< Server port */
    char *user;         /**< Username */
    char *catalog;      /**< Catalog name */
    int sockfd;         /**< Socket file descriptor */

    Column *columns;
    size_t columnsz;
    size_t columncap;

    /* Receive buffer */
    char   *rbuf;       /**< Receive buffer (FULLSZ bytes) */
    size_t  rlen;       /**< Valid bytes in buffer */
    size_t  rpos;       /**< Read position in buffer */
} ScrcConnection;

typedef void *ScrcRow;

typedef struct ScrcCell {
    const void *data;
    size_t size;
} ScrcCell;

ScrcConnection *scrc_connect(const char *host, int port, const char *user,
        const char *catalog);

ScrcConnection *scrc_reconnect(ScrcConnection *conn, const char *host,
        int port, const char *user, const char *catalog);

void scrc_close(ScrcConnection *conn);

ScrcStatus scrc_query(ScrcConnection *conn, const char *query);

ScrcStatus scrc_fetch_row(ScrcConnection *conn, ScrcRow *row);

ScrcStatus scrc_fetch_cell(ScrcConnection *conn, const ScrcRow row, size_t n,
        ScrcCell **cell);

#endif /* _SCRC_H_ */

