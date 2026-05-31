// ============================================================================
// composer_mode.cpp — AI-Powered Multi-File Composition Engine Implementation
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "composer/composer_mode.h"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <queue>
#include <stack>
#include <set>
#include <regex>
#include <random>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>
#include <shared_mutex>

namespace RawrXD::Composer {

// ============================================================================
// Internal Implementation
// ============================================================================

class ComposerEngine::Impl {
public:
    ComposerConfig config;
    CompositionCallback callback;
    
    std::unordered_map<uint32_t, CompositionPlan> activePlans;
    std::unordered_map<uint32_t, CompositionResult> completedResults;
    std::unordered_map<std::string, std::vector<FileChange>> pendingChanges;
    std::unordered_map<std::string, std::string> originalFileContents;
    
    std::atomic<uint32_t> nextPlanId{1};
    std::atomic<uint32_t> nextChangeId{1};
    std::atomic<uint32_t> nextGroupId{1};
    std::atomic<uint32_t> nextStepId{1};
    
    mutable std::shared_mutex mutex;
    
    // Undo stack for rollback
    std::stack<std::vector<FileChange>> undoStack;
    std::unordered_map<uint32_t, std::vector<FileChange>> planUndoStack;
    
    Impl() = default;
    ~Impl() = default;
    
    void emitEvent(CompositionEvent event, uint32_t planId = 0, 
                   uint32_t stepId = 0, uint32_t changeId = 0,
                   const std::string& message = "",
                   float progress = 0.0f) {
        if (callback) {
            CompositionEventArgs args;
            args.event = event;
            args.planId = planId;
            args.stepId = stepId;
            args.changeId = changeId;
            args.message = message;
            args.progress = progress;
            callback(args);
        }
    }
    
    // Dependency graph analysis
    std::unordered_map<std::string, std::vector<std::string>> 
    buildDependencyGraph(const std::vector<FileContext>& files) {
        std::unordered_map<std::string, std::vector<std::string>> graph;
        
        for (const auto& file : files) {
            graph[file.uri] = file.dependencies;
        }
        
        return graph;
    }
    
    // Topological sort for execution order
    std::vector<std::string> topologicalSort(
        const std::unordered_map<std::string, std::vector<std::string>>& graph) {
        
        std::unordered_map<std::string, int> inDegree;
        std::vector<std::string> result;
        std::queue<std::string> queue;
        
        // Initialize in-degrees
        for (const auto& [node, deps] : graph) {
            if (inDegree.find(node) == inDegree.end()) {
                inDegree[node] = 0;
            }
            for (const auto& dep : deps) {
                inDegree[dep]++;
            }
        }
        
        // Find nodes with no dependencies
        for (const auto& [node, degree] : inDegree) {
            if (degree == 0) {
                queue.push(node);
            }
        }
        
        // Process queue
        while (!queue.empty()) {
            std::string current = queue.front();
            queue.pop();
            result.push_back(current);
            
            auto it = graph.find(current);
            if (it != graph.end()) {
                for (const auto& dep : it->second) {
                    if (--inDegree[dep] == 0) {
                        queue.push(dep);
                    }
                }
            }
        }
        
        return result;
    }
    
    // Detect conflicts between changes
    std::vector<FileConflict> detectConflicts(
        const std::vector<FileChange>& changes) {
        
        std::vector<FileConflict> conflicts;
        std::unordered_map<std::string, std::vector<FileChange>> changesByFile;
        
        // Group changes by file
        for (const auto& change : changes) {
            changesByFile[change.uri].push_back(change);
        }
        
        // Check for overlapping ranges within each file
        for (auto& [uri, fileChanges] : changesByFile) {
            std::sort(fileChanges.begin(), fileChanges.end(),
                [](const FileChange& a, const FileChange& b) {
                    if (a.range.startLine != b.range.startLine) {
                        return a.range.startLine < b.range.startLine;
                    }
                    return a.range.startColumn < b.range.startColumn;
                });
            
            for (size_t i = 0; i < fileChanges.size(); ++i) {
                for (size_t j = i + 1; j < fileChanges.size(); ++j) {
                    if (fileChanges[i].range.overlaps(fileChanges[j].range)) {
                        FileConflict conflict;
                        conflict.uri = uri;
                        conflict.ourChanges.push_back(fileChanges[i]);
                        conflict.theirChanges.push_back(fileChanges[j]);
                        conflict.resolution = ConflictResolution::Manual;
                        conflicts.push_back(std::move(conflict));
                    }
                }
            }
        }
        
        return conflicts;
    }
    
