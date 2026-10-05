#include "dml.h"
#include "table.h"
#include "server.h"
#include "session.h"
#include "type.h"
#include "pagecache.h"
#include "sequence.h"
#include "tran.h"
#include "array.h"
#include "flog.h"
#include <stdbool.h>
#include <assert.h>

/**
 * @brief Finds relation my name
 * @param session current session
 * @param schema schema name
 * @param relation relation name
 * @param return_tail returns tail git instead of data gid of relation. It is suitable for insert
 * @param create_if_data_undef if true creates data grid for relation if it is set to GID_UNDEF
 *        in system relation table (typical for new tables without data)
 * @return GidPair of relation, if relation not found returns grid with header.full = GID_UNDEF and data.full = GID_UNDEF
 */
static GidPair find_relation(Session *session, const char *schema, const char *relation, bool return_tail, bool create_if_data_undef);

ScrcStatus
insert(Session *session, const char *schema, const char *table, const char **names, Datum *values) {
    GidPair gp_relation = find_relation(session, schema, table, true, true);
    Grid *header;
    Titor row;
    uint16_t *indices = NULL;

    assert(array_size(names) == array_size(values));

    /* Exit if table header not found */
    if (gp_relation.header.full == GID_UNDEF) {
        ferr("Unknown relation '%s'", table);
        return SCRS_UNKNOWN_RELATION;
    }

    assert(gp_relation.data.full != GID_UNDEF);

    /* Load table header */
    header = pagecache_put_page(g_pagecache, gp_relation.header);

    assert(header);

    /* Put mvcc columns */
    if (header->content == GC_MVCC) {
        /* Create transaction id if there is no
         * @note Because we actually need transaction id if only we update data
         * (INSERT, UPDATE, DELETE) in mvcc tables, to save catalog transaction
         * sequence from overflow  we assign transaction ids if only we update data */
        if (session->tran == NULL) {
            Grid *hcatalog = pagecache_put_page(g_pagecache, g_server.system.catalog.header);
            Grid *catalog = pagecache_put_page(g_pagecache, g_server.system.catalog.data);
            const uint16_t name_idx = htable_get_column_idx(hcatalog, "name");
            const uint16_t tran_sequence_idx = htable_get_column_idx(hcatalog, "tran_sequence");
            ssize_t tranid;
            Gid catalog_sequence_gid = { . full = GID_UNDEF };
            Grid *hsequence;
            Grid *catalog_sequence;

            assert(hcatalog && catalog &&
                    grid_idx_is_valid(name_idx) &&
                    grid_idx_is_valid(tran_sequence_idx));

            /* Find catalog transaction sequence gid */
            for (Titor i = titor_init(hcatalog, catalog); titor_is_valid(i); titor_next(&i)) {
                const Datum dcatalog = titor_get_datum(i, name_idx);

                if (eq_character(dcatalog, make_char((char *)session->catalog))) {
                    const Datum dsequence = titor_get_datum(i, tran_sequence_idx);
                    catalog_sequence_gid = (Gid) { .full = dsequence.value.bigint };

                    break;
                }
            }

            assert(catalog_sequence_gid.full != GID_UNDEF);

            hsequence = pagecache_put_page(g_pagecache, g_server.system.sequence.header);
            catalog_sequence = pagecache_put_page(g_pagecache, catalog_sequence_gid);

            if (sequence_nextval(hsequence, catalog_sequence, &tranid)) /* Increment catalog transaction sequence */
                return SCRS_SEQUENCE_OVERFLOW;

            session->tran = ihash_put_key(g_tran, tranid);  /* Put transaction into cache */
            if (session->tran == 0)
                return SCRS_TRANSACTION_CACHE_OVERFLOW;

            pagecache_flush(g_pagecache, catalog_sequence_gid);
        }

        /* Put mvcc columns */
        array_put(names, "*tmin");
        array_put(values, make_bigint(session->tran->key));  /* Transaction id */

        array_put(names, "*tmax");
        array_put(values, make_bigint(0));  /* Should be 0 */
    }

    /* Get indices of all the queried columns */
    for (size_t i = 0, ie = array_size(names); i != ie; ++i) {
        uint16_t column_idx = htable_get_column_idx(header, names[i]);

        if (!grid_idx_is_valid(column_idx)) {
            ferr("Unknown column '%s'", names[i]);
            return SCRS_UNKNOWN_COLUMN;
        }

        if (array_put(indices, column_idx) == NULL) {
            ferr("Bad alloc");
            return SCRS_BAD_ALLOC;
        }
    }

    /* Allocate a new row in the table, note that table_alloc_row can update gp_relation.data */
    row = table_alloc_row(&gp_relation);
    if (!titor_is_valid(row))
        return SCRS_SEQUENCE_OVERFLOW;

    for (size_t i = 0, ie = array_size(names); i != ie; ++i) {
        if (titor_put_datum(row, indices[i], values[i]) != 0)
            return SCRS_DATUM_TYPE_MISMATCH;
    }

    pagecache_flush(g_pagecache, gp_relation.data);

    return SCRS_OK;
}

