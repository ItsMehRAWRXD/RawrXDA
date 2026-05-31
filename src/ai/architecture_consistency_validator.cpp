// ============================================================================
// architecture_consistency_validator.cpp — Architecture Consistency Guardian
// Validates codebase adherence to architectural principles.
//
// Implementation Strategy:
//   Phase 1: Static snapshot validation (rule-based + AI-enhanced)
//   Phase 2: Temporal drift detection (baseline comparison)
//   Phase 3: LSP integration for real-time feedback
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured ValidationResult returns only.
// ============================================================================
#include "architecture_consistency_validator.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <algorithm>
#include <future>
#include <thread>

namespace fs = std::filesystem;

namespace RawrXD {
namespace AI {

// ============================================================================
// SemanticGraph Implementation
// ============================================================================
void SemanticGraph::AddEntity(const CodeEntity& entity) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entities[entity.name] = entity;
}

void SemanticGraph::AddEdge(const std::string& from, const std::string& to, 
                            const std::string& relation) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_edges[from].push_back({to, relation});
}

std::vector<CodeEntity> SemanticGraph::FindByType(CodeEntity::Type type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeEntity> result;
    for (const auto& [name, entity] : m_entities) {
        if (entity.type == type) result.push_back(entity);
    }
    return result;
}

std::vector<CodeEntity> SemanticGraph::FindByScope(const std::string& scope) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<CodeEntity> result;
    for (const auto& [name, entity] : m_entities) {
        if (entity.parentScope == scope) result.push_back(entity);
    }
    return result;
}

std::vector<std::string> SemanticGraph::GetDependencies(const std::string& entityName) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_entities.find(entityName);
    if (it != m_entities.end()) return it->second.dependencies;
    return {};
}

bool SemanticGraph::HasEntity(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_entities.find(name) != m_entities.end();
}

void SemanticGraph::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entities.clear();
    m_edges.clear();
}

// ============================================================================
// ArchitectureConsistencyValidator Implementation
// ============================================================================
ArchitectureConsistencyValidator::ArchitectureConsistencyValidator(
    std::shared_ptr<RawrXD::Agent::SovereignInferenceClient> inferenceClient)
    : m_inferenceClient(inferenceClient) {}

ArchitectureConsistencyValidator::~ArchitectureConsistencyValidator() = default;

bool ArchitectureConsistencyValidator::Initialize(const std::string& architectureDocPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Load built-in principles (always available)
    LoadBuiltInPrinciples();
    
    // Try to load from markdown doc if available
    if (fs::exists(architectureDocPath)) {
        LoadPrinciplesFromMarkdown(architectureDocPath);
    }
    
    m_initialized = true;
    return true;
}

void ArchitectureConsistencyValidator::LoadBuiltInPrinciples() {
    // Core principles from ARCHITECTURE.md v7.4.0
    m_principles = {
        // Hotpatch principles
        {"HOTPATCH-01", "Hotpatch", 
         "No exceptions in hotpatch code",
         "hotpatch code must use PatchResult structs, not throw",
         1},  // Error
        
        {"HOTPATCH-02", "Hotpatch",
         "No STL allocators inside MASM bridge code",
         "MASM bridge must not use std::allocator or new/delete",
         1},
        
        // MASM64 principles
        {"MASM-01", "MASM64",
         "All pointer math uses uintptr_t in memory layer",
         "memory layer pointer arithmetic must use uintptr_t",
         2},  // Warning
        
        {"MASM-02", "MASM64",
         "Function pointer callbacks instead of signals/slots",
         "no Qt dependency in callback mechanisms",
         2},
        
        // Build principles
        {"BUILD-01", "Build",
         "Header isolation — no circular includes between layers",
         "circular include detected between architectural layers",
         2},
        
        {"BUILD-02", "Build",
         "Singleton pattern for engine registries",
         "engine registry must use thread-safe singleton",
         3},  // Information
        
        // Security principles
        {"SEC-01", "Security",
         "All threading via std::mutex + std::lock_guard",
         "no recursive locks, no raw mutex operations",
         1},
        
        {"SEC-02", "Security",
         "No exceptions in security-critical paths",
         "security code must return error codes, not throw",
         1},
        
        // Architecture principles
        {"ARCH-01", "Architecture",
         "Zero Electron/Qt/CEF dependencies in core",
         "core engine must not depend on UI frameworks",
         1},
        
        {"ARCH-02", "Architecture",
         "Three-Layer Hotpatch System integrity",
         "Memory -> Byte -> Server layers must be preserved",
         2},
        
        {"ARCH-03", "Architecture",
         "Four-Pane Layout Canon",
         "Editor, Sidebar, Terminal, Chat panels must exist",
         3},
        
        // Inference principles
        {"INFER-01", "Inference",
         "SovereignInferenceClient for all AI operations",
         "legacy HTTP-based inference clients should be migrated",
         2},
        
        {"INFER-02", "Inference",
         "KV cache quantization configuration",
         "KV quant types must be explicitly configured",
         3},
    };
}

