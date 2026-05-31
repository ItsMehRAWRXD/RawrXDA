// ============================================================================
// security_analyzer.cpp — AI-Powered Security Analysis Engine Implementation
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "code_review/security_analyzer.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <filesystem>
#include <cctype>

namespace fs = std::filesystem;

namespace RawrXD::CodeReview {

// ============================================================================
// Security Analyzer Implementation
// ============================================================================

class SecurityAnalyzer::Impl {
public:
    LSP::WorkspaceSymbolIndex* symbolIndex = nullptr;
    SecurityAnalyzerConfig config;
    std::vector<std::unique_ptr<ISecurityRule>> rules;
    std::unordered_map<std::string, ISecurityRule*> rulesById;
    std::vector<VulnerabilityFinding> allFindings;
    std::unordered_map<std::string, std::vector<VulnerabilityFinding>> findingsByFile;
    std::unordered_map<uint64_t, VulnerabilityFinding*> findingsById;
    SecurityAnalyzerStats stats;
    std::atomic<uint64_t> nextFindingId{1};
    mutable std::shared_mutex mutex;
    
    // Cache
    std::unordered_map<std::string, AnalysisResult> analysisCache;
    std::unordered_map<std::string, std::string> fileHashCache;
    
    Impl() = default;
    ~Impl() = default;
};

SecurityAnalyzer::SecurityAnalyzer(LSP::WorkspaceSymbolIndex* index)
    : m_impl(std::make_unique<Impl>()) {
    m_impl->symbolIndex = index;
    Rules::registerBuiltinRules(*this);
}

SecurityAnalyzer::~SecurityAnalyzer() = default;

void SecurityAnalyzer::setConfig(const SecurityAnalyzerConfig& config) {
    std::unique_lock lock(m_impl->mutex);
    m_impl->config = config;
}

SecurityAnalyzerConfig SecurityAnalyzer::getConfig() const {
    std::shared_lock lock(m_impl->mutex);
    return m_impl->config;
}

void SecurityAnalyzer::registerRule(std::unique_ptr<ISecurityRule> rule) {
    std::unique_lock lock(m_impl->mutex);
    std::string id = rule->id();
    m_impl->rulesById[id] = rule.get();
    m_impl->rules.push_back(std::move(rule));
}

void SecurityAnalyzer::unregisterRule(const std::string& ruleId) {
    std::unique_lock lock(m_impl->mutex);
    m_impl->rulesById.erase(ruleId);
    m_impl->rules.erase(
        std::remove_if(m_impl->rules.begin(), m_impl->rules.end(),
            [&ruleId](const auto& r) { return r->id() == ruleId; }),
        m_impl->rules.end());
}

std::vector<std::string> SecurityAnalyzer::getAvailableRules() const {
    std::shared_lock lock(m_impl->mutex);
    std::vector<std::string> result;
    for (const auto& rule : m_impl->rules) {
        result.push_back(rule->id());
    }
    return result;
}

std::vector<std::string> SecurityAnalyzer::getRulesForLanguage(
    const std::string& lang) const {
    std::shared_lock lock(m_impl->mutex);
    std::vector<std::string> result;
    for (const auto& rule : m_impl->rules) {
        auto langs = rule->languages();
        if (langs.empty() || 
            std::find(langs.begin(), langs.end(), lang) != langs.end() ||
            std::find(langs.begin(), langs.end(), "*") != langs.end()) {
            result.push_back(rule->id());
        }
    }
    return result;
}

AnalysisResult SecurityAnalyzer::analyzeFile(const std::string& uri,
                                              const std::string& content,
                                              const std::string& language) {
    auto start = std::chrono::high_resolution_clock::now();
    
    AnalysisResult result;
    result.uri = uri;
    
    // Split content into lines
    AnalysisContext ctx;
    ctx.uri = uri;
    ctx.content = content;
    ctx.language = language;
    ctx.symbolIndex = m_impl->symbolIndex;
    ctx.suppressedRules = m_impl->config.suppressedRules;
    ctx.maxFindings = m_impl->config.maxFindingsPerFile;
    ctx.includeInfo = m_impl->config.includeInfo;
    ctx.includeLow = m_impl->config.includeLow;
    
    // Split into lines
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        ctx.lines.push_back(line);
    }
    
    // Run each rule
    std::shared_lock lock(m_impl->mutex);
    for (const auto& rule : m_impl->rules) {
        if (!rule->isEnabled(ctx.suppressedRules)) {
            continue;
        }
        
        // Check language support
        auto langs = rule->languages();
        if (!langs.empty()) {
            bool supported = false;
            for (const auto& lang : langs) {
                if (lang == "*" || lang == language) {
                    supported = true;
                    break;
                }
            }
            if (!supported) continue;
        }
        
        // Run rule analysis
        auto findings = rule->analyze(ctx);
        
        // Apply severity overrides and filters
        for (auto& finding : findings) {
            // Apply severity override
            auto it = m_impl->config.severityOverrides.find(rule->id());
            if (it != m_impl->config.severityOverrides.end()) {
                finding.severity = it->second;
            }
            
            // Filter by confidence
            if (finding.confidence < m_impl->config.minConfidence) {
                continue;
            }
            
            // Filter by severity
            if (finding.severity == Severity::Info && !ctx.includeInfo) {
                continue;
            }
            if (finding.severity == Severity::Low && !ctx.includeLow) {
                continue;
            }
            
            // Assign ID
            finding.id = m_impl->nextFindingId++;
            
            // Add to results
            result.findings.push_back(std::move(finding));
            
            // Check limit
            if (result.findings.size() >= ctx.maxFindings) {
                break;
            }
        }
        
        if (result.findings.size() >= ctx.maxFindings) {
            break;
        }
    }
    lock.unlock();
    
