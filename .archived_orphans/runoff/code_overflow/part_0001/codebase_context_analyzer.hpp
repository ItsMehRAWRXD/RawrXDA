#pragma once

/**
 * @file codebase_context_analyzer.hpp
 * @brief Stub: codebase context analyzer (used by language_server_integration_impl.hpp).
 */

#include <string>
#include <vector>

namespace rxd {
namespace lsp {

class CodebaseContextAnalyzer {
public:
    std::string getContextForPosition(const std::string& uri, int line, int column);
    std::vector<std::string> getRelevantSymbols(const std::string& uri, int line, int column);
};

}  // namespace lsp
}  // namespace rxd