ScrcStatus
dml_delete(Session *session, const char *schema, const char *table, Titor *out) {
    GidPair gp_relation = find_relation(session, schema, table, false, false);
    Grid *header;
    Grid *data;

    /* Exit if table header not found */
    if (gp_relation.header.full == GID_UNDEF) {
        ferr("Unknown relation '%s'", table);
        return SCRS_UNKNOWN_RELATION;
    }

    /* Load table header */
    header = pagecache_put_page(g_pagecache, gp_relation.header);

    assert(header);

    /* Check if table is empty */
    if (gp_relation.data.full == GID_UNDEF) {
        *out = titor_init(header, NULL);
        return SCRS_OK;
    }

    /* Load table data */
    data = pagecache_put_page(g_pagecache, gp_relation.data);

    *out = titor_init(header, data);

    return SCRS_OK;
}

ScrcStatus
dml_delete_row(Session *session, Titor row) {
    ScrcStatus ret = SCRS_OK;

    switch (row.data->content) {
        case GC_PURE: break;
        case GC_MVCC:
            int tpd_ret;
            const uint16_t tmax_idx = htable_get_column_idx(row.header, "*tmax");
            assert(grid_idx_is_valid(tmax_idx));

            tpd_ret = titor_put_datum(row, tmax_idx, make_bigint(session->tran->key));
            assert(tpd_ret == 0);

            //pagecache_flush(g_pagecache, catalog_sequence_gid);
            break;
    }

    return ret;
}

ScrcStatus
dml_select(Session *session, const char *schema, const char *table, const char **names, Titor *out) {
    GidPair gp_relation = find_relation(session, schema, table, false, false);
    Grid *header;
    Grid *data;
    uint16_t *indices = NULL;

    /* Exit if table header not found */
    if (gp_relation.header.full == GID_UNDEF) {
        ferr("Unknown relation '%s'", table);
        return SCRS_UNKNOWN_RELATION;
    }

    /* Load table header */
    header = pagecache_put_page(g_pagecache, gp_relation.header);

    assert(header);

    /* Check presence of all the queried columns */
    for (size_t i = 0, ie = array_size(names); i != ie; ++i) {
        uint16_t column_idx = htable_get_column_idx(header, names[i]);

        if (!grid_idx_is_valid(column_idx)) {
            ferr("Unknown column '%s'", names[i]);
            return SCRS_UNKNOWN_COLUMN;
        }

        if (array_put(indices, column_idx) == NULL) {
            ferr("Bad alloc");
            return SCRS_BAD_ALLOC;
        }
    }

    /* Check if table is empty */
    if (gp_relation.data.full == GID_UNDEF) {
        *out = titor_init(header, NULL);
        return SCRS_OK;
    }

    /* Load table data */
    data = pagecache_put_page(g_pagecache, gp_relation.data);

    *out = titor_init(header, data);

    return SCRS_OK;
}

