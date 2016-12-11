#pragma once

namespace edan35 
{
    struct edge_tables {
        int edge_connections[256][16];
        int edge_table[256];
    };

    edge_tables get_edge_tables();

}