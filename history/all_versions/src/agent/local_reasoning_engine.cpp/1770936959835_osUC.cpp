// ============================================================================
// local_reasoning_engine.cpp — API-Free Local Code Analysis Engine
// ============================================================================
// Architecture: Pure C++20 + MASM64, NO external API dependencies
// Expertise: x64 reverse engineering, static analysis, pattern recognition
// Author: Kernel RE Specialist
// Build: MSVC 2022 / Clang with /std:c++20
// ============================================================================

#include "local_reasoning_engine.hpp"
#include <chrono>
#include <algorithm>
#include <sstream>
#include <regex>
#include <iostream>
#include <fstream>
#include <cctype>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

LocalReasoningEngine::LocalReasoningEngine() {
    // Load default expert rules into knowledge base
    m_customRules = loadDefaultRules();
}

LocalReasoningEngine::~LocalReasoningEngine() {
    if (m_analyzing) {
        cancelAnalysis();
    }
}

// ════════════════════════════════════════════════════════════════════════
// CORE ANALYSIS ENGINE
// ════════════════════════════════════════════════════════════════════════

LocalReasoningEngine::AnalysisResult LocalReasoningEngine::analyze(const AnalysisContext& context) {
    auto startTime = std::chrono::high_resolution_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalAnalyses++;
    }

    AnalysisResult result;
    result.overallConfidence = 0.0f;
    result.passesCompleted = 0;

    try {
        // Multi-pass analysis
        result = performAnalysisPasses(context);
        
        // Generate summary
        result.summary = generateSummary(result.issues);
        
        // Calculate overall confidence
        result.overallConfidence = calculateOverallConfidence(result.issues);
        
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.issuesDetected += result.issues.size();
            for (const auto& issue : result.issues) {
                if (issue.severity == "critical") {
                    m_stats.criticalIssues++;
                }
                m_stats.issueTypeFrequency[issue.issueType]++;
            }
        }

    } catch (const std::exception& e) {
        if (m_verbose) {
            std::cerr << "[LocalReasoning] Analysis error: " << e.what() << "\n";
        }
        result.summary = std::string("Analysis error: ") + e.what();
        result.overallConfidence = 0.0f;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    result.elapsedMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.avgAnalysisTime = (m_stats.avgAnalysisTime * (m_stats.totalAnalyses - 1) + result.elapsedMicroseconds / 1000.0f)
                                 / m_stats.totalAnalyses;
        m_stats.avgConfidence = (m_stats.avgConfidence * (m_stats.totalAnalyses - 1) + result.overallConfidence)
                               / m_stats.totalAnalyses;
    }

    return result;
}

LocalReasoningEngine::AnalysisResult LocalReasoningEngine::performAnalysisPasses(const AnalysisContext& context) {
    AnalysisResult result;
    std::vector<CodeIssue> cumulativeIssues;

    // Pass 1: Syntax & Structure Scan
    if (m_verbose) std::cout << "[LocalReasoning] Pass 1: Syntax Scan\n";
    auto syntaxIssues = scanSyntaxStructure(context.sourceCode, context.language);
    cumulativeIssues.insert(cumulativeIssues.end(), syntaxIssues.begin(), syntaxIssues.end());
    result.passesCompleted++;

    // Pass 2: Pattern Matching (Bug patterns)
    if (m_verbose) std::cout << "[LocalReasoning] Pass 2: Pattern Matching\n";
    auto patternIssues = detectCommonPatterns(context.sourceCode);
    cumulativeIssues.insert(cumulativeIssues.end(), patternIssues.begin(), patternIssues.end());
    result.passesCompleted++;

    // Pass 3: Rule-based Analysis (Expert system)
    if (m_verbose) std::cout << "[LocalReasoning] Pass 3: Rule Application\n";
    auto ruleIssues = applyRules(context.sourceCode, m_customRules);
    cumulativeIssues.insert(cumulativeIssues.end(), ruleIssues.begin(), ruleIssues.end());
    result.passesCompleted++;

    // Pass 4: Assembly Analysis (if enabled)
    if (context.includeAssembly && (context.language == "asm" || context.language == "masm")) {
        if (m_verbose) std::cout << "[LocalReasoning] Pass 4: x64 Assembly Analysis\n";
        auto asmIssues = analyzeAssembly(context.sourceCode);
        cumulativeIssues.insert(cumulativeIssues.end(), asmIssues.begin(), asmIssues.end());
        result.passesCompleted++;
    }

    // Pass 5: Deep Analysis (if enabled) - CFG, data flow
    if (context.deepAnalysis) {
        if (m_verbose) std::cout << "[LocalReasoning] Pass 5: Deep Analysis (CFG)\n";
        auto cfg = buildControlFlowGraph(context.sourceCode);
        if (detectInfiniteLoop(cfg)) {
            CodeIssue issue;
            issue.issueType = "infinite-loop";
            issue.severity = "high";
            issue.description = "Potential infinite loop detected via CFG analysis";
            issue.confidence = 0.85f;
            issue.recommendation = "Add termination condition or timeout mechanism";
            cumulativeIssues.push_back(issue);
        }
        if (detectUnreachableCode(cfg)) {
            CodeIssue issue;
            issue.issueType = "dead-code";
            issue.severity = "low";
            issue.description = "Unreachable code detected via CFG analysis";
            issue.confidence = 0.90f;
            issue.recommendation = "Remove dead code to reduce binary size";
            cumulativeIssues.push_back(issue);
        }
        result.passesCompleted++;
    }

    result.issues = cumulativeIssues;
    return result;
}

