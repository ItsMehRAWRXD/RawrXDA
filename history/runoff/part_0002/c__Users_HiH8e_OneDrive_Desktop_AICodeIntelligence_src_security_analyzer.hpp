#pragma once

#include "findings.hpp"

#include <string>
#include <vector>

class SecurityAnalyzer {
public:
    // language: "clike" | "python" | "unknown"
    std::vector<Finding> analyze(const std::string& language, const std::string& path, const std::string& content) const;
};
