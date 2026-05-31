#pragma once
#include <string>
#include <vector>

namespace RawrXD {
namespace Agent {

struct ValidationResult {
    bool ok;
    std::string error;
};

class DeterministicValidator {
public:
    static ValidationResult ValidateProposal(const std::string& proposal) {
        (void)proposal;
        return { true, "" };
    }
};

} // namespace Agent

// Legacy aliasing for components using the old namespace structure
namespace Agentic {
    using DeterministicValidator = ::RawrXD::Agent::DeterministicValidator;
}

} // namespace RawrXD
