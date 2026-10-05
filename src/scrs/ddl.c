#include "ddl.h"
#include "server.h"
#include "session.h"
#include "table.h"
#include "type.h"
#include "pagecache.h"
#include "sequence.h"
#include "array.h"
#include <assert.h>

ScrcStatus create_user(const char *user);
ScrcStatus create_catalog(const char *catalog);
ScrcStatus create_schema(Session *session, const char *schema);
ScrcStatus create_table(Session *session, const char *schema, const char *tname, const Decl *decls);

ScrcStatus
create_user(const char *user) {
    int ret;
    GidPair gp_user = g_server.system.user;
    Grid *header = pagecache_put_page(g_pagecache, gp_user.header);
    uint16_t name_idx = htable_get_column_idx(header, "name");
    Titor row;

    assert(header && grid_idx_is_valid(name_idx));

    row = table_alloc_row(&gp_user);

    if (!titor_is_valid(row))
        return SCRS_SEQUENCE_OVERFLOW;

    ret = titor_put_datum(row, name_idx, make_char((char *)user));

    if (ret)
        return SCRS_DATUM_TYPE_MISMATCH;
    else
        pagecache_flush(g_pagecache, gp_user.data);

    return SCRS_OK;
}

ScrcStatus
create_catalog(const char *catalog) {
    int ret;
    int64_t currval;
    GidPair gp_catalog = g_server.system.catalog;
    Grid *header = pagecache_put_page(g_pagecache, gp_catalog.header);
    Grid *hsequence = pagecache_put_page(g_pagecache, g_server.system.sequence.header);
    Grid *sequence = pagecache_put_page(g_pagecache, g_server.system.sequence.data);
    Grid *catalog_sequence;
    Gid catalog_sequence_gid;

    uint16_t name_idx = htable_get_column_idx(header, "name");
    uint16_t tran_idx = htable_get_column_idx(header, "tran_sequence");
    Titor row;

    assert(header &&
        grid_idx_is_valid(name_idx) &&
        grid_idx_is_valid(tran_idx));

    row = table_alloc_row(&gp_catalog);

    if (!titor_is_valid(row))
        return SCRS_SEQUENCE_OVERFLOW;

    ret = titor_put_datum(row, name_idx, make_char((char *)catalog));

    if (ret)
        return SCRS_DATUM_TYPE_MISMATCH;

    /* Increment global Gid sequence */
    if (sequence_nextval(hsequence, sequence, &currval))
        return SCRS_SEQUENCE_OVERFLOW;

    /* Initialize catalog's transaction id sequence */
    catalog_sequence_gid = (Gid) { .parts = { .file_id = currval, .page = 0 } };
    catalog_sequence = pagecache_put_page(g_pagecache, catalog_sequence_gid);
    ret = sequence_init(
        hsequence,
        catalog_sequence,
        1,                  /* Min value */
        /* Max value */
#if (T_TRANID == I_SMALLINT)
        INT16_MAX
#elif (T_TRANID == T_INTEGER)
        INT32_MAX
#elif (T_TRANID == T_BIGINT)
        INT64_MAX
#else
    #error "Macro T_TRANID should be one of T_SMALLINT, T_INTEGER, T_BIGINT"
#endif
        ,
        1,                  /* Start transaction id with 1 (0 means no transaction) */
        1,                  /* Increment value */
        false);             /* No cycle */

    assert(ret == 0 && "Incorrect argument in sequence_init function");

    if (ret)
        return SCRS_UNKNOWN_SERVER_ERROR;

    /* Save catalog's transaction id sequence to gp_catalog */
    ret = titor_put_datum(row, tran_idx, make_bigint(currval));
    if (ret)
        return SCRS_DATUM_TYPE_MISMATCH;

    pagecache_flush(g_pagecache, gp_catalog.data);
    pagecache_flush(g_pagecache, catalog_sequence_gid);

    return SCRS_OK;
}

