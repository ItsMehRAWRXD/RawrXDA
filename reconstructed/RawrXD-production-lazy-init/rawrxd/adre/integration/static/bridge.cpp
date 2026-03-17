#include "../core/fact.h"

// Converts Disassembler output into Facts
Fact makeInstructionFact(uint64_t addr, const std::string& asmText) {
    return {
        addr,
        FactType::Instruction,
        asmText,
        {90}
    };
}