    // Count severities
    for (const auto& f : result.findings) {
        result.totalFindings++;
        switch (f.severity) {
            case Severity::Critical: result.criticalCount++; break;
            case Severity::High: result.highCount++; break;
            case Severity::Medium: result.mediumCount++; break;
            case Severity::Low: result.lowCount++; break;
            case Severity::Info: result.infoCount++; break;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.analysisTimeMs = 
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    result.success = true;
    
    // Update stats
    std::unique_lock statsLock(m_impl->mutex);
    m_impl->stats.filesAnalyzed++;
    m_impl->stats.totalFindings += result.totalFindings;
    m_impl->stats.criticalFindings += result.criticalCount;
    m_impl->stats.highFindings += result.highCount;
    m_impl->stats.mediumFindings += result.mediumCount;
    m_impl->stats.lowFindings += result.lowCount;
    m_impl->stats.infoFindings += result.infoCount;
    m_impl->stats.totalAnalysisTimeMs += result.analysisTimeMs;
    m_impl->stats.avgAnalysisTimeMs = 
        static_cast<double>(m_impl->stats.totalAnalysisTimeMs) / 
        m_impl->stats.filesAnalyzed;
    
    // Store findings
    for (const auto& f : result.findings) {
        m_impl->allFindings.push_back(f);
        m_impl->findingsByFile[uri].push_back(f);
        m_impl->findingsById[f.id] = &m_impl->allFindings.back();
    }
    
    return result;
}

AnalysisResult SecurityAnalyzer::analyzeFile(const std::string& uri) {
    // Load file from disk
    std::ifstream file(uri);
    if (!file.is_open()) {
        AnalysisResult result;
        result.uri = uri;
        result.success = false;
        result.errorMessage = "Failed to open file: " + uri;
        return result;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    // Detect language from extension
    std::string language = "text";
    fs::path p(uri);
    std::string ext = p.extension().string();
    
    static const std::unordered_map<std::string, std::string> extToLang = {
        {".cpp", "cpp"}, {".cxx", "cpp"}, {".cc", "cpp"}, {".C", "cpp"},
        {".h", "cpp"}, {".hpp", "cpp"}, {".hxx", "cpp"},
        {".c", "c"},
        {".js", "javascript"}, {".mjs", "javascript"}, {".cjs", "javascript"},
        {".ts", "typescript"}, {".tsx", "typescript"},
        {".py", "python"}, {".pyw", "python"},
        {".java", "java"},
        {".cs", "csharp"},
        {".go", "go"},
        {".rs", "rust"},
        {".rb", "ruby"},
        {".php", "php"},
        {".asm", "asm"}, {".s", "asm"},
        {".sql", "sql"},
        {".json", "json"},
        {".xml", "xml"},
        {".html", "html"}, {".htm", "html"},
        {".css", "css"},
        {".sh", "shell"}, {".bash", "shell"},
        {".ps1", "powershell"},
    };
    
    auto it = extToLang.find(ext);
    if (it != extToLang.end()) {
        language = it->second;
    }
    
    return analyzeFile(uri, content, language);
}

std::vector<AnalysisResult> SecurityAnalyzer::analyzeProject(
    const std::vector<std::string>& uris) {
    std::vector<AnalysisResult> results;
    results.reserve(uris.size());
    
    for (const auto& uri : uris) {
        results.push_back(analyzeFile(uri));
    }
    
    return results;
}

std::vector<VulnerabilityFinding> SecurityAnalyzer::getAllFindings() const {
    std::shared_lock lock(m_impl->mutex);
    return m_impl->allFindings;
}

std::vector<VulnerabilityFinding> SecurityAnalyzer::getFindingsByFile(
    const std::string& uri) const {
    std::shared_lock lock(m_impl->mutex);
    auto it = m_impl->findingsByFile.find(uri);
    if (it != m_impl->findingsByFile.end()) {
        return it->second;
    }
    return {};
}

std::vector<VulnerabilityFinding> SecurityAnalyzer::getFindingsByCategory(
    VulnerabilityCategory cat) const {
    std::shared_lock lock(m_impl->mutex);
    std::vector<VulnerabilityFinding> result;
    for (const auto& f : m_impl->allFindings) {
        if (f.category == cat) {
            result.push_back(f);
        }
    }
    return result;
}

std::vector<VulnerabilityFinding> SecurityAnalyzer::getFindingsBySeverity(
    Severity sev) const {
    std::shared_lock lock(m_impl->mutex);
    std::vector<VulnerabilityFinding> result;
    for (const auto& f : m_impl->allFindings) {
        if (f.severity == sev) {
            result.push_back(f);
        }
    }
    return result;
}

void SecurityAnalyzer::markFalsePositive(uint64_t findingId, 
                                          const std::string& reason) {
    std::unique_lock lock(m_impl->mutex);
    auto it = m_impl->findingsById.find(findingId);
    if (it != m_impl->findingsById.end()) {
        it->second->isFalsePositive = true;
        it->second->suppressionReason = reason;
        m_impl->stats.falsePositives++;
    }
}

void SecurityAnalyzer::suppressFinding(uint64_t findingId, 
                                         const std::string& reason) {
    std::unique_lock lock(m_impl->mutex);
    auto it = m_impl->findingsById.find(findingId);
    if (it != m_impl->findingsById.end()) {
        it->second->isSuppressed = true;
        it->second->suppressionReason = reason;
        m_impl->stats.suppressedFindings++;
    }
}

void SecurityAnalyzer::unsuppressFinding(uint64_t findingId) {
    std::unique_lock lock(m_impl->mutex);
    auto it = m_impl->findingsById.find(findingId);
    if (it != m_impl->findingsById.end()) {
        if (it->second->isSuppressed) {
            it->second->isSuppressed = false;
            it->second->suppressionReason.clear();
            m_impl->stats.suppressedFindings--;
        }
    }
}

SecurityAnalyzerStats SecurityAnalyzer::getStats() const {
    std::shared_lock lock(m_impl->mutex);
    return m_impl->stats;
}

void SecurityAnalyzer::resetStats() {
    std::unique_lock lock(m_impl->mutex);
    m_impl->stats = SecurityAnalyzerStats{};
    m_impl->allFindings.clear();
    m_impl->findingsByFile.clear();
    m_impl->findingsById.clear();
    m_impl->analysisCache.clear();
}

void SecurityAnalyzer::clearCache() {
    std::unique_lock lock(m_impl->mutex);
    m_impl->analysisCache.clear();
    m_impl->fileHashCache.clear();
}

std::string SecurityAnalyzer::exportToJson(
    const std::vector<AnalysisResult>& results) const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"results\": [\n";
    
    bool firstResult = true;
    for (const auto& result : results) {
        if (!firstResult) oss << ",\n";
        firstResult = false;
        
        oss << "    {\n";
        oss << "      \"uri\": \"" << result.uri << "\",\n";
        oss << "      \"totalFindings\": " << result.totalFindings << ",\n";
        oss << "      \"criticalCount\": " << result.criticalCount << ",\n";
        oss << "      \"highCount\": " << result.highCount << ",\n";
        oss << "      \"mediumCount\": " << result.mediumCount << ",\n";
        oss << "      \"lowCount\": " << result.lowCount << ",\n";
        oss << "      \"infoCount\": " << result.infoCount << ",\n";
        oss << "      \"analysisTimeMs\": " << result.analysisTimeMs << ",\n";
        oss << "      \"findings\": [\n";
        
        bool firstFinding = true;
        for (const auto& f : result.findings) {
            if (!firstFinding) oss << ",\n";
            firstFinding = false;
            
            oss << "        {\n";
            oss << "          \"id\": " << f.id << ",\n";
            oss << "          \"category\": " << static_cast<int>(f.category) << ",\n";
            oss << "          \"severity\": " << static_cast<int>(f.severity) << ",\n";
            oss << "          \"title\": \"" << f.title << "\",\n";
            oss << "          \"cwe\": \"" << f.cwe << "\",\n";
            oss << "          \"confidence\": " << f.confidence << ",\n";
            oss << "          \"line\": " << f.location.line << ",\n";
            oss << "          \"column\": " << f.location.column << "\n";
            oss << "        }";
        }
        
        oss << "\n      ]\n";
        oss << "    }";
    }
    
    oss << "\n  ]\n";
    oss << "}\n";
    
    return oss.str();
}

// ============================================================================
// Built-in Security Rules Implementation
// ============================================================================

namespace Rules {

// ---------------------------------------------------------------------------
// SQL Injection Rule
// ---------------------------------------------------------------------------
class SQLInjectionRule : public ISecurityRule {
public:
    std::string id() const override { return "SEC001"; }
    std::string name() const override { return "SQL Injection"; }
    std::string description() const override {
        return "Detects potential SQL injection vulnerabilities";
    }
    VulnerabilityCategory category() const override { 
        return VulnerabilityCategory::SQLInjection; 
    }
    Severity defaultSeverity() const override { return Severity::High; }
    std::vector<std::string> languages() const override {
        return {"cpp", "c", "javascript", "typescript", "python", "java", "csharp", "php", "go"};
    }
    
    std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
        std::vector<VulnerabilityFinding> findings;
        
        // Patterns for SQL injection (simplified to avoid regex escaping issues)
        static const std::vector<std::pair<std::regex, std::string>> patterns = {
            // String concatenation in SQL
            {std::regex(R"((SELECT|INSERT|UPDATE|DELETE|EXEC|EXECUTE)\s+.*\+.*)", 
                       std::regex_constants::icase), "SQL string concatenation"},
            // Format strings in SQL
            {std::regex(R"((SELECT|INSERT|UPDATE|DELETE|EXEC|EXECUTE)\s+.*%[sd])", 
                       std::regex_constants::icase), "SQL format string"},
            // sprintf with SQL
            {std::regex(R"(sprintf\s*\([^)]*(SELECT|INSERT|UPDATE|DELETE))", 
                       std::regex_constants::icase), "sprintf with SQL"},
            // Python f-strings with SQL
            {std::regex(R"(f[\"'].*SELECT.*\{)", 
                       std::regex_constants::icase), "Python f-string SQL"},
            // JavaScript template literals with SQL
            {std::regex(R"(`[^`]*(SELECT|INSERT|UPDATE|DELETE)[^`]*\$\{)", 
                       std::regex_constants::icase), "JS template literal SQL"},
        };
        
        for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
            const std::string& line = ctx.lines[i];
            
            for (const auto& [pattern, desc] : patterns) {
                std::smatch match;
                if (std::regex_search(line, match, pattern)) {
                    VulnerabilityFinding f;
                    f.id = 0; // Will be assigned by analyzer
                    f.category = VulnerabilityCategory::SQLInjection;
                    f.severity = defaultSeverity();
                    f.title = "Potential SQL Injection: " + desc;
                    f.description = "SQL query constructed with user-controllable input. "
                                   "Use parameterized queries or prepared statements.";
                    f.location.uri = ctx.uri;
                    f.location.line = i + 1;
                    f.location.column = match.position() + 1;
                    f.location.snippet = line;
                    f.cwe = "CWE-89";
                    f.owasp = "A03:2021 - Injection";
                    f.remediation = "Use parameterized queries or prepared statements. "
                                   "Never concatenate user input into SQL strings.";
                    f.confidence = 0.7f;
                    f.ruleId = id();
                    findings.push_back(std::move(f));
                }
            }
        }
        
        return findings;
    }
};

std::unique_ptr<ISecurityRule> createSQLInjectionRule() {
    return std::make_unique<SQLInjectionRule>();
}

// ---------------------------------------------------------------------------
// Command Injection Rule
// ---------------------------------------------------------------------------
class CommandInjectionRule : public ISecurityRule {
public:
    std::string id() const override { return "SEC002"; }
    std::string name() const override { return "Command Injection"; }
    std::string description() const override {
        return "Detects potential OS command injection vulnerabilities";
    }
    VulnerabilityCategory category() const override { 
        return VulnerabilityCategory::CommandInjection; 
    }
    Severity defaultSeverity() const override { return Severity::Critical; }
    std::vector<std::string> languages() const override {
        return {"cpp", "c", "javascript", "typescript", "python", "java", "csharp", "php", "go", "ruby"};
    }
    
