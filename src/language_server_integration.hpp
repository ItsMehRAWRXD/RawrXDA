#pragma once
#include <string>
#include <vector>
#include <map>

struct Position {
    int line;
    int character;
};

struct Range {
    Position start;
    Position end;
};

struct DiagnosticRelatedInformation {
    struct Location {
        std::string uri;
        Range range;
    } location;
    std::string message;
};

struct TextEdit {
    Range range;
    std::string newText;
};

struct WorkspaceEdit {
    std::map<std::string, std::vector<TextEdit>> changes;
};

struct CodeAction {
    std::string title;
    std::string kind;
    WorkspaceEdit edit;
};

struct Diagnostic {
    Range range;
    int severity;
    std::string code;
    std::string source;
    std::string message;
    
    struct RelatedInfo {
        struct Edit {
             WorkspaceEdit changes;
        } edit;
    };
    
    // Simplified structure to match usage in lsp integration
    // usage: diag.relatedInformation.first().edit.changes.value(uri).first().newText
    // This implies a complex structure. I'll approximate it for explicit logic compilation.
    
    struct ComplexRelated {
        WorkspaceEdit edit;
    };
    
    std::vector<ComplexRelated> relatedInformation;
};
