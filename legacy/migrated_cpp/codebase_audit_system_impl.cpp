#include "codebase_audit_system.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <thread>
#include <cctype>

namespace fs = std::filesystem;

namespace RawrXD::Audit {

extern "C" {
    __int64 CodebaseAudit_Initialize(void* config, __int64 enableMonitoring);
    __int64 CodebaseAudit_Shutdown();
    __int64 CodebaseAudit_ReadFileContent(const char* path, char* buffer, size_t capacity);
}

namespace {

size_t count_substring(const std::string& content, const std::string& needle) {
    if (needle.empty()) return 0;
    size_t count = 0;
    size_t pos = 0;
    while ((pos = content.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

size_t count_keyword(const std::string& content, const std::string& keyword) {
    if (keyword.empty()) return 0;
    size_t count = 0;
    size_t pos = 0;

    while ((pos = content.find(keyword, pos)) != std::string::npos) {
        const bool left_ok = (pos == 0) ||
            (!std::isalnum(static_cast<unsigned char>(content[pos - 1])) && content[pos - 1] != '_');
        const size_t right_index = pos + keyword.size();
        const bool right_ok = (right_index >= content.size()) ||
            (!std::isalnum(static_cast<unsigned char>(content[right_index])) && content[right_index] != '_');

        if (left_ok && right_ok) {
            ++count;
        }
        pos += keyword.size();
    }
    return count;
}

} // namespace

// =====================================================================
// CODEBASE AUDIT SYSTEM IMPLEMENTATION
// =====================================================================

CodebaseAuditSystem::CodebaseAuditSystem() {
    std::cout << "[AuditSystem] Codebase Audit System initialized" << std::endl;
}

CodebaseAuditSystem::~CodebaseAuditSystem() {
    shutdown();
}

bool CodebaseAuditSystem::initialize(const AuditConfiguration& config, 
                                    bool enable_continuous_monitoring) {
    if (initialized_.load()) {
        return true;
    }
    
    try {
        if (!CodebaseAudit_Initialize((void*)&config, enable_continuous_monitoring ? 1 : 0)) {
            std::cerr << "[AuditSystem] MASM backend initialization failed" << std::endl;
            return false;
        }

        config_ = config;
        initialization_time_ = std::chrono::system_clock::now();
        
        // Set default file filters if not configured
        if (include_patterns_.empty()) {
            include_patterns_ = {"**/*.cpp", "**/*.hpp", "**/*.h", "**/*.cc"};
        }
        
        // Initialize quality thresholds if not set
        if (custom_thresholds_.empty()) {
            custom_thresholds_["min_comment_ratio"] = config_.min_comment_ratio;
            custom_thresholds_["max_complexity"] = config_.max_function_complexity;
            custom_thresholds_["max_file_size"] = config_.max_file_size_kb;
            custom_thresholds_["min_test_coverage"] = config_.min_test_coverage;
        }
        
        if (enable_continuous_monitoring) {
            continuous_monitoring_enabled_ = true;
        }
        
        initialized_ = true;
        
        std::cout << "[AuditSystem] Audit system initialized successfully" << std::endl;
        std::cout << "  - Deep static analysis: " << (config_.perform_deep_static_analysis ? "enabled" : "disabled") << std::endl;
        std::cout << "  - Security analysis: " << (config_.check_security_issues ? "enabled" : "disabled") << std::endl;
        std::cout << "  - Performance analysis: " << (config_.analyze_performance ? "enabled" : "disabled") << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "[AuditSystem] Initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void CodebaseAuditSystem::shutdown() {
    if (!initialized_.load()) return;
    
    std::cout << "[AuditSystem] Shutting down audit system..." << std::endl;
    
    shutting_down_ = true;
    continuous_monitoring_enabled_ = false;
    
    // Wait for monitoring thread if running
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }

    CodebaseAudit_Shutdown();
    
    std::cout << "[AuditSystem] Audit system shutdown complete" << std::endl;
    initialized_ = false;
}

ProjectAuditResult CodebaseAuditSystem::audit_full_project(const std::string& project_path,
                                                          const std::string& project_name) {
    if (!initialized_.load()) {
        std::cerr << "[AuditSystem] System not initialized" << std::endl;
        ProjectAuditResult result;
        result.project_path = project_path;
        result.project_name = project_name.empty() ? "unknown" : project_name;
        return result;
    }
    
    std::cout << "[AuditSystem] Starting full project audit: " << project_path << std::endl;
    
    auto start_time = std::chrono::system_clock::now();
    ProjectAuditResult result;
    
    result.project_name = project_name.empty() ? fs::path(project_path).filename().string() : project_name;
    result.project_path = project_path;
    result.audit_start_time = start_time;
    
    try {
        // Collect source files
        std::vector<std::string> source_files = collect_source_files(project_path, true);
        std::cout << "[AuditSystem] Found " << source_files.size() << " source files to analyze" << std::endl;
        
        // Analyze each file
        for (const auto& file_path : source_files) {
            if (shutting_down_.load()) break;
            
            FileAnalysisResult file_result = analyze_source_file(file_path);
            result.file_results.push_back(file_result);
            
            // Categorize result
            std::string file_ext = fs::path(file_path).extension().string();
            result.results_by_category[file_ext].push_back(file_result);
            
            // Track statistics
            result.total_files_analyzed++;
            if (file_result.production_ready) {
                result.files_passed++;
            } else {
                result.files_failed++;
            }
            
            result.critical_issues += file_result.issues_found.size();
        }
        
        // Calculate aggregate metrics
        if (!result.file_results.empty()) {
            // Aggregate quality metrics
            double total_comment_ratio = 0.0;
            uint32_t total_loc = 0;
            
            for (const auto& file_result : result.file_results) {
                result.aggregate_quality.lines_of_code += file_result.quality_metrics.lines_of_code;
                result.aggregate_quality.comment_lines += file_result.quality_metrics.comment_lines;
                result.aggregate_quality.cyclomatic_complexity += file_result.quality_metrics.cyclomatic_complexity;
                result.aggregate_quality.function_count += file_result.quality_metrics.function_count;
                result.aggregate_quality.class_count += file_result.quality_metrics.class_count;
                
                // Aggregate security issues
                result.aggregate_security.buffer_overflow_risks += file_result.security_analysis.buffer_overflow_risks;
                result.aggregate_security.unchecked_inputs += file_result.security_analysis.unchecked_inputs;
                result.aggregate_security.hardcoded_secrets += file_result.security_analysis.hardcoded_secrets;
                result.aggregate_security.memory_leaks_potential += file_result.security_analysis.memory_leaks_potential;
                result.aggregate_security.race_condition_risks += file_result.security_analysis.race_condition_risks;
                
                // Aggregate performance issues
                result.aggregate_performance.potential_bottlenecks += file_result.performance_analysis.potential_bottlenecks;
                result.aggregate_performance.inefficient_algorithms += file_result.performance_analysis.inefficient_algorithms;
                result.aggregate_performance.memory_intensive_operations += file_result.performance_analysis.memory_intensive_operations;
            }
            
            // Calculate comment ratio
            if (result.aggregate_quality.lines_of_code > 0) {
                result.aggregate_quality.comment_ratio = 
                    static_cast<double>(result.aggregate_quality.comment_lines) / 
                    result.aggregate_quality.lines_of_code;
            }
            
            // Average complexity per file
            if (result.total_files_analyzed > 0) {
                result.aggregate_quality.complexity_per_function = 
                    static_cast<double>(result.aggregate_quality.cyclomatic_complexity) / 
                    std::max(1u, result.aggregate_quality.function_count);
            }
        }
        
        // Assess overall production readiness
        result.production_readiness = assess_production_readiness(project_path);
        
        // Calculate overall project score
        double quality_score = std::min(1.0, (1.0 - result.aggregate_quality.comment_ratio) * 0.3 + 0.7);
        double security_score = 1.0 - (static_cast<double>(result.critical_issues) / 
            std::max(1.0, static_cast<double>(result.total_files_analyzed) * 10));
        double performance_score = 1.0 - (static_cast<double>(result.aggregate_performance.potential_bottlenecks) / 
            std::max(1.0, static_cast<double>(result.total_files_analyzed)));
        
        result.project_overall_score = (quality_score * 0.3 + 
                                       security_score * 0.4 + 
                                       performance_score * 0.3);
        
        result.project_production_ready = result.project_overall_score >= 0.75 && 
                                         result.files_failed == 0;
        
        result.audit_completion_time = std::chrono::system_clock::now();
        result.total_audit_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            result.audit_completion_time - result.audit_start_time);
        
        // Update statistics
        update_statistics(result);
        
        std::cout << "[AuditSystem] Full project audit complete" << std::endl;
        std::cout << "  - Total score: " << result.project_overall_score << std::endl;
        std::cout << "  - Files passed: " << result.files_passed << "/" << result.total_files_analyzed << std::endl;
        std::cout << "  - Critical issues: " << result.critical_issues << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "[AuditSystem] Project audit failed: " << e.what() << std::endl;
        result.project_overall_score = 0.0;
        result.project_production_ready = false;
    }
    
    return result;
}

FileAnalysisResult CodebaseAuditSystem::analyze_source_file(const std::string& file_path) {
    FileAnalysisResult result;
    result.file_path = file_path;
    result.analysis_time = std::chrono::system_clock::now();
    
    try {
        // Read file content
        std::string content = read_file_content(file_path);
        if (content.empty()) {
            result.analysis_successful = false;
            return result;
        }
        
        // Determine file type
        std::string ext = fs::path(file_path).extension().string();
        result.file_type = ext;
        
        // Get file size
        result.file_size_bytes = fs::file_size(file_path);
        
        // Perform analysis based on file type
        if (ext == ".cpp" || ext == ".cc" || ext == ".c" || ext == ".h" || ext == ".hpp") {
            // C++ file analysis
            result.quality_metrics = analyze_cpp_quality(content);
            result.security_analysis = analyze_cpp_security(content, file_path);
            result.performance_analysis = analyze_cpp_performance(content);
            result.architecture_compliance = analyze_cpp_architecture(content);
        }
        
        // Aggregate issues
        result.issues_found.insert(result.issues_found.end(),
            result.security_analysis.security_issues.begin(),
            result.security_analysis.security_issues.end());
        result.issues_found.insert(result.issues_found.end(),
            result.performance_analysis.performance_warnings.begin(),
            result.performance_analysis.performance_warnings.end());
        result.issues_found.insert(result.issues_found.end(),
            result.architecture_compliance.architecture_issues.begin(),
            result.architecture_compliance.architecture_issues.end());
        
        // Aggregate recommendations
        result.recommendations.insert(result.recommendations.end(),
            result.security_analysis.recommendations.begin(),
            result.security_analysis.recommendations.end());
        result.recommendations.insert(result.recommendations.end(),
            result.performance_analysis.optimization_opportunities.begin(),
            result.performance_analysis.optimization_opportunities.end());
        result.recommendations.insert(result.recommendations.end(),
            result.architecture_compliance.design_improvements.begin(),
            result.architecture_compliance.design_improvements.end());
        
        // Calculate overall file score
        result.overall_score = calculate_overall_score(
            result.quality_metrics,
            result.security_analysis,
            result.performance_analysis,
            result.architecture_compliance);
        
        // Determine production readiness
        result.production_ready = result.overall_score >= 0.75 && 
                                 result.security_analysis.passes_security_threshold &&
                                 result.performance_analysis.meets_performance_requirements;
        
        result.analysis_successful = true;
        
    } catch (const std::exception& e) {
        std::cerr << "[AuditSystem] File analysis failed for " << file_path << ": " << e.what() << std::endl;
        result.analysis_successful = false;
    }
    
    return result;
}

CodeQualityMetrics CodebaseAuditSystem::analyze_cpp_quality(const std::string& content) {
    CodeQualityMetrics metrics;
    
    // Count lines
    size_t loc = 0, comment_lines = 0, blank_lines = 0;
    std::istringstream iss(content);
    std::string line;
    bool in_block_comment = false;
    
    while (std::getline(iss, line)) {
        if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos) {
            blank_lines++;
        } else if (line.find("//") != std::string::npos || 
                  line.find("/*") != std::string::npos ||
                  in_block_comment) {
            comment_lines++;
            if (line.find("/*") != std::string::npos) in_block_comment = true;
            if (line.find("*/") != std::string::npos) in_block_comment = false;
        }
        loc++;
    }
    
    metrics.lines_of_code = loc;
    metrics.comment_lines = comment_lines;
    metrics.blank_lines = blank_lines;
    metrics.comment_ratio = loc > 0 ? static_cast<double>(comment_lines) / loc : 0.0;
    
    // Count function-like declarations and type declarations.
    const size_t total_parens = count_substring(content, "(");
    const size_t control_parens =
        count_substring(content, "if(") +
        count_substring(content, "for(") +
        count_substring(content, "while(") +
        count_substring(content, "switch(");
    metrics.function_count = static_cast<uint32_t>(
        total_parens > control_parens ? (total_parens - control_parens) : 0);
    metrics.class_count = static_cast<uint32_t>(
        count_keyword(content, "class") + count_keyword(content, "struct"));

    // Estimate cyclomatic complexity via decision points.
    metrics.cyclomatic_complexity = static_cast<uint32_t>(
        count_keyword(content, "if") +
        count_keyword(content, "else") +
        count_keyword(content, "for") +
        count_keyword(content, "while") +
        count_keyword(content, "case") +
        count_keyword(content, "catch") +
        count_substring(content, "&&") +
        count_substring(content, "||") +
        count_substring(content, "?"));
    
    // Check modern C++ features
    metrics.modern_cpp_features = 
        (content.find("std::unique_ptr") != std::string::npos ? 1 : 0) +
        (content.find("std::shared_ptr") != std::string::npos ? 1 : 0) +
        (content.find("std::optional") != std::string::npos ? 1 : 0) +
        (content.find("std::variant") != std::string::npos ? 1 : 0) +
        (content.find("std::string_view") != std::string::npos ? 1 : 0);
    
    // Check for error handling
    metrics.has_error_handling = content.find("try") != std::string::npos && 
                               content.find("catch") != std::string::npos;
    metrics.has_logging = content.find("cout") != std::string::npos || 
                         content.find("log") != std::string::npos;
    metrics.has_documentation = content.find("/**") != std::string::npos || 
                               content.find("//!") != std::string::npos;
    
    metrics.follows_naming_conventions = true; // Simplified check
    
    return metrics;
}

SecurityAnalysis CodebaseAuditSystem::analyze_cpp_security(const std::string& content, 
                                                         const std::string& file_path) {
    SecurityAnalysis analysis;
    
    // Check for dangerous functions
    if (content.find("strcpy(") != std::string::npos) {
        analysis.buffer_overflow_risks++;
        analysis.security_issues.push_back("Dangerous function: strcpy detected");
    }
    if (content.find("strcat(") != std::string::npos) {
        analysis.buffer_overflow_risks++;
        analysis.security_issues.push_back("Dangerous function: strcat detected");
    }
    if (content.find("sprintf(") != std::string::npos) {
        analysis.buffer_overflow_risks++;
        analysis.security_issues.push_back("Dangerous function: sprintf detected");
    }
    
    // Check for hardcoded secrets
    if (content.find("password") != std::string::npos && 
        content.find("\"") != std::string::npos) {
        analysis.hardcoded_secrets++;
        analysis.security_issues.push_back("Possible hardcoded password found");
    }
    
    // Check for unsafe casts
    if (content.find("reinterpret_cast") != std::string::npos) {
        analysis.unsafe_casts++;
        analysis.security_issues.push_back("Unsafe reinterpret_cast usage detected");
    }
    
    // Check for memory leaks
    if (content.find("new ") != std::string::npos && 
        content.find("delete") == std::string::npos) {
        analysis.memory_leaks_potential++;
        analysis.security_issues.push_back("Potential memory leak: 'new' without 'delete'");
    }
    
    // Calculate security score
    analysis.overall_security_score = 1.0 - 
        (analysis.buffer_overflow_risks * 0.2 + 
         analysis.unsafe_casts * 0.15 + 
         analysis.hardcoded_secrets * 0.3 + 
         analysis.memory_leaks_potential * 0.2) / 10.0;
    
    analysis.overall_security_score = std::max(0.0, analysis.overall_security_score);
    analysis.passes_security_threshold = analysis.overall_security_score >= 0.7;
    
    // Add recommendations
    if (!analysis.security_issues.empty()) {
        analysis.recommendations.push_back("Review and fix identified security issues");
        analysis.recommendations.push_back("Use secure string functions (strcpy_s, strcat_s)");
        analysis.recommendations.push_back("Enable compiler security warnings (/W4)");
    }
    
    return analysis;
}

PerformanceAnalysis CodebaseAuditSystem::analyze_cpp_performance(const std::string& content) {
    PerformanceAnalysis analysis;
    
    // Check for performance issues
    size_t for_count = 0;
    size_t find_pos = content.find("for(");
    while (find_pos != std::string::npos) {
        for_count++;
        find_pos = content.find("for(", find_pos + 1);
    }
    
    // Nested loops indicate potential O(n²) complexity
    if (for_count > 2) {
        analysis.potential_bottlenecks++;
        analysis.performance_warnings.push_back("Potential nested loops detected");
    }
    
    // Check for inefficient string operations
    if (content.find("std::endl") != std::string::npos) {
        analysis.inefficient_algorithms++;
        analysis.optimization_opportunities.push_back("Replace std::endl with '\\n' for better performance");
    }
    
    // Check for unnecessary allocations
    if (content.find("std::vector") != std::string::npos &&
        content.find("reserve(") == std::string::npos) {
        analysis.unnecessary_allocations++;
        analysis.optimization_opportunities.push_back("Use reserve() for vectors when size is known");
    }
    
    // Calculate performance score
    analysis.performance_score = 1.0 - 
        (analysis.potential_bottlenecks * 0.3 + 
         analysis.inefficient_algorithms * 0.2 + 
         analysis.unnecessary_allocations * 0.1) / 10.0;
    
    analysis.performance_score = std::max(0.0, analysis.performance_score);
    analysis.meets_performance_requirements = analysis.performance_score >= 0.7;
    
    return analysis;
}

ArchitectureCompliance CodebaseAuditSystem::analyze_cpp_architecture(const std::string& content) {
    ArchitectureCompliance compliance;
    
    // Check for design pattern indicators
    if (content.find("class ") != std::string::npos) {
        compliance.has_proper_abstraction_layers = true;
    }
    
    // Check for SOLID principles indicators
    if (content.find("interface") != std::string::npos ||
        content.find("virtual ") != std::string::npos) {
        compliance.adheres_to_solid_principles = true;
    }
    
    // Check for dependency injection patterns
    if (content.find("constructor(") != std::string::npos &&
        content.find("m_") != std::string::npos) {
        compliance.uses_dependency_injection = true;
    }
    
    // Calculate architecture score
    compliance.architecture_score = 0.5;
    if (compliance.has_proper_abstraction_layers) compliance.architecture_score += 0.15;
    if (compliance.adheres_to_solid_principles) compliance.architecture_score += 0.15;
    if (compliance.uses_dependency_injection) compliance.architecture_score += 0.1;
    if (compliance.proper_separation_of_concerns) compliance.architecture_score += 0.1;
    
    return compliance;
}

ProductionReadinessAssessment CodebaseAuditSystem::assess_production_readiness(const std::string& project_path) {
    ProductionReadinessAssessment assessment;
    
    try {
        // Check for required files and configurations
        bool has_cmake = fs::exists(fs::path(project_path) / "CMakeLists.txt");
        bool has_readme = fs::exists(fs::path(project_path) / "README.md");
        bool has_tests = fs::exists(fs::path(project_path) / "tests") ||
                        fs::exists(fs::path(project_path) / "test");
        bool has_docs = fs::exists(fs::path(project_path) / "docs") ||
                       fs::exists(fs::path(project_path) / "documentation");
        
        assessment.has_configuration_management = has_cmake;
        assessment.has_api_documentation = has_readme || has_docs;
        assessment.has_unit_tests = has_tests;
        
        // Estimate readiness score
        double readiness_score = 0.0;
        if (assessment.has_comprehensive_error_handling) readiness_score += 0.15;
        if (assessment.has_proper_logging_system) readiness_score += 0.10;
        if (assessment.has_configuration_management) readiness_score += 0.10;
        if (assessment.has_unit_tests) readiness_score += 0.15;
        if (assessment.has_api_documentation) readiness_score += 0.10;
        if (assessment.has_deployment_guide) readiness_score += 0.10;
        if (assessment.has_graceful_shutdown) readiness_score += 0.15;
        if (assessment.has_monitoring_capabilities) readiness_score += 0.05;
        
        assessment.overall_readiness_score = std::max(0.0, std::min(1.0, readiness_score));
        
        // Add recommendations
        if (!assessment.has_unit_tests) {
            assessment.recommended_improvements.push_back("Add comprehensive unit tests");
        }
        if (!assessment.has_api_documentation) {
            assessment.recommended_improvements.push_back("Add API documentation");
        }
        if (!assessment.has_monitoring_capabilities) {
            assessment.recommended_improvements.push_back("Implement monitoring and metrics collection");
        }
        if (!assessment.has_graceful_shutdown) {
            assessment.recommended_improvements.push_back("Implement graceful shutdown mechanisms");
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[AuditSystem] Production readiness assessment error: " << e.what() << std::endl;
    }
    
    return assessment;
}

std::vector<std::string> CodebaseAuditSystem::collect_source_files(const std::string& root_path, bool recursive) {
    std::vector<std::string> files;
    
    try {
        if (!fs::exists(root_path)) {
            std::cerr << "[AuditSystem] Path does not exist: " << root_path << std::endl;
            return files;
        }
        
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(root_path)) {
                if (entry.is_regular_file() && is_source_file(entry.path().string())) {
                    if (should_analyze_file(entry.path().string())) {
                        files.push_back(entry.path().string());
                    }
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(root_path)) {
                if (entry.is_regular_file() && is_source_file(entry.path().string())) {
                    if (should_analyze_file(entry.path().string())) {
                        files.push_back(entry.path().string());
                    }
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "[AuditSystem] Error collecting source files: " << e.what() << std::endl;
    }
    
    return files;
}

bool CodebaseAuditSystem::is_source_file(const std::string& file_path) {
    std::string ext = fs::path(file_path).extension().string();
    return ext == ".cpp" || ext == ".cc" || ext == ".c" || 
           ext == ".hpp" || ext == ".h" || ext == ".hh";
}

bool CodebaseAuditSystem::should_analyze_file(const std::string& file_path) {
    return is_source_file(file_path);
}

std::string CodebaseAuditSystem::read_file_content(const std::string& file_path) {
    try {
        if (!fs::exists(file_path)) {
            return "";
        }

        const auto size = fs::file_size(file_path);
        if (size == 0) {
            return "";
        }

        std::string content;
        content.resize(static_cast<size_t>(size));

        const auto readBytes = CodebaseAudit_ReadFileContent(
            file_path.c_str(),
            content.data(),
            content.size());

        if (readBytes > 0) {
            content.resize(static_cast<size_t>(readBytes));
            return content;
        }

        std::ifstream file(file_path, std::ios::binary);
        if (!file) {
            return "";
        }
        return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    } catch (const std::exception& e) {
        std::cerr << "[AuditSystem] Error reading file: " << e.what() << std::endl;
        return "";
    }
}

double CodebaseAuditSystem::calculate_overall_score(const CodeQualityMetrics& quality,
                                                   const SecurityAnalysis& security,
                                                   const PerformanceAnalysis& performance,
                                                   const ArchitectureCompliance& architecture) {
    return (quality.comment_ratio * 0.2 + 
           security.overall_security_score * 0.3 + 
           performance.performance_score * 0.25 + 
           architecture.architecture_score * 0.25);
}

void CodebaseAuditSystem::update_statistics(const ProjectAuditResult& result) {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    
    audit_statistics_.total_projects_audited++;
    audit_statistics_.total_files_analyzed += result.total_files_analyzed;
    audit_statistics_.total_issues_found += result.critical_issues;
    audit_statistics_.total_analysis_time += result.total_audit_time;
    audit_statistics_.last_audit_time = result.audit_completion_time;
    
    audit_history_.push_back(result);
}

AuditStatistics CodebaseAuditSystem::get_audit_statistics() const {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    return audit_statistics_;
}

} // namespace RawrXD::Audit