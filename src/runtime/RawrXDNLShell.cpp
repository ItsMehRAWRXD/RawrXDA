#include "RawrXDNLShell.h"
#include <iostream>

namespace RawrXD::Runtime {

RawrXDNLShell& RawrXDNLShell::instance() {
    static RawrXDNLShell instance;
    return instance;
}

uint32_t RawrXDNLShell::validateCommand(const std::string& cmd) {
    uint32_t risk = NLShell_ValidateCommand(cmd.c_str(), static_cast<uint64_t>(cmd.size()));
    if (risk > 50) {
        std::cerr << "[NLShell] WARNING: High Risk Command Detected: " << cmd << " (Risk: " << risk << ")" << std::endl;
    }
    return risk;
}

} // namespace RawrXD::Runtime
