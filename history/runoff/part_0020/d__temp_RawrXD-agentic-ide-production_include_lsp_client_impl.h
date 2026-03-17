#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>

// ============================================================================
// LSP CLIENT - Language Server Protocol Integration
// ============================================================================

class LSPClient {
public:
    struct Position {
        int line = 0;
        int character = 0;
    };

    struct Range {
        Position start;
        Position end;
    };

    struct Location {
        std::string uri;
        Range range;
    };

    struct Definition {
        std::string uri;
        int line = 0;
        int column = 0;
    };

    struct Reference {
        std::string uri;
        int line = 0;
        int column = 0;
    };

    struct Diagnostic {
        std::string message;
        int line = 0;
        int column = 0;
        std::string severity;  // "error", "warning", "info", "hint"
    };

    LSPClient();
    ~LSPClient();

    bool connect(const std::string& uri);
    bool disconnect();

    std::vector<std::string> complete(const std::string& file, int line, int column);
    Definition gotoDefinition(const std::string& file, int line, int column);
    std::vector<Reference> findReferences(const std::string& file, int line, int column);
    std::vector<Diagnostic> getDiagnostics(const std::string& file);

    void setCallback(std::function<void(const std::string&)> callback);

private:
    std::function<void(const std::string&)> m_callback;
};