// ════════════════════════════════════════════════════════════════════════
// PATTERN DETECTION (Core heuristics - no API needed)
// ════════════════════════════════════════════════════════════════════════

std::vector<LocalReasoningEngine::CodeIssue> LocalReasoningEngine::detectCommonPatterns(const std::string& code) {
    std::vector<CodeIssue> issues;
    std::string evidence;

    // Memory safety checks
    if (detectMemoryLeak(code, evidence)) {
        CodeIssue issue;
        issue.issueType = "memory-leak";
        issue.severity = "high";
        issue.description = "Potential memory leak: allocation without corresponding free";
        issue.confidence = 0.80f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Ensure all allocated memory is freed, or use RAII (std::unique_ptr)";
        issues.push_back(issue);
    }

    if (detectBufferOverflow(code, evidence)) {
        CodeIssue issue;
        issue.issueType = "buffer-overflow";
        issue.severity = "critical";
        issue.description = "Buffer overflow risk: unbounded string/array operation";
        issue.confidence = 0.85f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Use bounds-checked functions (strncpy, memcpy_s) or std::string";
        issues.push_back(issue);
    }

    if (detectUseAfterFree(code, evidence)) {
        CodeIssue issue;
        issue.issueType = "use-after-free";
        issue.severity = "critical";
        issue.description = "Use-after-free vulnerability detected";
        issue.confidence = 0.75f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Set pointers to nullptr after free, or use smart pointers";
        issues.push_back(issue);
    }

    if (detectNullDereference(code, evidence)) {
        CodeIssue issue;
        issue.issueType = "null-dereference";
        issue.severity = "high";
        issue.description = "Potential null pointer dereference";
        issue.confidence = 0.70f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Add null check before dereferencing pointer";
        issues.push_back(issue);
    }

    // Threading issues
    if (detectRaceCondition(code, evidence)) {
        CodeIssue issue;
        issue.issueType = "race-condition";
        issue.severity = "critical";
        issue.description = "Race condition: shared variable accessed without synchronization";
        issue.confidence = 0.75f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Protect shared data with std::mutex or std::atomic";
        issues.push_back(issue);
    }

    if (detectDeadlock(code, evidence)) {
        CodeIssue issue;
        issue.issueType = "deadlock";
        issue.severity = "critical";
        issue.description = "Potential deadlock: circular lock dependency";
        issue.confidence = 0.70f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Establish lock ordering, or use std::scoped_lock for multiple locks";
        issues.push_back(issue);
    }

    // Security vulnerabilities
    if (detectCommandInjection(code, evidence)) {
        CodeIssue issue;
        issue.issueType = "command-injection";
        issue.severity = "critical";
        issue.description = "Command injection vulnerability: unsanitized input passed to shell";
        issue.confidence = 0.90f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Avoid system()/exec() with user input, or use parameterized commands";
        issues.push_back(issue);
    }

    if (detectIntegerOverflow(code, evidence)) {
        CodeIssue issue;
        issue.issueType = "integer-overflow";
        issue.severity = "high";
        issue.description = "Integer overflow risk in arithmetic operation";
        issue.confidence = 0.65f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Add overflow checks before arithmetic, or use SafeInt library";
        issues.push_back(issue);
    }

    // Performance anti-patterns
    if (detectExpensiveLoop(code, evidence)) {
        CodeIssue issue;
        issue.issueType = "performance";
        issue.severity = "medium";
        issue.description = "Expensive operation in loop body";
        issue.confidence = 0.75f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Hoist invariant operations out of loop";
        issues.push_back(issue);
    }

    return issues;
}

