#include "codebase_audit_system.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>

namespace RawrXD::Audit {

// =====================================================================
// ANALYSIS PATTERN DEFINITIONS
// =====================================================================

// Security vulnerability patterns
const std::vector<std::pair<std::regex, std::string>> SECURITY_PATTERNS = {
    {std::regex(R"(strcpy\s*\()", std::regex_constants::icase), "Buffer overflow risk: strcpy usage"},
    {std::regex(R"(strcat\s*\()", std::regex_constants::icase), "Buffer overflow risk: strcat usage"},
    {std::regex(R"(sprintf\s*\()", std::regex_constants::icase), "Buffer overflow risk: sprintf usage"},
    {std::regex(R"(gets\s*\()", std::regex_constants::icase), "Buffer overflow risk: gets usage"},
    {std::regex(R"(\bsystem\s*\()", std::regex_constants::icase), "Command injection risk: system() call"},
    {std::regex(R"(eval\s*\()", std::regex_constants::icase), "Code injection risk: eval usage"},
    {std::regex(R"(reinterpret_cast\s*<)", std::regex_constants::icase), "Unsafe cast: reinterpret_cast"},
    {std::regex(R"(\bnew\s+[^[]+(?!\[))", std::regex_constants::icase), "Memory leak risk: raw new without delete"},
    {std::regex(R"(delete\s+[^[\]]+(?!\[\]))", std::regex_constants::icase), "Memory safety: manual delete"},
    {std::regex(R"(password\s*=\s*[\"'][^\"']+[\"'])", std::regex_constants::icase), "Hardcoded password detected"},
    {std::regex(R"(api[_-]?key\s*=\s*[\"'][^\"']+[\"'])", std::regex_constants::icase), "Hardcoded API key detected"},
    {std::regex(R"(secret\s*=\s*[\"'][^\"']+[\"'])", std::regex_constants::icase), "Hardcoded secret detected"},
    {std::regex(R"(\bmutex(?!\s*\w*lock))", std::regex_constants::icase), "Race condition risk: unprotected mutex"},
};

// Performance issue patterns
const std::vector<std::pair<std::regex, std::string>> PERFORMANCE_PATTERNS = {
    {std::regex(R"(for\s*\([^}]*\+\+\s*\)\s*{[^}]*for\s*\([^}]*\+\+)", std::regex_constants::icase), "Nested loops detected - O(n²) complexity"},
    {std::regex(R"(std::find\s*\([^)]*\.begin\(\)\s*,\s*[^)]*\.end\(\))", std::regex_constants::icase), "Linear search in container - consider std::set/map"},
    {std::regex(R"(std::vector<[^>]+>\s+\w+\s*;\s*for)", std::regex_constants::icase), "Vector without reserve() before loop"},
    {std::regex(R"(std::string\s+\w+\s*=\s*[^;]*\+)", std::regex_constants::icase), "String concatenation - consider stringstream"},
    {std::regex(R"(\bnew\s+\w+\[)", std::regex_constants::icase), "Dynamic array allocation - consider std::vector"},
    {std::regex(R"(std::endl)", std::regex_constants::icase), "std::endl usage - consider '\\n' for performance"},
    {std::regex(R"(recursive.*function)", std::regex_constants::icase), "Recursive function detected - check stack depth"},
    {std::regex(R"(while\s*\(\s*true\s*\))", std::regex_constants::icase), "Infinite loop detected"},
};

// Code quality patterns
const std::vector<std::pair<std::regex, std::string>> QUALITY_PATTERNS = {
    {std::regex(R"(throw\s+\w+)", std::regex_constants::icase), "Exception usage detected"},
    {std::regex(R"(catch\s*\([^)]*\))", std::regex_constants::icase), "Exception handling present"},
    {std::regex(R"(std::(unique_ptr|shared_ptr|weak_ptr))", std::regex_constants::icase), "Smart pointer usage"},
    {std::regex(R"(\*\s*\w+\s*=)", std::regex_constants::icase), "Raw pointer dereference"},
    {std::regex(R"(template\s*<)", std::regex_constants::icase), "Template usage"},
    {std::regex(R"(constexpr|const\s+)", std::regex_constants::icase), "Const correctness"},
    {std::regex(R"(auto\s+\w+\s*=)", std::regex_constants::icase), "Modern C++ auto keyword"},
    {std::regex(R"(\[\w*\]\s*\([^)]*\)\s*{)", std::regex_constants::icase), "Lambda function usage"},
};

// =====================================================================
// CODEBASE AUDIT SYSTEM IMPLEMENTATION  
// =====================================================================

CodebaseAuditSystem::CodebaseAuditSystem()
    : initialization_time_(std::chrono::system_clock::now()) {
    std::cout << "[CodebaseAudit] Codebase Audit System created" << std::endl;
}

CodebaseAuditSystem::~CodebaseAuditSystem() {
    shutdown();
}

bool CodebaseAuditSystem::initialize(const AuditConfiguration& config, 
                                   bool enable_continuous_monitoring) {
    if (initialized_.load()) {
        std::cout << "[CodebaseAudit] Already initialized" << std::endl;
        return true;
    }
    
    try {
        config_ = config;
        
        // Initialize default file patterns
        if (include_patterns_.empty()) {
            include_patterns_ = {"*.cpp", "*.hpp", "*.h", "*.cc", "*.cxx"};
        }
        
        exclude_patterns_ = {"*/build/*", "*/CMakeFiles/*", "*/.git/*", "*/node_modules/*"};
        
        // Initialize statistics
        audit_statistics_ = AuditStatistics{};
        
        if (enable_continuous_monitoring) {
            continuous_monitoring_enabled_ = true;
            monitoring_thread_ = std::thread(&CodebaseAuditSystem::monitoring_thread_function, this);
        }
        
        initialized_ = true;
        
        std::cout << "[CodebaseAudit] Codebase Audit System initialized" << std::endl;
        std::cout << "  - Deep static analysis: " << (config_.perform_deep_static_analysis ? "enabled" : "disabled") << std::endl;
        std::cout << "  - Security analysis: " << (config_.check_security_issues ? "enabled" : "disabled") << std::endl;
        std::cout << "  - Performance analysis: " << (config_.analyze_performance ? "enabled" : "disabled") << std::endl;
        std::cout << "  - Continuous monitoring: " << (enable_continuous_monitoring ? "enabled" : "disabled") << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[CodebaseAudit] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void CodebaseAuditSystem::shutdown() {
    if (!initialized_.load()) return;
    
    std::cout << "[CodebaseAudit] Shutting down audit system..." << std::endl;
    
    shutting_down_ = true;
    
    // Stop continuous monitoring
    continuous_monitoring_enabled_ = false;
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    // Wait for analysis threads
    for (auto& thread : analysis_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    analysis_threads_.clear();
    
    // Clear caches
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        analysis_cache_.clear();
        cache_timestamps_.clear();
    }
    
    initialized_ = false;
    shutting_down_ = false;
    
    std::cout << "[CodebaseAudit] Audit system shutdown complete" << std::endl;
}

// ================================================================
// COMPREHENSIVE AUDIT OPERATIONS
// ================================================================

ProjectAuditResult CodebaseAuditSystem::audit_full_project(const std::string& project_path,
                                                          const std::string& project_name) {
    if (!initialized_.load()) {
        ProjectAuditResult result;
        result.project_name = project_name.empty() ? "Unknown Project" : project_name;
        result.project_path = project_path;
        std::cout << "[CodebaseAudit] Error: Audit system not initialized" << std::endl;
        return result;
    }
    
    std::cout << "[CodebaseAudit] Starting full project audit: " << project_path << std::endl;
    
    ProjectAuditResult result;
    result.project_name = project_name.empty() ? std::filesystem::path(project_path).filename().string() : project_name;
    result.project_path = project_path;
    result.audit_start_time = std::chrono::system_clock::now();
    
    try {
        // Collect all source files
        std::vector<std::string> source_files = collect_source_files(project_path, true);
        std::cout << "[CodebaseAudit] Found " << source_files.size() << " source files" << std::endl;
        
        if (source_files.empty()) {
            std::cout << "[CodebaseAudit] No source files found in project" << std::endl;
            result.audit_completion_time = std::chrono::system_clock::now();
            return result;
        }
        
        // Analyze each file
        result.file_results.reserve(source_files.size());
        
        for (const auto& file_path : source_files) {
            try {
                FileAnalysisResult file_result = analyze_source_file(file_path);
                result.file_results.push_back(file_result);
                
                // Categorize by file type
                std::string extension = std::filesystem::path(file_path).extension().string();
                result.results_by_category[extension].push_back(file_result);
                
                result.total_files_analyzed++;
                
                if (file_result.production_ready) {
                    result.files_passed++;
                } else {
                    result.files_failed++;
                }
                
                // Count issues by severity
                for (const auto& issue : file_result.issues_found) {
                    if (issue.find("Critical:") == 0) {
                        result.critical_issues++;
                    } else if (issue.find("Major:") == 0) {
                        result.major_issues++;
                    } else {
                        result.minor_issues++;
                    }
                }
                
            } catch (const std::exception& e) {
                std::cerr << "[CodebaseAudit] Error analyzing file " << file_path << ": " << e.what() << std::endl;
                result.files_failed++;
            }
        }
        
        // Calculate aggregate metrics
        calculate_aggregate_metrics(result);
        
        // Assess overall production readiness
        result.production_readiness = assess_production_readiness(project_path);
        
        // Calculate project overall score
        result.project_overall_score = calculate_project_score(result);
        result.project_production_ready = (result.project_overall_score >= 0.8 && 
                                         result.production_readiness.overall_readiness_score >= 0.7);
        
        // Generate top priority fixes
        result.top_priority_fixes = generate_improvement_recommendations(result, 10);
        
        result.audit_completion_time = std::chrono::system_clock::now();
        result.total_audit_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.audit_completion_time - result.audit_start_time);
        
        // Update statistics
        update_statistics(result);
        
        std::cout << "[CodebaseAudit] Project audit completed in " << result.total_audit_time.count() 
                  << "ms" << std::endl;
        std::cout << "  - Files analyzed: " << result.total_files_analyzed << std::endl;
        std::cout << "  - Files passed: " << result.files_passed << std::endl;
        std::cout << "  - Critical issues: " << result.critical_issues << std::endl;
        std::cout << "  - Overall score: " << std::fixed << std::setprecision(2) 
                  << result.project_overall_score << std::endl;
        std::cout << "  - Production ready: " << (result.project_production_ready ? "YES" : "NO") << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[CodebaseAudit] Project audit failed: " << e.what() << std::endl;
        result.audit_completion_time = std::chrono::system_clock::now();
        result.total_audit_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.audit_completion_time - result.audit_start_time);
    }
    
    return result;
}

FileAnalysisResult CodebaseAuditSystem::analyze_source_file(const std::string& file_path) {
    FileAnalysisResult result;
    result.file_path = file_path;
    result.analysis_time = std::chrono::system_clock::now();
    
    try {
        // Check cache first
        if (is_cache_valid(file_path)) {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto cache_it = analysis_cache_.find(file_path);
            if (cache_it != analysis_cache_.end()) {
                return cache_it->second;
            }
        }
        
        // Read file content
        std::string file_content = read_file_content(file_path);
        if (file_content.empty()) {
            result.issues_found.push_back("Critical: Unable to read file content");
            return result;
        }
        
        result.file_size_bytes = file_content.length();
        result.file_type = std::filesystem::path(file_path).extension().string();
        
        std::cout << "[CodebaseAudit] Analyzing file: " << std::filesystem::path(file_path).filename().string() 
                  << " (" << result.file_size_bytes << " bytes)" << std::endl;
        
        // Perform comprehensive analysis
        result.quality_metrics = analyze_code_quality(file_content, result.file_type);
        
        if (config_.check_security_issues) {
            result.security_analysis = analyze_security_vulnerabilities(file_content, file_path);
        }
        
        if (config_.analyze_performance) {
            result.performance_analysis = analyze_performance_characteristics(file_content, file_path);
        }
        
        if (config_.check_coding_standards) {
            std::map<std::string, std::string> project_context; // Simplified for this implementation
            result.architecture_compliance = assess_architecture_compliance(file_content, project_context);
        }
        
        // Collect all issues
        collect_file_issues(result);
        
        // Generate recommendations
        generate_file_recommendations(result);
        
        // Calculate overall file score
        result.overall_score = calculate_file_score(result);
        
        // Determine production readiness
        result.production_ready = (result.overall_score >= 0.7 && 
                                 result.security_analysis.passes_security_threshold &&
                                 result.performance_analysis.meets_performance_requirements);
        
        result.analysis_successful = true;
        
        // Update cache
        update_cache(file_path, result);
        
    } catch (const std::exception& e) {
        std::cerr << "[CodebaseAudit] Error analyzing file " << file_path << ": " << e.what() << std::endl;
        result.issues_found.push_back("Critical: Analysis failed - " + std::string(e.what()));
        result.analysis_successful = false;
    }
    
    return result;
}

ProductionReadinessAssessment CodebaseAuditSystem::assess_production_readiness(const std::string& project_path) {
    ProductionReadinessAssessment assessment;
    
    try {
        std::cout << "[CodebaseAudit] Assessing production readiness for: " << project_path << std::endl;
        
        // Check for essential infrastructure files
        std::vector<std::string> essential_files = {
            "CMakeLists.txt", "Makefile", "README.md", "LICENSE", 
            "Dockerfile", "docker-compose.yml", ".gitignore"
        };
        
        for (const auto& file : essential_files) {
            std::string file_path = project_path + "/" + file;
            if (std::filesystem::exists(file_path)) {
                if (file == "CMakeLists.txt" || file == "Makefile") {
                    assessment.has_configuration_management = true;
                }
                if (file == "README.md") {
                    assessment.has_api_documentation = true;
                }
                if (file == "Dockerfile") {
                    assessment.supports_container_deployment = true;
                }
            }
        }
        
        // Check for logging infrastructure
        std::vector<std::string> source_files = collect_source_files(project_path, true);
        bool has_logging_system = false;
        bool has_error_handling = false;
        bool has_monitoring = false;
        
        for (const auto& file_path : source_files) {
            try {
                std::string content = read_file_content(file_path);
                
                // Check for logging
                if (content.find("std::cout") != std::string::npos ||
                    content.find("LOG_") != std::string::npos ||
                    content.find("logger") != std::string::npos ||
                    content.find("spdlog") != std::string::npos) {
                    has_logging_system = true;
                }
                
                // Check for error handling
                if (content.find("try") != std::string::npos ||
                    content.find("catch") != std::string::npos ||
                    content.find("error") != std::string::npos ||
                    content.find("exception") != std::string::npos) {
                    has_error_handling = true;
                }
                
                // Check for monitoring
                if (content.find("health") != std::string::npos ||
                    content.find("monitor") != std::string::npos ||
                    content.find("telemetry") != std::string::npos ||
                    content.find("metrics") != std::string::npos) {
                    has_monitoring = true;
                }
                
            } catch (const std::exception& e) {
                std::cerr << "[CodebaseAudit] Error reading file for readiness assessment: " << e.what() << std::endl;
            }
        }
        
        assessment.has_proper_logging_system = has_logging_system;
        assessment.has_comprehensive_error_handling = has_error_handling;
        assessment.has_monitoring_capabilities = has_monitoring;
        
        // Check for testing infrastructure  
        std::vector<std::string> test_patterns = {"test", "spec", "gtest", "catch"};
        for (const auto& file_path : source_files) {
            std::string filename = std::filesystem::path(file_path).filename().string();
            for (const auto& pattern : test_patterns) {
                if (filename.find(pattern) != std::string::npos) {
                    assessment.has_unit_tests = true;
                    break;
                }
            }
        }
        
        // Calculate readiness score
        uint32_t readiness_indicators = 0;
        uint32_t total_indicators = 20;
        
        if (assessment.has_comprehensive_error_handling) readiness_indicators += 2;
        if (assessment.has_proper_logging_system) readiness_indicators += 2;
        if (assessment.has_configuration_management) readiness_indicators += 2;
        if (assessment.has_monitoring_capabilities) readiness_indicators += 2;
        if (assessment.has_unit_tests) readiness_indicators += 2;
        if (assessment.has_api_documentation) readiness_indicators += 1;
        if (assessment.supports_container_deployment) readiness_indicators += 2;
        if (assessment.has_graceful_shutdown) readiness_indicators += 1;
        if (assessment.has_health_checks) readiness_indicators += 1;
        if (assessment.has_resource_limits) readiness_indicators += 1;
        if (assessment.has_backup_recovery) readiness_indicators += 1;
        if (assessment.has_rollback_capability) readiness_indicators += 1;
        if (assessment.has_deployment_guide) readiness_indicators += 1;
        if (assessment.has_troubleshooting_guide) readiness_indicators += 1;
        
        assessment.overall_readiness_score = static_cast<double>(readiness_indicators) / total_indicators;
        
        // Identify critical blockers
        if (!assessment.has_comprehensive_error_handling) {
            assessment.critical_blockers.push_back("Missing comprehensive error handling");
        }
        if (!assessment.has_proper_logging_system) {
            assessment.critical_blockers.push_back("Missing proper logging system");
        }
        if (!assessment.has_unit_tests) {
            assessment.critical_blockers.push_back("Missing unit tests");
        }
        if (!assessment.has_monitoring_capabilities) {
            assessment.critical_blockers.push_back("Missing monitoring capabilities");
        }
        
        // Generate recommendations
        if (assessment.overall_readiness_score < 0.8) {
            assessment.recommended_improvements.push_back("Implement comprehensive error handling throughout codebase");
            assessment.recommended_improvements.push_back("Add structured logging with appropriate log levels");
            assessment.recommended_improvements.push_back("Create unit tests for all critical functions");
            assessment.recommended_improvements.push_back("Add health monitoring and telemetry collection");
            assessment.recommended_improvements.push_back("Implement graceful shutdown procedures");
        }
        
        std::cout << "[CodebaseAudit] Production readiness assessment complete" << std::endl;
        std::cout << "  - Overall readiness score: " << std::fixed << std::setprecision(2) 
                  << assessment.overall_readiness_score << std::endl;
        std::cout << "  - Critical blockers: " << assessment.critical_blockers.size() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[CodebaseAudit] Production readiness assessment failed: " << e.what() << std::endl;
        assessment.overall_readiness_score = 0.0;
    }
    
    return assessment;
}

// ================================================================
// SPECIFIC ANALYSIS IMPLEMENTATIONS  
// ================================================================

CodeQualityMetrics CodebaseAuditSystem::analyze_code_quality(const std::string& file_content,
                                                           const std::string& file_extension) {
    CodeQualityMetrics metrics;
    
    if (file_extension == ".cpp" || file_extension == ".hpp" || 
        file_extension == ".h" || file_extension == ".cc") {
        return analyze_cpp_quality(file_content);
    }
    
    // Basic metrics for any file type
    std::istringstream iss(file_content);
    std::string line;
    
    while (std::getline(iss, line)) {
        metrics.lines_of_code++;
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) {
            metrics.blank_lines++;
        } else if (line.find("//") == 0 || line.find("/*") != std::string::npos ||
                   line.find("*") == 0) {
            metrics.comment_lines++;
        }
    }
    