bool ArchitectureConsistencyValidator::LoadPrinciplesFromMarkdown(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    std::string line;
    std::regex principleRegex(R"((\w+-\d+)\s*:\s*(.+)\s*-\s*(.+))");
    
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, principleRegex)) {
            ArchitecturalPrinciple principle;
            principle.id = match[1];
            principle.description = match[2];
            principle.rule = match[3];
            principle.severity = 2;  // Default to warning
            m_principles.push_back(principle);
        }
    }
    
    return true;
}

bool ArchitectureConsistencyValidator::BuildSemanticGraph(const std::string& sourceRoot) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    m_currentGraph.Clear();
    
    if (!fs::exists(sourceRoot)) {
        return false;
    }
    
    // Recursively scan source files
    for (const auto& entry : fs::recursive_directory_iterator(sourceRoot)) {
        if (!entry.is_regular_file()) continue;
        
        const auto& path = entry.path();
        const std::string ext = path.extension().string();
        
        if (ext == ".cpp" || ext == ".hpp" || ext == ".h") {
            ParseSourceFile(path.string());
        } else if (ext == ".asm") {
            ParseSourceFile(path.string());
        } else if (ext == ".cmake" || path.filename() == "CMakeLists.txt") {
            ParseSourceFile(path.string());
        }
    }
    
    return true;
}

void ArchitectureConsistencyValidator::ParseSourceFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) return;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    const std::string ext = fs::path(filePath).extension().string();
    
    if (ext == ".cpp" || ext == ".hpp" || ext == ".h") {
        ParseCppFile(filePath, content);
    } else if (ext == ".asm") {
        ParseMASMFile(filePath, content);
    } else if (ext == ".cmake" || fs::path(filePath).filename() == "CMakeLists.txt") {
        ParseCMakeFile(filePath, content);
    }
}

void ArchitectureConsistencyValidator::ParseCppFile(const std::string& filePath, 
                                                     const std::string& content) {
    // Extract namespaces
    std::regex nsRegex(R"(namespace\s+(\w+)\s*\{)");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    
    while (std::regex_search(searchStart, content.cend(), match, nsRegex)) {
        CodeEntity entity;
        entity.type = CodeEntity::Type::Namespace;
        entity.name = match[1];
        entity.filePath = filePath;
        entity.lineNumber = static_cast<uint32_t>(
            std::count(content.cbegin(), searchStart, '\n') + 1);
        m_currentGraph.AddEntity(entity);
        searchStart = match.suffix().first;
    }
    
    // Extract classes
    std::regex classRegex(R"(class\s+(\w+)(?:\s*:\s*(?:public|private|protected)\s+(\w+))?)");
    searchStart = content.cbegin();
    
    while (std::regex_search(searchStart, content.cend(), match, classRegex)) {
        CodeEntity entity;
        entity.type = CodeEntity::Type::Class;
        entity.name = match[1];
        entity.filePath = filePath;
        entity.lineNumber = static_cast<uint32_t>(
            std::count(content.cbegin(), searchStart, '\n') + 1);
        
        if (match[2].matched) {
            entity.dependencies.push_back(match[2]);
        }
        
        m_currentGraph.AddEntity(entity);
        searchStart = match.suffix().first;
    }
    
    // Extract functions
    std::regex funcRegex(R"((\w+)::(\w+)\s*\([^)]*\)\s*(?:const\s*)?\{)");
    searchStart = content.cbegin();
    
    while (std::regex_search(searchStart, content.cend(), match, funcRegex)) {
        CodeEntity entity;
        entity.type = CodeEntity::Type::Function;
        entity.name = match[1].str() + "::" + match[2].str();
        entity.parentScope = match[1];
        entity.filePath = filePath;
        entity.lineNumber = static_cast<uint32_t>(
            std::count(content.cbegin(), searchStart, '\n') + 1);
        m_currentGraph.AddEntity(entity);
        searchStart = match.suffix().first;
    }
    
    // Extract includes
    std::regex includeRegex(R"(#include\s+[<"]([^\u003e"]+)[>"])");
    searchStart = content.cbegin();
    
    while (std::regex_search(searchStart, content.cend(), match, includeRegex)) {
        CodeEntity entity;
        entity.type = CodeEntity::Type::Include;
        entity.name = match[1];
        entity.filePath = filePath;
        entity.lineNumber = static_cast<uint32_t>(
            std::count(content.cbegin(), searchStart, '\n') + 1);
        m_currentGraph.AddEntity(entity);
        searchStart = match.suffix().first;
    }
}