// Memory leak pattern: new/malloc without delete/free
bool LocalReasoningEngine::detectMemoryLeak(const std::string& code, std::string& evidence) {
    // Look for allocation patterns
    std::vector<std::string> allocPatterns = {
        "new ", "malloc(", "calloc(", "realloc(", "VirtualAlloc", "HeapAlloc"
    };
    std::vector<std::string> freePatterns = {
        "delete ", "free(", "VirtualFree", "HeapFree"
    };

    int allocCount = 0;
    int freeCount = 0;
    size_t allocPos = 0;

    for (const auto& pattern : allocPatterns) {
        size_t pos = 0;
        while ((pos = code.find(pattern, pos)) != std::string::npos) {
            allocCount++;
            if (allocPos == 0) allocPos = pos;  // Remember first allocation
            pos += pattern.length();
        }
    }

    for (const auto& pattern : freePatterns) {
        size_t pos = 0;
        while ((pos = code.find(pattern, pos)) != std::string::npos) {
            freeCount++;
            pos += pattern.length();
        }
    }

    // Heuristic: more allocations than frees (simple check)
    if (allocCount > freeCount && allocCount > 0) {
        // Extract evidence (line with allocation)
        size_t lineStart = code.rfind('\n', allocPos);
        size_t lineEnd = code.find('\n', allocPos);
        if (lineStart == std::string::npos) lineStart = 0; else lineStart++;
        if (lineEnd == std::string::npos) lineEnd = code.length();
        evidence = code.substr(lineStart, lineEnd - lineStart);
        return true;
    }

    return false;
}

// Buffer overflow: strcpy, sprintf, gets
bool LocalReasoningEngine::detectBufferOverflow(const std::string& code, std::string& evidence) {
    std::vector<std::string> unsafeFunctions = {
        "strcpy(", "strcat(", "sprintf(", "gets(", "scanf(\"%s", "vsprintf("
    };

    for (const auto& func : unsafeFunctions) {
        size_t pos = code.find(func);
        if (pos != std::string::npos) {
            // Extract line
            size_t lineStart = code.rfind('\n', pos);
            size_t lineEnd = code.find('\n', pos);
            if (lineStart == std::string::npos) lineStart = 0; else lineStart++;
            if (lineEnd == std::string::npos) lineEnd = code.length();
            evidence = code.substr(lineStart, lineEnd - lineStart);
            return true;
        }
    }

    return false;
}

// Use-after-free: delete/free followed by usage
bool LocalReasoningEngine::detectUseAfterFree(const std::string& code, std::string& evidence) {
    // Regex: find patterns like "delete ptr;" followed later by "ptr->"
    // Simplified heuristic: look for delete/free then check if pointer used again
    
    std::regex deletePattern(R"((delete|free)\s*\(?(\w+)\)?;)");
    std::smatch match;
    std::string::const_iterator searchStart(code.cbegin());

    while (std::regex_search(searchStart, code.cend(), match, deletePattern)) {
        std::string ptrName = match[2].str();
        size_t deletePos = match.position(0) + std::distance(code.cbegin(), searchStart);
        
        // Look for usage after delete
        std::string afterDelete = code.substr(deletePos + match.length(0));
        size_t usagePos = afterDelete.find(ptrName);
        
        if (usagePos != std::string::npos && usagePos < 1000) {  // Within reasonable distance
            // Check it's not being reassigned (ptr = ...)
            if (afterDelete[usagePos + ptrName.length()] != '=') {
                evidence = match.str() + " ... later: " + ptrName;
                return true;
            }
        }
        
        searchStart = match.suffix().first;
    }

    return false;
}