    std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
        std::vector<VulnerabilityFinding> findings;
        
        // Dangerous functions
        static const std::unordered_map<std::string, std::string> dangerousFuncs = {
            {"system(", "system() call"},
            {"popen(", "popen() call"},
            {"exec(", "exec() call"},
            {"execve(", "execve() call"},
            {"execl(", "execl() call"},
            {"execlp(", "execlp() call"},
            {"ShellExecute(", "ShellExecute() call"},
            {"WinExec(", "WinExec() call"},
            {"subprocess.call(", "subprocess.call()"},
            {"subprocess.Popen(", "subprocess.Popen()"},
            {"os.system(", "os.system()"},
            {"os.popen(", "os.popen()"},
            {"eval(", "eval() call"},
            {"child_process.exec(", "child_process.exec()"},
            {"child_process.spawn(", "child_process.spawn() with shell"},
        };
        
        for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
            const std::string& line = ctx.lines[i];
            
            for (const auto& [func, desc] : dangerousFuncs) {
                size_t pos = line.find(func);
                if (pos != std::string::npos) {
                    VulnerabilityFinding f;
                    f.category = VulnerabilityCategory::CommandInjection;
                    f.severity = defaultSeverity();
                    f.title = "Potential Command Injection: " + desc;
                    f.description = "OS command execution with potentially "
                                   "user-controllable input.";
                    f.location.uri = ctx.uri;
                    f.location.line = i + 1;
                    f.location.column = pos + 1;
                    f.location.snippet = line;
                    f.cwe = "CWE-78";
                    f.owasp = "A03:2021 - Injection";
                    f.remediation = "Avoid shell execution with user input. "
                                   "Use allowlists for commands and arguments.";
                    f.confidence = 0.8f;
                    f.ruleId = id();
                    findings.push_back(std::move(f));
                }
            }
        }
        
