/**
 * @file ai_debug_assistant.cpp
 * @brief AI-powered debugging assistance implementation
 * Batch 5 - Item 73: AI debug assistant
 */

#include "ai/ai_debug_assistant.h"
#include <sstream>
#include <algorithm>
#include <regex>

namespace RawrXD::AI {

AIDebugAssistant::AIDebugAssistant()
    : m_initialized(false)
    , m_maxTokens(1000)
    , m_temperature(0.3f) {
}

AIDebugAssistant::~AIDebugAssistant() {
    shutdown();
}

bool AIDebugAssistant::initialize() {
    m_initialized = true;
    return true;
}

void AIDebugAssistant::shutdown() {
    m_initialized = false;
}

DebugAnalysis AIDebugAssistant::analyzeError(const ErrorInfo& error) {
    DebugAnalysis analysis;
    
    if (!m_initialized) {
        return analysis;
    }
    
    // Analyze error type
    analysis.rootCause = determineRootCause(error);
    
    // Generate suggestions
    analysis.suggestions = generateSuggestions(error);
    
    // Find related issues
    analysis.relatedIssues = findRelatedIssues(error);
    
    // Generate documentation reference
    analysis.documentation = generateDocumentationReference(error);
    
    return analysis;
}

DebugAnalysis AIDebugAssistant::analyzeException(const std::string& exceptionType,
                                                  const std::string& message,
                                                  const std::vector<StackFrame>& stackTrace) {
    ErrorInfo error;
    error.type = exceptionType;
    error.message = message;
    error.stackTrace = stackTrace;
    
    return analyzeError(error);
}

std::string AIDebugAssistant::explainVariable(const VariableInfo& variable) {
    std::stringstream explanation;
    
    explanation << "Variable: " << variable.name << "\n";
    explanation << "Type: " << variable.type << "\n";
    explanation << "Value: " << variable.value << "\n\n";
    
    if (variable.isPointer) {
        if (variable.isNull) {
            explanation << "This is a null pointer. Attempting to dereference it will cause a crash.\n";
        } else {
            explanation << "This is a pointer to memory. Ensure it's properly allocated before use.\n";
        }
    }
    
    // Add type-specific explanations
    if (variable.type.find("std::vector") != std::string::npos) {
        explanation << "This is a dynamic array. Check bounds before accessing elements.\n";
    } else if (variable.type.find("std::string") != std::string::npos) {
        explanation << "This is a string object. Operations are generally safe.\n";
    } else if (variable.type.find("*") != std::string::npos) {
        explanation << "This is a raw pointer. Consider using smart pointers for safety.\n";
    }
    
    return explanation.str();
}

std::vector<std::string> AIDebugAssistant::suggestVariableInspection(const std::vector<VariableInfo>& variables) {
    std::vector<std::string> suggestions;
    
    for (const auto& var : variables) {
        // Suggest checking null pointers
        if (var.isPointer && var.isNull) {
            suggestions.push_back("Check " + var.name + " - it's a null pointer");
        }
        
        // Suggest checking uninitialized variables
        if (var.value == "???" || var.value == "garbage") {
            suggestions.push_back("Inspect " + var.name + " - may be uninitialized");
        }
        
        // Suggest checking large values
        try {
            double val = std::stod(var.value);
            if (val > 1e9 || val < -1e9) {
                suggestions.push_back("Verify " + var.name + " - unusually large value: " + var.value);
            }
        } catch (...) {
            // Not a number, ignore
        }
    }
    
    return suggestions;
}

std::vector<int> AIDebugAssistant::suggestBreakpoints(const std::string& code) {
    std::vector<int> breakpoints;
    
    std::istringstream stream(code);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(stream, line)) {
        lineNum++;
        
        // Suggest breakpoints at function definitions
        if (std::regex_search(line, std::regex("^\\s*(void|int|bool|string|auto)\\s+\\w+\\s*\\("))) {
            breakpoints.push_back(lineNum);
        }
        
        // Suggest breakpoints at return statements
        if (std::regex_search(line, std::regex("\\breturn\\b"))) {
            breakpoints.push_back(lineNum);
        }
        
        // Suggest breakpoints at loop conditions
        if (std::regex_search(line, std::regex("\\b(for|while)\\s*\\("))) {
            breakpoints.push_back(lineNum);
        }
        
        // Suggest breakpoints at if statements
        if (std::regex_search(line, std::regex("\\bif\\s*\\("))) {
            breakpoints.push_back(lineNum);
        }
    }
    
    return breakpoints;
}

std::vector<int> AIDebugAssistant::suggestConditionalBreakpoints(const std::string& code,
                                                                     const std::string& condition) {
    std::vector<int> breakpoints;
    
    // Find lines where the condition variable is used
    std::istringstream stream(code);
    std::string line;
    int lineNum = 0;
    
    while (std::getline(stream, line)) {
        lineNum++;
        
        // Check if condition variable appears in this line
        if (line.find(condition) != std::string::npos) {
            breakpoints.push_back(lineNum);
        }
    }
    
    return breakpoints;
}

