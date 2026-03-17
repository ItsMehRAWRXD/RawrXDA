#include "intelligent_error_analysis.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QRegularExpression>
#include <QDateTime>

IntelligentErrorAnalysis::IntelligentErrorAnalysis(QObject* parent)
    : QObject(parent)
    , m_analysisTimer(new QTimer(this))
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
    };
    
    connect(m_analysisTimer, &QTimer::timeout, this, &IntelligentErrorAnalysis::onAutoRefresh);
}

IntelligentErrorAnalysis::~IntelligentErrorAnalysis() {
    delete m_analysisTimer;
}

QJsonObject IntelligentErrorAnalysis::analyzeError(const QString& errorText, const QString& context) {
    qDebug() << "[IntelligentErrorAnalysis] Analyzing error:" << errorText.left(100);
    
    QJsonObject analysis;
    analysis["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    analysis["input_error"] = errorText;
    analysis["context"] = context;
    
    // Classify error
    QString errorType = classifyError(errorText);
    analysis["error_type"] = errorType;
    analysis["category"] = m_errorCategories[errorType].name;
    
    // Parse error message
    QJsonObject parsed = parseErrorMessage(errorText);
    analysis["parsed_error"] = parsed;
    
    // Identify root cause
    QJsonObject rootCause = identifyRootCause(parsed);
    analysis["root_cause"] = rootCause;
    
    // Calculate confidence
    double confidence = calculateErrorConfidence(analysis);
    analysis["confidence"] = confidence;
    
    // Generate fix options
    QJsonArray fixOptions = generateFixOptions(analysis);
    analysis["fix_options"] = fixOptions;
    
    // Store analysis
    QString analysisId = QUuid::createUuid().toString();
    analysis["id"] = analysisId;
    
    emit errorAnalyzed(analysis);
    return analysis;
}

QJsonObject IntelligentErrorAnalysis::diagnoseCompilationError(const QString& compilerOutput) {
    qDebug() << "[IntelligentErrorAnalysis] Diagnosing compilation error";
    
    QJsonObject diagnosis;
    diagnosis["type"] = "compilation";
    diagnosis["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    diagnosis["output"] = compilerOutput;
    
    // Parse common compilation errors
    QRegularExpression errorRegex(R"((\w+):(\d+):(\d+):\s*(error|warning):\s*(.+))");
    QRegularExpressionMatchIterator it = errorRegex.globalMatch(compilerOutput);
    
    QJsonArray errors;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QJsonObject error;
        error["file"] = match.captured(1);
        error["line"] = match.captured(2).toInt();
        error["column"] = match.captured(3).toInt();
        error["severity"] = match.captured(4);
        error["message"] = match.captured(5);
        
        // Classify the error
        QString msg = match.captured(5).toLower();
        QString category = "general";
        if (msg.contains("undefined reference")) category = "linking";
        else if (msg.contains("no such file")) category = "file_not_found";
        else if (msg.contains("expected")) category = "syntax";
        else if (msg.contains("implicit")) category = "type_conversion";
        
        error["category"] = category;
        errors.append(error);
    }
    
    diagnosis["errors"] = errors;
    
    // Generate overall diagnosis
    QString diagnosisSummary;
    if (errors.size() > 0) {
        diagnosisSummary = QString("Found %1 compilation error(s). ").arg(errors.size());
        
        // Count error types
        QMap<QString, int> errorCounts;
        for (const QJsonValue& errVal : errors) {
            QJsonObject err = errVal.toObject();
            errorCounts[err["category"].toString()]++;
        }
        
        QStringList topIssues;
        for (auto it = errorCounts.begin(); it != errorCounts.end(); ++it) {
            topIssues.append(QString("%1 %2").arg(it.value()).arg(it.key()));
        }
        
        diagnosisSummary += "Most common issues: " + topIssues.join(", ");
    } else {
        diagnosisSummary = "No clear compilation errors detected in output.";
    }
    
    diagnosis["summary"] = diagnosisSummary;
    diagnosis["confidence"] = errors.size() > 0 ? 0.9 : 0.3;
    
    return diagnosis;
}

QJsonObject IntelligentErrorAnalysis::diagnoseRuntimeError(const QString& runtimeError, const QString& stackTrace) {
    qDebug() << "[IntelligentErrorAnalysis] Diagnosing runtime error";
    
    QJsonObject diagnosis;
    diagnosis["type"] = "runtime";
    diagnosis["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    diagnosis["error"] = runtimeError;
    diagnosis["stack_trace"] = stackTrace;
    
    // Analyze error pattern
    QString errorLower = runtimeError.toLower();
    QString category = "general";
    
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
    QJsonArray stackFrames;
    QStringList lines = stackTrace.split('\n');
    for (const QString& line : lines) {
        if (line.trimmed().isEmpty()) continue;
        
        QJsonObject frame;
        frame["raw"] = line.trimmed();
        
        // Try to extract function and file info
        QRegularExpression frameRegex(R"((.+?)!([^\s]+)\s*\((.+)\))");
        QRegularExpressionMatch match = frameRegex.match(line.trimmed());
        if (match.hasMatch()) {
            frame["module"] = match.captured(1);
            frame["function"] = match.captured(2);
            frame["location"] = match.captured(3);
        }
        
        stackFrames.append(frame);
    }
    
    diagnosis["stack_frames"] = stackFrames;
    
    // Generate diagnosis
    QStringList suggestions;
    
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
    
    diagnosis["suggestions"] = QJsonArray::fromStringList(suggestions);
    diagnosis["confidence"] = 0.85;
    
    return diagnosis;
}

QJsonObject IntelligentErrorAnalysis::diagnoseLogicError(const QString& code, const QString& testFailure) {
    qDebug() << "[IntelligentErrorAnalysis] Diagnosing logic error";
    
    QJsonObject diagnosis;
    diagnosis["type"] = "logic";
    diagnosis["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    diagnosis["code_snippet"] = code;
    diagnosis["test_failure"] = testFailure;
    
    // Analyze code for common logic issues
    QStringList issues;
    QString codeLower = code.toLower();
    
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
    
    diagnosis["detected_issues"] = QJsonArray::fromStringList(issues);
    
    // Analyze test failure
    QStringList testSuggestions;
    QString failureLower = testFailure.toLower();
    
    if (failureLower.contains("expected")) {
        testSuggestions << "Review expected vs actual values in test";
    }
    if (failureLower.contains("assertion")) {
        testSuggestions << "Check assertion conditions and inputs";
    }
    if (failureLower.contains("null") || failureLower.contains("none")) {
        testSuggestions << "Verify null handling and edge cases";
    }
    
    diagnosis["test_suggestions"] = QJsonArray::fromStringList(testSuggestions);
    diagnosis["confidence"] = issues.size() > 0 ? 0.75 : 0.4;
    
    return diagnosis;
}

QJsonArray IntelligentErrorAnalysis::generateFixOptions(const QJsonObject& errorAnalysis) {
    qDebug() << "[IntelligentErrorAnalysis] Generating fix options";
    
    QJsonArray options;
    QString errorType = errorAnalysis["error_type"].toString();
    QJsonObject rootCause = errorAnalysis["root_cause"].toObject();
    
    if (errorType == "compilation") {
        // Compilation error fixes
        QJsonObject fix1;
        fix1["title"] = "Check Missing Headers";
        fix1["description"] = "Add missing include directives";
        fix1["confidence"] = 0.8;
        fix1["code_change"] = "#include <missing_header.h>";
        options.append(fix1);
        
        QJsonObject fix2;
        fix2["title"] = "Fix Syntax Error";
        fix2["description"] = "Correct syntax based on error location";
        fix2["confidence"] = 0.7;
        fix2["code_change"] = "// Review and fix syntax at error line";
        options.append(fix2);
        
        QJsonObject fix3;
        fix3["title"] = "Resolve Type Mismatch";
        fix3["description"] = "Fix type conversion issues";
        fix3["confidence"] = 0.6;
        fix3["code_change"] = "// Cast or convert types appropriately";
        options.append(fix3);
        
    } else if (errorType == "runtime") {
        // Runtime error fixes
        QJsonObject fix1;
        fix1["title"] = "Add Null Check";
        fix1["description"] = "Validate pointer before dereferencing";
        fix1["confidence"] = 0.85;
        fix1["code_change"] = "if (ptr != nullptr) { /* use ptr */ }";
        options.append(fix1);
        
        QJsonObject fix2;
        fix2["title"] = "Bounds Checking";
        fix2["description"] = "Add array bounds validation";
        fix2["confidence"] = 0.8;
        fix2["code_change"] = "if (index >= 0 && index < size) { /* access array */ }";
        options.append(fix2);
        
        QJsonObject fix3;
        fix3["title"] = "Error Handling";
        fix3["description"] = "Add proper error handling";
        fix3["confidence"] = 0.7;
        fix3["code_change"] = "try { /* risky operation */ } catch (...) { /* handle error */ }";
        options.append(fix3);
        
    } else if (errorType == "logic") {
        // Logic error fixes
        QJsonObject fix1;
        fix1["title"] = "Review Algorithm";
        fix1["description"] = "Examine algorithm logic and edge cases";
        fix1["confidence"] = 0.6;
        fix1["code_change"] = "// Review algorithm implementation";
        options.append(fix1);
        
        QJsonObject fix2;
        fix2["title"] = "Add Tests";
        fix2["description"] = "Create additional test cases";
        fix2["confidence"] = 0.7;
        fix2["code_change"] = "// Add test cases for edge scenarios";
        options.append(fix2);
        
    } else {
        // Generic fixes
        QJsonObject fix1;
        fix1["title"] = "Manual Review";
        fix1["description"] = "Review error and code manually";
        fix1["confidence"] = 0.5;
        fix1["code_change"] = "// Manual investigation required";
        options.append(fix1);
    }
    
    // Sort by confidence (convert to QVector<QJsonObject> for proper sorting)
    QList<QPair<double, QJsonObject>> sortableOptions;
    for (const QJsonValue& val : options) {
        QJsonObject obj = val.toObject();
        double confidence = obj["confidence"].toDouble();
        sortableOptions.append(qMakePair(confidence, obj));
    }
    
    // Sort by confidence (descending)
    std::sort(sortableOptions.begin(), sortableOptions.end(), 
              [](const QPair<double, QJsonObject>& a, const QPair<double, QJsonObject>& b) {
        return a.first > b.first;
    });
    
    // Convert back to QJsonArray
    options = QJsonArray();
    for (const auto& pair : sortableOptions) {
        options.append(pair.second);
    }
    
    return options;
}

QJsonObject IntelligentErrorAnalysis::generateDetailedFix(const QJsonObject& errorAnalysis, int fixIndex) {
    QJsonArray options = errorAnalysis["fix_options"].toArray();
    if (fixIndex < 0 || fixIndex >= options.size()) {
        return QJsonObject();
    }
    
    QJsonObject baseFix = options[fixIndex].toObject();
    QJsonObject detailedFix = baseFix;
    
    // Add detailed implementation
    QString title = baseFix["title"].toString();
    QString implementation;
    
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

QString IntelligentErrorAnalysis::classifyError(const QString& errorText) {
    QString lowerText = errorText.toLower();
    
    // Check against known patterns
    for (auto it = m_errorCategories.begin(); it != m_errorCategories.end(); ++it) {
        QString category = it.key();
        QString pattern = it.value().pattern;
        
        QRegularExpression regex(pattern, QRegularExpression::CaseInsensitiveOption);
        if (regex.match(lowerText).hasMatch()) {
            return category;
        }
    }
    
    return "unknown";
}

double IntelligentErrorAnalysis::calculateErrorConfidence(const QJsonObject& errorAnalysis) {
    double confidence = 0.5; // Base confidence
    
    QString errorType = errorAnalysis["error_type"].toString();
    QJsonObject parsed = errorAnalysis["parsed_error"].toObject();
    QJsonObject rootCause = errorAnalysis["root_cause"].toObject();
    
    // Boost confidence based on clear patterns
    if (errorType != "unknown") {
        confidence += 0.2;
    }
    
    if (!parsed["file"].toString().isEmpty()) {
        confidence += 0.1;
    }
    
    if (!parsed["line"].toString().isEmpty()) {
        confidence += 0.1;
    }
    
    if (rootCause.contains("likely_cause")) {
        confidence += 0.1;
    }
    
    return qMin(1.0, confidence);
}

QJsonObject IntelligentErrorAnalysis::parseErrorMessage(const QString& errorText) {
    QJsonObject parsed;
    
    // Try to extract file and line information
    QRegularExpression fileLineRegex(R"((\w+):(\d+):(\d+):\s*(error|warning):\s*(.+))");
    QRegularExpressionMatch match = fileLineRegex.match(errorText);
    
    if (match.hasMatch()) {
        parsed["file"] = match.captured(1);
        parsed["line"] = match.captured(2).toInt();
        parsed["column"] = match.captured(3).toInt();
        parsed["severity"] = match.captured(4);
        parsed["message"] = match.captured(5);
    } else {
        parsed["message"] = errorText;
        parsed["severity"] = "unknown";
    }
    
    return parsed;
}

QJsonObject IntelligentErrorAnalysis::identifyRootCause(const QJsonObject& parsedError) {
    QJsonObject rootCause;
    
    QString message = parsedError["message"].toString().toLower();
    QString severity = parsedError["severity"].toString();
    
    QString likelyCause;
    QString fixStrategy;
    
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

void IntelligentErrorAnalysis::learnFromFix(const QString& errorType, const QString& appliedFix, bool success) {
    qDebug() << "[IntelligentErrorAnalysis] Learning from fix:" << errorType << "success:" << success;
    
    // Update success rate for this error type
    QString key = QString("%1:%2").arg(errorType).arg(appliedFix);
    int currentRate = m_fixSuccessRates[key];
    m_fixSuccessRates[key] = (currentRate + (success ? 1 : 0));
    
    // Add to fix history
    FixAttempt attempt;
    attempt.errorId = QUuid::createUuid().toString();
    attempt.errorType = errorType;
    attempt.appliedFix = appliedFix;
    attempt.success = success;
    attempt.timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
    
    m_fixHistory.append(attempt);
    
    // Emit pattern learned signal
    if (success) {
        emit errorPatternLearned(errorType, appliedFix);
    }
}

void IntelligentErrorAnalysis::processBuildError(const QString& compiler, const QString& output) {
    QJsonObject analysis = analyzeError(output, QString("Build error from %1").arg(compiler));
    emit analysisComplete(QUuid::createUuid().toString(), analysis);
}

void IntelligentErrorAnalysis::processRuntimeError(const QString& errorType, const QString& message, const QString& stackTrace) {
    QJsonObject diagnosis = diagnoseRuntimeError(message, stackTrace);
    QJsonObject analysis;
    analysis["error_type"] = "runtime";
    analysis["diagnosis"] = diagnosis;
    emit analysisComplete(QUuid::createUuid().toString(), analysis);
}

void IntelligentErrorAnalysis::processTestFailure(const QString& testName, const QString& failureMessage) {
    QJsonObject diagnosis = diagnoseLogicError("", failureMessage);
    QJsonObject analysis;
    analysis["error_type"] = "logic";
    analysis["diagnosis"] = diagnosis;
    analysis["test_name"] = testName;
    emit analysisComplete(QUuid::createUuid().toString(), analysis);
}

void IntelligentErrorAnalysis::processCodeAnalysisWarning(const QString& file, const QString& line, const QString& warning) {
    QJsonObject analysis = analyzeError(warning, QString("Code analysis warning in %1:%2").arg(file).arg(line));
    analysis["file"] = file;
    analysis["line"] = line;
    analysis["type"] = "warning";
    emit analysisComplete(QUuid::createUuid().toString(), analysis);
}
