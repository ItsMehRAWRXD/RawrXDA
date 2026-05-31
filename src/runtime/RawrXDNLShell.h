#pragma once
#include <string>

namespace RawrXD::Runtime {

extern "C" uint32_t NLShell_ValidateCommand(const char* cmd, uint64_t len);

class RawrXDNLShell {
public:
    static RawrXDNLShell& instance();
    uint32_t validateCommand(const std::string& cmd);
};

} // namespace RawrXD::Runtime
