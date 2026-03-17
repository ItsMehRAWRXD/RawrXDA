#pragma once
/**
 * @file dast_bridge.hpp
 * @brief DAST (Dynamic AST) bridge — stub for Win32_IDE / refactoring.
 */
#include <string>
#include <vector>
#include <memory>

struct DastNode;
struct DastRange {
    int startLine = 0, startCol = 0, endLine = 0, endCol = 0;
};

class DastBridge {
public:
    static DastBridge* instance() {
        static DastBridge s_instance;
        return &s_instance;
    }
    DastNode* parse(const std::string& uri, const std::string& source) { (void)uri; (void)source; return nullptr; }
    void release(DastNode* node) { (void)node; }
    std::vector<DastRange> getRanges(DastNode* root) { (void)root; return {}; }
private:
    DastBridge() = default;
};
