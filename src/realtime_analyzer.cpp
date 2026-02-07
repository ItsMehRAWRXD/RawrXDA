// Real-Time Code Analysis Engine
// Provides instant feedback on code quality, security, performance

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <iostream>
#include <algorithm>

// Real-time analyzer with multi-dimensional analysis
class RealTimeAnalyzer {
public:
    enum class IssueSeverity {
        Info,
        Warning,
        Error,
        Critical
    };
    
    enum class IssueCategory {
        Syntax,
        Security,
        Performance,
        Style,
        Memory,
        Logic,
        BestPractice
    };
    
    struct CodeIssue {
        int line_number;
        int column;
        std::string message;
        IssueSeverity severity;
        IssueCategory category;
        std::string suggestion;
        std::string code_snippet;
    };
    
    struct AnalysisResult {
        std::vector<CodeIssue> issues;
        float quality_score;  // 0.0 to 100.0
        std::map<IssueCategory, int> category_counts;
        std::string summary;
    };
    
    // Analyze code in real-time
    AnalysisResult Analyze(const std::string& code, const std::string& language = "cpp") {
        AnalysisResult result;
        result.quality_score = 100.0f;
        
        // Run all analyzers
        DetectSyntaxIssues(code, result);
        DetectSecurityVulnerabilities(code, result);
        DetectPerformanceIssues(code, result);
        DetectMemoryIssues(code, result);
        DetectStyleViolations(code, result);
        DetectLogicErrors(code, result);
        
        // Calculate quality score
        CalculateQualityScore(result);
        
        // Generate summary
        GenerateSummary(result);
        
        return result;
    }
    
    // Incremental analysis for single line edit
    std::vector<CodeIssue> AnalyzeLine(const std::string& line, int line_number) {
        std::vector<CodeIssue> issues;
        
        // Quick checks for common issues
        if (line.find("TODO") != std::string::npos) {
            CodeIssue issue;
            issue.line_number = line_number;
            issue.message = "TODO comment found";
            issue.severity = IssueSeverity::Info;
            issue.category = IssueCategory::Style;
            issue.suggestion = "Complete or remove TODO";
            issues.push_back(issue);
        }
        
        if (line.find("malloc") != std::string::npos && line.find("free") == std::string::npos) {
            CodeIssue issue;
            issue.line_number = line_number;
            issue.message = "Potential memory leak - malloc without free";
            issue.severity = IssueSeverity::Warning;
            issue.category = IssueCategory::Memory;
            issue.suggestion = "Use RAII or ensure free() is called";
            issues.push_back(issue);
        }
        
        if (line.find("gets(") != std::string::npos) {
            CodeIssue issue;
            issue.line_number = line_number;
            issue.message = "Unsafe function gets() - buffer overflow risk";
            issue.severity = IssueSeverity::Critical;
            issue.category = IssueCategory::Security;
            issue.suggestion = "Use fgets() or std::getline() instead";
            issues.push_back(issue);
        }
        
        return issues;
    }
    
private:
    void DetectSyntaxIssues(const std::string& code, AnalysisResult& result) {
        int line_num = 1;
        int brace_balance = 0;
        int paren_balance = 0;
        
        for (size_t i = 0; i < code.length(); i++) {
            char c = code[i];
            
            if (c == '\n') line_num++;
            if (c == '{') brace_balance++;
            if (c == '}') brace_balance--;
            if (c == '(') paren_balance++;
            if (c == ')') paren_balance--;
            
            // Check for unbalanced braces
            if (brace_balance < 0) {
                AddIssue(result, line_num, 0, "Unbalanced braces - extra '}'",
                        IssueSeverity::Error, IssueCategory::Syntax,
                        "Remove extra closing brace or add opening brace");
                brace_balance = 0;
            }
            
            if (paren_balance < 0) {
                AddIssue(result, line_num, 0, "Unbalanced parentheses - extra ')'",
                        IssueSeverity::Error, IssueCategory::Syntax,
                        "Remove extra closing paren or add opening paren");
                paren_balance = 0;
            }
        }
        
        if (brace_balance > 0) {
            AddIssue(result, line_num, 0, "Unbalanced braces - missing '}'",
                    IssueSeverity::Error, IssueCategory::Syntax,
                    "Add closing brace");
        }
        
        if (paren_balance > 0) {
            AddIssue(result, line_num, 0, "Unbalanced parentheses - missing ')'",
                    IssueSeverity::Error, IssueCategory::Syntax,
                    "Add closing parenthesis");
        }
    }
    
