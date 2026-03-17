#pragma once
#include "ids.h"
#include "confidence.h"
#include <string>

enum class FactType {
    Instruction,
    CFGEdge,
    MemoryAccess,
    Import,
    String,
    RuntimeTrace
};

struct Fact {
    FactID id;
    FactType type;
    std::string evidence;
    Confidence confidence;
};