        return findings;
    }
};

std::unique_ptr<ISecurityRule> createCommandInjectionRule() {
    return std::make_unique<CommandInjectionRule>();
}

// ---------------------------------------------------------------------------
// Path Traversal Rule
// ---------------------------------------------------------------------------
class PathTraversalRule : public ISecurityRule {
public:
    std::string id() const override { return "SEC003"; }
    std::string name() const override { return "Path Traversal"; }
    std::string description() const override {
        return "Detects potential path traversal vulnerabilities";
    }
    VulnerabilityCategory category() const override { 
        return VulnerabilityCategory::PathTraversal; 
    }
    Severity defaultSeverity() const override { return Severity::High; }
    std::vector<std::string> languages() const override { return {"*"}; }
    
    std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
        std::vector<VulnerabilityFinding> findings;
        
        // Patterns for path operations with user input
        static const std::vector<std::pair<std::regex, std::string>> patterns = {
            {std::regex(R"((fopen|ifstream|ofstream|open|readFile|writeFile|fs\.read|fs\.write)\s*\([^)]*\+)"), 
             "File operation with concatenated path"},
            {std::regex(R"(Path\.Combine\s*\([^)]*\+)"), 
             "Path.Combine with concatenated input"},
            {std::regex(R"(os\.path\.join\s*\([^)]*\+)"), 
             "os.path.join with concatenated input"},
            {std::regex(R"(\.\.[/\\\\])"), 
             "Directory traversal sequence"},
        };
        
        for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
            const std::string& line = ctx.lines[i];
            
            for (const auto& [pattern, desc] : patterns) {
                std::smatch match;
                if (std::regex_search(line, match, pattern)) {
                    VulnerabilityFinding f;
                    f.category = VulnerabilityCategory::PathTraversal;
                    f.severity = defaultSeverity();
                    f.title = "Potential Path Traversal: " + desc;
                    f.description = "File path operation with potentially "
                                   "user-controllable input.";
                    f.location.uri = ctx.uri;
                    f.location.line = i + 1;
                    f.location.column = match.position() + 1;
                    f.location.snippet = line;
                    f.cwe = "CWE-22";
                    f.owasp = "A01:2021 - Broken Access Control";
                    f.remediation = "Validate and sanitize file paths. "
                                   "Use allowlists for allowed directories.";
                    f.confidence = 0.7f;
                    f.ruleId = id();
                    findings.push_back(std::move(f));
                }
            }
        }
        
        return findings;
    }
};

std::unique_ptr<ISecurityRule> createPathTraversalRule() {
    return std::make_unique<PathTraversalRule>();
}

// ---------------------------------------------------------------------------
// Hardcoded Credentials Rule
// ---------------------------------------------------------------------------
class HardcodedCredentialsRule : public ISecurityRule {
public:
    std::string id() const override { return "SEC004"; }
    std::string name() const override { return "Hardcoded Credentials"; }
    std::string description() const override {
        return "Detects hardcoded passwords, API keys, and secrets";
    }
    VulnerabilityCategory category() const override { 
        return VulnerabilityCategory::HardcodedSecret; 
    }
    Severity defaultSeverity() const override { return Severity::High; }
    std::vector<std::string> languages() const override { return {"*"}; }
    
    std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
        std::vector<VulnerabilityFinding> findings;
        
        // Patterns for hardcoded secrets (simplified)
        static const std::vector<std::pair<std::regex, std::string>> patterns = {
            {std::regex(R"((password|passwd|pwd)\s*[=:]\s*[\"'][^\"']+[\"'])", 
                       std::regex_constants::icase), "Hardcoded password"},
            {std::regex(R"((api[_-]?key|apikey)\s*[=:]\s*[\"'][a-zA-Z0-9]{16,}[\"'])", 
                       std::regex_constants::icase), "Hardcoded API key"},
            {std::regex(R"((secret|token)\s*[=:]\s*[\"'][a-zA-Z0-9]{16,}[\"'])", 
                       std::regex_constants::icase), "Hardcoded secret/token"},
            {std::regex(R"((aws|azure|gcp)[_-]?(key|secret|token)\s*[=:]\s*[\"'][^\"']+[\"'])", 
                       std::regex_constants::icase), "Cloud provider secret"},
            {std::regex(R"(Bearer\s+[a-zA-Z0-9_-]{20,})"), "Hardcoded bearer token"},
            {std::regex(R"((private[_-]?key|rsa[_-]?key)\s*[=:]\s*[\"']-----BEGIN)", 
                       std::regex_constants::icase), "Hardcoded private key"},
        };
        
        for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
            const std::string& line = ctx.lines[i];
            
            for (const auto& [pattern, desc] : patterns) {
                std::smatch match;
                if (std::regex_search(line, match, pattern)) {
                    VulnerabilityFinding f;
                    f.category = VulnerabilityCategory::HardcodedSecret;
                    f.severity = defaultSeverity();
                    f.title = desc;
                    f.description = "Hardcoded credential detected in source code. "
                                   "Secrets should be stored securely.";
                    f.location.uri = ctx.uri;
                    f.location.line = i + 1;
                    f.location.column = match.position() + 1;
                    f.location.snippet = line;
                    f.cwe = "CWE-798";
                    f.owasp = "A07:2021 - Identification and Authentication Failures";
                    f.remediation = "Use environment variables, secure vaults, "
                                   "or configuration files outside source control.";
                    f.confidence = 0.9f;
                    f.ruleId = id();
                    findings.push_back(std::move(f));
                }
            }
        }
        
        return findings;
    }
};

std::unique_ptr<ISecurityRule> createHardcodedCredentialsRule() {
    return std::make_unique<HardcodedCredentialsRule>();
}