void ArchitectureConsistencyValidator::ParseMASMFile(const std::string& filePath,
                                                      const std::string& content) {
    // Extract procedures
    std::regex procRegex(R"((\w+)\s+PROC\b)");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    
    while (std::regex_search(searchStart, content.cend(), match, procRegex)) {
        CodeEntity entity;
        entity.type = CodeEntity::Type::MASMKernel;
        entity.name = match[1];
        entity.filePath = filePath;
        entity.lineNumber = static_cast<uint32_t>(
            std::count(content.cbegin(), searchStart, '\n') + 1);
        m_currentGraph.AddEntity(entity);
        searchStart = match.suffix().first;
    }
}

void ArchitectureConsistencyValidator::ParseCMakeFile(const std::string& filePath,
                                                         const std::string& content) {
    // Extract targets
    std::regex targetRegex(R"(add_(?:executable|library)\s*\(\s*(\w+))");
    std::smatch match;
    std::string::const_iterator searchStart(content.cbegin());
    
    while (std::regex_search(searchStart, content.cend(), match, targetRegex)) {
        CodeEntity entity;
        entity.type = CodeEntity::Type::GlobalVariable;  // Reuse for targets
        entity.name = match[1];
        entity.filePath = filePath;
        entity.lineNumber = static_cast<uint32_t>(
            std::count(content.cbegin(), searchStart, '\n') + 1);
        m_currentGraph.AddEntity(entity);
        searchStart = match.suffix().first;
    }
}

ArchitectureValidationResult ArchitectureConsistencyValidator::ValidateSnapshot() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto t0 = std::chrono::high_resolution_clock::now();
    
    ArchitectureValidationResult result;
    
    // Phase 1: Fast rule-based validation
    auto ruleResult = RunRuleBasedValidation();
    result.inconsistencies.insert(result.inconsistencies.end(),
                                   ruleResult.inconsistencies.begin(),
                                   ruleResult.inconsistencies.end());
    
    // Phase 2: AI-enhanced validation (if inference available)
    if (m_useAI && m_inferenceClient && m_inferenceClient->IsLoaded()) {
        auto aiResult = RunAIValidation();
        result.inconsistencies.insert(result.inconsistencies.end(),
                                       aiResult.inconsistencies.begin(),
                                       aiResult.inconsistencies.end());
    }
    
    // Calculate architecture score
    if (!result.inconsistencies.empty()) {
        size_t errors = std::count_if(result.inconsistencies.begin(), 
                                       result.inconsistencies.end(),
                                       [](const auto& i) { return i.severity == 1; });
        size_t warnings = std::count_if(result.inconsistencies.begin(),
                                          result.inconsistencies.end(),
                                          [](const auto& i) { return i.severity == 2; });
        
        // Score: 1.0 - weighted penalty
        float penalty = (errors * 0.3f + warnings * 0.1f) / result.inconsistencies.size();
        result.architectureScore = std::max(0.0f, 1.0f - penalty);
    } else {
        result.architectureScore = 1.0f;
    }
    
    auto t1 = std::chrono::high_resolution_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0);
    result.success = true;
    
    return result;
}

