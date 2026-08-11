#include "dml.h"
#include "table.h"
#include "server.h"
#include "session.h"
#include "pagecache.h"
#include "sequence.h"
#include "array.h"
#include "flog.h"
#include <assert.h>

static GidPair find_relation(Session *session, const char *schema, const char *relation);

int
insert(Session *session, const char *schema, const char *table, const char **names, const char **values) {
    int ret = 0;
    GidPair gp_relation = find_relation(session, schema, table);
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

    header = pagecache_put_page(g_pagecache, gp_relation.header.full);

    for (size_t i = 0, ie = array_size(names); i != ie; ++i) {
        uint16_t column_idx = htable_get_column_idx(header, names[i]);

        if (column_idx == GRID_INVALID_IDX) {
            ferr("Unknown column '%s'", names[i]);
            return 1;
        }

        array_put(indices, column_idx);
    }

    /* Load table data or initialize if not found */
    if (gp_relation.data.full == GID_UNDEF) {
        Grid *hsequence = pagecache_put_page(g_pagecache, g_server.system.sequence.header.full);
        Grid *sequence = pagecache_put_page(g_pagecache, g_server.system.sequence.data.full);

        sequence_nextval(hsequence, sequence, (int64_t *)&gp_relation.data.full); /* Increment sequence */
        pagecache_flush(g_pagecache, g_server.system.sequence.data.full);

        data = pagecache_put_page(g_pagecache, gp_relation.data.full); /* Init data table */
        data = dtable_init(data, PAGESZ, GT_FIXED, header);
    } else
        data = pagecache_put_page(g_pagecache, gp_relation.data.full);

    row = table_alloc_row(header, data);

    for (size_t i = 0, ie = array_size(names); i != ie; ++i) {
        ret |= titor_put_datum(row, indices[i], make_char((char *)values[i]));
    }

    if (ret == 0)
        pagecache_flush(g_pagecache, gp_relation.data.full);

    return ret;
}

GidPair
find_relation(Session *session, const char *schema, const char *relation) {
    Grid *header = pagecache_put_page(g_pagecache, g_server.system.relation.header.full);
    Grid *data = pagecache_put_page(g_pagecache, g_server.system.relation.data.full);
    uint16_t catalog_idx = htable_get_column_idx(header, "catalog");
    uint16_t schema_idx = htable_get_column_idx(header, "schema");
    uint16_t relation_idx = htable_get_column_idx(header, "relation");
    uint16_t header_gid_idx = htable_get_column_idx(header, "header_gid");
    uint16_t data_gid_idx = htable_get_column_idx(header, "data_gid");

    for (Titor i = titor_init(header, data); titor_is_valid(i); titor_next(&i)) {
        const Datum dcatalog = titor_get_datum(i, catalog_idx);
        const Datum dschema = titor_get_datum(i, schema_idx);
        const Datum drelation = titor_get_datum(i, relation_idx);

        if (eq_character(dcatalog, make_char((char *)session->catalog)) &&
            eq_character(dschema, make_char((char *)schema)) &&
            eq_character(drelation, make_char((char *)relation))
            ) {
            const Datum header = titor_get_datum(i, header_gid_idx);
            const Datum data = titor_get_datum(i, data_gid_idx);
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