// ---------------------------------------------------------------------------
// Buffer Overflow Rule (C/C++)
// ---------------------------------------------------------------------------
class BufferOverflowRule : public ISecurityRule {
public:
    std::string id() const override { return "SEC005"; }
    std::string name() const override { return "Buffer Overflow"; }
    std::string description() const override {
        return "Detects potential buffer overflow vulnerabilities";
    }
    VulnerabilityCategory category() const override { 
        return VulnerabilityCategory::BufferOverflow; 
    }
    Severity defaultSeverity() const override { return Severity::Critical; }
    std::vector<std::string> languages() const override { 
        return {"cpp", "c"}; 
    }
    
    std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
        std::vector<VulnerabilityFinding> findings;
        
        // Dangerous functions without bounds checking
        static const std::unordered_map<std::string, std::string> dangerousFuncs = {
            {"strcpy(", "strcpy() - no bounds check"},
            {"strcat(", "strcat() - no bounds check"},
            {"sprintf(", "sprintf() - no bounds check"},
            {"gets(", "gets() - extremely dangerous"},
            {"scanf(", "scanf() - potential overflow"},
            {"vsprintf(", "vsprintf() - no bounds check"},
            {"memcpy(", "memcpy() - check size parameter"},
            {"memmove(", "memmove() - check size parameter"},
        };
        
        for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
            const std::string& line = ctx.lines[i];
            
            for (const auto& [func, desc] : dangerousFuncs) {
                size_t pos = line.find(func);
                if (pos != std::string::npos) {
                    VulnerabilityFinding f;
                    f.category = VulnerabilityCategory::BufferOverflow;
                    f.severity = defaultSeverity();
                    f.title = "Potential Buffer Overflow: " + desc;
                    f.description = "Use of unsafe function that can cause buffer overflow.";
                    f.location.uri = ctx.uri;
                    f.location.line = i + 1;
                    f.location.column = pos + 1;
                    f.location.snippet = line;
                    f.cwe = "CWE-119";
                    f.remediation = "Use safe alternatives: strcpy_s, strcat_s, "
                                   "snprintf, fgets, memcpy_s.";
                    f.confidence = 0.85f;
                    f.ruleId = id();
                    findings.push_back(std::move(f));
                }
            }
        }
        
        return findings;
    }
};

std::unique_ptr<ISecurityRule> createBufferOverflowRule() {
    return std::make_unique<BufferOverflowRule>();
}

// ---------------------------------------------------------------------------
// Weak Cryptography Rule
// ---------------------------------------------------------------------------
class WeakCryptoRule : public ISecurityRule {
public:
    std::string id() const override { return "SEC006"; }
    std::string name() const override { return "Weak Cryptography"; }
    std::string description() const override {
        return "Detects use of weak cryptographic algorithms";
    }
    VulnerabilityCategory category() const override { 
        return VulnerabilityCategory::WeakCrypto; 
    }
    Severity defaultSeverity() const override { return Severity::High; }
    std::vector<std::string> languages() const override { return {"*"}; }
    
    std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
        std::vector<VulnerabilityFinding> findings;
        
        static const std::vector<std::pair<std::regex, std::string>> patterns = {
            {std::regex(R"((MD5|md5)\s*\(|hashlib\.md5|MD5CryptoServiceProvider)", 
                       std::regex_constants::icase), "MD5 - cryptographically broken"},
            {std::regex(R"((SHA1|sha1)\s*\(|hashlib\.sha1|SHA1CryptoServiceProvider)", 
                       std::regex_constants::icase), "SHA1 - deprecated"},
            {std::regex(R"((DES|des)\s*\(|DESCryptoServiceProvider|Cipher\.DES)", 
                       std::regex_constants::icase), "DES - insufficient key size"},
            {std::regex(R"((RC4|rc4|ARC4))", 
                       std::regex_constants::icase), "RC4 - broken stream cipher"},
            {std::regex(R"((ECB|ecb)\s*mode|AES_.*_ECB)", 
                       std::regex_constants::icase), "ECB mode - insecure"},
            {std::regex(R"(rand\s*\(\)|Math\.random\(\)|random\.random\(\)"), 
                       "Weak PRNG for security purposes"},
        };
        
        for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
            const std::string& line = ctx.lines[i];
            
            for (const auto& [pattern, desc] : patterns) {
                std::smatch match;
                if (std::regex_search(line, match, pattern)) {
                    VulnerabilityFinding f;
                    f.category = VulnerabilityCategory::WeakCrypto;
                    f.severity = defaultSeverity();
                    f.title = "Weak Cryptography: " + desc;
                    f.description = "Use of weak or deprecated cryptographic algorithm.";
                    f.location.uri = ctx.uri;
                    f.location.line = i + 1;
                    f.location.column = match.position() + 1;
                    f.location.snippet = line;
                    f.cwe = "CWE-327";
                    f.remediation = "Use strong algorithms: SHA-256+, AES-GCM, "
                                   "ChaCha20-Poly1305.";
                    f.confidence = 0.9f;
                    f.ruleId = id();
                    findings.push_back(std::move(f));
                }
            }
        }
        
        return findings;
    }
};

std::unique_ptr<ISecurityRule> createWeakCryptoRule() {
    return std::make_unique<WeakCryptoRule>();
}

// ---------------------------------------------------------------------------
// XSS Rule
// ---------------------------------------------------------------------------
std::unique_ptr<ISecurityRule> createXSSRule() {
    // Simplified XSS detection
    class XSSRule : public ISecurityRule {
    public:
        std::string id() const override { return "SEC007"; }
        std::string name() const override { return "Cross-Site Scripting"; }
        std::string description() const override {
            return "Detects potential XSS vulnerabilities";
        }
        VulnerabilityCategory category() const override { 
            return VulnerabilityCategory::XSS; 
        }
        Severity defaultSeverity() const override { return Severity::High; }
        std::vector<std::string> languages() const override { 
            return {"javascript", "typescript", "html"}; 
        }
        
        std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
            std::vector<VulnerabilityFinding> findings;
            
            static const std::vector<std::pair<std::regex, std::string>> patterns = {
                {std::regex(R"(innerHTML\s*=\s*[^;]*\+)"), 
                 "innerHTML assignment with concatenation"},
                {std::regex(R"(document\.write\s*\()"), 
                 "document.write() - XSS risk"},
                {std::regex(R"(\$\{[^}]*\}.*<)"), 
                 "Template literal in HTML context"},
                {std::regex(R"(dangerouslySetInnerHTML)"), 
                 "React dangerouslySetInnerHTML"},
                {std::regex(R"(v-html\s*=)"), 
                 "Vue v-html directive"},
            };
            
            for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
                const std::string& line = ctx.lines[i];
                
                for (const auto& [pattern, desc] : patterns) {
                    std::smatch match;
                    if (std::regex_search(line, match, pattern)) {
                        VulnerabilityFinding f;
                        f.category = VulnerabilityCategory::XSS;
                        f.severity = defaultSeverity();
                        f.title = "Potential XSS: " + desc;
                        f.description = "User input rendered without proper escaping.";
                        f.location.uri = ctx.uri;
                        f.location.line = i + 1;
                        f.location.column = match.position() + 1;
                        f.location.snippet = line;
                        f.cwe = "CWE-79";
                        f.owasp = "A03:2021 - Injection";
                        f.remediation = "Sanitize user input. Use textContent instead "
                                       "of innerHTML. Apply output encoding.";
                        f.confidence = 0.75f;
                        f.ruleId = id();
                        findings.push_back(std::move(f));
                    }
                }
            }
            
            return findings;
        }
    };
    
    return std::make_unique<XSSRule>();
}