ScrcStatus
create_schema(Session *session, const char *schema) {
    int ret;
    GidPair gp_schema = g_server.system.schema;
    Grid *header = pagecache_put_page(g_pagecache, gp_schema.header);
    uint16_t catalog_idx = htable_get_column_idx(header, "catalog");
    uint16_t schema_idx = htable_get_column_idx(header, "schema");
    Titor row;

    assert(header &&
            grid_idx_is_valid(catalog_idx)
            && grid_idx_is_valid(schema_idx));

    row = table_alloc_row(&gp_schema);

    if (!titor_is_valid(row))
        return SCRS_SEQUENCE_OVERFLOW;

    ret = titor_put_datum(row, catalog_idx, make_char((char *)session->catalog));

    if (ret)
        return SCRS_DATUM_TYPE_MISMATCH;

    ret = titor_put_datum(row, schema_idx, make_char((char *)schema));

    if (ret)
        return SCRS_DATUM_TYPE_MISMATCH;
    else
        pagecache_flush(g_pagecache, gp_schema.data);

    return SCRS_OK;
}

ScrcStatus
create_table(Session *session, const char *schema, const char *tname, const Decl *decls) {
    int ret;
    int64_t currval;
    Gid new_gid;        /* Gid of the new table header */
    Grid *table;
    Grid *hsequence = pagecache_put_page(g_pagecache, g_server.system.sequence.header);
    Grid *sequence = pagecache_put_page(g_pagecache, g_server.system.sequence.data);

    GidPair gp_relation = g_server.system.relation;
    Grid *header = pagecache_put_page(g_pagecache, gp_relation.header);
    uint16_t catalog_idx = htable_get_column_idx(header, "catalog");
    uint16_t schema_idx = htable_get_column_idx(header, "schema");
    uint16_t relation_idx = htable_get_column_idx(header, "relation");
    uint16_t header_gid_idx = htable_get_column_idx(header, "header_gid");
    uint16_t data_gid_idx = htable_get_column_idx(header, "data_gid");
    uint16_t tail_gid_idx = htable_get_column_idx(header, "tail_gid");
    Titor row;

    assert(hsequence && sequence && header &&
            grid_idx_is_valid(catalog_idx) &&
            grid_idx_is_valid(schema_idx) &&
            grid_idx_is_valid(relation_idx) &&
            grid_idx_is_valid(header_gid_idx) &&
            grid_idx_is_valid(data_gid_idx) &&
            grid_idx_is_valid(tail_gid_idx));

    /* Increment sequence */
    if (sequence_nextval(hsequence, sequence, &currval))
        return SCRS_SEQUENCE_OVERFLOW;

    new_gid = (Gid) { .parts = { .file_id = currval, .page = 0 } };

    table = pagecache_put_page(g_pagecache, new_gid); /* Init header table */
    table = htable_init(table, PAGESZ, GL_FIXED, GC_MVCC);

    /* Add columns */
    for (int i = 0, ie = array_size(decls); i != ie; ++i) {
        const Decl *decl = &decls[i];
        if (htable_add_column(table, decl->name, decl->type, decl->size) == NULL)
            return SCRS_BAD_ALLOC;
    }

    /* Add a new row of the new table into relation table */
    row = table_alloc_row(&gp_relation);

    if (!titor_is_valid(row))
        return SCRS_SEQUENCE_OVERFLOW;

    ret = titor_put_datum(row, catalog_idx, make_char((char *)session->catalog));
    ret |= titor_put_datum(row, schema_idx, make_char((char *)schema));
    ret |= titor_put_datum(row, relation_idx, make_char((char *)tname));
    ret |= titor_put_datum(row, header_gid_idx, make_bigint(currval));
    ret |= titor_put_datum(row, data_gid_idx, make_bigint(GID_UNDEF));
    ret |= titor_put_datum(row, tail_gid_idx, make_bigint(GID_UNDEF));

    if (ret)
        return SCRS_DATUM_TYPE_MISMATCH;

    pagecache_flush(g_pagecache, g_server.system.sequence.data);
    pagecache_flush(g_pagecache, gp_relation.data);
    pagecache_flush(g_pagecache, new_gid);

    return SCRS_OK;
}

