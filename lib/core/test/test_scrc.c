/*
 * In this example we will use TEST() and TEST_END() as the main test unit,
 * which consists of one test suite (but may consist of any) which in turn
 * includes and runs test cases.
 * Every test should start with TEST(name) and end with TEST_END() definition
 * where name defines the whole test name and is any allowed c identifier.
 * There can only be one TEST(name) - TEST_END() pair inside one test file.
 * Inside TEST(name) and TEST_END() there should be zero or more test suits.
 * Test suites begin with TEST_SUITE_BEGIN(name) and end with TEST_SUITE_END()
 * where name defines the test suite name and is any allowed c identifier.
 * Inside every test suite the should be zero or more test cases. Every test
 * case shoud begin with TEST_CASE(name) followed with a code block "{}" with
 * the test code inside and where name defines the test case name and is any
 * allowed c identifier.
 * Inside every test case there should be one or more TEST_REQUIRE(condition)
 * with the condition being any allowed c expression.
 * If the expression equals to 0, the test case is considered failed and
 * occurs in statistics. All other results are considered successful and
 * passed.
 */

#include "quin.h"
#include "scrc.h"
#include "grid.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>

/* ==============================
 *
 * Test helpers
 *
 ================================ */

/*
 * Mock server side: uses a real socketpair but bypasses connect().
 * We manually initialize ScrcConnection with a socket fd from socketpair.
 */

typedef struct MockServer {
    int client_fd;   /* Client side of socketpair */
    int server_fd;   /* Server side of socketpair */
    ScrcConnection *conn;
} MockServer;

static MockServer *
mock_server_create(void) {
    MockServer *ms = calloc(1, sizeof(MockServer));
    if (!ms)
        return NULL;

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        free(ms);
        return NULL;
    }

    ms->client_fd = fds[0];
    ms->server_fd = fds[1];

    /* Create a fake ScrcConnection with our socket */
    ms->conn = calloc(1, sizeof(ScrcConnection));
    if (!ms->conn) {
        close(fds[0]);
        close(fds[1]);
        free(ms);
        return NULL;
    }

    ms->conn->sockfd = ms->client_fd;
    ms->conn->rbuf = malloc(2 * 65536);
    if (!ms->conn->rbuf) {
        close(fds[0]);
        close(fds[1]);
        free(ms->conn);
        free(ms);
        return NULL;
    }

    ms->conn->status = SCRC_OK;

    return ms;
}

static void
mock_server_destroy(MockServer *ms) {
    if (!ms)
        return;

    if (ms->conn) {
        free(ms->conn->rbuf);
        free(ms->conn);
    }

    close(ms->client_fd);
    close(ms->server_fd);
    free(ms);
}

/* Write raw bytes to server side (client will read them) */
static void
mock_server_send(MockServer *ms, const void *data, size_t len) {
    const char *p = data;
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(ms->server_fd, p + total, len - total);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        total += n;
    }
}

/* Write header end marker */
static void
mock_server_send_header_end(MockServer *ms) {
    mock_server_send(ms, "$$\n", 3);
}

/* Write a header line */
static void
mock_server_send_header_line(MockServer *ms, const char *line) {
    mock_server_send(ms, line, strlen(line));
    mock_server_send(ms, "\n", 1);
}

/* Write ScrcCmd */
static void
mock_server_send_cmd(MockServer *ms, ScrcCmd cmd) {
    mock_server_send(ms, &cmd, sizeof(cmd));
}

/* Write size_t */
static void
mock_server_send_size(MockServer *ms, size_t size) {
    mock_server_send(ms, &size, sizeof(size));
}

/* Write a column */
static void
mock_server_send_column(MockServer *ms, const char *name, size_t offs, size_t size) {
    Column col;
    memset(&col, 0, sizeof(col));
    strncpy(col.name, name, sizeof(col.name) - 1);
    col.offs = offs;
    col.size = size;

    mock_server_send_cmd(ms, SCRC_CMD_ROW);
    mock_server_send_size(ms, sizeof(Column));
    mock_server_send(ms, &col, sizeof(Column));
}

/* Write a data row */
static void
mock_server_send_row(MockServer *ms, const void *data, size_t size) {
    mock_server_send_cmd(ms, SCRC_CMD_ROW);
    mock_server_send_size(ms, size);
    mock_server_send(ms, data, size);
}

/* Send full response header (Status + $$) */
static void
mock_server_send_full_header(MockServer *ms, int status) {
    char line[64];
    snprintf(line, sizeof(line), "Status: %d", status);
    mock_server_send_header_line(ms, line);
    mock_server_send_header_end(ms);
}

/* ==============================
 *
 * Test suite scrc
 *
 ================================ */