// ---------------------------------------------------------------------------
// Format String Rule
// ---------------------------------------------------------------------------
std::unique_ptr<ISecurityRule> createFormatStringRule() {
    class FormatStringRule : public ISecurityRule {
    public:
        std::string id() const override { return "SEC008"; }
        std::string name() const override { return "Format String"; }
        std::string description() const override {
            return "Detects format string vulnerabilities";
        }
        VulnerabilityCategory category() const override { 
            return VulnerabilityCategory::FormatString; 
        }
        Severity defaultSeverity() const override { return Severity::High; }
        std::vector<std::string> languages() const override { return {"cpp", "c"}; }
        
        std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
            std::vector<VulnerabilityFinding> findings;
            
            // printf family with user-controlled format string
            static const std::regex pattern(
                R"((printf|fprintf|sprintf|snprintf|vprintf|vfprintf|vsprintf|vsnprintf)\s*\(\s*[^,]*\))");
            
            for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
                const std::string& line = ctx.lines[i];
                std::smatch match;
                
                if (std::regex_search(line, match, pattern)) {
                    VulnerabilityFinding f;
                    f.category = VulnerabilityCategory::FormatString;
                    f.severity = defaultSeverity();
                    f.title = "Potential Format String Vulnerability";
                    f.description = "printf-family function with potentially "
                                   "user-controlled format string.";
                    f.location.uri = ctx.uri;
                    f.location.line = i + 1;
                    f.location.column = match.position() + 1;
                    f.location.snippet = line;
                    f.cwe = "CWE-134";
                    f.remediation = "Always use format string literals. "
                                   "Never pass user input as format string.";
                    f.confidence = 0.8f;
                    f.ruleId = id();
                    findings.push_back(std::move(f));
                }
            }
            
            return findings;
        }
    };
    
    return std::make_unique<FormatStringRule>();
}

// ---------------------------------------------------------------------------
// Integer Overflow Rule
// ---------------------------------------------------------------------------
std::unique_ptr<ISecurityRule> createIntegerOverflowRule() {
    class IntegerOverflowRule : public ISecurityRule {
    public:
        std::string id() const override { return "SEC009"; }
        std::string name() const override { return "Integer Overflow"; }
        std::string description() const override {
            return "Detects potential integer overflow vulnerabilities";
        }
        VulnerabilityCategory category() const override { 
            return VulnerabilityCategory::IntegerOverflow; 
        }
        Severity defaultSeverity() const override { return Severity::Medium; }
        std::vector<std::string> languages() const override { return {"cpp", "c"}; }
        
        std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
            std::vector<VulnerabilityFinding> findings;
            
            // malloc/new with arithmetic
            static const std::regex pattern(
                R"((malloc|calloc|realloc|new)\s*[\(\[]\s*[^)\]]*\s*[\+\*]\s*[^)\]]*)");
            
            for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
                const std::string& line = ctx.lines[i];
                std::smatch match;
                
                if (std::regex_search(line, match, pattern)) {
                    VulnerabilityFinding f;
                    f.category = VulnerabilityCategory::IntegerOverflow;
                    f.severity = defaultSeverity();
                    f.title = "Potential Integer Overflow in Memory Allocation";
                    f.description = "Memory allocation with arithmetic that could overflow.";
                    f.location.uri = ctx.uri;
                    f.location.line = i + 1;
                    f.location.column = match.position() + 1;
                    f.location.snippet = line;
                    f.cwe = "CWE-190";
                    f.remediation = "Check for overflow before allocation. "
                                   "Use safe integer arithmetic functions.";
                    f.confidence = 0.6f;
                    f.ruleId = id();
                    findings.push_back(std::move(f));
                }
            }
            
            return findings;
        }
    };
    
    return std::make_unique<IntegerOverflowRule>();
}

// ---------------------------------------------------------------------------
// Null Pointer Rule
// ---------------------------------------------------------------------------
std::unique_ptr<ISecurityRule> createNullPointerRule() {
    class NullPointerRule : public ISecurityRule {
    public:
        std::string id() const override { return "SEC010"; }
        std::string name() const override { return "Null Pointer Dereference"; }
        std::string description() const override {
            return "Detects potential null pointer dereferences";
        }
        VulnerabilityCategory category() const override { 
            return VulnerabilityCategory::NullPointerDeref; 
        }
        Severity defaultSeverity() const override { return Severity::Medium; }
        std::vector<std::string> languages() const override { return {"cpp", "c"}; }
        
        std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
            std::vector<VulnerabilityFinding> findings;
            
            // Pointer dereference without null check (simplified)
            static const std::regex pattern(R"(\*\s*\w+\s*->|->\s*\w+\s*\()"); 
            
            for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
                const std::string& line = ctx.lines[i];
                std::smatch match;
                
                if (std::regex_search(line, match, pattern)) {
                    // Check if there's a null check nearby (simplified)
                    bool hasNullCheck = false;
                    if (i > 0) {
            static const std::regex nullCheck(R"(if\s*\(\s*!?\s*\w+\s*\))");
                        hasNullCheck = std::regex_search(ctx.lines[i-1], nullCheck);
                    }
                    
                    if (!hasNullCheck) {
                        VulnerabilityFinding f;
                        f.category = VulnerabilityCategory::NullPointerDeref;
                        f.severity = defaultSeverity();
                        f.title = "Potential Null Pointer Dereference";
                        f.description = "Pointer dereference without visible null check.";
                        f.location.uri = ctx.uri;
                        f.location.line = i + 1;
                        f.location.column = match.position() + 1;
                        f.location.snippet = line;
                        f.cwe = "CWE-476";
                        f.remediation = "Add null pointer check before dereference.";
                        f.confidence = 0.5f; // Lower confidence due to false positives
                        f.ruleId = id();
                        findings.push_back(std::move(f));
                    }
                }
            }
            
            return findings;
        }
    };
    
    return std::make_unique<NullPointerRule>();
}