    // Calculate confidence score for a change
    float calculateConfidence(const FileChange& change, 
                               const ProjectContext& context) {
        float confidence = 0.5f;
        
        // Higher confidence for smaller changes
        if (change.newContent.size() < 100) confidence += 0.1f;
        else if (change.newContent.size() < 500) confidence += 0.05f;
        
        // Higher confidence for well-described changes
        if (!change.description.empty()) confidence += 0.1f;
        
        // Higher confidence for files with fewer dependents
        auto it = context.reverseDependencyGraph.find(change.uri);
        if (it != context.reverseDependencyGraph.end()) {
            if (it->second.size() < 3) confidence += 0.1f;
            else if (it->second.size() < 10) confidence += 0.05f;
        }
        
        // Lower confidence for protected files
        if (std::find(config.protectedFiles.begin(), 
                      config.protectedFiles.end(), 
                      change.uri) != config.protectedFiles.end()) {
            confidence -= 0.3f;
        }
        
        return std::max(0.0f, std::min(1.0f, confidence));
    }
    
    // Generate change description from diff
    std::string generateDescription(const FileChange& change) {
        std::ostringstream oss;
        
        switch (change.type) {
            case ChangeType::Insert:
                oss << "Insert " << change.newContent.size() << " chars at line " 
                    << change.range.startLine;
                break;
            case ChangeType::Delete:
                oss << "Delete " << change.originalContent.size() << " chars at line " 
                    << change.range.startLine;
                break;
            case ChangeType::Replace:
                oss << "Replace " << change.originalContent.size() << " chars with " 
                    << change.newContent.size() << " chars at line " 
                    << change.range.startLine;
                break;
            case ChangeType::Move:
                oss << "Move content from " << change.uri << " to " << change.targetUri;
                break;
            case ChangeType::Rename:
                oss << "Rename " << change.uri << " to " << change.targetUri;
                break;
        }
        
        return oss.str();
    }
    
    // Apply a single change to file content
    bool applyChangeToFile(std::string& content, const FileChange& change) {
        std::vector<std::string> lines;
        std::istringstream iss(content);
        std::string line;
        
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        
        // Validate range
        if (change.range.startLine > lines.size() ||
            change.range.endLine > lines.size()) {
            return false;
        }
        
        switch (change.type) {
            case ChangeType::Insert: {
                // Insert new content at position
                std::vector<std::string> newLines;
                std::istringstream newIss(change.newContent);
                while (std::getline(newIss, line)) {
                    newLines.push_back(line);
                }
                
                lines.insert(lines.begin() + change.range.startLine,
                            newLines.begin(), newLines.end());
                break;
            }
            
            case ChangeType::Delete: {
                // Delete lines in range
                lines.erase(lines.begin() + change.range.startLine,
                          lines.begin() + change.range.endLine + 1);
                break;
            }
            
            case ChangeType::Replace: {
                // Replace lines in range
                std::vector<std::string> newLines;
                std::istringstream newIss(change.newContent);
                while (std::getline(newIss, line)) {
                    newLines.push_back(line);
                }
                
                lines.erase(lines.begin() + change.range.startLine,
                          lines.begin() + change.range.endLine + 1);
                lines.insert(lines.begin() + change.range.startLine,
                            newLines.begin(), newLines.end());
                break;
            }
            
            default:
                return false;
        }
        
        // Reconstruct content
        std::ostringstream oss;
        for (size_t i = 0; i < lines.size(); ++i) {
            oss << lines[i];
            if (i < lines.size() - 1) oss << "\n";
        }
        content = oss.str();
        
        return true;
    }
    