std::string AIDebugAssistant::suggestNextStep(const std::vector<StackFrame>& currentStack,
                                               const std::map<std::string, VariableInfo>& variables) {
    std::stringstream suggestion;
    
    if (currentStack.empty()) {
        suggestion << "No stack information available. Check if the program is running.";
        return suggestion.str();
    }
    
    const auto& currentFrame = currentStack[0];
    
    suggestion << "Current location: " << currentFrame.function << " at " 
              << currentFrame.file << ":" << currentFrame.line << "\n\n";
    
    // Analyze variables
    bool hasNullPointer = false;
    bool hasUninitialized = false;
    
    for (const auto& [name, var] : variables) {
        if (var.isPointer && var.isNull) {
            hasNullPointer = true;
        }
        if (var.value == "???") {
            hasUninitialized = true;
        }
    }
    
    if (hasNullPointer) {
        suggestion << "Step over (F10) to see if null pointer is dereferenced.\n";
        suggestion << "Watch for any pointer dereferences on the next line.\n";
    } else if (hasUninitialized) {
        suggestion << "Step into (F11) to trace where uninitialized variables come from.\n";
    } else {
        suggestion << "Step over (F10) to continue execution.\n";
        suggestion << "If entering a function, use Step Into (F11).\n";
    }
    
    return suggestion.str();
}

std::vector<DebugSuggestion> AIDebugAssistant::suggestFixes(const ErrorInfo& error) {
    std::vector<DebugSuggestion> fixes;
    
    // Null pointer fixes
    if (error.type == "NullPointerException" || error.type == "AccessViolation" ||
        error.message.find("null") != std::string::npos) {
        DebugSuggestion fix;
        fix.description = "Add null check before dereferencing";
        fix.code = "if (ptr != nullptr) {\n    ptr->doSomething();\n}";
        fix.explanation = "Always check if a pointer is null before dereferencing it to prevent crashes.";
        fix.confidence = 95;
        fixes.push_back(fix);
    }
    
    // Array bounds fixes
    if (error.type == "ArrayIndexOutOfBounds" || error.type == "OutOfRange" ||
        error.message.find("bounds") != std::string::npos) {
        DebugSuggestion fix;
        fix.description = "Add bounds checking";
        fix.code = "if (index >= 0 && index < array.size()) {\n    // access array[index]\n}";
        fix.explanation = "Always verify array indices are within valid bounds before accessing.";
        fix.confidence = 90;
        fixes.push_back(fix);
    }
    
    // Memory fixes
    if (error.type == "MemoryAccess" || error.type == "SegmentationFault") {
        DebugSuggestion fix;
        fix.description = "Use smart pointers instead of raw pointers";
        fix.code = "std::unique_ptr<Type> ptr = std::make_unique<Type>();";
        fix.explanation = "Smart pointers automatically manage memory and prevent common errors.";
        fix.confidence = 85;
        fixes.push_back(fix);
    }
    
    // Exception handling
    if (error.type == "UnhandledException") {
        DebugSuggestion fix;
        fix.description = "Add try-catch block";
        fix.code = "try {\n    // risky code\n} catch (const std::exception& e) {\n    // handle error\n}";
        fix.explanation = "Wrap code that may throw exceptions in try-catch blocks.";
        fix.confidence = 80;
        fixes.push_back(fix);
    }
    
    return fixes;
}

std::optional<DebugSuggestion> AIDebugAssistant::getBestFix(const ErrorInfo& error) {
    auto fixes = suggestFixes(error);
    
    if (fixes.empty()) {
        return std::nullopt;
    }
    
    // Return fix with highest confidence
    return *std::max_element(fixes.begin(), fixes.end(),
        [](const DebugSuggestion& a, const DebugSuggestion& b) {
            return a.confidence < b.confidence;
        });
}

std::vector<std::string> AIDebugAssistant::analyzeLog(const std::vector<std::string>& logLines) {
    std::vector<std::string> issues;
    
    for (const auto& line : logLines) {
        // Check for error patterns
        if (line.find("ERROR") != std::string::npos ||
            line.find("FATAL") != std::string::npos ||
            line.find("Exception") != std::string::npos) {
            issues.push_back("Error found: " + line);
        }
        
        // Check for warning patterns
        if (line.find("WARNING") != std::string::npos ||
            line.find("WARN") != std::string::npos) {
            issues.push_back("Warning: " + line);
        }
        
        // Check for memory issues
        if (line.find("memory") != std::string::npos ||
            line.find("leak") != std::string::npos) {
            issues.push_back("Memory issue detected: " + line);
        }
    }
    
    return issues;
}

std::optional<ErrorInfo> AIDebugAssistant::extractErrorFromLog(const std::vector<std::string>& logLines) {
    ErrorInfo error;
    bool foundError = false;
    
    for (const auto& line : logLines) {
        // Look for exception patterns
        std::regex exceptionRegex("(\\w+Exception|\\w+Error):\\s*(.+)$");
        std::smatch match;
        
        if (std::regex_search(line, match, exceptionRegex)) {
            error.type = match[1].str();
            error.message = match[2].str();
            foundError = true;
            break;
        }
        
        // Look for assertion failures
        if (line.find("Assertion failed") != std::string::npos) {
            error.type = "AssertionFailure";
            error.message = line;
            foundError = true;
            break;
        }
    }
    
    if (foundError) {
        return error;
    }
    
    return std::nullopt;
}