// ---------------------------------------------------------------------------
// Use After Free Rule
// ---------------------------------------------------------------------------
std::unique_ptr<ISecurityRule> createUseAfterFreeRule() {
    class UseAfterFreeRule : public ISecurityRule {
    public:
        std::string id() const override { return "SEC011"; }
        std::string name() const override { return "Use After Free"; }
        std::string description() const override {
            return "Detects potential use-after-free vulnerabilities";
        }
        VulnerabilityCategory category() const override { 
            return VulnerabilityCategory::UseAfterFree; 
        }
        Severity defaultSeverity() const override { return Severity::High; }
        std::vector<std::string> languages() const override { return {"cpp", "c"}; }
        
        std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
            std::vector<VulnerabilityFinding> findings;
            
            // Track free/delete and subsequent use (simplified)
            std::unordered_map<std::string, uint32_t> freedVars;
            
            static const std::regex freePattern(R"((free|delete)\s*\(\s*(\w+)\s*\))");
            static const std::regex usePattern(R"((\w+)\s*(->|\.|\[))");
            
            for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
                const std::string& line = ctx.lines[i];
                
                // Check for free
                std::smatch freeMatch;
                if (std::regex_search(line, freeMatch, freePattern)) {
                    freedVars[freeMatch[2].str()] = i + 1;
                }
                
                // Check for use of freed variable
                std::smatch useMatch;
                if (std::regex_search(line, useMatch, usePattern)) {
                    auto it = freedVars.find(useMatch[1].str());
                    if (it != freedVars.end()) {
                        VulnerabilityFinding f;
                        f.category = VulnerabilityCategory::UseAfterFree;
                        f.severity = defaultSeverity();
                        f.title = "Potential Use After Free";
                        f.description = "Variable used after being freed at line " + 
                                       std::to_string(it->second);
                        f.location.uri = ctx.uri;
                        f.location.line = i + 1;
                        f.location.column = useMatch.position() + 1;
                        f.location.snippet = line;
                        f.cwe = "CWE-416";
                        f.remediation = "Set pointer to NULL after free. "
                                       "Avoid using freed memory.";
                        f.confidence = 0.6f;
                        f.ruleId = id();
                        findings.push_back(std::move(f));
                    }
                }
            }
            
            return findings;
        }
    };
    
    return std::make_unique<UseAfterFreeRule>();
}

// ---------------------------------------------------------------------------
// Insecure Random Rule
// ---------------------------------------------------------------------------
std::unique_ptr<ISecurityRule> createInsecureRandomRule() {
    class InsecureRandomRule : public ISecurityRule {
    public:
        std::string id() const override { return "SEC012"; }
        std::string name() const override { return "Insecure Random"; }
        std::string description() const override {
            return "Detects use of insecure random number generators for security";
        }
        VulnerabilityCategory category() const override { 
            return VulnerabilityCategory::InsecureRandom; 
        }
        Severity defaultSeverity() const override { return Severity::Medium; }
        std::vector<std::string> languages() const override { return {"*"}; }
        
        std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
            std::vector<VulnerabilityFinding> findings;
            
            static const std::vector<std::pair<std::regex, std::string>> patterns = {
                {std::regex(R"((rand\s*\(\)|srand\s*\())"), "rand()/srand() - not crypto-safe"},
                {std::regex(R"(Math\.random\s*\(\))"), "Math.random() - not crypto-safe"},
                {std::regex(R"(random\.random\s*\(\))"), "Python random - not crypto-safe"},
                {std::regex(R"(new Random\s*\(\))"), "Java Random - not crypto-safe"},
            };
            
            for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
                const std::string& line = ctx.lines[i];
                
                for (const auto& [pattern, desc] : patterns) {
                    std::smatch match;
                    if (std::regex_search(line, match, pattern)) {
                        VulnerabilityFinding f;
                        f.category = VulnerabilityCategory::InsecureRandom;
                        f.severity = defaultSeverity();
                        f.title = "Insecure Random Number Generator: " + desc;
                        f.description = "Non-cryptographic PRNG used in potentially "
                                       "security-sensitive context.";
                        f.location.uri = ctx.uri;
                        f.location.line = i + 1;
                        f.location.column = match.position() + 1;
                        f.location.snippet = line;
                        f.cwe = "CWE-338";
                        f.remediation = "Use cryptographically secure RNG: "
                                       "CryptGenRandom, BCryptGenRandom, "
                                       "crypto.getRandomValues, secrets module.";
                        f.confidence = 0.7f;
                        f.ruleId = id();
                        findings.push_back(std::move(f));
                    }
                }
            }
            
            return findings;
        }
    };
    
    return std::make_unique<InsecureRandomRule>();
}

