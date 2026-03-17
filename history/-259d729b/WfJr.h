#pragma once
#include "../core/ids.h"
#include <string>

enum class NodeType {
    Fact,
    Hypothesis,
    Function,
    Struct
};

struct CKGNode {
    uint64_t id;
    NodeType type;
    std::string label;
};
