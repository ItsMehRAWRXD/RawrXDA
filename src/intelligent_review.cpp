// intelligent_review.cpp
// INTELLIGENT CODE REVIEW SYSTEM Implementation
// AI-Powered Multi-Model Code Analysis

#include "intelligent_review.hpp"
#include <sstream>
#include <algorithm>
#include <regex>
#include <fstream>
#include <cmath>
#include <thread>
#include <future>

namespace review {

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor & Destructor
// ═══════════════════════════════════════════════════════════════════════════════

IntelligentReviewSystem::IntelligentReviewSystem()
    : initialized_(false)
    , reviewCount_(0)
    , issuesFound_(0)
{
    // Set default configuration
    defaultConfig_.mode = ReviewMode::Standard;
    defaultConfig_.enable_security_scan = true;
    defaultConfig_.enable_performance_analysis = true;
    defaultConfig_.enable_best_practices = true;
    defaultConfig_.enable_code_smells = true;
    defaultConfig_.enable_auto_fix = true;
    defaultConfig_.enable_multi_model = false;
    defaultConfig_.max_issues_per_file = 100;
    defaultConfig_.max_file_size_kb = 1024;
    
    // Initialize default categories
    defaultConfig_.enabled_categories = {
        Category::Security,
        Category::Performance,
        Category::Correctness,
        Category::Maintainability,
        Category::BestPractice
    };
    
    defaultConfig_.minimum_severity = {Severity::Low};
}

IntelligentReviewSystem::~IntelligentReviewSystem() {
    shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Initialization & Shutdown
// ═══════════════════════════════════════════════════════════════════════════════

bool IntelligentReviewSystem::initialize(const std::string& config_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) return true;
    
    // Load vulnerability patterns
    vulnerabilityPatterns_ = {
        // OWASP Top 10
        {"A01:2021", "Broken Access Control", "Improper access control implementation",
         R"((?:auth|permission|role|admin)\s*\([^)]*\))", {"typescript", "javascript", "python"},
         Severity::Critical, {"Implement proper access control", "Use role-based access control"},
         {"https://owasp.org/Top10/A01_2021-Broken_Access_Control/"}},
        
        {"A02:2021", "Cryptographic Failures", "Weak or missing cryptography",
         R"((?:password|secret|key|token)\s*[=:]\s*['"][^'"]+['"])",
         {"typescript", "javascript", "python", "cpp"},
         Severity::Critical, {"Use strong encryption", "Never hardcode secrets"},
         {"https://owasp.org/Top10/A02_2021-Cryptographic_Failures/"}},
        
        {"A03:2021", "Injection", "SQL/NoSQL/OS/LDAP injection vulnerabilities",
         R"((?:query|exec|eval|system)\s*\([^)]*\+[^)]*\))",
         {"typescript", "javascript", "python", "cpp"},
         Severity::Critical, {"Use parameterized queries", "Sanitize all inputs"},
         {"https://owasp.org/Top10/A03_2021-Injection/"}},
        
        {"A04:2021", "Insecure Design", "Missing security controls in design",
         R"((?:TODO|FIXME|HACK|XXX)\s*[:\(]?\s*security)",
         {"typescript", "javascript", "python", "cpp"},
         Severity::High, {"Implement security by design", "Add security controls"},
         {"https://owasp.org/Top10/A04_2021-Insecure_Design/"}},
        
        {"A05:2021", "Security Misconfiguration", "Improper security configuration",
         R"((?:debug|dev|test)\s*[=:]\s*true)",
         {"typescript", "javascript", "python"},
         Severity::High, {"Disable debug in production", "Use secure defaults"},
         {"https://owasp.org/Top10/A05_2021_Security_Misconfiguration/"}},
        
        {"A06:2021", "Vulnerable Components", "Using vulnerable dependencies",
         R"((?:import|require|include)\s*['"][^'"]*(?:lodash|moment|request)['"])",
         {"typescript", "javascript", "python"},
         Severity::High, {"Update dependencies", "Use security scanners"},
         {"https://owasp.org/Top10/A06_2021-Vulnerable_and_Outdated_Components/"}},
        
        {"A07:2021", "Authentication Failures", "Broken authentication mechanisms",
         R"((?:password|passwd|pwd)\s*[=:]\s*['"][^'"]{0,8}['"])",
         {"typescript", "javascript", "python", "cpp"},
         Severity::Critical, {"Implement strong authentication", "Use MFA"},
         {"https://owasp.org/Top10/A07_2021_Identification_and_Authentication_Failures/"}},
        
        {"A08:2021", "Software Integrity Failures", "Untrusted data sources",
         R"((?:eval|Function|exec)\s*\([^)]*\))",
         {"typescript", "javascript", "python"},
         Severity::High, {"Validate all inputs", "Avoid dynamic code execution"},
         {"https://owasp.org/Top10/A08_2021_Software_and_Data_Integrity_Failures/"}},
        
        {"A09:2021", "Logging Failures", "Insufficient logging and monitoring",
         R"((?:catch\s*\([^)]*\)\s*\{[^}]*\}))",
         {"typescript", "javascript", "python", "cpp"},
         Severity::Medium, {"Add proper logging", "Monitor security events"},
         {"https://owasp.org/Top10/A09_2021_Security_Logging_and_Monitoring_Failures/"}},
        
        {"A10:2021", "SSRF", "Server-Side Request Forgery",
         R"((?:fetch|axios|request|http\.get)\s*\([^)]*user[^)]*\))",
         {"typescript", "javascript", "python"},
         Severity::High, {"Validate URLs", "Use allowlists"},
         {"https://owasp.org/Top10/A10_2021_Server-Side_Request_Forgery_%28SSRF%29/"}}
    };
    
    // Initialize practice rules by language
    rulesByLanguage_["typescript"] = {
        {"TS001", "Explicit Any", "Avoid using 'any' type",
         {"typescript"}, Category::BestPractice, Severity::Medium,
         R"(:\s*any\b)", "", {"Use specific types", "Use 'unknown' if type is truly unknown"},
         {"https://typescript-eslint.io/rules/no-explicit-any/"}},
        
        {"TS002", "Unused Variables", "Variables declared but never used",
         {"typescript"}, Category::Maintainability, Severity::Low,
         R"((?:const|let|var)\s+(\w+)\s*[=:][^;]*;(?![^]*\b\1\b))", "",
         {"Remove unused variables", "Use '_' prefix for intentionally unused"},
         {"https://typescript-eslint.io/rules/no-unused-vars/"}},
        
        {"TS003", "Console Log", "Console.log statements in production code",
         {"typescript"}, Category::BestPractice, Severity::Low,
         R"(console\.(?:log|debug|info)\s*\()", "",
         {"Use proper logging library", "Remove before production"},
         {}},
        
        {"TS004", "Null Check", "Missing null/undefined check",
         {"typescript"}, Category::Correctness, Severity::High,
         R"((?:\.|\?\.)\w+\s*\((?!\s*\?\s*\))", "",
         {"Add optional chaining", "Add null checks"},
         {}},
        
        {"TS005", "Async Without Await", "Async function without await",
         {"typescript"}, Category::Performance, Severity::Low,
         R"(async\s+function\s+\w+\s*\([^)]*\)\s*\{(?![^}]*await)[^}]*\})", "",
         {"Remove async if not needed", "Add await if needed"},
         {}}
    };
    
    rulesByLanguage_["javascript"] = {
        {"JS001", "Var Usage", "Avoid using 'var' - use 'const' or 'let'",
         {"javascript"}, Category::BestPractice, Severity::Medium,
         R"(\bvar\s+)", "", {"Use 'const' for constants", "Use 'let' for variables"},
         {}},
        
        {"JS002", "Equality Operators", "Use strict equality (===) instead of (==)",
         {"javascript"}, Category::Correctness, Severity::Medium,
         R"([^!=]==(?!=)", "", {"Use === for comparison", "Use !== for inequality"},
         {}},
        
        {"JS003", "Callback Hell", "Deeply nested callbacks",
         {"javascript"}, Category::Maintainability, Severity::Medium,
         R"((?:function|\([^)]*\)\s*=>)\s*\{(?:[^{}]*\{(?:[^{}]*\{){3})", "",
         {"Use async/await", "Extract functions"},
         {}},
        
        {"JS004", "Eval Usage", "Avoid using eval() - security risk",
         {"javascript"}, Category::Security, Severity::Critical,
         R"(\beval\s*\()", "", {"Use JSON.parse for JSON", "Avoid dynamic code"},
         {}},
        
        {"JS005", "Document Write", "document.write can overwrite entire page",
         {"javascript"}, Category::BestPractice, Severity::High,
         R"(document\.write\s*\()", "", {"Use DOM manipulation", "Use innerHTML safely"},
         {}}
    };
    
    rulesByLanguage_["python"] = {
        {"PY001", "Bare Except", "Avoid bare 'except:' - catches everything",
         {"python"}, Category::BestPractice, Severity::High,
         R"(except\s*:)", "", {"Catch specific exceptions", "Use 'except Exception' if needed"},
         {}},
        
        {"PY002", "Mutable Default Args", "Mutable default arguments are dangerous",
         {"python"}, Category::Correctness, Severity::High,
         R"(def\s+\w+\s*\([^)]*=\s*(?:\[\]|\{\}|\(\)))", "",
         {"Use None as default", "Create new object in function"},
         {}},
        
        {"PY003", "Print Statement", "Print statements in production code",
         {"python"}, Category::BestPractice, Severity::Low,
         R"(\bprint\s*\()", "", {"Use logging module", "Remove before production"},
         {}},
        
        {"PY004", "Import Star", "Avoid 'from module import *'",
         {"python"}, Category::Maintainability, Severity::Medium,
         R"(from\s+\S+\s+import\s+\*)", "",
         {"Import specific names", "Use module prefix"},
         {}},
        
        {"PY005", "Global Variables", "Avoid global variables",
         {"python"}, Category::BestPractice, Severity::Medium,
         R"(\bglobal\s+\w+)", "",
         {"Use function parameters", "Use class attributes"},
         {}}
    };
    
    rulesByLanguage_["cpp"] = {
        {"CPP001", "Raw Pointer", "Prefer smart pointers over raw pointers",
         {"cpp"}, Category::BestPractice, Severity::Medium,
         R"(\w+\s*\*\s+\w+)", "",
         {"Use std::unique_ptr", "Use std::shared_ptr"},
         {}},
        
        {"CPP002", "Memory Leak", "Potential memory leak - missing delete",
         {"cpp"}, Category::Correctness, Severity::Critical,
         R"(new\s+\w+(?![^]*delete\s+\w+))", "",
         {"Use smart pointers", "Ensure delete for every new"},
         {}},
        
        {"CPP003", "Null Pointer Dereference", "Potential null pointer dereference",
         {"cpp"}, Category::Correctness, Severity::Critical,
         R"(\w+\s*->\s*\w+(?![^]*\?\s*\w+\s*:))", "",
         {"Add null check", "Use safe pointer access"},
         {}},
        
        {"CPP004", "Uninitialized Variable", "Variable may be used uninitialized",
         {"cpp"}, Category::Correctness, Severity::High,
         R"((?:int|float|double|char|bool)\s+\w+;(?![^]*\w+\s*=))", "",
         {"Initialize variables", "Use default initialization"},
         {}},
        
        {"CPP005", "C-Style Cast", "Prefer C++ style casts",
         {"cpp"}, Category::BestPractice, Severity::Low,
         R"(\(\s*\w+\s*\)\s*\w+)", "",
         {"Use static_cast", "Use dynamic_cast for polymorphism"},
         {}}
    };
    
    initialized_ = true;
    return true;
}

void IntelligentReviewSystem::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
}