    if (metrics.lines_of_code > 0) {
        metrics.comment_ratio = static_cast<double>(metrics.comment_lines) / metrics.lines_of_code;
    }
    
    return metrics;
}

CodeQualityMetrics CodebaseAuditSystem::analyze_cpp_quality(const std::string& content) {
    CodeQualityMetrics metrics;
    
    // Basic line counting
    std::istringstream iss(content);
    std::string line;
    
    while (std::getline(iss, line)) {
        metrics.lines_of_code++;
        
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (line.empty()) {
            metrics.blank_lines++;
        } else if (line.find("//") == 0 || line.find("/*") != std::string::npos) {
            metrics.comment_lines++;
        }
    }
    
    // Calculate comment ratio
    if (metrics.lines_of_code > 0) {
        metrics.comment_ratio = static_cast<double>(metrics.comment_lines) / metrics.lines_of_code;
    }
    
    // Count C++ constructs
    metrics.function_count = count_functions(content);
    metrics.class_count = count_classes(content);
    metrics.cyclomatic_complexity = calculate_cyclomatic_complexity(content);
    
    if (metrics.function_count > 0) {
        metrics.complexity_per_function = static_cast<double>(metrics.cyclomatic_complexity) / metrics.function_count;
    }
    
    // Count modern C++ features
    for (const auto& [pattern, description] : QUALITY_PATTERNS) {
        auto matches = find_regex_matches(content, pattern);
        if (description.find("Smart pointer") != std::string::npos) {
            metrics.smart_pointer_usage += matches.size();
        } else if (description.find("Template") != std::string::npos) {
            metrics.template_usage_count += matches.size();
        } else if (description.find("Modern C++") != std::string::npos) {
            metrics.modern_cpp_features += matches.size();
        } else if (description.find("Exception") != std::string::npos) {
            metrics.exception_usage_count += matches.size();
        }
    }
    
    // Count raw pointer usage
    std::regex raw_pointer_pattern(R"(\*\s*\w+\s*[=;,)])");
    metrics.raw_pointer_usage = count_pattern_occurrences(content, raw_pointer_pattern);
    
    // Check for production readiness indicators
    metrics.has_error_handling = contains_pattern(content, std::regex(R"(try|catch|throw|error)"));
    metrics.has_logging = contains_pattern(content, std::regex(R"(std::cout|LOG_|logger|spdlog)"));
    metrics.follows_naming_conventions = check_naming_conventions(content);
    metrics.has_documentation = (metrics.comment_ratio > config_.min_comment_ratio);
    
    return metrics;
}

