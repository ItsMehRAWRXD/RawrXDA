#pragma once
#include <string>
#include <vector>
#include <map>

namespace RawrXD {
namespace IDE {

// Forward declaration to avoid circular include with SmartRewriteEngine.h
struct TextEditEntry {
    int startLine = 0;
    int startCol  = 0;
    int endLine   = 0;
    int endCol    = 0;
    std::string newText;
};

struct MultiFileChange {
    std::string filePath;
    std::string originalContent;
    std::string newContent;
    std::vector<TextEditEntry> edits;
};

struct MultiFilePlan {
    std::string planDescription;
    std::vector<MultiFileChange> changes;
    bool requiresRefactor;
};

class MultiFileRewriteEngine {
public:
    MultiFileRewriteEngine();
    
    // Analyze cross-file dependencies and generate coordinated plan
    MultiFilePlan planCoordinatedEdits(
        const std::string& userGoal,
        const std::vector<std::string>& targetFiles);
        
    // Apply changes as an atomic transaction
    bool applyPlan(const MultiFilePlan& plan);
    
    // Rollback if anything fails
    bool rollback(const MultiFilePlan& plan);

private:
    bool validatePlan(const MultiFilePlan& plan);
    std::string generateDependencyGraph(const std::vector<std::string>& files);
};

} // namespace IDE
} // namespace RawrXD
