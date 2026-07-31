#include "ddl.h"
#include "server.h"
#include "table.h"
#include "pagecache.h"

int create_user(Server *server, const char *user);
int create_catalog(Server *server, const char *catalog);
int create_schema(Server *server, const char *schema);

int
create_user(Server *server, const char *user) {
    Grid *header = pagecache_put_page(g_pagecache, server->system.user.header.full);
    Grid *data = pagecache_put_page(g_pagecache, server->system.user.data.full);
    uint16_t name_idx = htable_get_column_idx(header, "name");
    Titor row = table_alloc_row(header, data);
    Cell cell = table_get_cell(row, name_idx);

    put_char(cell, user, 32);

    pagecache_flush(g_pagecache, server->system.user.data.full);

    return 0;
}

int
create_catalog(Server *server, const char *catalog) {
    Grid *header = pagecache_put_page(g_pagecache, server->system.catalog.header.full);
    Grid *data = pagecache_put_page(g_pagecache, server->system.catalog.data.full);
    uint16_t name_idx = htable_get_column_idx(header, "name");
    Titor row = table_alloc_row(header, data);
    Cell cell = table_get_cell(row, name_idx);

    put_char(cell, catalog, 32);

    pagecache_flush(g_pagecache, server->system.catalog.data.full);

    return 0;
}

int create_schema(Server *server, const char *schema) {
    Grid *header = pagecache_put_page(g_pagecache, server->system.schema.header.full);
    Grid *data = pagecache_put_page(g_pagecache, server->system.schema.data.full);
    uint16_t catalog_idx = htable_get_column_idx(header, "catalog");
    uint16_t schema_idx = htable_get_column_idx(header, "schema");
    Titor row = table_alloc_row(header, data);
    Cell cell = table_get_cell(row, catalog_idx);

    put_char(cell, server->catalog, 32);

    cell = table_get_cell(row, schema_idx);
    put_char(cell, schema, 32);

    pagecache_flush(g_pagecache, server->system.schema.data.full);

    return 0;
}