    // Parse imports from file content
    std::vector<std::string> parseImports(const std::string& content,
                                           const std::string& language) {
        std::vector<std::string> imports;
        
        static const std::vector<std::pair<std::regex, int>> patterns = {
            {std::regex(R"(import\s+['"]([^'"]+)['"])"), 1},           // JS/TS
            {std::regex(R"(require\s*\(\s*['"]([^'"]+)['"]\s*\))"), 1}, // JS CommonJS
            {std::regex(R"(from\s+['"]([^'"]+)['"]\s+import)"), 1},    // Python
            {std::regex(R"(#include\s*[<"]([^>"]+)[>"])"), 1},         // C/C++
            {std::regex(R"(using\s+[\w.]+;|\s*//)"), 0},               // C# using
        };
        
        for (const auto& [pattern, group] : patterns) {
            std::sregex_iterator it(content.begin(), content.end(), pattern);
            std::sregex_iterator end;
            
            while (it != end) {
                std::smatch match = *it;
                if (match.size() > group) {
                    imports.push_back(match[group].str());
                }
                ++it;
            }
        }
        
        return imports;
    }
    
    // Parse exports from file content
    std::vector<std::string> parseExports(const std::string& content,
                                           const std::string& language) {
        std::vector<std::string> exports;
        
        static const std::vector<std::pair<std::regex, int>> patterns = {
            {std::regex(R"(export\s+(?:default\s+)?(?:function|class|const|let|var)\s+(\w+))"), 1},
            {std::regex(R"(export\s*\{\s*([^}]+)\s*\})"), 1},
            {std::regex(R"(def\s+(\w+)\s*\()"), 1},                     // Python
            {std::regex(R"((?:class|struct|interface)\s+(\w+))"), 1},  // C++/TS
        };
        
        for (const auto& [pattern, group] : patterns) {
            std::sregex_iterator it(content.begin(), content.end(), pattern);
            std::sregex_iterator end;
            
            while (it != end) {
                std::smatch match = *it;
                if (match.size() > group) {
                    std::string exportName = match[group].str();
                    // Handle comma-separated exports
                    if (exportName.find(',') != std::string::npos) {
                        std::istringstream ess(exportName);
                        std::string name;
                        while (std::getline(ess, name, ',')) {
                            // Trim whitespace
                            size_t start = name.find_first_not_of(" \t");
                            size_t end = name.find_last_not_of(" \t");
                            if (start != std::string::npos && end != std::string::npos) {
                                exports.push_back(name.substr(start, end - start + 1));
                            }
                        }
                    } else {
                        exports.push_back(exportName);
                    }
                }
                ++it;
            }
        }
        
        return exports;
    }
    
    // Detect language from file extension
    std::string detectLanguage(const std::string& uri) {
        static const std::unordered_map<std::string, std::string> extMap = {
            {".cpp", "cpp"}, {".cxx", "cpp"}, {".cc", "cpp"}, {".C", "cpp"},
            {".h", "cpp"}, {".hpp", "cpp"}, {".hxx", "cpp"},
            {".c", "c"},
            {".py", "python"}, {".pyw", "python"},
            {".js", "javascript"}, {".mjs", "javascript"},
            {".ts", "typescript"}, {".tsx", "typescript"},
            {".jsx", "javascriptreact"}, {".tsx", "typescriptreact"},
            {".java", "java"},
            {".cs", "csharp"},
            {".go", "go"},
            {".rs", "rust"},
            {".rb", "ruby"},
            {".php", "php"},
            {".swift", "swift"},
            {".kt", "kotlin"}, {".kts", "kotlin"},
            {".scala", "scala"},
            {".lua", "lua"},
            {".r", "r"}, {".R", "r"},
            {".sql", "sql"},
            {".sh", "shell"}, {".bash", "shell"},
            {".ps1", "powershell"},
            {".json", "json"},
            {".yaml", "yaml"}, {".yml", "yaml"},
            {".xml", "xml"},
            {".html", "html"}, {".htm", "html"},
            {".css", "css"}, {".scss", "scss"}, {".less", "less"},
            {".md", "markdown"},
        };
        
        size_t dotPos = uri.rfind('.');
        if (dotPos != std::string::npos) {
            std::string ext = uri.substr(dotPos);
            auto it = extMap.find(ext);
            if (it != extMap.end()) {
                return it->second;
            }
        }
        
        return "plaintext";
    }
    
    // Read file content
    std::string readFile(const std::string& uri) {
        // Convert URI to path
        std::string path = uri;
        if (path.find("file://") == 0) {
            path = path.substr(7);
        }
        
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        
        std::ostringstream content;
        content << file.rdbuf();
        return content.str();
    }
    
    // Write file content
    bool writeFile(const std::string& uri, const std::string& content) {
        std::string path = uri;
        if (path.find("file://") == 0) {
            path = path.substr(7);
        }
        
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        
        file << content;
        return true;
    }
};

// ============================================================================
// Composer Engine Implementation
// ============================================================================

ComposerEngine::ComposerEngine() 
    : m_impl(std::make_unique<Impl>()) {}

ComposerEngine::~ComposerEngine() = default;

void ComposerEngine::setConfig(const ComposerConfig& config) {
    std::unique_lock lock(m_impl->mutex);
    m_impl->config = config;
}

ComposerConfig ComposerEngine::getConfig() const {
    std::shared_lock lock(m_impl->mutex);
    return m_impl->config;
}

void ComposerEngine::setCallback(CompositionCallback callback) {
    std::unique_lock lock(m_impl->mutex);
    m_impl->callback = std::move(callback);
}

std::unique_ptr<CompositionPlan> ComposerEngine::createPlan(
    const std::string& prompt,
    const std::vector<std::string>& fileUris,
    CompositionMode mode) {
    
    auto plan = std::make_unique<CompositionPlan>();
    plan->planId = m_impl->nextPlanId++;
    plan->userPrompt = prompt;
    plan->mode = mode;
    plan->status = CompositionStatus::Pending;
    plan->created = std::chrono::system_clock::now();
    
    // Analyze project context
    auto context = analyzeProject(fileUris);
    
    // Generate changes based on prompt
    auto suggestedChanges = suggestChanges(prompt, context);
    
    // Filter and validate changes
    for (auto& change : suggestedChanges) {
        change.changeId = m_impl->nextChangeId++;
        change.confidence = m_impl->calculateConfidence(change, context);
        
        // Skip low confidence changes unless in crazy mode
        if (mode == CompositionMode::Crazy) {
            if (change.confidence < m_impl->config.crazyModeConfidenceThreshold) {
                continue;
            }
        } else {
            if (change.confidence < m_impl->config.minConfidenceThreshold) {
                continue;
            }
        }
        
        // Skip excluded patterns
        bool excluded = false;
        for (const auto& pattern : m_impl->config.excludedPatterns) {
            if (change.uri.find(pattern) != std::string::npos) {
                excluded = true;
                break;
            }
        }
        if (excluded) continue;
        
        plan->changes.push_back(std::move(change));
    }
    
    // Limit changes based on mode
    uint32_t maxChanges = (mode == CompositionMode::Crazy) 
        ? m_impl->config.crazyModeMaxChanges 
        : m_impl->config.maxChangesPerPlan;
    
    if (plan->changes.size() > maxChanges) {
        // Sort by confidence and keep top changes
        std::sort(plan->changes.begin(), plan->changes.end(),
            [](const FileChange& a, const FileChange& b) {
                return a.confidence > b.confidence;
            });
        plan->changes.resize(maxChanges);
    }
    
    // Create execution steps based on dependency order
    auto sortedFiles = m_impl->topologicalSort(context.dependencyGraph);
    std::unordered_map<std::string, uint32_t> fileOrder;
    for (size_t i = 0; i < sortedFiles.size(); ++i) {
        fileOrder[sortedFiles[i]] = static_cast<uint32_t>(i);
    }
    
    // Group changes by file and sort by dependency order
    std::sort(plan->changes.begin(), plan->changes.end(),
        [&fileOrder](const FileChange& a, const FileChange& b) {
            auto orderA = fileOrder.find(a.uri);
            auto orderB = fileOrder.find(b.uri);
            uint32_t ordA = (orderA != fileOrder.end()) ? orderA->second : UINT32_MAX;
            uint32_t ordB = (orderB != fileOrder.end()) ? orderB->second : UINT32_MAX;
            return ordA < ordB;
        });
    
    // Create steps (batch changes for efficiency)
    uint32_t stepId = 0;
    uint32_t changesPerStep = m_impl->config.maxConcurrentChanges;
    
    for (size_t i = 0; i < plan->changes.size(); i += changesPerStep) {
        CompositionStep step;
        step.stepId = stepId++;
        
        std::ostringstream desc;
        desc << "Apply changes " << (i + 1) << "-" 
             << std::min(i + changesPerStep, plan->changes.size());
        step.description = desc.str();
        
        for (size_t j = i; j < std::min(i + changesPerStep, plan->changes.size()); ++j) {
            step.changeIds.push_back(plan->changes[j].changeId);
        }
        
        plan->steps.push_back(std::move(step));
    }
    
    plan->totalChanges = static_cast<uint32_t>(plan->changes.size());
    
    // Detect conflicts
    plan->conflicts = m_impl->detectConflicts(plan->changes);
    
    // Calculate overall confidence
    if (!plan->changes.empty()) {
        float totalConf = 0.0f;
        for (const auto& change : plan->changes) {
            totalConf += change.confidence;
        }
        plan->overallConfidence = totalConf / plan->changes.size();
    }
    
    // Store plan
    {
        std::unique_lock lock(m_impl->mutex);
        m_impl->activePlans[plan->planId] = *plan;
    }
    
    m_impl->emitEvent(CompositionEvent::PlanCreated, plan->planId, 0, 0,
                     "Plan created with " + std::to_string(plan->totalChanges) + " changes");
    
    return plan;
}

bool ComposerEngine::executePlan(CompositionPlan& plan) {
    plan.status = CompositionStatus::Executing;
    plan.started = std::chrono::system_clock::now();
    
    m_impl->emitEvent(CompositionEvent::PlanStarted, plan.planId);
    
    // Check for unresolved conflicts
    if (!plan.conflicts.empty()) {
        bool hasUnresolved = false;
        for (const auto& conflict : plan.conflicts) {
            if (conflict.resolution == ConflictResolution::Manual) {
                hasUnresolved = true;
                break;
            }
        }
        if (hasUnresolved) {
            plan.status = CompositionStatus::Failed;
            m_impl->emitEvent(CompositionEvent::PlanFailed, plan.planId, 0, 0,
                             "Unresolved conflicts detected");
            return false;
        }
    }
    
    // Store original file contents for rollback
    std::unordered_set<std::string> affectedUris;
    for (const auto& change : plan.changes) {
        affectedUris.insert(change.uri);
    }
    
    for (const auto& uri : affectedUris) {
        m_impl->originalFileContents[uri] = m_impl->readFile(uri);
    }
    
    // Execute steps
    for (auto& step : plan.steps) {
        if (step.isSkipped) continue;
        
        m_impl->emitEvent(CompositionEvent::StepStarted, plan.planId, step.stepId);
        
        bool stepSuccess = true;
        for (uint32_t changeId : step.changeIds) {
            // Find the change
            FileChange* change = nullptr;
            for (auto& c : plan.changes) {
                if (c.changeId == changeId) {
                    change = &c;
                    break;
                }
            }
            
            if (!change) {
                stepSuccess = false;
                continue;
            }
            
            // Check if auto-confirm applies
            bool shouldConfirm = true;
            if (m_impl->config.autoConfirmHighConfidence && 
                change->confidence >= m_impl->config.autoConfirmThreshold) {
                shouldConfirm = false; // Auto-confirmed
            }
            
            // In crazy mode, skip confirmation entirely
            if (plan.mode == CompositionMode::Crazy) {
                shouldConfirm = false;
            }
            
            if (shouldConfirm && !change->isConfirmed) {
                // Would need user confirmation here
                // For now, auto-confirm in aggressive mode
                if (plan.mode == CompositionMode::Aggressive) {
                    change->isConfirmed = true;
                } else {
                    // Pause for confirmation
                    step.isSkipped = true;
                    step.skipReason = "Awaiting user confirmation";
                    continue;
                }
            }
            
            // Apply the change
            if (applyChange(*change)) {
                change->isApplied = true;
                plan.appliedChanges++;
                
                // Store for undo
                m_impl->planUndoStack[plan.planId].push_back(*change);
                
                m_impl->emitEvent(CompositionEvent::ChangeApplied, plan.planId, 
                                 step.stepId, changeId, change->description);
            } else {
                change->isApplied = false;
                plan.failedChanges++;
                stepSuccess = false;
                
                m_impl->emitEvent(CompositionEvent::ChangeFailed, plan.planId,
                                 step.stepId, changeId, "Failed to apply change");
                
                // Check if we should rollback
                if (m_impl->config.enableAutoRollback && 
                    plan.failedChanges > plan.totalChanges * 0.3f) {
                    // Too many failures, rollback
                    rollbackPlan(plan);
                    return false;
                }
            }
        }
        
        step.isComplete = stepSuccess;
        m_impl->emitEvent(CompositionEvent::StepCompleted, plan.planId, step.stepId);
        
        float progress = static_cast<float>(plan.appliedChanges) / plan.totalChanges;
        m_impl->emitEvent(CompositionEvent::StepCompleted, plan.planId, step.stepId,
                         "", progress);
    }
    
    plan.completed = std::chrono::system_clock::now();
    plan.status = (plan.failedChanges == 0) ? 
        CompositionStatus::Completed : CompositionStatus::Failed;
    
    // Create result
    CompositionResult result;
    result.planId = plan.planId;
    result.status = plan.status;
    result.totalChanges = plan.totalChanges;
    result.appliedChanges = plan.appliedChanges;
    result.failedChanges = plan.failedChanges;
    
    for (const auto& change : plan.changes) {
        if (change.isApplied) {
            result.applied.push_back(change);
        } else {
            result.failed.push_back(change);
        }
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        plan.completed - plan.started);
    result.duration = duration;
    result.finalConfidence = plan.overallConfidence;
    
    std::ostringstream summary;
    summary << "Composition completed: " << result.appliedChanges << "/" 
            << result.totalChanges << " changes applied in " 
            << duration.count() << "ms";
    result.summary = summary.str();
    
    // Store result
    {
        std::unique_lock lock(m_impl->mutex);
        m_impl->completedResults[plan.planId] = result;
    }
    
    m_impl->emitEvent(CompositionEvent::PlanCompleted, plan.planId, 0, 0,
                     result.summary);
    
    return plan.status == CompositionStatus::Completed;
}

bool ComposerEngine::rollbackPlan(CompositionPlan& plan) {
    m_impl->emitEvent(CompositionEvent::PlanRolledBack, plan.planId);
    
    // Get changes to rollback (in reverse order)
    auto& undoStack = m_impl->planUndoStack[plan.planId];
    
    uint32_t rolledBack = 0;
    while (!undoStack.empty()) {
        FileChange change = undoStack.top();
        undoStack.pop();
        
        // Revert the change
        if (revertChange(change)) {
            rolledBack++;
            plan.rolledBackChanges++;
        }
    }
    
    plan.status = CompositionStatus::RolledBack;
    
    return rolledBack > 0;
}

bool ComposerEngine::pausePlan(CompositionPlan& plan) {
    if (plan.status != CompositionStatus::Executing) {
        return false;
    }
    
    plan.status = CompositionStatus::Pending;
    return true;
}

bool ComposerEngine::resumePlan(CompositionPlan& plan) {
    if (plan.status != CompositionStatus::Pending) {
        return false;
    }
    
    return executePlan(plan);
}

bool ComposerEngine::applyChange(FileChange& change) {
    std::string content = m_impl->readFile(change.uri);
    if (content.empty() && change.type != ChangeType::Insert) {
        return false;
    }
    
    // Store original for revert
    change.originalContent = content;
    
    if (!m_impl->applyChangeToFile(content, change)) {
        return false;
    }
    
    change.newContent = content;
    change.isApplied = true;
    change.timestamp = std::chrono::system_clock::now();
    
    return m_impl->writeFile(change.uri, content);
}

bool ComposerEngine::revertChange(FileChange& change) {
    if (!change.isApplied) {
        return false;
    }
    
    if (!m_impl->writeFile(change.uri, change.originalContent)) {
        return false;
    }
    
    change.isApplied = false;
    return true;
}

bool ComposerEngine::confirmChange(FileChange& change) {
    change.isConfirmed = true;
    return true;
}

bool ComposerEngine::detectConflicts(CompositionPlan& plan) {
    plan.conflicts = m_impl->detectConflicts(plan.changes);
    return plan.conflicts.empty();
}

bool ComposerEngine::resolveConflict(FileConflict& conflict, 
                                     ConflictResolution resolution) {
    conflict.resolution = resolution;
    
    switch (resolution) {
        case ConflictResolution::AcceptOurs:
            // Keep our changes
            for (auto& change : conflict.ourChanges) {
                change.isConfirmed = true;
            }
            for (auto& change : conflict.theirChanges) {
                change.isConfirmed = false;
            }
            break;
            
        case ConflictResolution::AcceptTheirs:
            // Keep their changes
            for (auto& change : conflict.ourChanges) {
                change.isConfirmed = false;
            }
            for (auto& change : conflict.theirChanges) {
                change.isConfirmed = true;
            }
            break;
            
        case ConflictResolution::Merge:
            // Attempt automatic merge
            // This is a simplified merge - real implementation would be more sophisticated
            if (!conflict.mergedContent.empty()) {
                // Create a merged change
                FileChange mergedChange;
                mergedChange.uri = conflict.uri;
                mergedChange.type = ChangeType::Replace;
                mergedChange.newContent = conflict.mergedContent;
                mergedChange.isConfirmed = true;
                // Add to plan...
            }
            break;
            
        case ConflictResolution::Manual:
            // Leave for manual resolution
            break;
    }
    
    m_impl->emitEvent(CompositionEvent::ConflictResolved, 0, 0, 0,
                     "Conflict resolved: " + std::to_string(static_cast<int>(resolution)));
    
    return true;
}

ProjectContext ComposerEngine::analyzeProject(const std::vector<std::string>& fileUris) {
    ProjectContext context;
    
    // Read and analyze each file
    for (const auto& uri : fileUris) {
        FileContext fileCtx;
        fileCtx.uri = uri;
        fileCtx.content = m_impl->readFile(uri);
        fileCtx.language = m_impl->detectLanguage(uri);
        
        if (!fileCtx.content.empty()) {
            fileCtx.imports = m_impl->parseImports(fileCtx.content, fileCtx.language);
            fileCtx.exports = m_impl->parseExports(fileCtx.content, fileCtx.language);
        }
        
        context.files.push_back(std::move(fileCtx));
    }
    
    // Build dependency graph
    context.dependencyGraph = m_impl->buildDependencyGraph(context.files);
    
    // Build reverse dependency graph
    for (const auto& [uri, deps] : context.dependencyGraph) {
        for (const auto& dep : deps) {
            context.reverseDependencyGraph[dep].push_back(uri);
        }
    }
    
    return context;
}

std::vector<FileChange> ComposerEngine::suggestChanges(
    const std::string& prompt,
    const ProjectContext& context) {
    
    std::vector<FileChange> changes;
    
    // This is where AI integration would happen
    // For now, return empty - actual implementation would call LLM
    
    // Placeholder: Parse prompt for simple refactoring instructions
    // e.g., "rename X to Y", "extract function Z", etc.
    
    static const std::regex renamePattern(R"(rename\s+(\w+)\s+to\s+(\w+))", 
                                           std::regex_constants::icase);
    static const std::regex extractPattern(R"(extract\s+(?:function|method)\s+(\w+))",
                                           std::regex_constants::icase);
    
    std::smatch match;
    if (std::regex_search(prompt, match, renamePattern)) {
        std::string oldName = match[1].str();
        std::string newName = match[2].str();
        
        // Find all occurrences and create rename changes
        for (const auto& file : context.files) {
            std::regex nameRegex("\\b" + oldName + "\\b");
            std::sregex_iterator it(file.content.begin(), file.content.end(), nameRegex);
            std::sregex_iterator end;
            
            std::vector<std::pair<uint32_t, uint32_t>> occurrences;
            while (it != end) {
                // Calculate line number from position
                size_t pos = it->position();
                uint32_t line = 1;
                for (size_t i = 0; i < pos && i < file.content.size(); ++i) {
                    if (file.content[i] == '\n') line++;
                }
                occurrences.push_back({line, static_cast<uint32_t>(pos)});
                ++it;
            }
            
            if (!occurrences.empty()) {
                FileChange change;
                change.uri = file.uri;
                change.type = ChangeType::Replace;
                change.description = "Rename " + oldName + " to " + newName;
                
                // Create new content with replacements
                std::string newContent = std::regex_replace(file.content, nameRegex, newName);
                change.newContent = newContent;
                change.originalContent = file.content;
                
                changes.push_back(std::move(change));
            }
        }
    }
    
    return changes;
}

CompositionStatus ComposerEngine::getStatus(uint32_t planId) const {
    std::shared_lock lock(m_impl->mutex);
    
    auto it = m_impl->activePlans.find(planId);
    if (it != m_impl->activePlans.end()) {
        return it->second.status;
    }
    
    auto rit = m_impl->completedResults.find(planId);
    if (rit != m_impl->completedResults.end()) {
        return rit->second.status;
    }
    
    return CompositionStatus::Pending;
}

float ComposerEngine::getProgress(uint32_t planId) const {
    std::shared_lock lock(m_impl->mutex);
    
    auto it = m_impl->activePlans.find(planId);
    if (it != m_impl->activePlans.end()) {
        const auto& plan = it->second;
        if (plan.totalChanges == 0) return 0.0f;
        return static_cast<float>(plan.appliedChanges) / plan.totalChanges;
    }
    
    auto rit = m_impl->completedResults.find(planId);
    if (rit != m_impl->completedResults.end()) {
        return 1.0f;
    }
    
    return 0.0f;
}

std::optional<CompositionResult> ComposerEngine::getResult(uint32_t planId) const {
    std::shared_lock lock(m_impl->mutex);
    
    auto it = m_impl->completedResults.find(planId);
    if (it != m_impl->completedResults.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

// Factory function
std::unique_ptr<IComposerEngine> createComposerEngine() {
    return std::make_unique<ComposerEngine>();
}

} // namespace RawrXD::Composer
