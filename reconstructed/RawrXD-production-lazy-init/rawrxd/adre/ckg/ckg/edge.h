#pragma once
#include <cstdint>

enum class EdgeType {
    SUPPORTS,
    CONTRADICTS,
    DERIVES_FROM,
    USES
};

struct CKGEdge {
    uint64_t from;
    uint64_t to;
    EdgeType type;
};