    void DetectSecurityVulnerabilities(const std::string& code, AnalysisResult& result) {
        // Dangerous functions
        std::vector<std::pair<std::string, std::string>> dangerous_funcs = {
            {"gets(", "Use fgets() or std::getline()"},
            {"strcpy(", "Use strncpy() or std::string"},
            {"strcat(", "Use strncat() or std::string::append()"},
            {"sprintf(", "Use snprintf() for bounds checking"},
            {"scanf(", "Use fgets() and sscanf() with bounds checking"},
            {"system(", "Avoid system() calls - use safer alternatives"}
        };
        
        int line_num = 1;
        size_t pos = 0;
        
        for (size_t i = 0; i < code.length(); i++) {
            if (code[i] == '\n') line_num++;
            
            for (const auto& dangerous : dangerous_funcs) {
                if (code.substr(i).find(dangerous.first) == 0) {
                    AddIssue(result, line_num, 0,
                            "Security vulnerability: " + dangerous.first + " is unsafe",
                            IssueSeverity::Critical, IssueCategory::Security,
                            dangerous.second);
                }
            }
        }
        
        // SQL injection patterns
        if (code.find("SELECT") != std::string::npos && 
            code.find("+") != std::string::npos) {
            AddIssue(result, 1, 0, "Potential SQL injection - string concatenation in query",
                    IssueSeverity::Critical, IssueCategory::Security,
                    "Use prepared statements or parameterized queries");
        }
        
        // Hardcoded credentials
        std::regex password_pattern(R"((password|passwd|pwd)\s*=\s*['\"][^'\"]+['\"])");
        std::smatch match;
        std::string code_copy = code;
        if (std::regex_search(code_copy, match, password_pattern)) {
            AddIssue(result, 1, 0, "Hardcoded credentials detected",
                    IssueSeverity::Critical, IssueCategory::Security,
                    "Use environment variables or secure credential storage");
        }
    }
    
    void DetectPerformanceIssues(const std::string& code, AnalysisResult& result) {
        // Inefficient patterns
        if (code.find("string +") != std::string::npos) {
            AddIssue(result, 1, 0, "String concatenation in loop can be slow",
                    IssueSeverity::Warning, IssueCategory::Performance,
                    "Use std::stringstream or reserve() capacity");
        }
        
        if (code.find("pow(") != std::string::npos && code.find(", 2)") != std::string::npos) {
            AddIssue(result, 1, 0, "Using pow(x, 2) is slower than x * x",
                    IssueSeverity::Info, IssueCategory::Performance,
                    "Replace pow(x, 2) with x * x");
        }
        
        // Nested loops with high complexity
        int loop_depth = 0;
        int max_loop_depth = 0;
        for (char c : code) {
            if (c == '{') loop_depth++;
            if (c == '}') loop_depth--;
            max_loop_depth = std::max(max_loop_depth, loop_depth);
        }
        
        if (max_loop_depth > 3) {
            AddIssue(result, 1, 0, "Deep nesting detected - may impact performance",
                    IssueSeverity::Warning, IssueCategory::Performance,
                    "Consider refactoring to reduce nesting depth");
        }
    }
    
    void DetectMemoryIssues(const std::string& code, AnalysisResult& result) {
        // Memory allocation without deallocation
        bool has_new = code.find("new ") != std::string::npos;
        bool has_delete = code.find("delete ") != std::string::npos;
        
        if (has_new && !has_delete) {
            AddIssue(result, 1, 0, "Potential memory leak - new without delete",
                    IssueSeverity::Warning, IssueCategory::Memory,
                    "Use smart pointers (std::unique_ptr, std::shared_ptr) or ensure delete is called");
        }
        
        // Array access without bounds checking
        std::regex array_access(R"(\w+\[\w+\](?!\s*=))");
        if (std::regex_search(code, array_access) && 
            code.find("if") == std::string::npos) {
            AddIssue(result, 1, 0, "Array access without bounds checking",
                    IssueSeverity::Warning, IssueCategory::Memory,
                    "Add bounds checking before array access");
        }
        
        // Double free detection
        if (code.find("delete") != std::string::npos) {
            size_t first_delete = code.find("delete");
            size_t second_delete = code.find("delete", first_delete + 6);
            if (second_delete != std::string::npos) {
                AddIssue(result, 1, 0, "Potential double free - multiple delete statements",
                        IssueSeverity::Error, IssueCategory::Memory,
                        "Ensure pointer is set to nullptr after delete");
            }
        }
    }
    