// Null dereference: pointer used without null check
bool LocalReasoningEngine::detectNullDereference(const std::string& code, std::string& evidence) {
    // Look for pointer dereference (-> or *ptr) without prior null check
    std::regex derefPattern(R"((\w+)->|\*(\w+)[^\w])");
    std::smatch match;
    std::string::const_iterator searchStart(code.cbegin());

    while (std::regex_search(searchStart, code.cend(), match, derefPattern)) {
        std::string ptrName = match[1].matched ? match[1].str() : match[2].str();
        size_t derefPos = match.position(0) + std::distance(code.cbegin(), searchStart);
        
        // Look backwards for null check
        std::string before = code.substr(0, derefPos);
        std::string nullCheck = ptrName + " != nullptr";
        std::string nullCheck2 = ptrName + " != NULL";
        std::string nullCheck3 = "if (" + ptrName + ")";
        std::string nullCheck4 = "if (!" + ptrName + ")";
        
        bool hasCheck = (before.rfind(nullCheck, derefPos) != std::string::npos) ||
                       (before.rfind(nullCheck2, derefPos) != std::string::npos) ||
                       (before.rfind(nullCheck3, derefPos) != std::string::npos);
        
        if (!hasCheck) {
            size_t lineStart = code.rfind('\n', derefPos);
            size_t lineEnd = code.find('\n', derefPos);
            if (lineStart == std::string::npos) lineStart = 0; else lineStart++;
            if (lineEnd == std::string::npos) lineEnd = code.length();
            evidence = code.substr(lineStart, lineEnd - lineStart);
            return true;
        }
        
        searchStart = match.suffix().first;
    }

    return false;
}

// Race condition: shared variable without lock
bool LocalReasoningEngine::detectRaceCondition(const std::string& code, std::string& evidence) {
    // Look for shared (static/global) variables accessed without mutex
    // Heuristic: find "static" or global variables, check for mutex usage nearby
    
    std::regex staticVar(R"(static\s+\w+\s+(\w+);)");
    std::smatch match;
    std::string::const_iterator searchStart(code.cbegin());

    while (std::regex_search(searchStart, code.cend(), match, staticVar)) {
        std::string varName = match[1].str();
        size_t varPos = match.position(0) + std::distance(code.cbegin(), searchStart);
        
        // Look for mutex/lock usage near this variable
        std::string context = code.substr(varPos, 500);  // Check next 500 chars
        bool hasMutex = (context.find("std::mutex") != std::string::npos) ||
                       (context.find("std::lock_guard") != std::string::npos) ||
                       (context.find("std::unique_lock") != std::string::npos) ||
                       (context.find("pthread_mutex") != std::string::npos) ||
                       (context.find("CRITICAL_SECTION") != std::string::npos);
        
        if (!hasMutex && context.find(varName) != std::string::npos) {
            evidence = match.str() + " (no mutex protection)";
            return true;
        }
        
        searchStart = match.suffix().first;
    }

    return false;
}

// Deadlock: multiple lock acquisitions in different order
bool LocalReasoningEngine::detectDeadlock(const std::string& code, std::string& evidence) {
    // Simplified: look for multiple lock() calls - requires manual review
    int lockCount = countOccurrences(code, ".lock()");
    lockCount += countOccurrences(code, "pthread_mutex_lock");
    lockCount += countOccurrences(code, "EnterCriticalSection");

    if (lockCount >= 2) {
        evidence = "Multiple locks detected - potential deadlock if order inconsistent";
        return true;
    }

    return false;
}

// Command injection: system()/exec() with user input
bool LocalReasoningEngine::detectCommandInjection(const std::string& code, std::string& evidence) {
    std::vector<std::string> dangerousFuncs = {
        "system(", "exec(", "popen(", "ShellExecute", "CreateProcess"
    };

    for (const auto& func : dangerousFuncs) {
        size_t pos = code.find(func);
        if (pos != std::string::npos) {
            // Check if user input is nearby (simplified heuristic)
            std::string before = code.substr(std::max(0, (int)pos - 200), 200);
            bool hasUserInput = (before.find("cin >>") != std::string::npos) ||
                               (before.find("scanf") != std::string::npos) ||
                               (before.find("gets") != std::string::npos) ||
                               (before.find("getline") != std::string::npos) ||
                               (before.find("argv") != std::string::npos);
            
            if (hasUserInput) {
                size_t lineStart = code.rfind('\n', pos);
                size_t lineEnd = code.find('\n', pos);
                if (lineStart == std::string::npos) lineStart = 0; else lineStart++;
                if (lineEnd == std::string::npos) lineEnd = code.length();
                evidence = code.substr(lineStart, lineEnd - lineStart);
                return true;
            }
        }
    }

    return false;
}

