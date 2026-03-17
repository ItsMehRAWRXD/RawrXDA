#pragma once

#include <string>

struct Finding {
    std::string id;        // e.g., CL001, PY001
    std::string title;     // short title
    std::string severity;  // Low/Medium/High
    std::string file;      // file path
    int line = 0;          // 1-based
    std::string details;   // description / rationale
};