SecurityAnalysis CodebaseAuditSystem::analyze_security_vulnerabilities(const std::string& file_content,
                                                                      const std::string& file_path) {
    SecurityAnalysis analysis;
    
    // Check for security vulnerability patterns
    for (const auto& [pattern, description] : SECURITY_PATTERNS) {
        auto matches = find_regex_matches(file_content, pattern);
        if (!matches.empty()) {
            analysis.security_issues.push_back(description + " (found " + std::to_string(matches.size()) + " instances)");
            
            if (description.find("Buffer overflow") != std::string::npos) {
                analysis.buffer_overflow_risks += matches.size();
            } else if (description.find("injection") != std::string::npos) {
                analysis.injection_vulnerabilities += matches.size();
            } else if (description.find("Hardcoded") != std::string::npos) {
                analysis.hardcoded_secrets += matches.size();
            } else if (description.find("Unsafe cast") != std::string::npos) {
                analysis.unsafe_casts += matches.size();
            } else if (description.find("Memory leak") != std::string::npos) {
                analysis.memory_leaks_potential += matches.size();
            } else if (description.find("Race condition") != std::string::npos) {
                analysis.race_condition_risks += matches.size();
            }
        }
    }
    
    // Check for input validation patterns
    std::regex input_validation_pattern(R"(if\s*\([^)]*\.(empty|size|length)\(\))");
    if (!contains_pattern(file_content, input_validation_pattern)) {
        analysis.unchecked_inputs++;
        analysis.security_issues.push_back("Potential unchecked input - no input validation detected");
    }
    
    // Generate security recommendations
    if (analysis.buffer_overflow_risks > 0) {
        analysis.recommendations.push_back("Replace unsafe string functions with safe alternatives (strncpy, snprintf, etc.)");
    }
    if (analysis.hardcoded_secrets > 0) {
        analysis.recommendations.push_back("Move secrets to secure configuration or environment variables");
    }
    if (analysis.unsafe_casts > 0) {
        analysis.recommendations.push_back("Review reinterpret_cast usage and consider safer alternatives");
    }
    if (analysis.memory_leaks_potential > 0) {
        analysis.recommendations.push_back("Use smart pointers instead of raw memory management");
    }
    if (analysis.race_condition_risks > 0) {
        analysis.recommendations.push_back("Ensure proper mutex locking around shared resources");
    }
    
    // Calculate security score
    uint32_t total_issues = analysis.buffer_overflow_risks + analysis.unchecked_inputs + 
                           analysis.hardcoded_secrets + analysis.unsafe_casts + 
                           analysis.memory_leaks_potential + analysis.race_condition_risks + 
                           analysis.injection_vulnerabilities;
    
    // Security score decreases with issues found
    analysis.overall_security_score = std::max(0.0, 1.0 - (total_issues * 0.1));
    analysis.passes_security_threshold = (analysis.overall_security_score >= 0.7);
    
    return analysis;
}