// Integer overflow: arithmetic without overflow checks
bool LocalReasoningEngine::detectIntegerOverflow(const std::string& code, std::string& evidence) {
    // Look for multiplications/additions with large numbers or user input
    std::regex arithPattern(R"((\w+)\s*[+*]\s*(\w+))");
    std::smatch match;
    std::string::const_iterator searchStart(code.cbegin());

    while (std::regex_search(searchStart, code.cend(), match, arithPattern)) {
        // Check if there's overflow check nearby
        size_t pos = match.position(0) + std::distance(code.cbegin(), searchStart);
        std::string before = code.substr(std::max(0, (int)pos - 100), 100);
        std::string after = code.substr(pos, 100);
        
        bool hasCheck = (before.find("INT_MAX") != std::string::npos) ||
                       (after.find("overflow") != std::string::npos) ||
                       (after.find("< ") != std::string::npos && after.find(">") != std::string::npos);
        
        if (!hasCheck) {
            evidence = match.str();
            return true;  // Report first case only
        }
        
        searchStart = match.suffix().first;
    }

    return false;
}

// Expensive loop: allocation/syscall in loop
bool LocalReasoningEngine::detectExpensiveLoop(const std::string& code, std::string& evidence) {
    // Find loops
    std::vector<std::string> loopKeywords = {"for (", "for(", "while (", "while("};
    
    for (const auto& keyword : loopKeywords) {
        size_t pos = code.find(keyword);
        if (pos != std::string::npos) {
            // Extract loop body (simplified: next 200 chars)
            std::string loopBody = code.substr(pos, 200);
            
            // Check for expensive operations
            bool hasAlloc = (loopBody.find("new ") != std::string::npos) ||
                           (loopBody.find("malloc(") != std::string::npos);
            bool hasSyscall = (loopBody.find("WriteFile") != std::string::npos) ||
                             (loopBody.find("ReadFile") != std::string::npos) ||
                             (loopBody.find("::open(") != std::string::npos);
            
            if (hasAlloc || hasSyscall) {
                evidence = keyword + " { ... expensive operation ... }";
                return true;
            }
        }
    }

    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// x64 ASSEMBLY ANALYSIS (MASM expertise)
// ═══════════════════════════════════════════════════════════════════════════

std::vector<LocalReasoningEngine::CodeIssue> LocalReasoningEngine::analyzeAssembly(const std::string& asmCode) {
    std::vector<CodeIssue> issues;
    std::string evidence;

    // Check stack alignment (x64 requires 16-byte alignment)
    if (detectMissingStackAlignment(asmCode, evidence)) {
        CodeIssue issue;
        issue.issueType = "asm-misalignment";
        issue.severity = "high";
        issue.description = "Stack not 16-byte aligned before call (x64 ABI violation)";
        issue.confidence = 0.90f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Ensure RSP % 16 == 8 before call instruction";
        issues.push_back(issue);
    }

    // Check shadow space (Win64 ABI requires 32 bytes)
    if (asmCode.find("call") != std::string::npos && !checkShadowSpace(asmCode)) {
        CodeIssue issue;
        issue.issueType = "asm-no-shadow-space";
        issue.severity = "critical";
        issue.description = "Missing shadow space allocation (Win64 ABI violation)";
        issue.confidence = 0.95f;
        issue.recommendation = "sub rsp, 32 (or higher multiple of 16) before call";
        issues.push_back(issue);
    }

    // Check register preservation (non-volatile: RBX, RBP, RDI, RSI, R12-R15)
    if (!checkRegisterPreservation(asmCode, {"rbx", "rbp", "rdi", "rsi", "r12", "r13", "r14", "r15"})) {
        CodeIssue issue;
        issue.issueType = "asm-clobbered-register";
        issue.severity = "high";
        issue.description = "Non-volatile register modified without push/pop";
        issue.confidence = 0.85f;
        issue.recommendation = "Push non-volatile registers at function entry, pop before ret";
        issues.push_back(issue);
    }

    // Detect suboptimal patterns
    if (detectIneffientInstruction(asmCode, evidence)) {
        CodeIssue issue;
        issue.issueType = "asm-inefficient";
        issue.severity = "low";
        issue.description = "Inefficient instruction sequence detected";
        issue.confidence = 0.75f;
        issue.evidence.push_back(evidence);
        issue.recommendation = "Replace with more efficient alternative (see evidence)";
        issues.push_back(issue);
    }

    return issues;
}

bool LocalReasoningEngine::detectMissingStackAlignment(const std::string& asm, std::string& evidence) {
    // x64: RSP must be 16-byte aligned at function entry, and 16-byte aligned + 8 before call
    // Simplified check: look for "call" without preceding "and rsp" or "sub rsp" with multiple of 16
    
    size_t callPos = asm.find("call");
    if (callPos == std::string::npos) return false;

    // Look backward for stack adjustment
    std::string before = asm.substr(0, callPos);
    bool hasAlignment = (before.rfind("and rsp,") != std::string::npos) ||
                       (before.rfind("sub rsp, 16") != std::string::npos) ||
                       (before.rfind("sub rsp, 32") != std::string::npos) ||
                       (before.rfind("sub rsp, 48") != std::string::npos);

    if (!hasAlignment) {
        evidence = "call without stack alignment";
        return true;
    }

    return false;
}

bool LocalReasoningEngine::checkShadowSpace(const std::string& asm) {
    // Win64 ABI: requires 32 bytes of shadow space before call
    // Look for "sub rsp, 32" (or larger) before call
    
    size_t callPos = asm.find("call");
    if (callPos == std::string::npos) return true;  // No call, no requirement

    std::string before = asm.substr(0, callPos);
    std::regex shadowPattern(R"(sub\s+rsp,\s*(\d+))");
    std::smatch match;

    if (std::regex_search(before, match, shadowPattern)) {
        int subAmount = std::stoi(match[1].str());
        return subAmount >= 32;  // Must be at least 32 bytes
    }

    return false;  // No sub rsp found
}

bool LocalReasoningEngine::checkRegisterPreservation(const std::string& asm, const std::vector<std::string>& nonVolatile) {
    // Check if non-volatile registers are pushed at start and popped at end
    for (const auto& reg : nonVolatile) {
        size_t modifyPos = asm.find(reg + ",");  // Modified if used as destination
        if (modifyPos == std::string::npos) continue;

        // Check for push at start
        std::string start = asm.substr(0, 100);
        bool hasPush = (start.find("push " + reg) != std::string::npos);

        // Check for pop at end
        std::string end = asm.substr(asm.length() > 100 ? asm.length() - 100 : 0);
        bool hasPop = (end.find("pop " + reg) != std::string::npos);

        if (!hasPush || !hasPop) {
            return false;  // Register modified without preservation
        }
    }

    return true;
}

bool LocalReasoningEngine::detectIneffientInstruction(const std::string& asm, std::string& evidence) {
    // Detect common inefficiencies
    
    // xor reg, reg instead of mov reg, 0 is better
    if (asm.find("mov ") != std::string::npos && asm.find(", 0") != std::string::npos) {
        size_t pos = asm.find(", 0");
        size_t start = asm.rfind("mov ", pos);
        if (start != std::string::npos) {
            size_t end = asm.find('\n', pos);
            evidence = asm.substr(start, end - start) + " → use xor instead";
            return true;
        }
    }

    // imul with power-of-2 should be shl
    std::regex imulPattern(R"(imul\s+\w+,\s*(\d+))");
    std::smatch match;
    if (std::regex_search(asm, match, imulPattern)) {
        int multiplier = std::stoi(match[1].str());
        if ((multiplier & (multiplier - 1)) == 0) {  // Power of 2
            evidence = match.str() + " → use shl instead";
            return true;
        }
    }

    return false;
}

// ════════════════════════════════════════════════════════════════════════
// CONTROL FLOW ANALYSIS (CFG construction)
// ════════════════════════════════════════════════════════════════════════

std::vector<LocalReasoningEngine::BasicBlock> LocalReasoningEngine::buildControlFlowGraph(const std::string& code) {
    //Simplified CFG: split on branches/returns
    std::vector<BasicBlock> cfg;
    BasicBlock currentBlock;
    currentBlock.id = 0;

    std::istringstream iss(code);
    std::string line;
    int blockId = 0;

    while (std::getline(iss, line)) {
        currentBlock.instructions.push_back(line);

        // Check for control flow changes
        if (line.find("if ") != std::string::npos ||
            line.find("else") != std::string::npos ||
            line.find("return") != std::string::npos ||
            line.find("goto") != std::string::npos ||
            line.find("break") != std::string::npos) {
            
            // End current block
            cfg.push_back(currentBlock);
            
            // Start new block
            currentBlock = BasicBlock();
            currentBlock.id = ++blockId;
        }
    }

    if (!currentBlock.instructions.empty()) {
        cfg.push_back(currentBlock);
    }

    // Build edges (simplified: sequential blocks are connected)
    for (size_t i = 0; i + 1 < cfg.size(); ++i) {
        cfg[i].successors.push_back(cfg[i + 1].id);
        cfg[i + 1].predecessors.push_back(cfg[i].id);
    }

    return cfg;
}

bool LocalReasoningEngine::detectInfiniteLoop(const std::vector<BasicBlock>& cfg) {
    // Simplified: look for back edges without exit conditions
    for (const auto& block : cfg) {
        for (int succId : block.successors) {
            if (succId <= block.id) {  // Back edge (loop)
                // Check if loop has exit
                bool hasBreak = false;
                for (const auto& instr : block.instructions) {
                    if (instr.find("break") != std::string::npos ||
                        instr.find("return") != std::string::npos) {
                        hasBreak = true;
                        break;
                    }
                }
                if (!hasBreak) return true;  // No obvious exit
            }
        }
    }
    return false;
}

bool LocalReasoningEngine::detectUnreachableCode(const std::vector<BasicBlock>& cfg) {
    // Simplified: blocks with no predecessors (except entry)
    for (size_t i = 1; i < cfg.size(); ++i) {
        if (cfg[i].predecessors.empty()) {
            return true;  // Unreachable block
        }
    }
    return false;
}

// ════════════════════════════════════════════════════════════════════════
// RULE-BASED EXPERT SYSTEM
// ════════════════════════════════════════════════════════════════════════

std::vector<LocalReasoningEngine::Rule> LocalReasoningEngine::loadDefaultRules() {
    std::vector<Rule> rules;

    // Combine all expert rules
    auto memRules = getMemorySafetyRules();
    auto threadRules = getThreadSafetyRules();
    auto secRules = getSecurityRules();
    auto perfRules = getPerformanceRules();

    rules.insert(rules.end(), memRules.begin(), memRules.end());
    rules.insert(rules.end(), threadRules.begin(), threadRules.end());
    rules.insert(rules.end(), secRules.begin(), secRules.end());
    rules.insert(rules.end(), perfRules.begin(), perfRules.end());

    return rules;
}

std::vector<LocalReasoningEngine::Rule> LocalReasoningEngine::getMemorySafetyRules() {
    return {
        {"dangling-pointer", "delete.*nullptr", "use-after-free", "high", "Set pointer to nullptr after delete", 0.80f},
        {"raw-pointer", "new \\w+\\[", "memory-leak", "medium", "Use std::vector or std::unique_ptr instead", 0.70f},
        {"manual-alloc", "malloc\\(", "memory-leak", "medium", "Prefer C++ containers over manual malloc", 0.75f},
    };
}

std::vector<LocalReasoningEngine::Rule> LocalReasoningEngine::getThreadSafetyRules() {
    return {
        {"unprotected-static", "static.*=", "race-condition", "high", "Protect static variables with std::mutex", 0.75f},
        {"missing-atomic", "\\+\\+.*;", "race-condition", "medium", "Use std::atomic for shared counters", 0.65f},
    };
}

std::vector<LocalReasoningEngine::Rule> LocalReasoningEngine::getSecurityRules() {
    return {
        {"unsafe-sprintf", "sprintf\\(", "buffer-overflow", "critical", "Use snprintf with buffer size limit", 0.95f},
        {"unsafe-strcpy", "strcpy\\(", "buffer-overflow", "critical", "Use strncpy or std::string", 0.95f},
    };
}

std::vector<LocalReasoningEngine::Rule> LocalReasoningEngine::getPerformanceRules() {
    return {
        {"pass-by-value", "void.*\\(std::string ", "performance", "low", "Pass std::string by const reference", 0.70f},
        {"virtual-loop", "for.*virtual", "performance", "medium", "Avoid virtual calls in hot loops", 0.60f},
    };
}

std::vector<LocalReasoningEngine::CodeIssue> LocalReasoningEngine::applyRules(const std::string& code, const std::vector<Rule>& rules) {
    std::vector<CodeIssue> issues;

    for (const auto& rule : rules) {
        if (matchesPattern(code, rule.pattern)) {
            CodeIssue issue;
            issue.issueType = rule.issueType;
            issue.severity = rule.severity;
            issue.description = "Rule '" + rule.name + "' triggered";
            issue.confidence = rule.confidence;
            issue.recommendation = rule.recommendation;
            issues.push_back(issue);
        }
    }

    return issues;
}

// ════════════════════════════════════════════════════════════════════════
// UTILITIES
// ════════════════════════════════════════════════════════════════════════

std::vector<LocalReasoningEngine::CodeIssue> LocalReasoningEngine::scanSyntaxStructure(
    const std::string& code, const std::string& language) {
    std::vector<CodeIssue> issues;

    // Basic brace matching
    int braceCount = 0;
    for (char c : code) {
        if (c == '{') braceCount++;
        if (c == '}') braceCount--;
        if (braceCount < 0) {
            CodeIssue issue;
            issue.issueType = "syntax-error";
            issue.severity = "critical";
            issue.description = "Unmatched closing brace";
            issue.confidence = 1.0f;
            issues.push_back(issue);
            break;
        }
    }
    if (braceCount > 0) {
        CodeIssue issue;
        issue.issueType = "syntax-error";
        issue.severity = "critical";
        issue.description = "Unmatched opening brace";
        issue.confidence = 1.0f;
        issues.push_back(issue);
    }

    return issues;
}

bool LocalReasoningEngine::matchesPattern(const std::string& code, const std::string& pattern) {
    try {
        std::regex re(pattern, std::regex::icase);
        return std::regex_search(code, re);
    } catch (...) {
        // Invalid regex, fall back to substring search
        return code.find(pattern) != std::string::npos;
    }
}

int LocalReasoningEngine::countOccurrences(const std::string& code, const std::string& substring) {
    int count = 0;
    size_t pos = 0;
    while ((pos = code.find(substring, pos)) != std::string::npos) {
        count++;
        pos += substring.length();
    }
    return count;
}

std::string LocalReasoningEngine::generateSummary(const std::vector<CodeIssue>& issues) {
    if (issues.empty()) {
        return "No issues detected. Code appears clean.";
    }

    int critical = 0, high = 0, medium = 0, low = 0;
    for (const auto& issue : issues) {
        if (issue.severity == "critical") critical++;
        else if (issue.severity == "high") high++;
        else if (issue.severity == "medium") medium++;
        else low++;
    }

    std::ostringstream summary;
    summary << "Found " << issues.size() << " issue(s): ";
    if (critical > 0) summary << critical << " critical, ";
    if (high > 0) summary << high << " high, ";
    if (medium > 0) summary << medium << " medium, ";
    if (low > 0) summary << low << " low.";

    return summary.str();
}

float LocalReasoningEngine::calculateOverallConfidence(const std::vector<CodeIssue>& issues) {
    if (issues.empty()) return 1.0f;

    float totalConf = 0.0f;
    for (const auto& issue : issues) {
        totalConf += issue.confidence;
    }
    return totalConf / issues.size();
}

void LocalReasoningEngine::cancelAnalysis() {
    m_analyzing = false;
    if (m_analysisThread.joinable()) {
        m_analysisThread.join();
    }
}

LocalReasoningEngine::AnalysisStats LocalReasoningEngine::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void LocalReasoningEngine::resetStats() {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    m_stats = AnalysisStats();
}

void LocalReasoningEngine::addCustomPattern(const std::string& name, const std::string& regex) {
    m_customPatterns[name] = regex;
}

void LocalReasoningEngine::clearKnowledgeBase() {
    m_customRules.clear();
    m_customPatterns.clear();
}