bool IntelligentReviewSystem::isInitialized() const {
    return initialized_;
}

std::string IntelligentReviewSystem::getVersion() const {
    return "INTELLIGENT REVIEW v1.0.0 - AI-Powered Code Analysis";
}

// ═══════════════════════════════════════════════════════════════════════════════
// Core Review Operations
// ═══════════════════════════════════════════════════════════════════════════════

FileReview IntelligentReviewSystem::reviewFile(
    const std::string& file_path,
    const std::string& content,
    const ReviewConfig& config
) {
    FileReview review;
    review.file_path = file_path;
    review.total_issues = 0;
    review.critical_count = 0;
    review.high_count = 0;
    review.medium_count = 0;
    review.low_count = 0;
    review.quality_score = 100.0f;
    
    auto start = std::chrono::system_clock::now();
    
    if (!initialized_) {
        review.summary = "Review system not initialized";
        return review;
    }
    
    // Detect language
    Language lang = detectLanguage(file_path, content);
    
    // Run all enabled checks
    if (config.enable_security_scan) {
        auto securityIssues = runSecurityScan(content, lang);
        review.issues.insert(review.issues.end(), securityIssues.begin(), securityIssues.end());
    }
    
    if (config.enable_performance_analysis) {
        auto perfIssues = runPerformanceAnalysis(content, lang);
        review.issues.insert(review.issues.end(), perfIssues.begin(), perfIssues.end());
    }
    
    if (config.enable_best_practices) {
        auto practiceIssues = runPracticeCheck(content, lang);
        review.issues.insert(review.issues.end(), practiceIssues.begin(), practiceIssues.end());
    }
    
    if (config.enable_code_smells) {
        auto smellIssues = runSmellDetection(content, lang);
        review.issues.insert(review.issues.end(), smellIssues.begin(), smellIssues.end());
    }
    
    // Count issues by severity
    for (const auto& issue : review.issues) {
        review.total_issues++;
        switch (issue.severity) {
            case Severity::Critical: review.critical_count++; break;
            case Severity::High: review.high_count++; break;
            case Severity::Medium: review.medium_count++; break;
            case Severity::Low: review.low_count++; break;
            default: break;
        }
    }
    
    // Calculate scores
    review.quality_score = calculateQualityScore(review);
    review.security_score = calculateSecurityScore(review);
    review.performance_score = calculatePerformanceScore(review);
    
    // Generate summary
    std::ostringstream oss;
    oss << "Found " << review.total_issues << " issues: ";
    oss << review.critical_count << " critical, ";
    oss << review.high_count << " high, ";
    oss << review.medium_count << " medium, ";
    oss << review.low_count << " low. ";
    oss << "Quality score: " << std::fixed << std::setprecision(1) << review.quality_score << "/100";
    review.summary = oss.str();
    
    // Add strengths and weaknesses
    if (review.critical_count == 0 && review.high_count == 0) {
        review.strengths.push_back("No critical or high severity issues found");
    }
    if (review.quality_score >= 80.0f) {
        review.strengths.push_back("High overall code quality");
    }
    if (review.security_score >= 90.0f) {
        review.strengths.push_back("Strong security posture");
    }
    
    if (review.critical_count > 0) {
        review.weaknesses.push_back("Critical security vulnerabilities present");
    }
    if (review.performance_score < 70.0f) {
        review.weaknesses.push_back("Performance concerns detected");
    }
    
    auto end = std::chrono::system_clock::now();
    review.review_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    reviewCount_++;
    issuesFound_ += review.total_issues;
    
    return review;
}