PerformanceAnalysis CodebaseAuditSystem::analyze_performance_characteristics(const std::string& file_content,
                                                                           const std::string& file_path) {
    PerformanceAnalysis analysis;
    
    // Check for performance issue patterns
    for (const auto& [pattern, description] : PERFORMANCE_PATTERNS) {
        auto matches = find_regex_matches(file_content, pattern);
        if (!matches.empty()) {
            analysis.performance_warnings.push_back(description + " (found " + std::to_string(matches.size()) + " instances)");
            
            if (description.find("O(n²)") != std::string::npos) {
                analysis.inefficient_algorithms += matches.size();
            } else if (description.find("allocation") != std::string::npos) {
                analysis.unnecessary_allocations += matches.size();
            } else if (description.find("bottleneck") != std::string::npos) {
                analysis.potential_bottlenecks += matches.size();
            }
        }
    }
    
    // Check for blocking operations
    std::regex blocking_pattern(R"(sleep|wait|block|synchronous|std::this_thread::sleep)");
    auto blocking_matches = find_regex_matches(file_content, blocking_pattern);
    analysis.blocking_operations = blocking_matches.size();
    
    if (analysis.blocking_operations > 0) {
        analysis.performance_warnings.push_back("Blocking operations detected - consider asynchronous alternatives");
    }
    
    // Check for memory-intensive operations
    std::regex memory_intensive_pattern(R"(vector<.*>\s+\w+\s*\([0-9]+\))");
    auto memory_matches = find_regex_matches(file_content, memory_intensive_pattern);
    analysis.memory_intensive_operations = memory_matches.size();
    
    // Generate optimization opportunities
    if (analysis.inefficient_algorithms > 0) {
        analysis.optimization_opportunities.push_back("Replace O(n²) algorithms with more efficient alternatives");
    }
    if (analysis.unnecessary_allocations > 0) {
        analysis.optimization_opportunities.push_back("Use object pooling or pre-allocation for frequently created objects");
    }
    if (analysis.memory_intensive_operations > 0) {
        analysis.optimization_opportunities.push_back("Consider streaming or chunked processing for large data sets");
    }
    
    // Calculate performance score
    uint32_t total_issues = analysis.potential_bottlenecks + analysis.inefficient_algorithms + 
                           analysis.memory_intensive_operations + analysis.blocking_operations + 
                           analysis.unnecessary_allocations;
    
    analysis.performance_score = std::max(0.0, 1.0 - (total_issues * 0.05));
    analysis.meets_performance_requirements = (analysis.performance_score >= 0.8);
    
    return analysis;
}

