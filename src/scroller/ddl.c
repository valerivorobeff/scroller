#include "ddl.h"
#include "server.h"
#include "session.h"
#include "table.h"
#include "type.h"
#include "pagecache.h"
#include "sequence.h"
#include "array.h"

int create_user(const char *user);
int create_catalog(const char *catalog);
int create_schema(Session *session, const char *schema);
int create_table(Session *session, const char *schema, const char *tname, const Decl *decls);

int
create_user(const char *user) {
    int ret;
    Grid *header = pagecache_put_page(g_pagecache, g_server.system.user.header.full);
    Grid *data = pagecache_put_page(g_pagecache, g_server.system.user.data.full);
    uint16_t name_idx = htable_get_column_idx(header, "name");
    Titor row = table_alloc_row(header, data);

    ret = titor_put_datum(row, name_idx, make_char((char *)user));

    if (ret == 0)
        pagecache_flush(g_pagecache, g_server.system.user.data.full);

    return ret;
}

int
create_catalog(const char *catalog) {
    int ret;
    Grid *header = pagecache_put_page(g_pagecache, g_server.system.catalog.header.full);
    Grid *data = pagecache_put_page(g_pagecache, g_server.system.catalog.data.full);
    uint16_t name_idx = htable_get_column_idx(header, "name");
    Titor row = table_alloc_row(header, data);

    ret = titor_put_datum(row, name_idx, make_char((char *)catalog));

    if (ret == 0)
        pagecache_flush(g_pagecache, g_server.system.catalog.data.full);

    return ret; 
}

int
create_schema(Session *session, const char *schema) {
    int ret;
    Grid *header = pagecache_put_page(g_pagecache, g_server.system.schema.header.full);
    Grid *data = pagecache_put_page(g_pagecache, g_server.system.schema.data.full);
    uint16_t catalog_idx = htable_get_column_idx(header, "catalog");
    uint16_t schema_idx = htable_get_column_idx(header, "schema");
    Titor row = table_alloc_row(header, data);

    ret = titor_put_datum(row, catalog_idx, make_char((char *)session->catalog));

    if (ret)
        return ret;

    ret = titor_put_datum(row, schema_idx, make_char((char *)schema));

    if (ret == 0)
        pagecache_flush(g_pagecache, g_server.system.schema.data.full);

    return ret;
}

int
create_table(Session *session, const char *schema, const char *tname, const Decl *decls) {
    int ret;
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

    sequence_nextval(hsequence, sequence, &currval); /* Increment sequence */

    table = pagecache_put_page(g_pagecache, currval); /* Init header table */
    table = htable_init(table, PAGESZ, GT_FIXED);

    /* Add columns */
    for (int i = 0, ie = array_size(decls); i != ie; ++i) {
        const Decl *decl = &decls[i];
        htable_add_column(table, decl->name, decl->type, decl->size);
    }

    /* Add a new row of the new table into relation table */
    row = table_alloc_row(header, data);

    ret = titor_put_datum(row, catalog_idx, make_char((char *)session->catalog));
    ret |= titor_put_datum(row, schema_idx, make_char((char *)schema));
    ret |= titor_put_datum(row, relation_idx, make_char((char *)tname));
    ret |= titor_put_datum(row, header_gid_idx, make_bigint(currval));
    ret |= titor_put_datum(row, data_gid_idx, make_bigint(GID_UNDEF));

    if (ret == 0) {
        pagecache_flush(g_pagecache, g_server.system.sequence.data.full);
        pagecache_flush(g_pagecache, g_server.system.relation.data.full);
        pagecache_flush(g_pagecache, currval);
    }

    return ret;
}