    void DetectStyleViolations(const std::string& code, AnalysisResult& result) {
        // Line length check
        std::istringstream iss(code);
        std::string line;
        int line_num = 1;
        
        while (std::getline(iss, line)) {
            if (line.length() > 120) {
                AddIssue(result, line_num, 0, "Line too long (" + std::to_string(line.length()) + " chars)",
                        IssueSeverity::Info, IssueCategory::Style,
                        "Keep lines under 120 characters");
            }
            
            // Magic numbers
            std::regex magic_number(R"(\d{4,})");
            if (std::regex_search(line, magic_number) && line.find("const") == std::string::npos) {
                AddIssue(result, line_num, 0, "Magic number detected",
                        IssueSeverity::Info, IssueCategory::Style,
                        "Define as a named constant");
            }
            
            line_num++;
        }
        
        // Missing documentation
        if (code.find("/**") == std::string::npos && code.find("///") == std::string::npos) {
            if (code.find("class") != std::string::npos || code.find("function") != std::string::npos) {
                AddIssue(result, 1, 0, "Missing documentation comments",
                        IssueSeverity::Info, IssueCategory::Style,
                        "Add documentation for public APIs");
            }
        }
    }
    
    void DetectLogicErrors(const std::string& code, AnalysisResult& result) {
        // Assignment in condition
        std::regex assignment_in_if(R"(if\s*\([^=]*=[^=][^)]*\))");
        if (std::regex_search(code, assignment_in_if)) {
            AddIssue(result, 1, 0, "Assignment in if condition - possible typo",
                    IssueSeverity::Warning, IssueCategory::Logic,
                    "Use == for comparison, = for assignment");
        }
        
        // Dead code after return
        if (code.find("return") != std::string::npos) {
            size_t return_pos = code.find("return");
            size_t semicolon = code.find(";", return_pos);
            if (semicolon != std::string::npos) {
                size_t next_statement = code.find_first_not_of(" \t\n", semicolon + 1);
                if (next_statement != std::string::npos && 
                    code[next_statement] != '}') {
                    AddIssue(result, 1, 0, "Unreachable code after return statement",
                            IssueSeverity::Warning, IssueCategory::Logic,
                            "Remove or move code before return");
                }
            }
        }
        
        // Infinite loop
        std::regex infinite_loop(R"(while\s*\(\s*true\s*\)|for\s*\(\s*;\s*;\s*\))");
        if (std::regex_search(code, infinite_loop)) {
            if (code.find("break") == std::string::npos) {
                AddIssue(result, 1, 0, "Potential infinite loop without break",
                        IssueSeverity::Warning, IssueCategory::Logic,
                        "Add break condition or ensure loop can terminate");
            }
        }
    }
    
    void AddIssue(AnalysisResult& result, int line, int col, const std::string& msg,
                  IssueSeverity severity, IssueCategory category, const std::string& suggestion) {
        CodeIssue issue;
        issue.line_number = line;
        issue.column = col;
        issue.message = msg;
        issue.severity = severity;
        issue.category = category;
        issue.suggestion = suggestion;
        
        result.issues.push_back(issue);
        result.category_counts[category]++;
    }
    
    void CalculateQualityScore(AnalysisResult& result) {
        float penalty = 0.0f;
        
        for (const auto& issue : result.issues) {
            switch (issue.severity) {
                case IssueSeverity::Critical: penalty += 20.0f; break;
                case IssueSeverity::Error:    penalty += 10.0f; break;
                case IssueSeverity::Warning:  penalty += 5.0f;  break;
                case IssueSeverity::Info:     penalty += 1.0f;  break;
            }
        }
        
        result.quality_score = std::max(0.0f, 100.0f - penalty);
    }
    
    void GenerateSummary(AnalysisResult& result) {
        std::ostringstream ss;
        
        ss << "Code Quality: " << std::fixed << std::setprecision(1) << result.quality_score << "%\n";
        ss << "Total Issues: " << result.issues.size() << "\n";
        
        int critical = 0, errors = 0, warnings = 0, info = 0;
        for (const auto& issue : result.issues) {
            switch (issue.severity) {
                case IssueSeverity::Critical: critical++; break;
                case IssueSeverity::Error:    errors++;   break;
                case IssueSeverity::Warning:  warnings++; break;
                case IssueSeverity::Info:     info++;     break;
            }
        }
        
        if (critical > 0) ss << "  Critical: " << critical << "\n";
        if (errors > 0)   ss << "  Errors: " << errors << "\n";
        if (warnings > 0) ss << "  Warnings: " << warnings << "\n";
        if (info > 0)     ss << "  Info: " << info << "\n";
        
        result.summary = ss.str();
    }
};

// Global instance
static RealTimeAnalyzer* g_analyzer = nullptr;

extern "C" {
    void InitRealTimeAnalyzer() {
        if (!g_analyzer) {
            g_analyzer = new RealTimeAnalyzer();
            std::cout << "[ANALYZER] Real-time code analysis initialized\n";
        }
    }
    
    void ShutdownRealTimeAnalyzer() {
        if (g_analyzer) {
            delete g_analyzer;
            g_analyzer = nullptr;
        }
    }
}