ArchitectureValidationResult ArchitectureConsistencyValidator::RunRuleBasedValidation() {
    ArchitectureValidationResult result;
    
    // Check for circular includes
    auto includes = m_currentGraph.FindByType(CodeEntity::Type::Include);
    std::unordered_map<std::string, std::unordered_set<std::string>> includeMap;
    
    for (const auto& inc : includes) {
        includeMap[inc.filePath].insert(inc.name);
    }
    
    // Simple circular detection (A includes B, B includes A)
    for (const auto& [fileA, includesA] : includeMap) {
        for (const auto& inc : includesA) {
            auto it = includeMap.find(inc);
            if (it != includeMap.end() && it->second.count(fileA)) {
                ArchitectureValidationResult::Inconsistency inconsistency;
                inconsistency.principleId = "BUILD-01";
                inconsistency.description = "Circular include detected";
                inconsistency.filePath = fileA;
                inconsistency.severity = 2;
                inconsistency.suggestedFix = "Break circular dependency via forward declaration or interface abstraction";
                inconsistency.confidence = 1.0f;
                result.inconsistencies.push_back(inconsistency);
            }
        }
    }
    
    // Check for Qt dependencies in core
    auto classes = m_currentGraph.FindByType(CodeEntity::Type::Class);
    for (const auto& cls : classes) {
        if (cls.filePath.find("src/core/") != std::string::npos ||
            cls.filePath.find("src/inference/") != std::string::npos ||
            cls.filePath.find("src/agentic/") != std::string::npos) {
            
            for (const auto& dep : cls.dependencies) {
                if (dep.find("QObject") != std::string::npos ||
                    dep.find("QWidget") != std::string::npos ||
                    dep.find("Qt") != std::string::npos) {
                    ArchitectureValidationResult::Inconsistency inconsistency;
                    inconsistency.principleId = "ARCH-01";
                    inconsistency.description = "Core engine depends on Qt framework";
                    inconsistency.filePath = cls.filePath;
                    inconsistency.lineNumber = cls.lineNumber;
                    inconsistency.severity = 1;
                    inconsistency.suggestedFix = "Move UI dependency to src/ide/ layer or use function pointer callbacks";
                    inconsistency.confidence = 1.0f;
                    result.inconsistencies.push_back(inconsistency);
                }
            }
        }
    }
    
    // Check for exception usage in hotpatch code
    for (const auto& cls : classes) {
        if (cls.filePath.find("hotpatch") != std::string::npos) {
            // This is a simplified check - real implementation would parse try/catch blocks
            ArchitectureValidationResult::Inconsistency info;
            info.principleId = "HOTPATCH-01";
            info.description = "Verify hotpatch code uses PatchResult instead of exceptions";
            info.filePath = cls.filePath;
            info.severity = 3;  // Information level
            info.suggestedFix = "Audit: ensure all error paths return PatchResult structs";
            info.confidence = 0.5f;
            result.inconsistencies.push_back(info);
        }
    }
    
    return result;
}

ArchitectureValidationResult ArchitectureConsistencyValidator::RunAIValidation() {
    ArchitectureValidationResult result;
    
    if (!m_inferenceClient || !m_inferenceClient->IsLoaded()) {
        result.error_message = "Inference client not available";
        return result;
    }
    
    // Build prompt from semantic graph
    auto entities = m_currentGraph.FindByType(CodeEntity::Type::Class);
    std::string prompt = BuildValidationPrompt(entities);
    
    // Run inference
    std::vector<RawrXD::Agent::ChatMessage> messages;
    messages.push_back({"system", "You are an architecture consistency validator. Analyze code against principles and report inconsistencies in JSON format."});
    messages.push_back({"user", prompt});
    
    auto inferenceResult = m_inferenceClient->ChatSync(messages);
    if (!inferenceResult.success) {
        result.error_message = inferenceResult.error_message;
        return result;
    }
    
    // Parse AI response
    result.inconsistencies = ParseAIResponse(inferenceResult.response);
    result.success = true;
    
    return result;
}

std::string ArchitectureConsistencyValidator::BuildValidationPrompt(
    const std::vector<CodeEntity>& entities) {
    std::stringstream prompt;
    prompt << "Analyze the following code entities for architectural consistency.\n\n";
    prompt << "Principles to check:\n";
    
    for (const auto& principle : m_principles) {
        prompt << "- [" << principle.id << "] " << principle.description << "\n";
    }
    
    prompt << "\nCode entities:\n";
    for (const auto& entity : entities) {
        prompt << "- " << entity.name << " (" << entity.filePath << ":" << entity.lineNumber << ")\n";
        if (!entity.dependencies.empty()) {
            prompt << "  Dependencies: ";
            for (const auto& dep : entity.dependencies) {
                prompt << dep << " ";
            }
            prompt << "\n";
        }
    }
    
    prompt << "\nReport any inconsistencies found. Format:\n";
    prompt << "PRINCIPLE_ID|FILE|LINE|SEVERITY|DESCRIPTION|SUGGESTED_FIX\n";
    
    return prompt.str();
}