// ---------------------------------------------------------------------------
// Debug Exposure Rule
// ---------------------------------------------------------------------------
std::unique_ptr<ISecurityRule> createDebugExposureRule() {
    class DebugExposureRule : public ISecurityRule {
    public:
        std::string id() const override { return "SEC013"; }
        std::string name() const override { return "Debug Information Exposure"; }
        std::string description() const override {
            return "Detects debug code that may expose sensitive information";
        }
        VulnerabilityCategory category() const override { 
            return VulnerabilityCategory::DebugEnabled; 
        }
        Severity defaultSeverity() const override { return Severity::Low; }
        std::vector<std::string> languages() const override { return {"*"}; }
        
        std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
            std::vector<VulnerabilityFinding> findings;
            
            static const std::vector<std::pair<std::regex, std::string>> patterns = {
                {std::regex(R"(console\.log\s*\()"), "console.log() - remove in production"},
                {std::regex(R"(console\.debug\s*\()"), "console.debug() - remove in production"},
                {std::regex(R"(print\s*\()"), "print() - remove in production"},
                {std::regex(R"(printf\s*\(\s*DEBUG)"), "Debug printf - remove in production"},
                {std::regex(R"(DEBUG\s*=\s*true)", std::regex_constants::icase), 
                 "DEBUG flag enabled"},
                {std::regex(R"((assert|ASSERT)\s*\()"), "Assert statement - may expose info"},
                {std::regex(R"(TODO|FIXME|HACK|XXX)"), "TODO/FIXME comment"},
            };
            
            for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
                const std::string& line = ctx.lines[i];
                
                for (const auto& [pattern, desc] : patterns) {
                    std::smatch match;
                    if (std::regex_search(line, match, pattern)) {
                        VulnerabilityFinding f;
                        f.category = VulnerabilityCategory::DebugEnabled;
                        f.severity = defaultSeverity();
                        f.title = "Debug Information: " + desc;
                        f.description = "Debug code that may expose sensitive "
                                       "information in production.";
                        f.location.uri = ctx.uri;
                        f.location.line = i + 1;
                        f.location.column = match.position() + 1;
                        f.location.snippet = line;
                        f.cwe = "CWE-215";
                        f.remediation = "Remove debug code before production deployment.";
                        f.confidence = 0.9f;
                        f.ruleId = id();
                        findings.push_back(std::move(f));
                    }
                }
            }
            
            return findings;
        }
    };
    
    return std::make_unique<DebugExposureRule>();
}

// ---------------------------------------------------------------------------
// Input Validation Rule
// ---------------------------------------------------------------------------
std::unique_ptr<ISecurityRule> createInputValidationRule() {
    class InputValidationRule : public ISecurityRule {
    public:
        std::string id() const override { return "SEC014"; }
        std::string name() const override { return "Missing Input Validation"; }
        std::string description() const override {
            return "Detects missing input validation";
        }
        VulnerabilityCategory category() const override { 
            return VulnerabilityCategory::ImproperValidation; 
        }
        Severity defaultSeverity() const override { return Severity::Medium; }
        std::vector<std::string> languages() const override { return {"*"}; }
        
        std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
            std::vector<VulnerabilityFinding> findings;
            
            // Simplified: look for request/body/params access without validation
            static const std::vector<std::regex> patterns = {
                std::regex(R"(req\.(body|params|query)\.\w+)"),
                std::regex(R"(\$_(GET|POST|REQUEST)\[)"),
                std::regex(R"(request\.(form|args|data)\[)"),
                std::regex(R"(input\s*\(|scanf\s*\()"),
            };
            
            for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
                const std::string& line = ctx.lines[i];
                
                for (const auto& pattern : patterns) {
                    std::smatch match;
                    if (std::regex_search(line, match, pattern)) {
                        VulnerabilityFinding f;
                        f.category = VulnerabilityCategory::ImproperValidation;
                        f.severity = defaultSeverity();
                        f.title = "Potential Missing Input Validation";
                        f.description = "User input accessed without visible validation.";
                        f.location.uri = ctx.uri;
                        f.location.line = i + 1;
                        f.location.column = match.position() + 1;
                        f.location.snippet = line;
                        f.cwe = "CWE-20";
                        f.remediation = "Validate all user input: type, length, "
                                       "format, and allowlist values.";
                        f.confidence = 0.5f;
                        f.ruleId = id();
                        findings.push_back(std::move(f));
                    }
                }
            }
            
            return findings;
        }
    };
    
    return std::make_unique<InputValidationRule>();
}

// ---------------------------------------------------------------------------
// Race Condition Rule
// ---------------------------------------------------------------------------
std::unique_ptr<ISecurityRule> createRaceConditionRule() {
    class RaceConditionRule : public ISecurityRule {
    public:
        std::string id() const override { return "SEC015"; }
        std::string name() const override { return "Race Condition"; }
        std::string description() const override {
            return "Detects potential race conditions";
        }
        VulnerabilityCategory category() const override { 
            return VulnerabilityCategory::RaceCondition; 
        }
        Severity defaultSeverity() const override { return Severity::Medium; }
        std::vector<std::string> languages() const override { return {"cpp", "c", "java", "go"}; }
        
        std::vector<VulnerabilityFinding> analyze(const AnalysisContext& ctx) override {
            std::vector<VulnerabilityFinding> findings;
            
            // TOCTOU patterns
            static const std::regex pattern(
                R"((access|stat|lstat|exists|file_exists)\s*\([^)]*\)[^;]*(open|fopen|create|mkdir))");
            
            for (uint32_t i = 0; i < ctx.lines.size(); ++i) {
                const std::string& line = ctx.lines[i];
                std::smatch match;
                
                if (std::regex_search(line, match, pattern)) {
                    VulnerabilityFinding f;
                    f.category = VulnerabilityCategory::TOCTOU;
                    f.severity = defaultSeverity();
                    f.title = "Potential TOCTOU Race Condition";
                    f.description = "Time-of-check to time-of-use race condition.";
                    f.location.uri = ctx.uri;
                    f.location.line = i + 1;
                    f.location.column = match.position() + 1;
                    f.location.snippet = line;
                    f.cwe = "CWE-367";
                    f.remediation = "Use atomic operations or proper locking. "
                                   "Avoid check-then-act patterns.";
                    f.confidence = 0.6f;
                    f.ruleId = id();
                    findings.push_back(std::move(f));
                }
            }
            
            return findings;
        }
    };
    
    return std::make_unique<RaceConditionRule>();
}

// ---------------------------------------------------------------------------
// Register all built-in rules
// ---------------------------------------------------------------------------
void registerBuiltinRules(SecurityAnalyzer& analyzer) {
    analyzer.registerRule(createSQLInjectionRule());
    analyzer.registerRule(createCommandInjectionRule());
    analyzer.registerRule(createPathTraversalRule());
    analyzer.registerRule(createXSSRule());
    analyzer.registerRule(createBufferOverflowRule());
    analyzer.registerRule(createHardcodedCredentialsRule());
    analyzer.registerRule(createWeakCryptoRule());
    analyzer.registerRule(createFormatStringRule());
    analyzer.registerRule(createIntegerOverflowRule());
    analyzer.registerRule(createNullPointerRule());
    analyzer.registerRule(createUseAfterFreeRule());
    analyzer.registerRule(createInsecureRandomRule());
    analyzer.registerRule(createDebugExposureRule());
    analyzer.registerRule(createInputValidationRule());
    analyzer.registerRule(createRaceConditionRule());
}

} // namespace Rules

} // namespace RawrXD::CodeReview