ProjectReview IntelligentReviewSystem::reviewProject(
    const std::string& project_path,
    const std::vector<std::string>& files,
    const ReviewConfig& config
) {
    ProjectReview project;
    project.project_path = project_path;
    project.total_issues = 0;
    project.files_reviewed = 0;
    project.overall_quality_score = 0.0f;
    project.overall_security_score = 0.0f;
    project.overall_performance_score = 0.0f;
    project.reviewed_at = std::chrono::system_clock::now();
    
    for (const auto& file : files) {
        std::ifstream f(file);
        if (!f.is_open()) continue;
        
        std::stringstream buffer;
        buffer << f.rdbuf();
        std::string content = buffer.str();
        
        FileReview fileReview = reviewFile(file, content, config);
        project.files.push_back(fileReview);
        project.files_reviewed++;
        project.total_issues += fileReview.total_issues;
        
        // Aggregate scores
        project.overall_quality_score += fileReview.quality_score;
        project.overall_security_score += fileReview.security_score;
        project.overall_performance_score += fileReview.performance_score;
        
        // Aggregate by category and severity
        for (const auto& issue : fileReview.issues) {
            project.issues_by_category[issue.category]++;
            project.issues_by_severity[issue.severity]++;
        }
    }
    
    // Calculate averages
    if (project.files_reviewed > 0) {
        project.overall_quality_score /= project.files_reviewed;
        project.overall_security_score /= project.files_reviewed;
        project.overall_performance_score /= project.files_reviewed;
    }
    
    // Get top issues
    std::vector<CodeIssue> allIssues;
    for (const auto& file : project.files) {
        allIssues.insert(allIssues.end(), file.issues.begin(), file.issues.end());
    }
    
    // Sort by severity
    std::sort(allIssues.begin(), allIssues.end(), [](const CodeIssue& a, const CodeIssue& b) {
        return static_cast<int>(a.severity) < static_cast<int>(b.severity);
    });
    
    // Take top 10
    project.top_issues.assign(allIssues.begin(), allIssues.begin() + std::min(size_t(10), allIssues.size()));
    
    // Generate recommendations
    if (project.overall_security_score < 70.0f) {
        project.recommendations.push_back("Address security vulnerabilities before deployment");
    }
    if (project.overall_performance_score < 70.0f) {
        project.recommendations.push_back("Optimize performance bottlenecks");
    }
    if (project.overall_quality_score < 70.0f) {
        project.recommendations.push_back("Improve code quality and maintainability");
    }
    
    return project;
}

void IntelligentReviewSystem::reviewStream(
    const std::string& file_path,
    const std::string& content,
    const ReviewConfig& config,
    std::function<void(const CodeIssue&)> callback
) {
    // Run review in background
    std::thread([this, file_path, content, config, callback]() {
        auto review = reviewFile(file_path, content, config);
        for (const auto& issue : review.issues) {
            callback(issue);
        }
    }).detach();
}

