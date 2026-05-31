#include "MultiFileRewriteEngine.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

namespace fs = std::filesystem;

namespace RawrXD {
namespace IDE {

MultiFileRewriteEngine::MultiFileRewriteEngine() {
}

MultiFilePlan MultiFileRewriteEngine::planCoordinatedEdits(
    const std::string& userGoal,
    const std::vector<std::string>& targetFiles) {
    
    MultiFilePlan plan;
    plan.planDescription = "Coordinated edit for: " + userGoal;
    plan.requiresRefactor = true;

    // Build internal dependency graph of target files
    std::string graph = generateDependencyGraph(targetFiles);
    
    for (const auto& filePath : targetFiles) {
        if (!fs::exists(filePath)) continue;

        MultiFileChange change;
        change.filePath = filePath;
        
        // Read original content
        std::ifstream ifs(filePath);
        if (ifs) {
            std::stringstream ss;
            ss << ifs.rdbuf();
            change.originalContent = ss.str();
        }

        // Logic to generate coordinated newContent based on userGoal and dependency graph
        // This is where we would call the underlying LLM with cross-file context
        // For now, we stub out the transition logic
        change.newContent = change.originalContent; 
        
        plan.changes.push_back(change);
    }

    return plan;
}

bool MultiFileRewriteEngine::applyPlan(const MultiFilePlan& plan) {
    if (!validatePlan(plan)) return false;

    for (const auto& change : plan.changes) {
        std::ofstream ofs(change.filePath, std::ios::trunc);
        if (!ofs) {
            // Rollback if any file write fails mid-transaction
            rollback(plan);
            return false;
        }
        ofs << change.newContent;
    }

    return true;
}

bool MultiFileRewriteEngine::rollback(const MultiFilePlan& plan) {
    bool allSuccess = true;
    for (const auto& change : plan.changes) {
        if (change.originalContent.empty()) continue;
        
        std::ofstream ofs(change.filePath, std::ios::trunc);
        if (ofs) {
            ofs << change.originalContent;
        } else {
            allSuccess = false;
        }
    }
    return allSuccess;
}

bool MultiFileRewriteEngine::validatePlan(const MultiFilePlan& plan) {
    if (plan.changes.empty()) return false;
    
    for (const auto& change : plan.changes) {
        if (change.filePath.empty()) return false;
        if (!fs::exists(change.filePath)) return false;
    }
    
    return true;
}

std::string MultiFileRewriteEngine::generateDependencyGraph(const std::vector<std::string>& files) {
    // Build a map of file → included files by parsing #include directives
    std::unordered_map<std::string, std::vector<std::string>> deps;

    for (const auto& file : files) {
        if (!fs::exists(file)) continue;

        std::ifstream ifs(file);
        if (!ifs) continue;

        std::string line;
        while (std::getline(ifs, line)) {
            // Match #include "..." (local includes only — these form the dependency graph)
            size_t pos = line.find("#include");
            if (pos == std::string::npos) continue;
            pos = line.find('"', pos + 8);
            if (pos == std::string::npos) continue;
            size_t end = line.find('"', pos + 1);
            if (end == std::string::npos) continue;

            std::string included = line.substr(pos + 1, end - pos - 1);

            // Resolve relative to the including file's directory
            fs::path resolved = fs::path(file).parent_path() / included;
            std::string resolvedStr = resolved.lexically_normal().string();

            // Check if the included file is in our target set
            for (const auto& target : files) {
                if (fs::path(target).lexically_normal() == fs::path(resolvedStr).lexically_normal()) {
                    deps[file].push_back(target);
                    break;
                }
            }
        }
    }

    std::stringstream graph;
    graph << "Dependency Graph:\n";
    for (const auto& file : files) {
        graph << "  " << file << " -> [";
        auto it = deps.find(file);
        if (it != deps.end()) {
            for (size_t i = 0; i < it->second.size(); ++i) {
                if (i > 0) graph << ", ";
                graph << fs::path(it->second[i]).filename().string();
            }
        }
        graph << "]\n";
    }
    return graph.str();
}

} // namespace IDE
} // namespace RawrXD
