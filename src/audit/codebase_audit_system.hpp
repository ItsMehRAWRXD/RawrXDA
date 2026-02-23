#pragma once
// SCAFFOLD_186: Audit detect stubs and report
// SCAFFOLD_206: Audit log immutable checksum
// SCAFFOLD_325: Stub vs production wording sweep
// SCAFFOLD_352: SOURCE_CODE_AUDIT Phase 2
// Pure x64 MASM implementation: src/asm/RawrXD_CodebaseAuditSystem.asm

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace RawrXD::Audit {

// =====================================================================
// AUDIT ANALYSIS STRUCTURES  
// =====================================================================

/**
 * Code quality metrics for individual files and components
 */
struct CodeQualityMetrics {
    uint32_t lines_of_code = 0;
    uint32_t comment_lines = 0;
    uint32_t blank_lines = 0;
    uint32_t cyclomatic_complexity = 0;
    uint32_t function_count = 0;
    uint32_t class_count = 0;
    uint32_t namespace_count = 0;
    
    // Code quality indicators
    double comment_ratio = 0.0;
    double complexity_per_function = 0.0;
    uint32_t max_function_length = 0;
    uint32_t avg_function_length = 0;
    
    // C++ specific metrics
    uint32_t template_usage_count = 0;
    uint32_t modern_cpp_features = 0;
    uint32_t exception_usage_count = 0;
    uint32_t smart_pointer_usage = 0;
    uint32_t raw_pointer_usage = 0;
    
    // Production readiness indicators
    bool has_error_handling = false;
    bool has_logging = false;
    bool has_unit_tests = false;
    bool follows_naming_conventions = false;
    bool has_documentation = false;
};

/**
 * Security analysis results
 */
struct SecurityAnalysis {
    uint32_t buffer_overflow_risks = 0;
    uint32_t unchecked_inputs = 0;
    uint32_t hardcoded_secrets = 0; 
    uint32_t unsafe_casts = 0;
    uint32_t memory_leaks_potential = 0;
    uint32_t race_condition_risks = 0;
    uint32_t injection_vulnerabilities = 0;
    
    std::vector<std::string> security_issues;
    std::vector<std::string> recommendations;
    
    double overall_security_score = 0.0;
    bool passes_security_threshold = false;
};

/**
 * Performance analysis metrics
 */
struct PerformanceAnalysis {
    uint32_t potential_bottlenecks = 0;
    uint32_t inefficient_algorithms = 0;
    uint32_t memory_intensive_operations = 0;
    uint32_t blocking_operations = 0;
    uint32_t unnecessary_allocations = 0;
    
    std::vector<std::string> optimization_opportunities;
    std::vector<std::string> performance_warnings;
    
    double performance_score = 0.0;
    bool meets_performance_requirements = false;
};

/**
 * Architecture compliance assessment
 */
struct ArchitectureCompliance {
    bool follows_design_patterns = false;
    bool proper_separation_of_concerns = false;
    bool adheres_to_solid_principles = false;
    bool uses_dependency_injection = false;
    bool has_proper_abstraction_layers = false;
    
    uint32_t circular_dependencies = 0;
    uint32_t tight_coupling_issues = 0;
    uint32_t interface_violations = 0;
    
    std::vector<std::string> architecture_issues;
    std::vector<std::string> design_improvements;
    
    double architecture_score = 0.0;
};

/**
 * Production readiness assessment
 */
struct ProductionReadinessAssessment {
    // Core readiness indicators
    bool has_comprehensive_error_handling = false;
    bool has_proper_logging_system = false;
    bool has_configuration_management = false;
    bool has_monitoring_capabilities = false;
    bool has_health_checks = false;
    bool has_graceful_shutdown = false;
    
    // Testing and quality assurance
    bool has_unit_tests = false;
    bool has_integration_tests = false;
    bool has_performance_tests = false;
    bool has_security_tests = false;
    double test_coverage_percentage = 0.0;
    
    // Documentation and maintainability
    bool has_api_documentation = false;
    bool has_deployment_guide = false;
    bool has_troubleshooting_guide = false;
    bool has_code_comments = false;
    
    // Operational readiness
    bool supports_container_deployment = false;
    bool has_resource_limits = false;
    bool has_backup_recovery = false;
    bool has_rollback_capability = false;
    
    double overall_readiness_score = 0.0;
    std::vector<std::string> critical_blockers;
    std::vector<std::string> recommended_improvements;
};

/**
 * Comprehensive file analysis result
 */
struct FileAnalysisResult {
    std::string file_path;
    std::chrono::system_clock::time_point analysis_time;
    bool analysis_successful = false;
    std::string file_type;
    uint64_t file_size_bytes = 0;
    
    CodeQualityMetrics quality_metrics;
    SecurityAnalysis security_analysis;
    PerformanceAnalysis performance_analysis;
    ArchitectureCompliance architecture_compliance;
    
    std::vector<std::string> issues_found;
    std::vector<std::string> recommendations;
    
    double overall_score = 0.0;
    bool production_ready = false;
};

/**
 * Project-wide audit results
 */
struct ProjectAuditResult {
    std::string project_name;
    std::string project_path;
    std::chrono::system_clock::time_point audit_start_time;
    std::chrono::system_clock::time_point audit_completion_time;
    std::chrono::milliseconds total_audit_time{0};
    
    // File analysis results
    std::vector<FileAnalysisResult> file_results;
    std::map<std::string, std::vector<FileAnalysisResult>> results_by_category;
    
    // Aggregate metrics
    CodeQualityMetrics aggregate_quality;
    SecurityAnalysis aggregate_security;
    PerformanceAnalysis aggregate_performance;
    ArchitectureCompliance aggregate_architecture;
    ProductionReadinessAssessment production_readiness;
    
    // Summary statistics
    uint32_t total_files_analyzed = 0;
    uint32_t files_passed = 0;
    uint32_t files_failed = 0;
    uint32_t critical_issues = 0;
    uint32_t major_issues = 0;
    uint32_t minor_issues = 0;
    
    double project_overall_score = 0.0;
    bool project_production_ready = false;
    
    std::vector<std::string> top_priority_fixes;
    std::vector<std::string> enhancement_opportunities;
};

/**
 * Audit configuration parameters 
 */
struct AuditConfiguration {
    // Analysis scope
    bool analyze_cpp_files = true;
    bool analyze_header_files = true;
    bool analyze_cmake_files = true;
    bool analyze_documentation = true;
    bool analyze_tests = true;
    
    // Analysis depth
    bool perform_deep_static_analysis = true;
    bool check_coding_standards = true;
    bool analyze_dependencies = true;
    bool check_security_issues = true;
    bool analyze_performance = true;
    
    // Thresholds and criteria
    uint32_t max_function_complexity = 15;
    uint32_t max_function_length = 100;
    double min_comment_ratio = 0.15;
    double min_test_coverage = 0.8;
    uint32_t max_file_size_kb = 1000;
    
    // Security requirements
    bool enforce_memory_safety = true;
    bool require_input_validation = true;
    bool check_crypto_usage = true;
    bool analyze_privilege_escalation = true;
    
    // Performance requirements
    double max_acceptable_complexity = 1000.0;
    bool check_algorithm_efficiency = true;
    bool analyze_memory_usage = true;
    bool check_thread_safety = true;
    
    // Production readiness requirements
    bool require_error_handling = true;
    bool require_logging = true;
    bool require_monitoring = true;
    bool require_documentation = true;
    bool require_tests = true;
};

// =====================================================================
// COMPREHENSIVE CODEBASE AUDIT SYSTEM
// =====================================================================

/**
 * Advanced codebase audit system for RawrXD production readiness
 * 
 * Provides comprehensive analysis of:
 * - Code quality and maintainability
 * - Security vulnerabilities and risks  
 * - Performance bottlenecks and optimizations
 * - Architecture compliance and design patterns
 * - Production readiness and operational concerns
 * - Testing coverage and quality assurance
 * 
 * Features:
 * - Multi-threaded analysis for large codebases
 * - Configurable analysis depth and criteria
 * - Integration with existing development tools
 * - Automated report generation and recommendations
 * - Continuous monitoring and trend analysis
 * - Priority-based issue classification
 */
class CodebaseAuditSystem {
public:
    // ================================================================
    // CONSTRUCTION AND LIFECYCLE
    // ================================================================
    
    CodebaseAuditSystem();
    ~CodebaseAuditSystem();
    
    // No copy/move for audit system integrity
    CodebaseAuditSystem(const CodebaseAuditSystem&) = delete;
    CodebaseAuditSystem& operator=(const CodebaseAuditSystem&) = delete;
    CodebaseAuditSystem(CodebaseAuditSystem&&) = delete;
    CodebaseAuditSystem& operator=(CodebaseAuditSystem&&) = delete;
    
    /**
     * Initialize audit system with configuration
     * @param config Audit configuration parameters
     * @param enable_continuous_monitoring Enable background monitoring
     * @return Success status
     */
    bool initialize(const AuditConfiguration& config = AuditConfiguration{},
                   bool enable_continuous_monitoring = false);
    
    /**
     * Shutdown audit system and save final reports
     */
    void shutdown();
    
    /**
     * Check if audit system is initialized
     */
    bool is_initialized() const { return initialized_.load(); }
    
    // ================================================================
    // COMPREHENSIVE AUDIT OPERATIONS
    // ================================================================
    
    /**
     * Perform complete project audit
     * @param project_path Root path of project to audit
     * @param project_name Optional project name for reports
     * @return Comprehensive audit results
     */
    ProjectAuditResult audit_full_project(const std::string& project_path,
                                        const std::string& project_name = "");
    
    /**
     * Audit specific file or directory
     * @param file_path Path to file or directory
     * @param recursive Whether to recursively analyze subdirectories
     * @return File analysis results
     */
    std::vector<FileAnalysisResult> audit_file_or_directory(const std::string& file_path,
                                                           bool recursive = true);
    
    /**
     * Analyze single source file in detail
     * @param file_path Path to source file
     * @return Detailed file analysis
     */
    FileAnalysisResult analyze_source_file(const std::string& file_path);
    
    /**
     * Quick production readiness check
     * @param project_path Project root path
     * @return Production readiness assessment
     */
    ProductionReadinessAssessment quick_production_check(const std::string& project_path);
    
    /**
     * Security-focused audit
     * @param project_path Project to analyze for security
     * @return Security analysis results
     */
    SecurityAnalysis security_audit(const std::string& project_path);
    
    /**
     * Performance analysis audit
     * @param project_path Project to analyze for performance
     * @return Performance analysis results
     */
    PerformanceAnalysis performance_audit(const std::string& project_path);
    
    // ================================================================
    // SPECIFIC ANALYSIS TYPES
    // ================================================================
    
    /**
     * Analyze code quality metrics
     * @param file_content Source file content
     * @param file_extension File extension for language detection
     * @return Code quality metrics
     */
    CodeQualityMetrics analyze_code_quality(const std::string& file_content,
                                           const std::string& file_extension);
    
    /**
     * Perform security vulnerability analysis
     * @param file_content Source file content
     * @param file_path File path for context
     * @return Security analysis results
     */
    SecurityAnalysis analyze_security_vulnerabilities(const std::string& file_content,
                                                    const std::string& file_path);
    
    /**
     * Analyze performance characteristics
     * @param file_content Source file content
     * @param file_path File path for context
     * @return Performance analysis
     */
    PerformanceAnalysis analyze_performance_characteristics(const std::string& file_content,
                                                          const std::string& file_path);
    
    /**
     * Assess architecture compliance
     * @param file_content Source file content
     * @param project_context Project-wide context information
     * @return Architecture compliance assessment
     */
    ArchitectureCompliance assess_architecture_compliance(const std::string& file_content,
                                                        const std::map<std::string, std::string>& project_context);
    
    /**
     * Check production readiness indicators
     * @param project_path Project root path
     * @return Production readiness assessment
     */
    ProductionReadinessAssessment assess_production_readiness(const std::string& project_path);
    
    // ================================================================
    // REPORTING AND RECOMMENDATIONS
    // ================================================================
    
    /**
     * Generate comprehensive audit report
     * @param audit_result Project audit results
     * @param output_format Report format ("html", "json", "markdown", "text")
     * @return Generated report content
     */
    std::string generate_audit_report(const ProjectAuditResult& audit_result,
                                    const std::string& output_format = "markdown");
    
    /**
     * Generate prioritized improvement recommendations
     * @param audit_result Project audit results
     * @param max_recommendations Maximum number of recommendations
     * @return Prioritized list of improvements
     */
    std::vector<std::string> generate_improvement_recommendations(const ProjectAuditResult& audit_result,
                                                                uint32_t max_recommendations = 20);
    
    /**
     * Create action plan for production readiness
     * @param readiness_assessment Production readiness results
     * @return Structured action plan
     */
    std::map<std::string, std::vector<std::string>> create_production_action_plan(
        const ProductionReadinessAssessment& readiness_assessment);
    
    /**
     * Generate security remediation plan
     * @param security_analysis Security analysis results
     * @return Security remediation actions
     */
    std::vector<std::string> generate_security_remediation_plan(const SecurityAnalysis& security_analysis);
    
    /**
     * Save audit results to file
     * @param audit_result Results to save
     * @param output_path Output file path
     * @param format Output format
     * @return Success status
     */
    bool save_audit_results(const ProjectAuditResult& audit_result,
                           const std::string& output_path,
                           const std::string& format = "json");
    
    // ================================================================
    // CONTINUOUS MONITORING AND TRENDS
    // ================================================================
    
    /**
     * Enable continuous project monitoring
     * @param project_path Project to monitor
     * @param monitoring_interval Check interval
     * @return Success status
     */
    bool enable_continuous_monitoring(const std::string& project_path,
                                    std::chrono::minutes monitoring_interval = std::chrono::minutes{30});
    
    /**
     * Disable continuous monitoring
     * @param project_path Project to stop monitoring (empty for all)
     */
    void disable_continuous_monitoring(const std::string& project_path = "");
    
    /**
     * Get audit trend analysis
     * @param project_path Project path
     * @param time_range Analysis time range
     * @return Trend analysis results
     */
    struct AuditTrend {
        std::vector<double> quality_scores;
        std::vector<double> security_scores; 
        std::vector<double> performance_scores;
        std::vector<std::chrono::system_clock::time_point> timestamps;
        std::map<std::string, std::vector<uint32_t>> issue_counts_over_time;
        std::string trend_summary;
    };
    
    AuditTrend get_audit_trends(const std::string& project_path,
                               std::chrono::hours time_range = std::chrono::hours{24});
    
    // ================================================================
    // CONFIGURATION AND CUSTOMIZATION
    // ================================================================
    
    /**
     * Update audit configuration
     * @param new_config Updated configuration
     */
    void update_configuration(const AuditConfiguration& new_config);
    
    /**
     * Add custom analysis rule
     * @param rule_name Name of custom rule
     * @param rule_function Analysis function
     * @param category Rule category ("quality", "security", "performance", "architecture")
     */
    void add_custom_analysis_rule(const std::string& rule_name,
                                std::function<std::vector<std::string>(const std::string&)> rule_function,
                                const std::string& category);
    
    /**
     * Set custom quality thresholds
     * @param thresholds Map of metric names to threshold values
     */
    void set_quality_thresholds(const std::map<std::string, double>& thresholds);
    
    /**
     * Configure file filters
     * @param include_patterns Patterns for files to include
     * @param exclude_patterns Patterns for files to exclude
     */
    void configure_file_filters(const std::vector<std::string>& include_patterns,
                              const std::vector<std::string>& exclude_patterns);
    
    // ================================================================
    // STATISTICS AND REPORTING
    // ================================================================
    
    /**
     * Get audit system statistics
     */
    struct AuditStatistics {
        uint32_t total_projects_audited = 0;
        uint32_t total_files_analyzed = 0;
        uint32_t total_issues_found = 0;
        std::chrono::milliseconds total_analysis_time{0};
        std::map<std::string, uint32_t> issues_by_category;
        std::map<std::string, uint32_t> files_by_type;
        double average_project_score = 0.0;
        std::chrono::system_clock::time_point last_audit_time;
    };
    
    AuditStatistics get_audit_statistics() const;
    
    /**
     * Export audit history
     * @param output_path Output file path
     * @param format Export format
     * @return Success status
     */
    bool export_audit_history(const std::string& output_path, 
                             const std::string& format = "json");
    
private:
    // ================================================================
    // PRIVATE IMPLEMENTATION MEMBERS
    // ================================================================
    
    // System state
    std::atomic<bool> initialized_{false};
    std::atomic<bool> shutting_down_{false};
    std::chrono::system_clock::time_point initialization_time_;
    
    // Configuration and settings
    AuditConfiguration config_;
    std::map<std::string, double> custom_thresholds_;
    std::vector<std::string> include_patterns_;
    std::vector<std::string> exclude_patterns_;
    
    // Custom analysis rules
    std::map<std::string, std::function<std::vector<std::string>(const std::string&)>> custom_rules_;
    std::map<std::string, std::string> rule_categories_;
    
    // Statistics and history
    mutable std::mutex statistics_mutex_;
    AuditStatistics audit_statistics_;
    std::vector<ProjectAuditResult> audit_history_;
    
    // Continuous monitoring
    std::atomic<bool> continuous_monitoring_enabled_{false};
    std::map<std::string, std::chrono::minutes> monitored_projects_;
    std::map<std::string, std::chrono::system_clock::time_point> last_monitor_run_;
    std::map<std::string, uint64_t> project_signature_;  // file-count + mtime signature for change detection
    std::thread monitoring_thread_;
    mutable std::mutex monitoring_mutex_;
    
    // Thread pool for analysis
    std::vector<std::thread> analysis_threads_;
    std::mutex analysis_mutex_;
    
    // Caching and optimization
    std::map<std::string, FileAnalysisResult> analysis_cache_;
    std::map<std::string, std::chrono::system_clock::time_point> cache_timestamps_;
    mutable std::mutex cache_mutex_;
    
    // ================================================================
    // PRIVATE IMPLEMENTATION METHODS
    // ================================================================
    
    // File system operations
    std::vector<std::string> collect_source_files(const std::string& root_path, bool recursive);
    bool is_source_file(const std::string& file_path);
    bool should_analyze_file(const std::string& file_path);
    std::string read_file_content(const std::string& file_path);
    
    // Analysis implementation  
    CodeQualityMetrics analyze_cpp_quality(const std::string& content);
    SecurityAnalysis analyze_cpp_security(const std::string& content, const std::string& file_path);
    PerformanceAnalysis analyze_cpp_performance(const std::string& content);
    ArchitectureCompliance analyze_cpp_architecture(const std::string& content);
    
    // Specific checks and patterns
    std::vector<std::string> find_security_patterns(const std::string& content);
    std::vector<std::string> find_performance_issues(const std::string& content);
    std::vector<std::string> check_coding_standards(const std::string& content);
    std::vector<std::string> analyze_error_handling(const std::string& content);
    
    // Metrics calculation
    uint32_t calculate_cyclomatic_complexity(const std::string& content);
    uint32_t count_functions(const std::string& content);
    uint32_t count_classes(const std::string& content);
    double calculate_comment_ratio(const std::string& content);
    
    // Report generation helpers
    std::string generate_html_report(const ProjectAuditResult& result);
    std::string generate_json_report(const ProjectAuditResult& result);
    std::string generate_markdown_report(const ProjectAuditResult& result);
    std::string generate_text_report(const ProjectAuditResult& result);
    
    // Continuous monitoring implementation
    void monitoring_thread_function();
    void monitor_project(const std::string& project_path);
    bool has_project_changed(const std::string& project_path);
    uint64_t compute_project_signature(const std::string& project_path) const;
    
    // Caching and optimization
    bool is_cache_valid(const std::string& file_path);
    void update_cache(const std::string& file_path, const FileAnalysisResult& result);
    void cleanup_cache();
    
    // Statistics updates
    void update_statistics(const ProjectAuditResult& result);
    void record_analysis_metrics(const FileAnalysisResult& result);
    
    // Utility functions
    double calculate_overall_score(const CodeQualityMetrics& quality,
                                 const SecurityAnalysis& security,
                                 const PerformanceAnalysis& performance,
                                 const ArchitectureCompliance& architecture);
    
    std::string format_timestamp(const std::chrono::system_clock::time_point& time_point);
    std::string sanitize_file_path(const std::string& path);
    
    // Pattern matching utilities
    std::vector<std::smatch> find_regex_matches(const std::string& content, 
                                               const std::regex& pattern);
    bool contains_pattern(const std::string& content, const std::regex& pattern);
    uint32_t count_pattern_occurrences(const std::string& content, const std::regex& pattern);
};

} // namespace RawrXD::Audit