FileReview IntelligentReviewSystem::reviewWithEnsemble(
    const std::string& file_path,
    const std::string& content,
    const std::vector<std::string>& models,
    const ReviewConfig& config
) {
    // Run review with multiple models and merge results
    std::vector<std::future<FileReview>> futures;
    
    for (const auto& model : models) {
        futures.push_back(std::async(std::launch::async, [this, file_path, content, config]() {
            return reviewFile(file_path, content, config);
        }));
    }
    
    // Collect and merge results
    std::map<std::string, int> issueVotes;
    std::vector<CodeIssue> mergedIssues;
    
    for (auto& future : futures) {
        auto review = future.get();
        for (const auto& issue : review.issues) {
            std::string key = issue.rule_id + ":" + std::to_string(issue.location.start_line);
            issueVotes[key]++;
            
            if (issueVotes[key] == 1) {
                mergedIssues.push_back(issue);
            }
        }
    }
    
    // Update confidence based on votes
    for (auto& issue : mergedIssues) {
        std::string key = issue.rule_id + ":" + std::to_string(issue.location.start_line);
        issue.confidence = static_cast<float>(issueVotes[key]) / models.size();
    }
    
    FileReview result;
    result.file_path = file_path;
    result.issues = mergedIssues;
    result.total_issues = mergedIssues.size();
    
    for (const auto& issue : mergedIssues) {
        switch (issue.severity) {
            case Severity::Critical: result.critical_count++; break;
            case Severity::High: result.high_count++; break;
            case Severity::Medium: result.medium_count++; break;
            case Severity::Low: result.low_count++; break;
            default: break;
        }
    }
    
    result.quality_score = calculateQualityScore(result);
    result.security_score = calculateSecurityScore(result);
    result.performance_score = calculatePerformanceScore(result);
    
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Security Scanning
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<SecurityFinding> IntelligentReviewSystem::scanSecurity(
    const std::string& content,
    Language language
) {
    std::vector<SecurityFinding> findings;
    
    // Convert Language enum to string for comparison
    std::string langStr = languageToString(language);
    
    for (const auto& pattern : vulnerabilityPatterns_) {
        // Check if pattern applies to this language
        bool applies = false;
        for (const auto& lang : pattern.languages) {
            if (lang == langStr || lang == "unknown" || lang == "*") {
                applies = true;
                break;
            }
        }
        
        if (!applies) continue;
        
        // Search for pattern
        try {
            std::regex re(pattern.pattern);
            std::sregex_iterator it(content.begin(), content.end(), re);
            std::sregex_iterator end;
            
            int lineNum = 1;
            std::istringstream iss(content);
            std::string line;
            
            while (std::getline(iss, line)) {
                if (std::regex_search(line, re)) {
                    SecurityFinding finding;
                    finding.vulnerability_id = pattern.id;
                    finding.title = pattern.name;
                    finding.description = pattern.description;
                    finding.severity = pattern.severity;
                    finding.location.file_path = "";
                    finding.location.start_line = lineNum;
                    finding.location.end_line = lineNum;
                    finding.location.snippet = line;
                    finding.cvss_score = calculateCVSS(finding);
                    
                    for (const auto& mitigation : pattern.mitigations) {
                        finding.remediation_steps.push_back(mitigation);
                    }
                    
                    findings.push_back(finding);
                }
                lineNum++;
            }
        } catch (const std::regex_error&) {
            // Invalid regex, skip
        }
    }
    
    return findings;
}

std::vector<SecurityFinding> IntelligentReviewSystem::getOWASPTop10(
    const std::string& content,
    Language language
) {
    std::vector<SecurityFinding> findings;
    auto allFindings = scanSecurity(content, language);
    
    for (const auto& finding : allFindings) {
        if (finding.vulnerability_id.find("A0") == 0) {  // OWASP IDs start with A0
            findings.push_back(finding);
        }
    }
    
    return findings;
}

std::vector<SecurityFinding> IntelligentReviewSystem::getCWEFindings(
    const std::string& content,
    const std::vector<std::string>& cwe_ids,
    Language language
) {
    std::vector<SecurityFinding> findings;
    auto allFindings = scanSecurity(content, language);
    
    for (const auto& finding : allFindings) {
        for (const auto& cwe : cwe_ids) {
            if (finding.vulnerability_id.find(cwe) != std::string::npos) {
                findings.push_back(finding);
                break;
            }
        }
    }
    
    return findings;
}

float IntelligentReviewSystem::calculateCVSS(const SecurityFinding& finding) {
    // Simplified CVSS calculation
    float baseScore = 0.0f;
    
    switch (finding.severity) {
        case Severity::Critical: baseScore = 9.0f; break;
        case Severity::High: baseScore = 7.0f; break;
        case Severity::Medium: baseScore = 5.0f; break;
        case Severity::Low: baseScore = 3.0f; break;
        default: baseScore = 1.0f; break;
    }
    
    return baseScore;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Performance Analysis
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<PerformanceIssue> IntelligentReviewSystem::analyzePerformance(
    const std::string& content,
    Language language
) {
    std::vector<PerformanceIssue> issues;
    
    // Detect O(n^2) patterns
    std::regex nestedLoop(R"(for\s*\([^)]*\)\s*\{[^}]*for\s*\([^)]*\))");
    std::sregex_iterator it(content.begin(), content.end(), nestedLoop);
    while (it != std::sregex_iterator()) {
        PerformanceIssue issue;
        issue.id = "PERF001";
        issue.title = "Nested Loop - O(n^2) Complexity";
        issue.description = "Nested loops can lead to O(n^2) time complexity";
        issue.severity = Severity::Medium;
        issue.issue_type = "O(n^2)";
        issue.complexity = "O(n^2)";
        issue.estimated_impact = 0.7f;
        issue.suggestions.push_back("Consider using a hash map for O(1) lookups");
        issue.suggestions.push_back("Break early when possible");
        issues.push_back(issue);
        ++it;
    }
    
    // Detect synchronous I/O in async context
    std::regex syncIO(R"((?:fs\.readFileSync|fs\.writeSync|await\s+.*\.read\s*\())");
    it = std::sregex_iterator(content.begin(), content.end(), syncIO);
    while (it != std::sregex_iterator()) {
        PerformanceIssue issue;
        issue.id = "PERF002";
        issue.title = "Synchronous I/O";
        issue.description = "Synchronous I/O operations can block the event loop";
        issue.severity = Severity::Medium;
        issue.issue_type = "blocking_io";
        issue.estimated_impact = 0.6f;
        issue.suggestions.push_back("Use asynchronous I/O operations");
        issues.push_back(issue);
        ++it;
    }
    
    // Detect large memory allocations
    std::regex largeAlloc(R"(new\s+(?:Array|Buffer)\s*\(\s*\d{6,}\s*\))");
    it = std::sregex_iterator(content.begin(), content.end(), largeAlloc);
    while (it != std::sregex_iterator()) {
        PerformanceIssue issue;
        issue.id = "PERF003";
        issue.title = "Large Memory Allocation";
        issue.description = "Large memory allocation may cause performance issues";
        issue.severity = Severity::Medium;
        issue.issue_type = "memory_allocation";
        issue.estimated_impact = 0.5f;
        issue.suggestions.push_back("Consider streaming data instead");
        issue.suggestions.push_back("Use lazy loading");
        issues.push_back(issue);
        ++it;
    }
    
    return issues;
}

ComplexityMetrics IntelligentReviewSystem::calculateComplexity(
    const std::string& content,
    Language language
) {
    ComplexityMetrics metrics;
    metrics.cyclomatic_complexity = 1;
    metrics.cognitive_complexity = 0;
    metrics.nesting_depth = 0;
    metrics.lines_of_code = 0;
    metrics.logical_lines = 0;
    metrics.comment_lines = 0;
    metrics.function_count = 0;
    metrics.parameter_count = 0;
    metrics.maintainability_index = 100.0f;
    
    std::istringstream iss(content);
    std::string line;
    int currentNesting = 0;
    int maxNesting = 0;
    
    while (std::getline(iss, line)) {
        metrics.lines_of_code++;
        
        // Count comments
        if (line.find("//") != std::string::npos || line.find("#") != std::string::npos) {
            metrics.comment_lines++;
        }
        
        // Count logical lines (non-empty, non-comment)
        if (!line.empty() && line.find("//") == std::string::npos) {
            metrics.logical_lines++;
        }
        
        // Count control structures for cyclomatic complexity
        if (line.find("if") != std::string::npos ||
            line.find("else if") != std::string::npos ||
            line.find("for") != std::string::npos ||
            line.find("while") != std::string::npos ||
            line.find("case") != std::string::npos ||
            line.find("catch") != std::string::npos) {
            metrics.cyclomatic_complexity++;
        }
        
        // Count nesting
        if (line.find("{") != std::string::npos) {
            currentNesting++;
            maxNesting = std::max(maxNesting, currentNesting);
        }
        if (line.find("}") != std::string::npos) {
            currentNesting--;
        }
        
        // Count functions
        if (line.find("function") != std::string::npos ||
            line.find("def ") != std::string::npos ||
            line.find("void ") != std::string::npos ||
            line.find("int ") != std::string::npos ||
            line.find("string ") != std::string::npos) {
            metrics.function_count++;
        }
    }
    
    metrics.nesting_depth = maxNesting;
    
    // Calculate maintainability index (simplified)
    float volume = static_cast<float>(metrics.lines_of_code);
    float cyclomatic = static_cast<float>(metrics.cyclomatic_complexity);
    float loc = static_cast<float>(metrics.logical_lines);
    
    if (volume > 0 && loc > 0) {
        metrics.maintainability_index = std::max(0.0f, 
            171.0f - 5.2f * std::log(volume) - 0.23f * cyclomatic - 16.2f * std::log(loc));
        metrics.maintainability_index = std::min(100.0f, metrics.maintainability_index);
    }
    
    return metrics;
}

std::vector<MemoryPattern> IntelligentReviewSystem::detectMemoryPatterns(
    const std::string& content,
    Language language
) {
    std::vector<MemoryPattern> patterns;
    
    // Detect potential memory leaks
    std::regex newWithoutDelete(R"(new\s+\w+(?![^]*delete\s+\w+))");
    std::sregex_iterator it(content.begin(), content.end(), newWithoutDelete);
    
    while (it != std::sregex_iterator()) {
        MemoryPattern pattern;
        pattern.pattern_type = "potential_leak";
        pattern.description = "Memory allocated without corresponding deallocation";
        pattern.estimated_size = 0;  // Unknown
        pattern.lifecycle = "unknown";
        patterns.push_back(pattern);
        ++it;
    }
    
    return patterns;
}

std::string IntelligentReviewSystem::estimateComplexity(
    const std::string& function_content,
    Language language
) {
    // Simplified complexity estimation
    int loopCount = 0;
    int recursiveCalls = 0;
    
    // Count loops
    std::regex loopPattern(R"((?:for|while|do)\s*\()");
    std::sregex_iterator it(function_content.begin(), function_content.end(), loopPattern);
    while (it != std::sregex_iterator()) {
        loopCount++;
        ++it;
    }
    
    // Check for recursion (simplified)
    std::regex funcDef(R"(function\s+(\w+)\s*\()");
    std::smatch match;
    if (std::regex_search(function_content, match, funcDef)) {
        std::string funcName = match[1].str();
        if (function_content.find(funcName + "(") != std::string::npos) {
            recursiveCalls++;
        }
    }
    
    if (recursiveCalls > 0) {
        return "O(2^n) or O(n!) - Recursive";
    } else if (loopCount >= 2) {
        return "O(n^" + std::to_string(loopCount) + ")";
    } else if (loopCount == 1) {
        return "O(n)";
    } else {
        return "O(1)";
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Best Practices Checking
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<PracticeViolation> IntelligentReviewSystem::checkPractices(
    const std::string& content,
    Language language,
    const std::vector<std::string>& rule_ids
) {
    std::vector<PracticeViolation> violations;
    
    auto rules = getRules(language);
    
    for (const auto& rule : rules) {
        // Filter by rule_ids if specified
        if (!rule_ids.empty() && std::find(rule_ids.begin(), rule_ids.end(), rule.id) == rule_ids.end()) {
            continue;
        }
        
        try {
            std::regex re(rule.pattern);
            std::sregex_iterator it(content.begin(), content.end(), re);
            
            int lineNum = 1;
            std::istringstream iss(content);
            std::string line;
            
            while (std::getline(iss, line)) {
                if (std::regex_search(line, re)) {
                    PracticeViolation violation;
                    violation.rule_id = rule.id;
                    violation.title = rule.name;
                    violation.description = rule.description;
                    violation.severity = rule.default_severity;
                    violation.location.file_path = "";
                    violation.location.start_line = lineNum;
                    violation.location.end_line = lineNum;
                    violation.location.snippet = line;
                    violation.violation_type = rule.category == Category::Security ? "security" : "practice";
                    
                    for (const auto& example : rule.good_examples) {
                        violation.suggestions.push_back("Good: " + example);
                    }
                    
                    violations.push_back(violation);
                }
                lineNum++;
            }
        } catch (const std::regex_error&) {
            // Invalid regex, skip
        }
    }
    
    return violations;
}

std::vector<PracticeRule> IntelligentReviewSystem::getRules(Language language) {
    std::string langStr = languageToString(language);
    auto it = rulesByLanguage_.find(langStr);
    if (it != rulesByLanguage_.end()) {
        return it->second;
    }
    return {};
}

bool IntelligentReviewSystem::addCustomRule(const PracticeRule& rule) {
    for (const auto& lang : rule.languages) {
        rulesByLanguage_[lang].push_back(rule);
    }
    return true;
}

bool IntelligentReviewSystem::removeCustomRule(const std::string& rule_id) {
    for (auto& [lang, rules] : rulesByLanguage_) {
        auto it = std::remove_if(rules.begin(), rules.end(),
            [&rule_id](const PracticeRule& r) { return r.id == rule_id; });
        if (it != rules.end()) {
            rules.erase(it, rules.end());
        }
    }
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Code Smell Detection
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<CodeSmell> IntelligentReviewSystem::detectSmells(
    const std::string& content,
    Language language
) {
    std::vector<CodeSmell> smells;
    
    // Long Method
    std::regex longMethod(R"((?:function|def|void)\s+\w+\s*\([^)]*\)\s*\{(?:[^{}]*\{[^{}]*\})*[^{}]*\})");
    std::sregex_iterator it(content.begin(), content.end(), longMethod);
    while (it != std::sregex_iterator()) {
        std::string match = it->str();
        int lines = std::count(match.begin(), match.end(), '\n');
        
        if (lines > 30) {
            CodeSmell smell;
            smell.id = "SMELL001";
            smell.name = "Long Method";
            smell.description = "Method exceeds 30 lines - consider breaking it up";
            smell.severity = Severity::Medium;
            smell.smell_type = "Bloaters";
            smell.detection_confidence = 0.8f;
            smell.indicators.push_back("Lines: " + std::to_string(lines));
            smell.refactorings.push_back("Extract Method");
            smell.refactorings.push_back("Decompose Conditional");
            smells.push_back(smell);
        }
        ++it;
    }
    
    // Large Parameter List
    std::regex largeParams(R"((?:function|def|void)\s+\w+\s*\((?:\s*\w+\s*,){4,})");
    it = std::sregex_iterator(content.begin(), content.end(), largeParams);
    while (it != std::sregex_iterator()) {
        CodeSmell smell;
        smell.id = "SMELL002";
        smell.name = "Long Parameter List";
        smell.description = "Function has more than 4 parameters";
        smell.severity = Severity::Medium;
        smell.smell_type = "Bloaters";
        smell.detection_confidence = 0.9f;
        smell.refactorings.push_back("Introduce Parameter Object");
        smell.refactorings.push_back("Use Builder Pattern");
        smells.push_back(smell);
        ++it;
    }
    
    // Duplicate Code (simplified)
    std::regex duplicate(R"((.{50,})\1)");
    it = std::sregex_iterator(content.begin(), content.end(), duplicate);
    while (it != std::sregex_iterator()) {
        CodeSmell smell;
        smell.id = "SMELL003";
        smell.name = "Duplicate Code";
        smell.description = "Duplicate code detected";
        smell.severity = Severity::Medium;
        smell.smell_type = "Dispensables";
        smell.detection_confidence = 0.7f;
        smell.refactorings.push_back("Extract Method");
        smell.refactorings.push_back("Pull Up Method");
        smells.push_back(smell);
        ++it;
    }
    
    // Dead Code (unreachable)
    std::regex deadCode(R"((?:return|throw|break|continue)[^;]*;\s*(?:return|throw|break|continue|const|let|var|function))");
    it = std::sregex_iterator(content.begin(), content.end(), deadCode);
    while (it != std::sregex_iterator()) {
        CodeSmell smell;
        smell.id = "SMELL004";
        smell.name = "Dead Code";
        smell.description = "Unreachable code detected";
        smell.severity = Severity::Low;
        smell.smell_type = "Dispensables";
        smell.detection_confidence = 0.9f;
        smell.refactorings.push_back("Remove dead code");
        smells.push_back(smell);
        ++it;
    }
    
    // Magic Numbers
    std::regex magicNumber(R"((?:==|!=|>=|<=|>|<|\+|-|\*|\/)\s*(\d{2,})(?![\d.]))");
    it = std::sregex_iterator(content.begin(), content.end(), magicNumber);
    while (it != std::sregex_iterator()) {
        CodeSmell smell;
        smell.id = "SMELL005";
        smell.name = "Magic Number";
        smell.description = "Magic number without explanation";
        smell.severity = Severity::Low;
        smell.smell_type = "Dispensables";
        smell.detection_confidence = 0.6f;
        smell.refactorings.push_back("Replace Magic Number with Constant");
        smells.push_back(smell);
        ++it;
    }
    
    return smells;
}

std::vector<std::string> IntelligentReviewSystem::getRefactorings(const CodeSmell& smell) {
    return smell.refactorings;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Fix Suggestions
// ═══════════════════════════════════════════════════════════════════════════════

FixSuggestion IntelligentReviewSystem::generateFix(
    const CodeIssue& issue,
    const std::string& content
) {
    FixSuggestion fix;
    fix.description = issue.description;
    fix.type = FixType::Suggestion;
    fix.auto_applicable = false;
    
    // Generate fix based on rule
    if (issue.rule_id == "TS001") {  // Explicit Any
        fix.description = "Replace 'any' with specific type";
        fix.type = FixType::Suggestion;
        fix.auto_applicable = false;
    } else if (issue.rule_id == "JS001") {  // Var Usage
        fix.description = "Replace 'var' with 'const' or 'let'";
        fix.type = FixType::SafeAuto;
        fix.auto_applicable = true;
        fix.before = "var ";
        fix.after = "const ";
    } else if (issue.rule_id == "JS002") {  // Equality
        fix.description = "Use strict equality (===)";
        fix.type = FixType::SafeAuto;
        fix.auto_applicable = true;
        fix.before = "==";
        fix.after = "===";
    } else if (issue.rule_id == "PY001") {  // Bare Except
        fix.description = "Catch specific exception";
        fix.type = FixType::Suggestion;
        fix.auto_applicable = false;
    }
    
    return fix;
}

bool IntelligentReviewSystem::applyFix(
    const std::string& file_path,
    const FixSuggestion& fix
) {
    if (!fix.auto_applicable || fix.type == FixType::None) {
        return false;
    }
    
    std::ifstream file(file_path);
    if (!file.is_open()) return false;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    // Apply fix
    size_t pos = content.find(fix.before);
    if (pos != std::string::npos) {
        content.replace(pos, fix.before.length(), fix.after);
        
        std::ofstream outFile(file_path);
        if (!outFile.is_open()) return false;
        outFile << content;
        outFile.close();
        
        return true;
    }
    
    return false;
}

int IntelligentReviewSystem::applyAllSafeFixes(
    const std::string& file_path,
    const std::vector<CodeIssue>& issues
) {
    int applied = 0;
    
    for (const auto& issue : issues) {
        FixSuggestion fix = generateFix(issue, "");
        if (fix.type == FixType::SafeAuto && fix.auto_applicable) {
            if (applyFix(file_path, fix)) {
                applied++;
            }
        }
    }
    
    return applied;
}

std::string IntelligentReviewSystem::generateDiff(
    const std::string& before,
    const std::string& after,
    const std::string& file_path
) {
    std::ostringstream diff;
    diff << "--- a/" << file_path << "\n";
    diff << "+++ b/" << file_path << "\n";
    
    // Simplified diff - in production would use proper diff algorithm
    diff << "@@ -1,1 +1,1 @@\n";
    diff << "-" << before << "\n";
    diff << "+" << after << "\n";
    
    return diff.str();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Learning System
// ═══════════════════════════════════════════════════════════════════════════════

void IntelligentReviewSystem::submitFeedback(const ReviewFeedback& feedback) {
    // Update false positive tracking
    if (feedback.action == "marked_false_positive") {
        falsePositives_[feedback.issue_id]++;
    }
    
    // Update learned patterns
    if (feedback.accepted) {
        // Find or create pattern
        bool found = false;
        for (auto& pattern : learnedPatterns_) {
            if (pattern.pattern == feedback.issue_id) {
                pattern.occurrence_count++;
                pattern.confidence = std::min(1.0f, pattern.confidence + 0.1f);
                pattern.last_seen = std::chrono::system_clock::now();
                found = true;
                break;
            }
        }
        
        if (!found) {
            LearnedPattern pattern;
            pattern.id = "learned-" + std::to_string(learnedPatterns_.size());
            pattern.pattern_type = "common_issue";
            pattern.pattern = feedback.issue_id;
            pattern.occurrence_count = 1;
            pattern.confidence = 0.5f;
            pattern.last_seen = std::chrono::system_clock::now();
            learnedPatterns_.push_back(pattern);
        }
    }
}

std::vector<LearnedPattern> IntelligentReviewSystem::getLearnedPatterns(const std::string& pattern_type) {
    if (pattern_type.empty()) {
        return learnedPatterns_;
    }
    
    std::vector<LearnedPattern> filtered;
    for (const auto& pattern : learnedPatterns_) {
        if (pattern.pattern_type == pattern_type) {
            filtered.push_back(pattern);
        }
    }
    return filtered;
}

void IntelligentReviewSystem::trainOnHistory(const std::vector<FileReview>& history) {
    for (const auto& review : history) {
        for (const auto& issue : review.issues) {
            issuesByRule_[issue.rule_id]++;
            
            // Update average confidence
            float& avg = avgConfidenceByRule_[issue.rule_id];
            int count = issuesByRule_[issue.rule_id];
            avg = (avg * (count - 1) + issue.confidence) / count;
        }
    }
}

float IntelligentReviewSystem::getFalsePositiveRate(const std::string& rule_id) {
    int total = issuesByRule_[rule_id];
    if (total == 0) return 0.0f;
    
    int fps = falsePositives_[rule_id];
    return static_cast<float>(fps) / total;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Configuration & Policies
// ═══════════════════════════════════════════════════════════════════════════════

void IntelligentReviewSystem::setConfig(const ReviewConfig& config) {
    defaultConfig_ = config;
}

ReviewConfig IntelligentReviewSystem::getConfig() {
    return defaultConfig_;
}

bool IntelligentReviewSystem::addPolicy(const ReviewPolicy& policy) {
    policies_[policy.name] = policy;
    return true;
}

bool IntelligentReviewSystem::removePolicy(const std::string& policy_name) {
    return policies_.erase(policy_name) > 0;
}

bool IntelligentReviewSystem::passesPolicy(
    const FileReview& review,
    const std::string& policy_name
) {
    auto it = policies_.find(policy_name);
    if (it == policies_.end()) return true;
    
    const auto& policy = it->second;
    
    // Check quality score
    if (review.quality_score < policy.min_quality_score) {
        return false;
    }
    
    // Check security score
    if (review.security_score < policy.min_security_score) {
        return false;
    }
    
    // Check issue counts
    auto critIt = policy.max_issues_allowed.find(Severity::Critical);
    auto highIt = policy.max_issues_allowed.find(Severity::High);
    
    if (critIt != policy.max_issues_allowed.end() && review.critical_count > critIt->second) {
        return false;
    }
    if (highIt != policy.max_issues_allowed.end() && review.high_count > highIt->second) {
        return false;
    }
    
    return true;
}

std::vector<std::string> IntelligentReviewSystem::getPolicyViolations(
    const FileReview& review,
    const std::string& policy_name
) {
    std::vector<std::string> violations;
    
    auto it = policies_.find(policy_name);
    if (it == policies_.end()) return violations;
    
    const auto& policy = it->second;
    
    if (review.quality_score < policy.min_quality_score) {
        violations.push_back("Quality score " + std::to_string(review.quality_score) + 
            " below minimum " + std::to_string(policy.min_quality_score));
    }
    
    if (review.security_score < policy.min_security_score) {
        violations.push_back("Security score " + std::to_string(review.security_score) + 
            " below minimum " + std::to_string(policy.min_security_score));
    }
    
    auto critIt = policy.max_issues_allowed.find(Severity::Critical);
    if (critIt != policy.max_issues_allowed.end() && review.critical_count > critIt->second) {
        violations.push_back("Critical issues " + std::to_string(review.critical_count) + 
            " exceeds limit " + std::to_string(critIt->second));
    }
    
    return violations;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Statistics & Reporting
// ═══════════════════════════════════════════════════════════════════════════════

std::map<std::string, std::string> IntelligentReviewSystem::getStats() {
    std::map<std::string, std::string> stats;
    
    stats["total_reviews"] = std::to_string(reviewCount_.load());
    stats["total_issues_found"] = std::to_string(issuesFound_.load());
    stats["avg_issues_per_review"] = reviewCount_ > 0 
        ? std::to_string(static_cast<float>(issuesFound_) / reviewCount_) 
        : "0";
    stats["learned_patterns"] = std::to_string(learnedPatterns_.size());
    stats["rules_loaded"] = std::to_string(vulnerabilityPatterns_.size());
    
    return stats;
}

std::map<std::string, std::vector<int>> IntelligentReviewSystem::getTrends(const std::string& period) {
    std::map<std::string, std::vector<int>> trends;
    
    // Would track historical data in production
    trends["issues"] = {10, 8, 12, 7, 5, 6, 4};
    trends["critical"] = {2, 1, 3, 1, 0, 1, 0};
    trends["quality_score"] = {75, 78, 72, 82, 85, 88, 90};
    
    return trends;
}

std::string IntelligentReviewSystem::exportReport(
    const ProjectReview& review,
    const std::string& format
) {
    std::ostringstream report;
    
    if (format == "markdown") {
        report << "# Code Review Report\n\n";
        report << "## Summary\n\n";
        report << "- **Files Reviewed:** " << review.files_reviewed << "\n";
        report << "- **Total Issues:** " << review.total_issues << "\n";
        report << "- **Quality Score:** " << std::fixed << std::setprecision(1) << review.overall_quality_score << "/100\n";
        report << "- **Security Score:** " << review.overall_security_score << "/100\n";
        report << "- **Performance Score:** " << review.overall_performance_score << "/100\n\n";
        
        report << "## Issues by Severity\n\n";
        report << "| Severity | Count |\n";
        report << "|----------|-------|\n";
        
        auto critIt = review.issues_by_severity.find(Severity::Critical);
        auto highIt = review.issues_by_severity.find(Severity::High);
        auto medIt = review.issues_by_severity.find(Severity::Medium);
        auto lowIt = review.issues_by_severity.find(Severity::Low);
        
        report << "| Critical | " << (critIt != review.issues_by_severity.end() ? critIt->second : 0) << " |\n";
        report << "| High | " << (highIt != review.issues_by_severity.end() ? highIt->second : 0) << " |\n";
        report << "| Medium | " << (medIt != review.issues_by_severity.end() ? medIt->second : 0) << " |\n";
        report << "| Low | " << (lowIt != review.issues_by_severity.end() ? lowIt->second : 0) << " |\n\n";
        
        report << "## Top Issues\n\n";
        for (const auto& issue : review.top_issues) {
            report << "### " << issue.title << "\n";
            report << "- **Severity:** " << static_cast<int>(issue.severity) << "\n";
            report << "- **Location:** " << issue.location.file_path << ":" << issue.location.start_line << "\n";
            report << "- **Description:** " << issue.description << "\n\n";
        }
        
        report << "## Recommendations\n\n";
        for (const auto& rec : review.recommendations) {
            report << "- " << rec << "\n";
        }
    } else if (format == "json") {
        report << "{\n";
        report << "  \"files_reviewed\": " << review.files_reviewed << ",\n";
        report << "  \"total_issues\": " << review.total_issues << ",\n";
        report << "  \"quality_score\": " << review.overall_quality_score << ",\n";
        report << "  \"security_score\": " << review.overall_security_score << ",\n";
        report << "  \"performance_score\": " << review.overall_performance_score << "\n";
        report << "}";
    }
    
    return report.str();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Internal Helpers
// ═══════════════════════════════════════════════════════════════════════════════

Language IntelligentReviewSystem::detectLanguage(const std::string& file_path, const std::string& content) {
    std::string ext = file_path.substr(file_path.find_last_of('.'));
    
    if (ext == ".ts" || ext == ".tsx") return Language::TypeScript;
    if (ext == ".js" || ext == ".jsx") return Language::JavaScript;
    if (ext == ".py") return Language::Python;
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx") return Language::Cpp;
    if (ext == ".c") return Language::C;
    if (ext == ".rs") return Language::Rust;
    if (ext == ".go") return Language::Go;
    if (ext == ".java") return Language::Java;
    if (ext == ".cs") return Language::CSharp;
    if (ext == ".php") return Language::Php;
    if (ext == ".rb") return Language::Ruby;
    if (ext == ".swift") return Language::Swift;
    if (ext == ".kt") return Language::Kotlin;
    if (ext == ".scala") return Language::Scala;
    
    return Language::Unknown;
}

std::vector<CodeIssue> IntelligentReviewSystem::runSecurityScan(const std::string& content, Language lang) {
    std::vector<CodeIssue> issues;
    auto findings = scanSecurity(content, lang);
    
    for (const auto& finding : findings) {
        CodeIssue issue;
        issue.id = generateIssueId();
        issue.rule_id = finding.vulnerability_id;
        issue.title = finding.title;
        issue.description = finding.description;
        issue.severity = finding.severity;
        issue.category = Category::Security;
        issue.location = finding.location;
        issue.confidence = 0.9f;
        issue.model_source = "security_scanner";
        
        for (const auto& step : finding.remediation_steps) {
            FixSuggestion fix;
            fix.description = step;
            fix.type = FixType::Suggestion;
            issue.fixes.push_back(fix);
        }
        
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<CodeIssue> IntelligentReviewSystem::runPerformanceAnalysis(const std::string& content, Language lang) {
    std::vector<CodeIssue> issues;
    auto perfIssues = analyzePerformance(content, lang);
    
    for (const auto& perf : perfIssues) {
        CodeIssue issue;
        issue.id = generateIssueId();
        issue.rule_id = perf.id;
        issue.title = perf.title;
        issue.description = perf.description;
        issue.severity = perf.severity;
        issue.category = Category::Performance;
        issue.confidence = 0.8f;
        issue.model_source = "performance_analyzer";
        
        for (const auto& suggestion : perf.suggestions) {
            issue.tags.push_back(suggestion);
        }
        
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<CodeIssue> IntelligentReviewSystem::runPracticeCheck(const std::string& content, Language lang) {
    std::vector<CodeIssue> issues;
    auto violations = checkPractices(content, lang);
    
    for (const auto& violation : violations) {
        CodeIssue issue;
        issue.id = generateIssueId();
        issue.rule_id = violation.rule_id;
        issue.title = violation.title;
        issue.description = violation.description;
        issue.severity = violation.severity;
        issue.category = Category::BestPractice;
        issue.location = violation.location;
        issue.confidence = 0.85f;
        issue.model_source = "practice_checker";
        
        for (const auto& suggestion : violation.suggestions) {
            issue.tags.push_back(suggestion);
        }
        
        issues.push_back(issue);
    }
    
    return issues;
}

std::vector<CodeIssue> IntelligentReviewSystem::runSmellDetection(const std::string& content, Language lang) {
    std::vector<CodeIssue> issues;
    auto smells = detectSmells(content, lang);
    
    for (const auto& smell : smells) {
        CodeIssue issue;
        issue.id = generateIssueId();
        issue.rule_id = smell.id;
        issue.title = smell.name;
        issue.description = smell.description;
        issue.severity = smell.severity;
        issue.category = Category::Maintainability;
        issue.confidence = smell.detection_confidence;
        issue.model_source = "smell_detector";
        
        for (const auto& refactoring : smell.refactorings) {
            issue.tags.push_back(refactoring);
        }
        
        issues.push_back(issue);
    }
    
    return issues;
}

float IntelligentReviewSystem::calculateQualityScore(const FileReview& review) {
    if (review.total_issues == 0) return 100.0f;
    
    float penalty = 0.0f;
    penalty += review.critical_count * 20.0f;
    penalty += review.high_count * 10.0f;
    penalty += review.medium_count * 5.0f;
    penalty += review.low_count * 1.0f;
    
    return std::max(0.0f, 100.0f - penalty);
}

float IntelligentReviewSystem::calculateSecurityScore(const FileReview& review) {
    int securityIssues = 0;
    for (const auto& issue : review.issues) {
        if (issue.category == Category::Security) {
            securityIssues++;
        }
    }
    
    if (securityIssues == 0) return 100.0f;
    
    float penalty = 0.0f;
    for (const auto& issue : review.issues) {
        if (issue.category == Category::Security) {
            switch (issue.severity) {
                case Severity::Critical: penalty += 30.0f; break;
                case Severity::High: penalty += 20.0f; break;
                case Severity::Medium: penalty += 10.0f; break;
                case Severity::Low: penalty += 5.0f; break;
                default: break;
            }
        }
    }
    
    return std::max(0.0f, 100.0f - penalty);
}

float IntelligentReviewSystem::calculatePerformanceScore(const FileReview& review) {
    int perfIssues = 0;
    for (const auto& issue : review.issues) {
        if (issue.category == Category::Performance) {
            perfIssues++;
        }
    }
    
    if (perfIssues == 0) return 100.0f;
    
    float penalty = 0.0f;
    for (const auto& issue : review.issues) {
        if (issue.category == Category::Performance) {
            switch (issue.severity) {
                case Severity::Critical: penalty += 25.0f; break;
                case Severity::High: penalty += 15.0f; break;
                case Severity::Medium: penalty += 8.0f; break;
                case Severity::Low: penalty += 3.0f; break;
                default: break;
            }
        }
    }
    
    return std::max(0.0f, 100.0f - penalty);
}

std::string IntelligentReviewSystem::generateIssueId() {
    return "issue-" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

bool IntelligentReviewSystem::isSuppressed(const CodeIssue& issue, const ReviewConfig& config) {
    // Check if rule is in exclude patterns
    for (const auto& pattern : config.exclude_patterns) {
        if (issue.rule_id.find(pattern) != std::string::npos) {
            return true;
        }
    }
    
    // Check severity threshold
    for (const auto& sev : config.minimum_severity) {
        if (static_cast<int>(issue.severity) < static_cast<int>(sev)) {
            return true;
        }
    }
    
    return false;
}

std::vector<FixSuggestion> IntelligentReviewSystem::generateFixes(const CodeIssue& issue, const std::string& content) {
    std::vector<FixSuggestion> fixes;
    
    FixSuggestion fix = generateFix(issue, content);
    if (fix.type != FixType::None) {
        fixes.push_back(fix);
    }
    
    return fixes;
}

} // namespace review
