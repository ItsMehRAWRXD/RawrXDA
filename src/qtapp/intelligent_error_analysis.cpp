#include "intelligent_error_analysis.h"
IntelligentErrorAnalysis::IntelligentErrorAnalysis()
    
    , m_analysisTimer(new // Timer(this))
{
    // Initialize error categories
    m_errorCategories["compilation"] = {
        "Compilation Error",
        "C\\d{4}|error:",
        "high",
        "Missing includes, syntax errors, type mismatches",
        "Add missing headers, fix syntax, correct types"
    };
    
    m_errorCategories["runtime"] = {
        "Runtime Error", 
        "exception|segmentation fault|access violation",
        "critical",
        "Null pointers, out of bounds, invalid operations",
        "Add null checks, bounds validation, error handling"
    };
    
    m_errorCategories["logic"] = {
        "Logic Error",
        "assertion failed|test failed",
        "medium",
        "Incorrect business logic, edge cases, algorithmic errors",
        "Review logic, add tests, improve algorithm"
    };
    
    m_errorCategories["warning"] = {
        "Warning",
        "warning:",
        "low",
        "Non-critical issues, deprecated features, style issues",
        "Address warnings, update deprecated code, improve style"
    };
    
    m_errorCategories["performance"] = {
        "Performance Issue",
        "slow|memory leak|inefficient",
        "medium",
        "Algorithm complexity, memory usage, I/O bottlenecks",
        "Optimize algorithms, reduce memory usage, improve I/O"
    };  // Signal connection removed\n}

IntelligentErrorAnalysis::~IntelligentErrorAnalysis() {
    delete m_analysisTimer;
}

nlohmann::json IntelligentErrorAnalysis::analyzeError(const std::string& errorText, const std::string& context) {
    
    nlohmann::json analysis;
    analysis["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    analysis["input_error"] = errorText;
    analysis["context"] = context;
    
    // Classify error
    std::string errorType = classifyError(errorText);
    analysis["error_type"] = errorType;
    analysis["category"] = m_errorCategories[errorType].name;
    
    // Parse error message
    nlohmann::json parsed = parseErrorMessage(errorText);
    analysis["parsed_error"] = parsed;
    
    // Identify root cause
    nlohmann::json rootCause = identifyRootCause(parsed);
    analysis["root_cause"] = rootCause;
    
    // Calculate confidence
    double confidence = calculateErrorConfidence(analysis);
    analysis["confidence"] = confidence;
    
    // Generate fix options
    nlohmann::json fixOptions = generateFixOptions(analysis);
    analysis["fix_options"] = fixOptions;
    
    // Store analysis
    std::string analysisId = QUuid::createUuid().toString();
    analysis["id"] = analysisId;
    
    errorAnalyzed(analysis);
    return analysis;
}

nlohmann::json IntelligentErrorAnalysis::diagnoseCompilationError(const std::string& compilerOutput) {
    
    nlohmann::json diagnosis;
    diagnosis["type"] = "compilation";
    diagnosis["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    diagnosis["output"] = compilerOutput;
    
    // Parse common compilation errors
    std::regex errorRegex(R"((\w+):(\d+):(\d+):\s*(error|warning):\s*(.+))");
    std::regexMatchIterator it = errorRegex;
    
    nlohmann::json errors;
    while (itfalse) {
        std::regexMatch match = it;
        nlohmann::json error;
        error["file"] = match"";
        error["line"] = match"";
        error["column"] = match"";
        error["severity"] = match"";
        error["message"] = match"";
        
        // Classify the error
        std::string msg = match"".toLower();
        std::string category = "general";
        if (msg.contains("undefined reference")) category = "linking";
        else if (msg.contains("no such file")) category = "file_not_found";
        else if (msg.contains("expected")) category = "syntax";
        else if (msg.contains("implicit")) category = "type_conversion";
        
        error["category"] = category;
        errors.append(error);
    }
    
    diagnosis["errors"] = errors;
    
    // Generate overall diagnosis
    std::string diagnosisSummary;
    if (errors.size() > 0) {
        diagnosisSummary = std::string("Found %1 compilation error(s). "));
        
        // Count error types
        std::map<std::string, int> errorCounts;
        for (const void*& errVal : errors) {
            nlohmann::json err = errVal.toObject();
            errorCounts[err["category"].toString()]++;
        }
        
        std::stringList topIssues;
        for (auto it = errorCounts.begin(); it != errorCounts.end(); ++it) {
            topIssues.append(std::string("%1 %2"))));
        }
        
        diagnosisSummary += "Most common issues: " + topIssues.join(", ");
    } else {
        diagnosisSummary = "No clear compilation errors detected in output.";
    }
    
    diagnosis["summary"] = diagnosisSummary;
    diagnosis["confidence"] = errors.size() > 0 ? 0.9 : 0.3;
    
    return diagnosis;
}

nlohmann::json IntelligentErrorAnalysis::diagnoseRuntimeError(const std::string& runtimeError, const std::string& stackTrace) {
    
    nlohmann::json diagnosis;
    diagnosis["type"] = "runtime";
    diagnosis["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    diagnosis["error"] = runtimeError;
    diagnosis["stack_trace"] = stackTrace;
    
    // Analyze error pattern
    std::string errorLower = runtimeError.toLower();
    std::string category = "general";
    
    if (errorLower.contains("null") || errorLower.contains("nullptr")) {
        category = "null_pointer";
    } else if (errorLower.contains("access") || errorLower.contains("segmentation")) {
        category = "memory_access";
    } else if (errorLower.contains("overflow") || errorLower.contains("underflow")) {
        category = "buffer_overflow";
    } else if (errorLower.contains("timeout") || errorLower.contains("hang")) {
        category = "timeout";
    } else if (errorLower.contains("resource") || errorLower.contains("memory")) {
        category = "resource_exhaustion";
    }
    
    diagnosis["category"] = category;
    
    // Analyze stack trace
    nlohmann::json stackFrames;
    std::stringList lines = stackTrace.split('\n');
    for (const std::string& line : lines) {
        if (line.trimmed().empty()) continue;
        
        nlohmann::json frame;
        frame["raw"] = line.trimmed();
        
        // Try to extract function and file info
        std::regex frameRegex(R"((.+?)!([^\s]+)\s*\((.+)\))");
        std::regexMatch match = frameRegex.match(line.trimmed());
        if (match.hasMatch()) {
            frame["module"] = match"";
            frame["function"] = match"";
            frame["location"] = match"";
        }
        
        stackFrames.append(frame);
    }
    
    diagnosis["stack_frames"] = stackFrames;
    
    // Generate diagnosis
    std::stringList suggestions;
    
    if (category == "null_pointer") {
        suggestions << "Add null checks before dereferencing pointers"
                  << "Use smart pointers where appropriate"
                  << "Initialize variables properly";
    } else if (category == "memory_access") {
        suggestions << "Check array bounds before access"
                  << "Validate memory allocation success"
                  << "Use memory debugging tools";
    } else if (category == "buffer_overflow") {
        suggestions << "Validate buffer sizes before copying"
                  << "Use safe string functions"
                  << "Implement proper bounds checking";
    } else if (category == "timeout") {
        suggestions << "Review algorithm efficiency"
                  << "Add timeout handling"
                  << "Consider parallelization";
    } else if (category == "resource_exhaustion") {
        suggestions << "Monitor resource usage"
                  << "Implement proper cleanup"
                  << "Add resource limits";
    }
    
    diagnosis["suggestions"] = nlohmann::json::fromStringList(suggestions);
    diagnosis["confidence"] = 0.85;
    
    return diagnosis;
}

nlohmann::json IntelligentErrorAnalysis::diagnoseLogicError(const std::string& code, const std::string& testFailure) {
    
    nlohmann::json diagnosis;
    diagnosis["type"] = "logic";
    diagnosis["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    diagnosis["code_snippet"] = code;
    diagnosis["test_failure"] = testFailure;
    
    // Analyze code for common logic issues
    std::stringList issues;
    std::string codeLower = code.toLower();
    
    if (codeLower.contains("==") && codeLower.contains("=")) {
        issues.append("Potential assignment instead of comparison");
    }
    if (codeLower.contains("if") && codeLower.contains("else") && !codeLower.contains("else if")) {
        issues.append("Missing else-if chain may cause logic gaps");
    }
    if (codeLower.contains("for") && !codeLower.contains("break") && !codeLower.contains("continue")) {
        issues.append("Loop may be infinite without break conditions");
    }
    if (codeLower.contains("try") && !codeLower.contains("catch")) {
        issues.append("Exception handling incomplete");
    }
    
    diagnosis["detected_issues"] = nlohmann::json::fromStringList(issues);
    
    // Analyze test failure
    std::stringList testSuggestions;
    std::string failureLower = testFailure.toLower();
    
    if (failureLower.contains("expected")) {
        testSuggestions << "Review expected vs actual values in test";
    }
    if (failureLower.contains("assertion")) {
        testSuggestions << "Check assertion conditions and inputs";
    }
    if (failureLower.contains("null") || failureLower.contains("none")) {
        testSuggestions << "Verify null handling and edge cases";
    }
    
    diagnosis["test_suggestions"] = nlohmann::json::fromStringList(testSuggestions);
    diagnosis["confidence"] = issues.size() > 0 ? 0.75 : 0.4;
    
    return diagnosis;
}

nlohmann::json IntelligentErrorAnalysis::generateFixOptions(const nlohmann::json& errorAnalysis) {
    
    nlohmann::json options;
    std::string errorType = errorAnalysis["error_type"].toString();
    nlohmann::json rootCause = errorAnalysis["root_cause"].toObject();
    
    if (errorType == "compilation") {
        // Compilation error fixes
        nlohmann::json fix1;
        fix1["title"] = "Check Missing Headers";
        fix1["description"] = "Add missing include directives";
        fix1["confidence"] = 0.8;
        fix1["code_change"] = "#include <missing_header.h>";
        options.append(fix1);
        
        nlohmann::json fix2;
        fix2["title"] = "Fix Syntax Error";
        fix2["description"] = "Correct syntax based on error location";
        fix2["confidence"] = 0.7;
        fix2["code_change"] = "// Review and fix syntax at error line";
        options.append(fix2);
        
        nlohmann::json fix3;
        fix3["title"] = "Resolve Type Mismatch";
        fix3["description"] = "Fix type conversion issues";
        fix3["confidence"] = 0.6;
        fix3["code_change"] = "// Cast or convert types appropriately";
        options.append(fix3);
        
    } else if (errorType == "runtime") {
        // Runtime error fixes
        nlohmann::json fix1;
        fix1["title"] = "Add Null Check";
        fix1["description"] = "Validate pointer before dereferencing";
        fix1["confidence"] = 0.85;
        fix1["code_change"] = "if (ptr != nullptr) { /* use ptr */ }";
        options.append(fix1);
        
        nlohmann::json fix2;
        fix2["title"] = "Bounds Checking";
        fix2["description"] = "Add array bounds validation";
        fix2["confidence"] = 0.8;
        fix2["code_change"] = "if (index >= 0 && index < size) { /* access array */ }";
        options.append(fix2);
        
        nlohmann::json fix3;
        fix3["title"] = "Error Handling";
        fix3["description"] = "Add proper error handling";
        fix3["confidence"] = 0.7;
        fix3["code_change"] = "try { /* risky operation */ } catch (...) { /* handle error */ }";
        options.append(fix3);
        
    } else if (errorType == "logic") {
        // Logic error fixes
        nlohmann::json fix1;
        fix1["title"] = "Review Algorithm";
        fix1["description"] = "Examine algorithm logic and edge cases";
        fix1["confidence"] = 0.6;
        fix1["code_change"] = "// Review algorithm implementation";
        options.append(fix1);
        
        nlohmann::json fix2;
        fix2["title"] = "Add Tests";
        fix2["description"] = "Create additional test cases";
        fix2["confidence"] = 0.7;
        fix2["code_change"] = "// Add test cases for edge scenarios";
        options.append(fix2);
        
    } else {
        // Generic fixes
        nlohmann::json fix1;
        fix1["title"] = "Manual Review";
        fix1["description"] = "Review error and code manually";
        fix1["confidence"] = 0.5;
        fix1["code_change"] = "// Manual investigation required";
        options.append(fix1);
    }
    
    // Sort by confidence (convert to std::vector<nlohmann::json> for proper sorting)
    std::vector<std::pair<double, nlohmann::json>> sortableOptions;
    for (const void*& val : options) {
        nlohmann::json obj = val.toObject();
        double confidence = obj["confidence"].toDouble();
        sortableOptions.append(qMakePair(confidence, obj));
    }
    
    // Sort by confidence (descending)
    std::sort(sortableOptions.begin(), sortableOptions.end(), 
              [](const std::pair<double, nlohmann::json>& a, const std::pair<double, nlohmann::json>& b) {
        return a.first > b.first;
    });
    
    // Convert back to nlohmann::json
    options = nlohmann::json();
    for (const auto& pair : sortableOptions) {
        options.append(pair.second);
    }
    
    return options;
}

nlohmann::json IntelligentErrorAnalysis::generateDetailedFix(const nlohmann::json& errorAnalysis, int fixIndex) {
    nlohmann::json options = errorAnalysis["fix_options"].toArray();
    if (fixIndex < 0 || fixIndex >= options.size()) {
        return nlohmann::json();
    }
    
    nlohmann::json baseFix = options[fixIndex].toObject();
    nlohmann::json detailedFix = baseFix;
    
    // Add detailed implementation
    std::string title = baseFix["title"].toString();
    std::string implementation;
    
    if (title == "Check Missing Headers") {
        implementation = "Review compiler error output for missing header files. Add appropriate #include directives at the top of the source file.";
    } else if (title == "Add Null Check") {
        implementation = "Before dereferencing any pointer, check if it's not null. This prevents segmentation faults and undefined behavior.";
    } else if (title == "Bounds Checking") {
        implementation = "Always validate array indices before access. Check that the index is within the valid range [0, size-1].";
    } else if (title == "Fix Syntax Error") {
        implementation = "Review the error location in the source code. Check for missing semicolons, brackets, or incorrect syntax.";
    } else {
        implementation = "This fix requires manual review and implementation based on the specific error context.";
    }
    
    detailedFix["implementation"] = implementation;
    detailedFix["estimated_effort"] = "15-30 minutes";
    detailedFix["risk_level"] = (baseFix["confidence"].toDouble() > 0.8) ? "low" : "medium";
    
    return detailedFix;
}

std::string IntelligentErrorAnalysis::classifyError(const std::string& errorText) {
    std::string lowerText = errorText.toLower();
    
    // Check against known patterns
    for (auto it = m_errorCategories.begin(); it != m_errorCategories.end(); ++it) {
        std::string category = it.key();
        std::string pattern = it.value().pattern;
        
        std::regex regex(pattern, std::regex::CaseInsensitiveOption);
        if (regex.match(lowerText).hasMatch()) {
            return category;
        }
    }
    
    return "unknown";
}

double IntelligentErrorAnalysis::calculateErrorConfidence(const nlohmann::json& errorAnalysis) {
    double confidence = 0.5; // Base confidence
    
    std::string errorType = errorAnalysis["error_type"].toString();
    nlohmann::json parsed = errorAnalysis["parsed_error"].toObject();
    nlohmann::json rootCause = errorAnalysis["root_cause"].toObject();
    
    // Boost confidence based on clear patterns
    if (errorType != "unknown") {
        confidence += 0.2;
    }
    
    if (!parsed["file"].toString().empty()) {
        confidence += 0.1;
    }
    
    if (!parsed["line"].toString().empty()) {
        confidence += 0.1;
    }
    
    if (rootCause.contains("likely_cause")) {
        confidence += 0.1;
    }
    
    return qMin(1.0, confidence);
}

nlohmann::json IntelligentErrorAnalysis::parseErrorMessage(const std::string& errorText) {
    nlohmann::json parsed;
    
    // Try to extract file and line information
    std::regex fileLineRegex(R"((\w+):(\d+):(\d+):\s*(error|warning):\s*(.+))");
    std::regexMatch match = fileLineRegex.match(errorText);
    
    if (match.hasMatch()) {
        parsed["file"] = match"";
        parsed["line"] = match"";
        parsed["column"] = match"";
        parsed["severity"] = match"";
        parsed["message"] = match"";
    } else {
        parsed["message"] = errorText;
        parsed["severity"] = "unknown";
    }
    
    return parsed;
}

nlohmann::json IntelligentErrorAnalysis::identifyRootCause(const nlohmann::json& parsedError) {
    nlohmann::json rootCause;
    
    std::string message = parsedError["message"].toString().toLower();
    std::string severity = parsedError["severity"].toString();
    
    std::string likelyCause;
    std::string fixStrategy;
    
    if (message.contains("undefined reference")) {
        likelyCause = "Missing function implementation or library linkage";
        fixStrategy = "Add function definition or link required library";
    } else if (message.contains("no such file")) {
        likelyCause = "Missing header file or incorrect include path";
        fixStrategy = "Add correct include directive or adjust include paths";
    } else if (message.contains("expected")) {
        likelyCause = "Syntax error or incorrect token";
        fixStrategy = "Review syntax at error location and correct";
    } else if (message.contains("implicit")) {
        likelyCause = "Implicit declaration or type conversion";
        fixStrategy = "Add explicit declaration or cast";
    } else if (severity == "error") {
        likelyCause = "Code does not compile";
        fixStrategy = "Review error message and fix compilation issue";
    } else {
        likelyCause = "Unknown error pattern";
        fixStrategy = "Manual investigation required";
    }
    
    rootCause["likely_cause"] = likelyCause;
    rootCause["fix_strategy"] = fixStrategy;
    rootCause["complexity"] = (severity == "error") ? "high" : "medium";
    
    return rootCause;
}

void IntelligentErrorAnalysis::learnFromFix(const std::string& errorType, const std::string& appliedFix, bool success) {
    
    // Update success rate for this error type
    std::string key = std::string("%1:%2");
    int currentRate = m_fixSuccessRates[key];
    m_fixSuccessRates[key] = (currentRate + (success ? 1 : 0));
    
    // Add to fix history
    FixAttempt attempt;
    attempt.errorId = QUuid::createUuid().toString();
    attempt.errorType = errorType;
    attempt.appliedFix = appliedFix;
    attempt.success = success;
    attempt.timestamp = // DateTime::currentDateTime().toMSecsSinceEpoch();
    
    m_fixHistory.append(attempt);
    
    // pattern learned signal
    if (success) {
        errorPatternLearned(errorType, appliedFix);
    }
}

void IntelligentErrorAnalysis::processBuildError(const std::string& compiler, const std::string& output) {
    nlohmann::json analysis = analyzeError(output, std::string("Build error from %1"));
    analysisComplete(QUuid::createUuid().toString(), analysis);
}

void IntelligentErrorAnalysis::processRuntimeError(const std::string& errorType, const std::string& message, const std::string& stackTrace) {
    nlohmann::json diagnosis = diagnoseRuntimeError(message, stackTrace);
    nlohmann::json analysis;
    analysis["error_type"] = "runtime";
    analysis["diagnosis"] = diagnosis;
    analysisComplete(QUuid::createUuid().toString(), analysis);
}

void IntelligentErrorAnalysis::processTestFailure(const std::string& testName, const std::string& failureMessage) {
    nlohmann::json diagnosis = diagnoseLogicError("", failureMessage);
    nlohmann::json analysis;
    analysis["error_type"] = "logic";
    analysis["diagnosis"] = diagnosis;
    analysis["test_name"] = testName;
    analysisComplete(QUuid::createUuid().toString(), analysis);
}

void IntelligentErrorAnalysis::processCodeAnalysisWarning(const std::string& file, const std::string& line, const std::string& warning) {
    nlohmann::json analysis = analyzeError(warning, std::string("Code analysis warning in %1:%2"));
    analysis["file"] = file;
    analysis["line"] = line;
    analysis["type"] = "warning";
    analysisComplete(QUuid::createUuid().toString(), analysis);
}

// Slot implementation
void IntelligentErrorAnalysis::onAutoRefresh() {
    // Automatically refresh error analysis
    // This will be called periodically to re-analyze for new errors
}