TEST(scrc)

    TEST_SUITE(header)

        TEST_CASE(status_ok) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            /* Send header + TABHEADER + TABDATA to complete a query */
            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            mock_server_destroy(ms);
        }

        TEST_CASE(status_error) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            /* Send header with server error status */
            mock_server_send_full_header(ms, 100);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == 100);  /* Server error code passed through */

            mock_server_destroy(ms);
        }

        TEST_CASE(unknown_header_ignored) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            /* Send unknown header, then status, then end */
            mock_server_send_header_line(ms, "Unknown: value");
            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            mock_server_destroy(ms);
        }

        TEST_CASE(empty_header) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            /* Send $$ only */
            mock_server_send_header_end(ms);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            mock_server_destroy(ms);
        }

        TEST_CASE(header_no_colon) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            /* Line without ':' should be treated as name with NULL value */
            mock_server_send_header_line(ms, "justname");
            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            mock_server_destroy(ms);
        }

    TEST_SUITE_END()

    TEST_SUITE(columns)

        TEST_CASE(single_column) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "id", 0, 4);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(ms->conn->columnsz == 1);
            TEST_CHECK(strcmp(ms->conn->columns[0].name, "id") == 0);
            TEST_CHECK(ms->conn->columns[0].offs == 0);
            TEST_CHECK(ms->conn->columns[0].size == 4);

            mock_server_destroy(ms);
        }

        TEST_CASE(multiple_columns) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "id", 0, 4);
            mock_server_send_column(ms, "name", 4, 32);
            mock_server_send_column(ms, "age", 36, 4);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(ms->conn->columnsz == 3);
            TEST_CHECK(strcmp(ms->conn->columns[0].name, "id") == 0);
            TEST_CHECK(strcmp(ms->conn->columns[1].name, "name") == 0);
            TEST_CHECK(strcmp(ms->conn->columns[2].name, "age") == 0);

            mock_server_destroy(ms);
        }

        TEST_CASE(reuse_columns_buffer) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            /* First query */
            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "id", 0, 4);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "q1");
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(ms->conn->columnsz == 1);

            size_t cap_before = ms->conn->columncap;

            /* Second query — should reuse buffer */
            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "x", 0, 4);
            mock_server_send_column(ms, "y", 4, 4);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ret = scrc_query(ms->conn, "q2");
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(ms->conn->columnsz == 2);
            TEST_CHECK(ms->conn->columncap == cap_before);  /* Reused */

            mock_server_destroy(ms);
        }

    TEST_SUITE_END()

    TEST_SUITE(rows)

        TEST_CASE(fetch_one_row) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "id", 0, 4);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            /* Send one row + END */
            int row_data = 42;
            mock_server_send_row(ms, &row_data, sizeof(row_data));
            mock_server_send_cmd(ms, SCRC_CMD_END);

            ScrcRow row;
            ret = scrc_fetch_row(ms->conn, &row);
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(row != NULL);
            TEST_CHECK(*(int *)row == 42);

            /* Next fetch should return NULL (end) */
            ret = scrc_fetch_row(ms->conn, &row);
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(row == NULL);

            mock_server_destroy(ms);
        }

        TEST_CASE(fetch_multiple_rows) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "id", 0, 4);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            /* Send 3 rows + END */
            for (int i = 0; i < 3; i++) {
                int row_data = i * 10;
                mock_server_send_row(ms, &row_data, sizeof(row_data));
            }
            mock_server_send_cmd(ms, SCRC_CMD_END);

            /* Fetch and verify */
            ScrcRow row;
            for (int i = 0; i < 3; i++) {
                ret = scrc_fetch_row(ms->conn, &row);
                TEST_CHECK(ret == SCRC_OK);
                TEST_CHECK(row != NULL);
                TEST_CHECK(*(int *)row == i * 10);
            }

            ret = scrc_fetch_row(ms->conn, &row);
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(row == NULL);

            mock_server_destroy(ms);
        }

        TEST_CASE(fetch_cells_from_row) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "id", 0, 4);
            mock_server_send_column(ms, "name", 4, 8);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            /* Row: id=1, name="Alice" */
            struct {
                int id;
                char name[8];
            } row_data;
            row_data.id = 1;
            memset(row_data.name, 0, sizeof(row_data.name));
            strncpy(row_data.name, "Alice", sizeof(row_data.name) - 1);

            mock_server_send_row(ms, &row_data, sizeof(row_data));
            mock_server_send_cmd(ms, SCRC_CMD_END);

            ScrcRow row;
            ret = scrc_fetch_row(ms->conn, &row);
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(row != NULL);

            /* Extract cell 0 */
            ScrcCell cell;
            ret = scrc_fetch_cell(ms->conn, row, 0, &cell);
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(cell.size == 4);
            TEST_CHECK(*(int *)cell.data == 1);

            /* Extract cell 1 */
            ret = scrc_fetch_cell(ms->conn, row, 1, &cell);
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(cell.size == 8);
            TEST_CHECK(strcmp((char *)cell.data, "Alice") == 0);

            mock_server_destroy(ms);
        }

        TEST_CASE(fetch_cell_out_of_range) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "id", 0, 4);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            int row_data = 42;
            mock_server_send_row(ms, &row_data, sizeof(row_data));
            mock_server_send_cmd(ms, SCRC_CMD_END);

            ScrcRow row;
            ret = scrc_fetch_row(ms->conn, &row);
            TEST_CHECK(ret == SCRC_OK);

            ScrcCell cell;
            ret = scrc_fetch_cell(ms->conn, row, 5, &cell);
            TEST_CHECK(ret == SCRC_OUT_OF_RANGE);
            TEST_CHECK(cell.data == NULL);
            TEST_CHECK(cell.size == 0);

            mock_server_destroy(ms);
        }

    TEST_SUITE_END()

    TEST_SUITE(protocol_errors)

        TEST_CASE(unexpected_cmd_in_tabheader) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);

            /* Send TABDATA instead of TABHEADER */
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_PROTOCOL_ERROR);

            mock_server_destroy(ms);
        }

        TEST_CASE(unexpected_cmd_in_tabdata) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "id", 0, 4);
            mock_server_send_cmd(ms, SCRC_CMD_END);

            /* Send TABHEADER instead of TABDATA */
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_PROTOCOL_ERROR);

            mock_server_destroy(ms);
        }

        TEST_CASE(unknown_command) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);

            /* Send unknown command */
            ScrcCmd bad_cmd = 0xDEADBEEF;
            mock_server_send_cmd(ms, bad_cmd);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_UNKNOWN_COMMAND);

            mock_server_destroy(ms);
        }

        TEST_CASE(connection_closed_in_header) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            /* Close server side immediately */
            close(ms->server_fd);
            ms->server_fd = -1;

            ScrcStatus ret = scrc_query(ms->conn, "test");
            /* May return SEND_ERROR (if send fails) or CONNECTION_CLOSED (if recv fails) */
            TEST_CHECK(ret == SCRC_SEND_ERROR || ret == SCRC_CONNECTION_CLOSED);

            mock_server_destroy(ms);
        }

        TEST_CASE(connection_closed_during_recv) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            /* Start query in background? Нет, блокирующий. */

            /* Проще: закрыть после того, как клиент отправил данные */
            /* Но send_query блокируется... */

            /* Альтернатива: закрыть server_fd, но не client_fd */
            shutdown(ms->server_fd, SHUT_WR);  /* Только запись */

            ScrcStatus ret = scrc_query(ms->conn, "test");
            /* send пройдёт (буфер), recv вернёт EOF */
            TEST_CHECK(ret == SCRC_CONNECTION_CLOSED);

            mock_server_destroy(ms);
        }

    TEST_SUITE_END()

    TEST_SUITE(params)

        TEST_CASE(null_query) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            ScrcStatus ret = scrc_query(ms->conn, NULL);
            TEST_CHECK(ret == SCRC_INCORRECT_PARAM);

            mock_server_destroy(ms);
        }

        TEST_CASE(null_row) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            ScrcStatus ret = scrc_fetch_row(ms->conn, NULL);
            TEST_CHECK(ret == SCRC_INCORRECT_PARAM);

            mock_server_destroy(ms);
        }

        TEST_CASE(null_cell) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            ScrcStatus ret = scrc_fetch_cell(ms->conn, NULL, 0, NULL);
            TEST_CHECK(ret == SCRC_INCORRECT_PARAM);

            mock_server_destroy(ms);
        }

    TEST_SUITE_END()

    TEST_SUITE(buffer)

        TEST_CASE(large_row_within_buffer) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "data", 0, 1024);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            /* 1024-byte row */
            char row_data[1024];
            for (int i = 0; i < 1024; i++)
                row_data[i] = (char)(i & 0xFF);

            mock_server_send_row(ms, row_data, sizeof(row_data));
            mock_server_send_cmd(ms, SCRC_CMD_END);

            ScrcRow row;
            ret = scrc_fetch_row(ms->conn, &row);
            TEST_CHECK(ret == SCRC_OK);
            TEST_CHECK(row != NULL);
            TEST_CHECK(memcmp(row, row_data, 1024) == 0);

            mock_server_destroy(ms);
        }

        TEST_CASE(many_small_rows) {
            MockServer *ms = mock_server_create();
            TEST_REQUIRE(ms != NULL);

            mock_server_send_full_header(ms, 0);
            mock_server_send_cmd(ms, SCRC_CMD_TABHEADER);
            mock_server_send_column(ms, "n", 0, 4);
            mock_server_send_cmd(ms, SCRC_CMD_END);
            mock_server_send_cmd(ms, SCRC_CMD_TABDATA);

            ScrcStatus ret = scrc_query(ms->conn, "test");
            TEST_CHECK(ret == SCRC_OK);

            /* Send 85 small rows */
            /* socketpair buffer is limited and can't hold more than ~64K, so 85 is compromise */
            for (int i = 0; i < 85; i++) {
                mock_server_send_row(ms, &i, sizeof(i));
            }
            mock_server_send_cmd(ms, SCRC_CMD_END);

            /* Read all */
            ScrcRow row;
            int count = 0;
            while ((ret = scrc_fetch_row(ms->conn, &row)) == SCRC_OK && row != NULL) {
                TEST_CHECK(*(int *)row == count);
                count++;
            }
            TEST_CHECK(count == 85);

            mock_server_destroy(ms);
        }

    TEST_SUITE_END()

TEST_END()