std::vector<std::string> AIDebugAssistant::detectMemoryIssues(const std::vector<VariableInfo>& variables) {
    std::vector<std::string> issues;
    
    for (const auto& var : variables) {
        if (var.isPointer) {
            if (var.isNull) {
                issues.push_back(var.name + " is null - potential crash if dereferenced");
            }
            
            // Check for suspicious pointer values
            try {
                size_t addr = std::stoull(var.value, nullptr, 16);
                if (addr < 0x1000) {
                    issues.push_back(var.name + " has suspicious address: " + var.value);
                }
            } catch (...) {
                // Not a hex number, ignore
            }
        }
    }
    
    return issues;
}

std::string AIDebugAssistant::explainMemoryLayout(const std::string& type, size_t size) {
    std::stringstream explanation;
    
    explanation << "Memory layout for " << type << ":\n";
    explanation << "Total size: " << size << " bytes\n\n";
    
    if (type == "int") {
        explanation << "Integer: 4 bytes (32-bit) or 8 bytes (64-bit)\n";
        explanation << "Stored in 2's complement format\n";
    } else if (type == "double") {
        explanation << "Double: 8 bytes\n";
        explanation << "IEEE 754 floating-point format\n";
    } else if (type.find("std::vector") != std::string::npos) {
        explanation << "std::vector: typically 24 bytes (3 pointers)\n";
        explanation << "- Pointer to data\n";
        explanation << "- Size\n";
        explanation << "- Capacity\n";
    } else if (type.find("*") != std::string::npos) {
        explanation << "Pointer: 4 bytes (32-bit) or 8 bytes (64-bit)\n";
        explanation << "Contains memory address of pointed-to object\n";
    }
    
    return explanation.str();
}

void AIDebugAssistant::setModel(const std::string& model) {
    m_model = model;
}

void AIDebugAssistant::setMaxTokens(int maxTokens) {
    m_maxTokens = maxTokens;
}

void AIDebugAssistant::setTemperature(float temperature) {
    m_temperature = std::clamp(temperature, 0.0f, 2.0f);
}

void AIDebugAssistant::onAnalysisComplete(AnalysisCallback callback) {
    m_analysisCallback = callback;
}

std::string AIDebugAssistant::determineRootCause(const ErrorInfo& error) {
    if (error.type == "NullPointerException" || error.type == "AccessViolation") {
        return "Attempting to access memory through a null or invalid pointer";
    } else if (error.type == "ArrayIndexOutOfBounds" || error.type == "OutOfRange") {
        return "Accessing an array or container with an invalid index";
    } else if (error.type == "MemoryAccess" || error.type == "SegmentationFault") {
        return "Accessing memory that doesn't belong to the program";
    } else if (error.type == "StackOverflow") {
        return "Infinite recursion or excessive stack usage";
    } else if (error.type == "DivideByZero") {
        return "Division or modulo by zero";
    } else if (error.type == "AssertionFailure") {
        return "Program state doesn't match expected condition";
    }
    
    return "Unknown error - check stack trace for more details";
}

std::vector<DebugSuggestion> AIDebugAssistant::generateSuggestions(const ErrorInfo& error) {
    return suggestFixes(error);
}

std::vector<std::string> AIDebugAssistant::findRelatedIssues(const ErrorInfo& error) {
    std::vector<std::string> issues;
    
    // Based on error type, suggest related issues to check
    if (error.type == "NullPointerException") {
        issues.push_back("Check initialization order of member variables");
        issues.push_back("Verify factory methods return valid objects");
        issues.push_back("Review pointer assignment in constructors");
    } else if (error.type == "MemoryAccess") {
        issues.push_back("Check for use-after-free bugs");
        issues.push_back("Verify buffer sizes match data written");
        issues.push_back("Review pointer arithmetic calculations");
    } else if (error.type == "ArrayIndexOutOfBounds") {
        issues.push_back("Check loop boundary conditions");
        issues.push_back("Verify container sizes before access");
        issues.push_back("Review iterator validity");
    }
    
    return issues;
}

std::string AIDebugAssistant::generateDocumentationReference(const ErrorInfo& error) {
    std::stringstream doc;
    
    doc << "Documentation references for " << error.type << ":\n";
    
    if (error.type == "NullPointerException") {
        doc << "- C++ Core Guidelines: P.1 - Don't use raw pointers\n";
        doc << "- cppreference.com: nullptr\n";
    } else if (error.type == "MemoryAccess") {
        doc << "- C++ Core Guidelines: R.11 - Avoid calling new and delete explicitly\n";
        doc << "- cppreference.com: Smart pointers\n";
    } else if (error.type == "ArrayIndexOutOfBounds") {
        doc << "- C++ Core Guidelines: Bounds.2 - Only index into arrays using constant expressions\n";
        doc << "- cppreference.com: std::vector::at()\n";
    }
    
    return doc.str();
}

} // namespace RawrXD::AI