// ================================================================
// UTILITY FUNCTIONS
// ================================================================

std::vector<std::string> CodebaseAuditSystem::collect_source_files(const std::string& root_path, bool recursive) {
    std::vector<std::string> source_files;
    
    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path)) {
                if (entry.is_regular_file() && should_analyze_file(entry.path().string())) {
                    source_files.push_back(entry.path().string());
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(root_path)) {
                if (entry.is_regular_file() && should_analyze_file(entry.path().string())) {
                    source_files.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "[CodebaseAudit] Filesystem error: " << e.what() << std::endl;
    }
    
    return source_files;
}

bool CodebaseAuditSystem::should_analyze_file(const std::string& file_path) {
    // Check exclude patterns first
    for (const auto& pattern : exclude_patterns_) {
        std::regex exclude_regex(pattern, std::regex_constants::icase);
        if (std::regex_search(file_path, exclude_regex)) {
            return false;
        }
    }
    
    // Check include patterns
    for (const auto& pattern : include_patterns_) {
        std::regex include_regex(pattern, std::regex_constants::icase);
        if (std::regex_search(file_path, include_regex)) {
            return true;
        }
    }
    
    return false;
}

std::string CodebaseAuditSystem::read_file_content(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Unable to open file: " + file_path);
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

uint32_t CodebaseAuditSystem::count_functions(const std::string& content) {
    std::regex function_pattern(R"(\w+\s+\w+\s*\([^)]*\)\s*\{)");
    return count_pattern_occurrences(content, function_pattern);
}

uint32_t CodebaseAuditSystem::count_classes(const std::string& content) {
    std::regex class_pattern(R"(class\s+\w+)", std::regex_constants::icase);
    return count_pattern_occurrences(content, class_pattern);
}

uint32_t CodebaseAuditSystem::calculate_cyclomatic_complexity(const std::string& content) {
    // Simplified cyclomatic complexity calculation
    std::regex complexity_pattern(R"(\bif\b|\bwhile\b|\bfor\b|\bswitch\b|\bcase\b|\bcatch\b|\?)");
    return 1 + count_pattern_occurrences(content, complexity_pattern);
}

std::vector<std::smatch> CodebaseAuditSystem::find_regex_matches(const std::string& content, 
                                                               const std::regex& pattern) {
    std::vector<std::smatch> matches;
    std::sregex_iterator iter(content.begin(), content.end(), pattern);
    std::sregex_iterator end;
    
    while (iter != end) {
        matches.push_back(*iter);
        ++iter;
    }
    
    return matches;
}

bool CodebaseAuditSystem::contains_pattern(const std::string& content, const std::regex& pattern) {
    return std::regex_search(content, pattern);
}

uint32_t CodebaseAuditSystem::count_pattern_occurrences(const std::string& content, const std::regex& pattern) {
    auto matches = find_regex_matches(content, pattern);
    return static_cast<uint32_t>(matches.size());
}

// Helper functions for missing functionality
void CodebaseAuditSystem::calculate_aggregate_metrics(ProjectAuditResult& result) {
    // Reset aggregate metrics
    result.aggregate_quality = CodeQualityMetrics{};
    result.aggregate_security = SecurityAnalysis{};
    result.aggregate_performance = PerformanceAnalysis{};
    
    if (result.file_results.empty()) return;
    
    // Sum up all metrics
    for (const auto& file_result : result.file_results) {
        result.aggregate_quality.lines_of_code += file_result.quality_metrics.lines_of_code;
        result.aggregate_quality.comment_lines += file_result.quality_metrics.comment_lines;
        result.aggregate_quality.function_count += file_result.quality_metrics.function_count;
        result.aggregate_quality.class_count += file_result.quality_metrics.class_count;
        result.aggregate_quality.cyclomatic_complexity += file_result.quality_metrics.cyclomatic_complexity;
        
        result.aggregate_security.buffer_overflow_risks += file_result.security_analysis.buffer_overflow_risks;
        result.aggregate_security.hardcoded_secrets += file_result.security_analysis.hardcoded_secrets;
        result.aggregate_security.unsafe_casts += file_result.security_analysis.unsafe_casts;
        
        result.aggregate_performance.potential_bottlenecks += file_result.performance_analysis.potential_bottlenecks;
        result.aggregate_performance.inefficient_algorithms += file_result.performance_analysis.inefficient_algorithms;
    }
    
    // Calculate averages
    size_t file_count = result.file_results.size();
    if (file_count > 0) {
        result.aggregate_quality.comment_ratio = 
            static_cast<double>(result.aggregate_quality.comment_lines) / 
            std::max(1u, result.aggregate_quality.lines_of_code);
        
        result.aggregate_quality.complexity_per_function = 
            static_cast<double>(result.aggregate_quality.cyclomatic_complexity) / 
            std::max(1u, result.aggregate_quality.function_count);
    }
}

double CodebaseAuditSystem::calculate_project_score(const ProjectAuditResult& result) {
    if (result.file_results.empty()) return 0.0;
    
    double total_score = 0.0;
    for (const auto& file_result : result.file_results) {
        total_score += file_result.overall_score;
    }
    
    return total_score / result.file_results.size();
}

double CodebaseAuditSystem::calculate_file_score(const FileAnalysisResult& result) {
    double quality_weight = 0.3;
    double security_weight = 0.4;
    double performance_weight = 0.3;
    
    double quality_score = std::min(1.0, result.quality_metrics.comment_ratio * 2.0);
    
    double security_score = result.security_analysis.overall_security_score;
    double performance_score = result.performance_analysis.performance_score;
    
    return (quality_score * quality_weight + 
            security_score * security_weight + 
            performance_score * performance_weight);
}

void CodebaseAuditSystem::collect_file_issues(FileAnalysisResult& result) {
    // Collect issues from each analysis type
    for (const auto& issue : result.security_analysis.security_issues) {
        result.issues_found.push_back("Security: " + issue);
    }
    
    for (const auto& warning : result.performance_analysis.performance_warnings) {
        result.issues_found.push_back("Performance: " + warning);
    }
    
    // Add quality issues
    if (result.quality_metrics.comment_ratio < config_.min_comment_ratio) {
        result.issues_found.push_back("Quality: Low comment ratio (" + 
            std::to_string(result.quality_metrics.comment_ratio) + ")");
    }
    
    if (result.quality_metrics.complexity_per_function > config_.max_function_complexity) {
        result.issues_found.push_back("Quality: High complexity per function (" + 
            std::to_string(result.quality_metrics.complexity_per_function) + ")");
    }
}

void CodebaseAuditSystem::generate_file_recommendations(FileAnalysisResult& result) {
    // Add security recommendations
    for (const auto& rec : result.security_analysis.recommendations) {
        result.recommendations.push_back(rec);
    }
    
    // Add performance recommendations
    for (const auto& opp : result.performance_analysis.optimization_opportunities) {
        result.recommendations.push_back(opp);
    }
    
    // Add quality recommendations
    if (result.quality_metrics.comment_ratio < config_.min_comment_ratio) {
        result.recommendations.push_back("Add more comments to improve code documentation");
    }
    
    if (!result.quality_metrics.has_error_handling) {
        result.recommendations.push_back("Add comprehensive error handling");
    }
}

bool CodebaseAuditSystem::check_naming_conventions(const std::string& content) {
    // Simplified naming convention check
    std::regex snake_case_pattern(R"(\b[a-z][a-z0-9_]*\b)");
    std::regex camel_case_pattern(R"(\b[a-z][a-zA-Z0-9]*\b)");
    
    return contains_pattern(content, snake_case_pattern) || contains_pattern(content, camel_case_pattern);
}

void CodebaseAuditSystem::update_statistics(const ProjectAuditResult& result) {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    
    audit_statistics_.total_projects_audited++;
    audit_statistics_.total_files_analyzed += result.total_files_analyzed;
    audit_statistics_.total_issues_found += (result.critical_issues + result.major_issues + result.minor_issues);
    audit_statistics_.total_analysis_time += result.total_audit_time;
    audit_statistics_.last_audit_time = result.audit_completion_time;
    
    // Update average score
    audit_statistics_.average_project_score = 
        (audit_statistics_.average_project_score * (audit_statistics_.total_projects_audited - 1) + 
         result.project_overall_score) / audit_statistics_.total_projects_audited;
    
    // Store audit history  
    audit_history_.push_back(result);
}

void CodebaseAuditSystem::monitoring_thread_function() {
    // Placeholder for continuous monitoring
    while (continuous_monitoring_enabled_.load()) {
        std::this_thread::sleep_for(std::chrono::minutes(30));
        
        if (shutting_down_.load()) break;
        
        // Monitor projects would be implemented here
    }
}

bool CodebaseAuditSystem::is_cache_valid(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto cache_it = analysis_cache_.find(file_path);
    auto timestamp_it = cache_timestamps_.find(file_path);
    
    if (cache_it == analysis_cache_.end() || timestamp_it == cache_timestamps_.end()) {
        return false;
    }
    
    // Check if file has been modified since cache
    try {
        auto file_time = std::filesystem::last_write_time(file_path);
        auto cache_time = timestamp_it->second;
        
        // Convert file_time to system_clock time_point (simplified)
        auto cache_duration = cache_time.time_since_epoch();
        return true; // Simplified - assume cache is valid for this implementation
        
    } catch (const std::filesystem::filesystem_error&) {
        return false;
    }
}

void CodebaseAuditSystem::update_cache(const std::string& file_path, const FileAnalysisResult& result) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    analysis_cache_[file_path] = result;
    cache_timestamps_[file_path] = std::chrono::system_clock::now();
}

// Missing but referenced functions
std::vector<std::string> CodebaseAuditSystem::generate_improvement_recommendations(
    const ProjectAuditResult& audit_result, uint32_t max_recommendations) {
    
    std::vector<std::string> recommendations;
    
    // Priority-based recommendations
    if (audit_result.critical_issues > 0) {
        recommendations.push_back("CRITICAL: Address " + std::to_string(audit_result.critical_issues) + 
                                " critical security and stability issues");
    }
    
    if (audit_result.aggregate_security.buffer_overflow_risks > 0) {
        recommendations.push_back("HIGH: Fix buffer overflow vulnerabilities");
    }
    
    if (audit_result.aggregate_security.hardcoded_secrets > 0) {
        recommendations.push_back("HIGH: Remove hardcoded secrets and credentials");
    }
    
    if (!audit_result.production_readiness.has_comprehensive_error_handling) {
        recommendations.push_back("HIGH: Implement comprehensive error handling");
    }
    
    if (!audit_result.production_readiness.has_proper_logging_system) {
        recommendations.push_back("MEDIUM: Add structured logging system");
    }
    
    if (!audit_result.production_readiness.has_unit_tests) {
        recommendations.push_back("MEDIUM: Create unit test coverage");
    }
    
    if (audit_result.aggregate_performance.inefficient_algorithms > 0) {
        recommendations.push_back("MEDIUM: Optimize inefficient algorithms");
    }
    
    if (audit_result.aggregate_quality.comment_ratio < 0.15) {
        recommendations.push_back("LOW: Improve code documentation and comments");
    }
    
    // Limit to max_recommendations
    if (recommendations.size() > max_recommendations) {
        recommendations.resize(max_recommendations);
    }
    
    return recommendations;
}

} // namespace RawrXD::Audit