GidPair
find_relation(Session *session, const char *schema, const char *relation, bool return_tail, bool create_if_data_undef) {
    Grid *hrelation = pagecache_put_page(g_pagecache, g_server.system.relation.header);
    Grid *drelation = pagecache_put_page(g_pagecache, g_server.system.relation.data);
    uint16_t catalog_idx = htable_get_column_idx(hrelation, "catalog");
    uint16_t schema_idx = htable_get_column_idx(hrelation, "schema");
    uint16_t relation_idx = htable_get_column_idx(hrelation, "relation");
    uint16_t header_gid_idx = htable_get_column_idx(hrelation, "header_gid");
    uint16_t data_gid_idx = htable_get_column_idx(hrelation, "data_gid");
    uint16_t tail_gid_idx = htable_get_column_idx(hrelation, "tail_gid");

    assert(hrelation && drelation &&
            grid_idx_is_valid(catalog_idx) &&
            grid_idx_is_valid(schema_idx) &&
            grid_idx_is_valid(relation_idx) &&
            grid_idx_is_valid(header_gid_idx) &&
            grid_idx_is_valid(data_gid_idx) &&
            grid_idx_is_valid(tail_gid_idx));

    for (Titor i = titor_init(hrelation, drelation); titor_is_valid(i); titor_next(&i)) {
        const Datum dcatalog = titor_get_datum(i, catalog_idx);
        const Datum dschema = titor_get_datum(i, schema_idx);
        const Datum drelation = titor_get_datum(i, relation_idx);

        if (eq_character(dcatalog, make_char((char *)session->catalog)) &&
            eq_character(dschema, make_char((char *)schema)) &&
            eq_character(drelation, make_char((char *)relation))
            ) {
            const Datum header = titor_get_datum(i, header_gid_idx);
            Datum data = titor_get_datum(i, data_gid_idx);
            Datum tail = titor_get_datum(i, tail_gid_idx);

            assert(header.value.bigint != GID_UNDEF);

            if (create_if_data_undef && data.value.bigint == GID_UNDEF) {
                Grid *hsequence = pagecache_put_page(g_pagecache, g_server.system.sequence.header);
                Grid *sequence = pagecache_put_page(g_pagecache, g_server.system.sequence.data);
                Grid *htable = pagecache_put_page(g_pagecache, ((Gid) { .full = header.value.bigint }));
                Grid *dtable;
                Gid new_gid;                                                /* Gid of the new table data */

                assert(hsequence && sequence && htable);

                if (sequence_nextval(hsequence, sequence, &data.value.bigint)) /* Increment sequence */
                    break;  /* SCRS_SEQUENCE_OVERFLOW */

                new_gid = (Gid) { .parts = { .file_id = data.value.bigint, .page = 0 } };

                pagecache_flush(g_pagecache, g_server.system.sequence.data);

                tail = data;

                dtable = pagecache_put_page(g_pagecache, new_gid); /* Init data table */
                assert(dtable);
                dtable = dtable_init(dtable, PAGESZ, GL_FIXED, GC_MVCC, htable);
                pagecache_flush(g_pagecache, new_gid);

                titor_put_datum(i, data_gid_idx, data);                      /* Save new data gid to system relation table */
                titor_put_datum(i, tail_gid_idx, data);                      /* Save new tail gid to system relation table */
                pagecache_flush(g_pagecache, g_server.system.relation.data);
            }

            return (GidPair){
                .header.full = header.value.bigint,
                .data.full = return_tail ? tail.value.bigint : data.value.bigint
            };
        }
    }

    return (GidPair){
        .header.full = GID_UNDEF,
        .data.full = GID_UNDEF
    };
}

