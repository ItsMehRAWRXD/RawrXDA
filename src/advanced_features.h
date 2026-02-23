#pragma once
#include <string>
#include <regex>
#include <filesystem>
#include <vector>
#include <unordered_map>

class AdvancedFeatures {
public:
    // Chain of thought prompting
    static std::string ChainOfThought(const std::string& prompt);
    
    // No refusal mode
    static std::string NoRefusal(const std::string& prompt);
    
    // Deep research with workspace scanning
    static std::string DeepResearch(const std::string& prompt);
    
    // Auto-correction of hallucinations
    static std::string AutoCorrect(const std::string& text);
    
    // Hot patch application
    static bool ApplyHotPatch(const std::string& file_path, const std::string& old_code, const std::string& new_code);
    
    // Code analysis
    static std::string AnalyzeCode(const std::string& code);
    
    // Security scan
    static std::string SecurityScan(const std::string& code);
    
    // Performance optimization
    static std::string OptimizePerformance(const std::string& code);
    
private:
    static std::vector<std::string> ExtractKeywords(const std::string& prompt);
    
    struct FileContext {
        std::string path;
        std::string content;
    };
    
    static std::vector<FileContext> ScanWorkspace(const std::vector<std::string>& keywords);
    static std::string ValidateFileReferences(const std::string& text);
    static size_t FuzzyFind(const std::string& content, const std::string& pattern);
    
    // Security patterns
    static std::unordered_map<std::string, std::string> security_patterns_;
    static std::unordered_map<std::string, std::string> performance_patterns_;
};