std::vector<ArchitectureValidationResult::Inconsistency> 
ArchitectureConsistencyValidator::ParseAIResponse(const std::string& response) {
    std::vector<ArchitectureValidationResult::Inconsistency> inconsistencies;
    
    std::istringstream stream(response);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Parse format: PRINCIPLE_ID|FILE|LINE|SEVERITY|DESCRIPTION|SUGGESTED_FIX
        std::stringstream lineStream(line);
        std::string token;
        std::vector<std::string> parts;
        
        while (std::getline(lineStream, token, '|')) {
            parts.push_back(token);
        }
        
        if (parts.size() >= 5) {
            ArchitectureValidationResult::Inconsistency inconsistency;
            inconsistency.principleId = parts[0];
            inconsistency.filePath = parts[1];
            inconsistency.lineNumber = static_cast<uint32_t>(std::stoul(parts[2]));
            inconsistency.severity = static_cast<uint32_t>(std::stoul(parts[3]));
            inconsistency.description = parts[4];
            if (parts.size() >= 6) {
                inconsistency.suggestedFix = parts[5];
            }
            inconsistency.confidence = m_minConfidence;
            
            if (inconsistency.confidence >= m_minConfidence) {
                inconsistencies.push_back(inconsistency);
            }
        }
    }
    
    return inconsistencies;
}

ArchitectureValidationResult ArchitectureConsistencyValidator::DetectDrift(
    const SemanticGraph& baseline) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ArchitectureValidationResult result;
    
    // Compare current graph against baseline
    auto currentEntities = m_currentGraph.FindByType(CodeEntity::Type::Class);
    
    for (const auto& entity : currentEntities) {
        if (!baseline.HasEntity(entity.name)) {
            // New entity - check if it violates principles
            ArchitectureValidationResult::Inconsistency inconsistency;
            inconsistency.principleId = "DRIFT-01";
            inconsistency.description = "New architectural entity: " + entity.name;
            inconsistency.filePath = entity.filePath;
            inconsistency.lineNumber = entity.lineNumber;
            inconsistency.severity = 3;  // Information
            inconsistency.suggestedFix = "Review new entity for architectural compliance";
            inconsistency.confidence = 0.8f;
            result.inconsistencies.push_back(inconsistency);
        }
    }
    
    result.success = true;
    return result;
}

void ArchitectureConsistencyValidator::RegisterWithLSP(RawrXD::LSP::DiagnosticProvider& provider) {
    auto source = [this](const std::string& uri, const std::string& content) 
        -> std::vector<RawrXD::LSP::Diagnostic> {
        
        // Quick validation for this specific file
        ArchitectureValidationResult result;
        
        // Parse just this file
        ParseSourceFile(uri);
        
        // Run rule-based validation
        result = RunRuleBasedValidation();
        
        return ConvertToLSPDiagnostics(result);
    };
    
    provider.registerDiagnosticSource("architecture_validator", source);
}

std::vector<RawrXD::LSP::Diagnostic> 
ArchitectureConsistencyValidator::ConvertToLSPDiagnostics(
    const ArchitectureValidationResult& result) {
    std::vector<RawrXD::LSP::Diagnostic> diagnostics;
    
    for (const auto& inconsistency : result.inconsistencies) {
        RawrXD::LSP::Diagnostic diagnostic;
        diagnostic.range.start.line = inconsistency.lineNumber > 0 ? inconsistency.lineNumber - 1 : 0;
        diagnostic.range.start.character = 0;
        diagnostic.range.end.line = diagnostic.range.start.line;
        diagnostic.range.end.character = 100;
        
        switch (inconsistency.severity) {
            case 1: diagnostic.severity = RawrXD::LSP::DiagnosticSeverity::Error; break;
            case 2: diagnostic.severity = RawrXD::LSP::DiagnosticSeverity::Warning; break;
            default: diagnostic.severity = RawrXD::LSP::DiagnosticSeverity::Information; break;
        }
        
        diagnostic.message = "[" + inconsistency.principleId + "] " + inconsistency.description;
        if (!inconsistency.suggestedFix.empty()) {
            diagnostic.message += "\nSuggestion: " + inconsistency.suggestedFix;
        }
        
        diagnostic.source = "architecture_validator";
        diagnostics.push_back(diagnostic);
    }
    
    return diagnostics;
}

void ArchitectureConsistencyValidator::ValidateAsync(ValidationCallback callback) {
    std::thread([this, callback]() {
        auto result = ValidateSnapshot();
        callback(result);
    }).detach();
}

} // namespace AI
} // namespace RawrXD
