#pragma once

#include <string>
#include <vector>

namespace RawrXD {
namespace Security {

struct DastFinding {
    std::string source;
    std::string path;
    int line = 0;
    int column = 0;
    int severity = 2;
    std::string code;
    std::string message;
    std::string ruleId;
};

class DastBridge {
public:
    bool importReport(const std::string& reportPath, std::vector<DastFinding>& out) const;
    void reportToProblems(const std::vector<DastFinding>& findings) const;
};

} // namespace Security
} // namespace RawrXD

extern "C" {
#ifdef _WIN32
__declspec(dllexport)
#endif
void DastBridge_ImportReport(const char* reportPath);
}

