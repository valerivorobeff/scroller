#include "ddl.h"
#include "server.h"
#include "worker.h"
#include "table.h"
#include "pagecache.h"
#include "sequence.h"
#include "array.h"

int create_user(const char *user);
int create_catalog(const char *catalog);
int create_schema(Session *session, const char *schema);
int create_table(Session *session, const char *schema, const char *tname, const Decl *decls);

int
create_user(const char *user) {
    Grid *header = pagecache_put_page(g_pagecache, g_server.system.user.header.full);
    Grid *data = pagecache_put_page(g_pagecache, g_server.system.user.data.full);
    uint16_t name_idx = htable_get_column_idx(header, "name");
    Titor row = table_alloc_row(header, data);
    Cell cell = table_get_cell(row, name_idx);

    put_char(cell, user, 32);

    pagecache_flush(g_pagecache, g_server.system.user.data.full);

    return 0;
}

int
create_catalog(const char *catalog) {
    Grid *header = pagecache_put_page(g_pagecache, g_server.system.catalog.header.full);
    Grid *data = pagecache_put_page(g_pagecache, g_server.system.catalog.data.full);
    uint16_t name_idx = htable_get_column_idx(header, "name");
    Titor row = table_alloc_row(header, data);
    Cell cell = table_get_cell(row, name_idx);

    put_char(cell, catalog, 32);

    pagecache_flush(g_pagecache, g_server.system.catalog.data.full);

    return 0;
}

int
create_schema(Session *session, const char *schema) {
    Grid *header = pagecache_put_page(g_pagecache, g_server.system.schema.header.full);
    Grid *data = pagecache_put_page(g_pagecache, g_server.system.schema.data.full);
    uint16_t catalog_idx = htable_get_column_idx(header, "catalog");
    uint16_t schema_idx = htable_get_column_idx(header, "schema");
    Titor row = table_alloc_row(header, data);
    Cell cell = table_get_cell(row, catalog_idx);

    put_char(cell, session->catalog, 32);

    cell = table_get_cell(row, schema_idx);
    put_char(cell, schema, 32);

    pagecache_flush(g_pagecache, g_server.system.schema.data.full);

    return 0;
}

int
create_table(Session *session, const char *schema, const char *tname, const Decl *decls) {
    int64_t currval;
    Grid *table;
    Grid *hsequence = pagecache_put_page(g_pagecache, g_server.system.sequence.header.full);
    Grid *sequence = pagecache_put_page(g_pagecache, g_server.system.sequence.data.full);

    Grid *header = pagecache_put_page(g_pagecache, g_server.system.relation.header.full);
    Grid *data = pagecache_put_page(g_pagecache, g_server.system.relation.data.full);
    uint16_t catalog_idx = htable_get_column_idx(header, "catalog");
    uint16_t schema_idx = htable_get_column_idx(header, "schema");
    uint16_t relation_idx = htable_get_column_idx(header, "relation");
    uint16_t header_gid_idx = htable_get_column_idx(header, "header_gid");
    uint16_t data_gid_idx = htable_get_column_idx(header, "data_gid");
    Titor row;
    Cell cell;

    sequence_nextval(hsequence, sequence, &currval); /* Increment sequence */

    table = pagecache_put_page(g_pagecache, currval); /* Init header table */
    table = htable_init(table, PAGESZ, GT_FIXED);

    /* Add columns */
    for (int i = 0, ie = array_size(decls); i != ie; ++i) {
        const Decl *decl = &decls[i];
        htable_add_column(table, decl->name, T_CHAR, 32);
    }

    /* Add a new row of the new table into relation table */
    row = table_alloc_row(header, data);
    cell = table_get_cell(row, catalog_idx);
    put_char(cell, session->catalog, 32);

    cell = table_get_cell(row, schema_idx);
    put_char(cell, schema, 32);

    cell = table_get_cell(row, relation_idx);
    put_char(cell, tname, 32);

    cell = table_get_cell(row, header_gid_idx);
    put_bigint(cell, currval);

    cell = table_get_cell(row, data_gid_idx);
    put_bigint(cell, GID_UNDEF);

    pagecache_flush(g_pagecache, g_server.system.sequence.data.full);
    pagecache_flush(g_pagecache, g_server.system.relation.data.full);
    pagecache_flush(g_pagecache, currval);

    return 0;
}

