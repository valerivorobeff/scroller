#include "dml.h"
#include "table.h"
#include "server.h"
#include "session.h"
#include "type.h"
#include "pagecache.h"
#include "sequence.h"
#include "array.h"
#include "flog.h"
#include <stdbool.h>
#include <assert.h>

/**
 * @brief Finds relation my name
 * @param session current session
 * @param schema schema name
 * @param relation relation name
 * @param create_if_data_undef if true creates data grid for relation if it is set to GID_UNDEF
 *        in system relation table (typical for new tables without data)
 * @return GidPair of relation, if relation not found returns gid with header.full = GID_UNDEF and data.full = GID_UNDEF
 */
static GidPair find_relation(Session *session, const char *schema, const char *relation, bool create_if_data_undef);

int
insert(Session *session, const char *schema, const char *table, const char **names, const Datum *values) {
    int ret = 0;
    GidPair gp_relation = find_relation(session, schema, table, true);
    Grid *header;
    Grid *data;
    Titor row;
    uint16_t *indices = NULL;

    assert(array_size(names) == array_size(values));

    /* Exit if table header not found */
    if (gp_relation.header.full == GID_UNDEF) {
        ferr("Unknown table '%s'", table);
        return 1;
    }

    assert(gp_relation.data.full != GID_UNDEF);

    header = pagecache_put_page(g_pagecache, gp_relation.header.full);

    for (size_t i = 0, ie = array_size(names); i != ie; ++i) {
        uint16_t column_idx = htable_get_column_idx(header, names[i]);

        if (column_idx == GRID_INVALID_IDX) {
            ferr("Unknown column '%s'", names[i]);
            return 1;
        }

        array_put(indices, column_idx);
    }

    /* Load table data */
    data = pagecache_put_page(g_pagecache, gp_relation.data.full);

    row = table_alloc_row(header, data);

    for (size_t i = 0, ie = array_size(names); i != ie; ++i) {
        ret |= titor_put_datum(row, indices[i], values[i]);
    }

    if (ret == 0)
        pagecache_flush(g_pagecache, gp_relation.data.full);

    return ret;
}

GidPair
find_relation(Session *session, const char *schema, const char *relation, bool create_if_data_undef) {
    Grid *hrelation = pagecache_put_page(g_pagecache, g_server.system.relation.header.full);
    Grid *drelation = pagecache_put_page(g_pagecache, g_server.system.relation.data.full);
    uint16_t catalog_idx = htable_get_column_idx(hrelation, "catalog");
    uint16_t schema_idx = htable_get_column_idx(hrelation, "schema");
    uint16_t relation_idx = htable_get_column_idx(hrelation, "relation");
    uint16_t header_gid_idx = htable_get_column_idx(hrelation, "header_gid");
    uint16_t data_gid_idx = htable_get_column_idx(hrelation, "data_gid");

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

            assert(header.value.bigint != GID_UNDEF);

            if (create_if_data_undef && data.value.bigint == GID_UNDEF) {
                Grid *hsequence = pagecache_put_page(g_pagecache, g_server.system.sequence.header.full);
                Grid *sequence = pagecache_put_page(g_pagecache, g_server.system.sequence.data.full);
                Grid *htable = pagecache_put_page(g_pagecache, header.value.bigint);
                Grid *dtable;

                sequence_nextval(hsequence, sequence, &data.value.bigint); /* Increment sequence */
                pagecache_flush(g_pagecache, g_server.system.sequence.data.full);

                dtable = pagecache_put_page(g_pagecache, data.value.bigint); /* Init data table */
                dtable = dtable_init(dtable, PAGESZ, GT_FIXED, htable);
                pagecache_flush(g_pagecache, data.value.bigint);

                titor_put_datum(i, data_gid_idx, data);                     /* Save new data gid to system relation table */
                pagecache_flush(g_pagecache, g_server.system.relation.data.full);
            }

            return (GidPair){
                .header.full = header.value.bigint,
                .data.full = data.value.bigint
            };
        }
    }

    return (GidPair){
        .header.full = GID_UNDEF,
        .data.full = GID_UNDEF
    };
}

