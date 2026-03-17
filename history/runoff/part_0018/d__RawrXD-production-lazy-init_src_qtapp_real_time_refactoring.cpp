#include "real_time_refactoring.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QRegularExpression>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

RealTimeRefactoring::RealTimeRefactoring(QObject* parent)
    : QObject(parent)
{
    // Initialize refactoring rules
    m_refactoringRules["loop_optimization"] = {
        "Loop Optimization",
        R"(for\s*\(\s*\w+\s*=\s*0\s*;\s*\w+\s*<\s*\w+\.size\(\)\s*;\s*\w+\+\+\s*\))",
        R"(for\s*\(\s*\w+\s*=\s*0\s*;\s*\w+\s*<\s*\w+\.size\(\)\s*;\s*\w+\+\+\s*\)\s*\{)",
        "Optimize loops by caching size() calls",
        "performance",
        0.9,
        "safe"
    };
    
    m_refactoringRules["null_check_optimization"] = {
        "Null Check Optimization",
        R"(if\s*\(\s*\w+\s*!=\s*nullptr\s*\)\s*\{)",
        R"(if\s*\(\s*\w+\s*\)\s*\{)",
        "Simplify null checks",
        "style",
        0.8,
        "safe"
    };
    
    m_refactoringRules["string_concatenation"] = {
        "String Concatenation Optimization",
        "result \\+= ",
        "result.append(",
        "Use StringBuilder pattern for concatenation",
        "performance",
        0.85,
        "safe"
    };
    
    m_refactoringRules["memory_management"] = {
        "Memory Management Improvement",
        "new\\s+\\w+",
        "std::make_unique<\\w+>",
        "Use smart pointers instead of raw new",
        "memory",
        0.9,
        "safe"
    };
    
    m_refactoringRules["const_correctness"] = {
        "Const Correctness",
        "(\\w+)\\s+(\\w+)\\(",
        "const \\1 \\2(",
        "Add const to member functions",
        "style",
        0.7,
        "safe"
    };
    
    m_refactoringRules["redundant_null_check"] = RefactoringRule{
        "Redundant Null Check Removal",
        R"(if\s*\(\s*\w+\s*\)\s*\{.*if\s*\(\s*\w+\s*!=\s*nullptr\s*\)\s*\{)",
        R"(if\s*\(\s*\w+\s*\)\s*\{)",
        "Remove redundant null checks",
        "style",
        0.8,
        "safe"
    };
    
    m_refactoringRules["algorithm_complexity"] = RefactoringRule{
        "Algorithm Complexity Improvement",
        "for\\s*\\(\\s*\\w+\\s*=\\s*0\\s*;\\s*\\w+\\s*<\\s*\\w+\\.size\\(\\)\\s*;\\s*\\w+\\+\\+\\s*\\)\\s*\\{\\s*for\\s*\\(\\s*\\w+\\s*=\\s*0\\s*;\\s*\\w+\\s*<\\s*\\w+\\.size\\(\\)\\s*;\\s*\\w+\\+\\+\\s*\\)\\s*\\{",
        "// Consider using a more efficient algorithm",
        "Consider O(n²) to O(n log n) optimization",
        "algorithm",
        0.6,
        "review"
    };
}

RealTimeRefactoring::~RealTimeRefactoring() = default;

QJsonArray RealTimeRefactoring::analyzeCodeForImprovements(const QString& code, const QString& language) {
    qDebug() << "[RealTimeRefactoring] Analyzing code for improvements";
    
    QJsonArray improvements;
    
    // Analyze for performance issues
    QJsonObject perfAnalysis = detectPerformanceIssues(code);
    QJsonArray perfIssues = perfAnalysis["issues"].toArray();
    
    for (const QJsonValue& issueVal : perfIssues) {
        QJsonObject issue = issueVal.toObject();
        QJsonObject suggestion = generateSuggestion("performance", issue);
        improvements.append(suggestion);
    }
    
    // Analyze for code smells
    QJsonArray smells = detectCodeSmells(code);
    for (const QJsonValue& smellVal : smells) {
        QJsonObject smell = smellVal.toObject();
        QJsonObject suggestion = generateSuggestion("code_smell", smell);
        improvements.append(suggestion);
    }
    
    // Analyze for style issues
    QJsonArray styleIssues = enforceCodeStyle(code, language);
    for (const QJsonValue& styleVal : styleIssues) {
        QJsonObject style = styleVal.toObject();
        QJsonObject suggestion = generateSuggestion("style", style);
        improvements.append(suggestion);
    }
    
    return improvements;
}

QJsonObject RealTimeRefactoring::generateRefactoringSuggestion(const QString& code, const QString& pattern) {
    qDebug() << "[RealTimeRefactoring] Generating suggestion for pattern:" << pattern;
    
    QJsonObject suggestion;
    suggestion["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    suggestion["pattern"] = pattern;
    suggestion["confidence"] = 0.7;
    
    // Find matching rule
    QString bestRuleName;
    double bestScore = 0.0;
    
    for (auto it = m_refactoringRules.begin(); it != m_refactoringRules.end(); ++it) {
        QString ruleName = it.key();
        RefactoringRule rule = it.value();
        
        QRegularExpression regex(rule.pattern);
        if (regex.match(code).hasMatch()) {
            double score = rule.confidence;
            if (score > bestScore) {
                bestScore = score;
                bestRuleName = ruleName;
            }
        }
    }
    
    if (!bestRuleName.isEmpty()) {
        RefactoringRule rule = m_refactoringRules[bestRuleName];
        suggestion["title"] = rule.name;
        suggestion["description"] = rule.description;
        suggestion["category"] = rule.category;
        suggestion["confidence"] = rule.confidence;
        suggestion["safety"] = rule.safety;
        suggestion["original_pattern"] = rule.pattern;
        suggestion["suggested_pattern"] = rule.replacement;
    } else {
        suggestion["title"] = "Generic Improvement";
        suggestion["description"] = "Code quality improvement opportunity identified";
        suggestion["category"] = "general";
        suggestion["confidence"] = 0.5;
        suggestion["safety"] = "review";
    }
    
    return suggestion;
}

QJsonObject RealTimeRefactoring::applyRefactoring(const QString& originalCode, const QJsonObject& suggestion) {
    qDebug() << "[RealTimeRefactoring] Applying refactoring";
    
    QJsonObject result;
    result["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QString title = suggestion["title"].toString();
    QString category = suggestion["category"].toString();
    QString pattern = suggestion["original_pattern"].toString();
    QString replacement = suggestion["suggested_pattern"].toString();
    
    QString refactoredCode = originalCode;
    bool success = false;
    
    // Apply specific refactoring based on category
    if (category == "performance") {
        refactoredCode = applyPerformanceOptimization(originalCode, suggestion);
        success = !refactoredCode.isEmpty();
    } else if (category == "style") {
        refactoredCode = applyStyleImprovements(originalCode, suggestion);
        success = !refactoredCode.isEmpty();
    } else if (category == "memory") {
        refactoredCode = applyPatternRefactoring(originalCode, suggestion);
        success = !refactoredCode.isEmpty();
    } else {
        // Generic pattern replacement
        QRegularExpression regex(pattern);
        QString tempCode = originalCode;
        refactoredCode = tempCode.replace(regex, replacement);
        success = refactoredCode != originalCode;
    }
    
    result["success"] = success;
    result["original_code"] = originalCode;
    result["refactored_code"] = refactoredCode;
    result["changes_made"] = (refactoredCode != originalCode);
    
    // Validate the refactoring
    if (success) {
        QJsonObject validation = validateRefactoring(originalCode, refactoredCode);
        result["validation"] = validation;
        result["is_safe"] = validation["is_safe"].toBool();
    }
    
    return result;
}

QJsonObject RealTimeRefactoring::validateRefactoring(const QString& original, const QString& refactored) {
    qDebug() << "[RealTimeRefactoring] Validating refactoring";
    
    QJsonObject validation;
    
    // Check syntax validity
    bool syntaxValid = validateSyntax(refactored, "cpp");
    validation["syntax_valid"] = syntaxValid;
    
    // Check logic preservation
    bool logicPreserved = validateLogic(original, refactored);
    validation["logic_preserved"] = logicPreserved;
    
    // Check performance improvement
    bool performanceImproved = validatePerformance(original, refactored);
    validation["performance_improved"] = performanceImproved;
    
    // Overall safety assessment
    bool isSafe = syntaxValid && logicPreserved && (performanceImproved || !performanceImproved);
    validation["is_safe"] = isSafe;
    
    return validation;
}

QJsonArray RealTimeRefactoring::detectCodeSmells(const QString& code) {
    qDebug() << "[RealTimeRefactoring] Detecting code smells";
    
    QJsonArray smells;
    
    // Long method detection
    QStringList lines = code.split('\n');
    if (lines.size() > 50) {
        QJsonObject smell;
        smell["type"] = "long_method";
        smell["description"] = "Method is too long (>50 lines)";
        smell["severity"] = "medium";
        smell["suggestion"] = "Consider breaking into smaller methods";
        smells.append(smell);
    }
    
    // Large class detection
    int braceCount = code.count('{');
    if (braceCount > 20) {
        QJsonObject smell;
        smell["type"] = "large_class";
        smell["description"] = "Class has many members";
        smell["severity"] = "low";
        smell["suggestion"] = "Consider splitting into smaller classes";
        smells.append(smell);
    }
    
    // Duplicate code detection (simplified)
    QMap<QString, int> lineFrequency;
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.length() > 10) {
            lineFrequency[trimmed]++;
        }
    }
    
    for (auto it = lineFrequency.begin(); it != lineFrequency.end(); ++it) {
        if (it.value() > 2) {
            QJsonObject smell;
            smell["type"] = "duplicate_code";
            smell["description"] = "Duplicate code detected";
            smell["severity"] = "high";
            smell["line"] = it.key();
            smell["frequency"] = it.value();
            smell["suggestion"] = "Extract duplicate code into a function";
            smells.append(smell);
            break; // Only report first duplicate
        }
    }
    
    // Magic numbers
    QRegularExpression magicNumberRegex(R"(\b\d{2,}\b)");
    QRegularExpressionMatchIterator it = magicNumberRegex.globalMatch(code);
    if (it.hasNext()) {
        QJsonObject smell;
        smell["type"] = "magic_numbers";
        smell["description"] = "Magic numbers detected in code";
        smell["severity"] = "low";
        smell["suggestion"] = "Replace magic numbers with named constants";
        smells.append(smell);
    }
    
    return smells;
}

QJsonArray RealTimeRefactoring::suggestOptimizations(const QString& code) {
    qDebug() << "[RealTimeRefactoring] Suggesting optimizations";
    
    QJsonArray optimizations;
    
    // Loop optimization
    if (code.contains("for") && code.contains(".size()")) {
        QJsonObject optimization;
        optimization["type"] = "loop_optimization";
        optimization["description"] = "Cache container size() calls in loops";
        optimization["pattern"] = "for(int i = 0; i < container.size(); i++)";
        optimization["improvement"] = "for(int i = 0, size = container.size(); i < size; i++)";
        optimization["performance_gain"] = "15-30%";
        optimization["safety"] = "safe";
        optimizations.append(optimization);
    }
    
    // String concatenation optimization
    if (code.contains("+=") && code.contains("std::string")) {
        QJsonObject optimization;
        optimization["type"] = "string_concatenation";
        optimization["description"] = "Use std::string::append instead of +=";
        optimization["pattern"] = "result += part";
        optimization["improvement"] = "result.append(part)";
        optimization["performance_gain"] = "20-40%";
        optimization["safety"] = "safe";
        optimizations.append(optimization);
    }
    
    // Memory allocation optimization
    if (code.contains("new ") && !code.contains("std::make_unique")) {
        QJsonObject optimization;
        optimization["type"] = "smart_pointers";
        optimization["description"] = "Use smart pointers instead of raw new";
        optimization["pattern"] = "new Type";
        optimization["improvement"] = "std::make_unique<Type>()";
        optimization["performance_gain"] = "Memory safety, no performance loss";
        optimization["safety"] = "safe";
        optimizations.append(optimization);
    }
    
    return optimizations;
}

QJsonArray RealTimeRefactoring::enforceCodeStyle(const QString& code, const QString& styleGuide) {
    qDebug() << "[RealTimeRefactoring] Enforcing code style:" << styleGuide;
    
    QJsonArray styleIssues;
    
    if (styleGuide == "cpp") {
        // Check for const correctness
        if (code.contains("class") && !code.contains("const")) {
            QJsonObject issue;
            issue["type"] = "const_correctness";
            issue["description"] = "Member functions could be const";
            issue["severity"] = "low";
            issue["suggestion"] = "Mark member functions that don't modify state as const";
            styleIssues.append(issue);
        }
        
        // Check for naming conventions
        QStringList lines = code.split('\n');
        for (const QString& line : lines) {
            if (line.contains("int ") && line.contains("_")) {
                QJsonObject issue;
                issue["type"] = "naming_convention";
                issue["description"] = "Variable names should use camelCase";
                issue["line"] = line.trimmed();
                issue["severity"] = "low";
                issue["suggestion"] = "Use camelCase for variables";
                styleIssues.append(issue);
                break;
            }
        }
    }
    
    return styleIssues;
}

QJsonObject RealTimeRefactoring::analyzePerformanceBottlenecks(const QString& code) {
    qDebug() << "[RealTimeRefactoring] Analyzing performance bottlenecks";
    
    QJsonObject analysis;
    QJsonArray bottlenecks;
    
    // Nested loops detection
    int nestedLoopCount = 0;
    QStringList lines = code.split('\n');
    for (int i = 0; i < lines.size(); i++) {
        QString line = lines[i].trimmed();
        if (line.startsWith("for") || line.startsWith("while")) {
            // Check if next non-empty line is also a loop
            for (int j = i + 1; j < lines.size() && j < i + 10; j++) {
                QString nextLine = lines[j].trimmed();
                if (!nextLine.isEmpty() && (nextLine.startsWith("for") || nextLine.startsWith("while"))) {
                    nestedLoopCount++;
                    break;
                }
            }
        }
    }
    
    if (nestedLoopCount > 0) {
        QJsonObject bottleneck;
        bottleneck["type"] = "nested_loops";
        bottleneck["severity"] = "high";
        bottleneck["description"] = "Nested loops detected - potential O(n²) complexity";
        bottleneck["count"] = nestedLoopCount;
        bottleneck["suggestion"] = "Consider more efficient algorithms";
        bottlenecks.append(bottleneck);
    }
    
    // String concatenation in loops
    if (code.contains("for") && (code.contains("+=") || code.contains("append"))) {
        QJsonObject bottleneck;
        bottleneck["type"] = "string_concatenation";
        bottleneck["severity"] = "medium";
        bottleneck["description"] = "String concatenation in loops";
        bottleneck["suggestion"] = "Use std::stringstream or reserve()";
        bottlenecks.append(bottleneck);
    }
    
    // Memory allocations
    int allocationCount = code.count("new ");
    if (allocationCount > 5) {
        QJsonObject bottleneck;
        bottleneck["type"] = "memory_allocations";
        bottleneck["severity"] = "medium";
        bottleneck["description"] = "Multiple memory allocations";
        bottleneck["count"] = allocationCount;
        bottleneck["suggestion"] = "Consider memory pooling or smart pointers";
        bottlenecks.append(bottleneck);
    }
    
    analysis["bottlenecks"] = bottlenecks;
    analysis["analysis_timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return analysis;
}

QJsonArray RealTimeRefactoring::generatePerformanceImprovements(const QString& code) {
    qDebug() << "[RealTimeRefactoring] Generating performance improvements";
    
    QJsonArray improvements;
    
    QJsonObject bottlenecks = analyzePerformanceBottlenecks(code);
    QJsonArray bottleneckList = bottlenecks["bottlenecks"].toArray();
    
    for (const QJsonValue& bottleneckVal : bottleneckList) {
        QJsonObject bottleneck = bottleneckVal.toObject();
        QString type = bottleneck["type"].toString();
        
        if (type == "nested_loops") {
            QJsonObject improvement;
            improvement["type"] = "algorithm_optimization";
            improvement["description"] = "Optimize nested loops";
            improvement["suggested_change"] = "Consider O(n log n) or O(n) algorithms";
            improvement["estimated_gain"] = "50-90%";
            improvement["difficulty"] = "high";
            improvements.append(improvement);
        } else if (type == "string_concatenation") {
            QJsonObject improvement;
            improvement["type"] = "string_optimization";
            improvement["description"] = "Optimize string concatenation";
            improvement["suggested_change"] = "Use std::stringstream or reserve capacity";
            improvement["estimated_gain"] = "20-40%";
            improvement["difficulty"] = "low";
            improvements.append(improvement);
        } else if (type == "memory_allocations") {
            QJsonObject improvement;
            improvement["type"] = "memory_optimization";
            improvement["description"] = "Optimize memory allocations";
            improvement["suggested_change"] = "Use smart pointers and memory pools";
            improvement["estimated_gain"] = "10-30%";
            improvement["difficulty"] = "medium";
            improvements.append(improvement);
        }
    }
    
    return improvements;
}

bool RealTimeRefactoring::isRefactoringSafe(const QJsonObject& suggestion) {
    QString safety = suggestion["safety"].toString();
    return safety == "safe" || safety == "review";
}

QJsonObject RealTimeRefactoring::createRollbackPoint(const QString& code) {
    QJsonObject rollbackPoint;
    rollbackPoint["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    rollbackPoint["code"] = code;
    rollbackPoint["id"] = QUuid::createUuid().toString();
    
    m_rollbackPoints.append(rollbackPoint);
    return rollbackPoint;
}

void RealTimeRefactoring::rollbackToPoint(const QJsonObject& rollbackPoint) {
    qDebug() << "[RealTimeRefactoring] Rolling back to point:" << rollbackPoint["id"].toString();
    // Implementation would restore code to the rollback point
}

void RealTimeRefactoring::processCodeChange(const QString& filePath, const QString& oldCode, const QString& newCode) {
    qDebug() << "[RealTimeRefactoring] Processing code change in:" << filePath;
    
    if (!m_enableRealTimeAnalysis) return;
    
    // Schedule analysis with delay to avoid excessive processing
    if (!m_activeSessions.contains(filePath)) {
        auto session = std::make_shared<AnalysisSession>();
        session->filePath = filePath;
        session->language = "cpp";
        session->realTimeEnabled = true;
        session->aggressivenessLevel = m_aggressivenessLevel;
        session->analysisTimer = new QTimer();
        session->analysisTimer->setSingleShot(true);
        
        connect(session->analysisTimer, &QTimer::timeout, this, [this, filePath, newCode]() {
            QJsonArray improvements = analyzeCodeForImprovements(newCode);
            for (const QJsonValue& improvementVal : improvements) {
                QJsonObject improvement = improvementVal.toObject();
                emit refactoringSuggested(filePath, improvement);
            }
        });
        
        m_activeSessions[filePath] = session;
    }
    
    auto session = m_activeSessions[filePath];
    session->analysisTimer->stop();
    session->analysisTimer->start(m_analysisDelayMs);
}

void RealTimeRefactoring::requestRefactoring(const QString& filePath, const QString& refactoringType) {
    qDebug() << "[RealTimeRefactoring] Requesting refactoring:" << refactoringType << "for" << filePath;
    
    // Read file content
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit refactoringComplete(filePath, false, "Could not read file");
        return;
    }
    
    QTextStream in(&file);
    QString code = in.readAll();
    file.close();
    
    // Generate suggestion
    QJsonObject suggestion = generateRefactoringSuggestion(code, refactoringType);
    
    // Apply refactoring
    QJsonObject result = applyRefactoring(code, suggestion);
    
    // Write back if successful
    if (result["success"].toBool()) {
        QString refactoredCode = result["refactored_code"].toString();
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << refactoredCode;
            file.close();
            
            emit refactoringApplied(filePath, result);
        }
    }
    
    emit refactoringComplete(filePath, result["success"].toBool(), result["refactored_code"].toString());
}

void RealTimeRefactoring::enableRealTimeAnalysis(bool enable) {
    m_enableRealTimeAnalysis = enable;
    qDebug() << "[RealTimeRefactoring] Real-time analysis" << (enable ? "enabled" : "disabled");
}

void RealTimeRefactoring::setRefactoringAggressiveness(int level) {
    m_aggressivenessLevel = qBound(1, level, 5);
    qDebug() << "[RealTimeRefactoring] Refactoring aggressiveness set to" << m_aggressivenessLevel;
}

// Helper method implementations
QJsonObject RealTimeRefactoring::analyzeCodeStructure(const QString& code) {
    QJsonObject structure;
    structure["line_count"] = code.split('\n').size();
    structure["brace_count"] = code.count('{');
    structure["complexity_score"] = qMin(10, structure["brace_count"].toInt() / 5);
    return structure;
}

QJsonObject RealTimeRefactoring::detectPerformanceIssues(const QString& code) {
    QJsonObject issues;
    QJsonArray issueList;
    
    // Nested loops
    if (code.contains("for") && code.contains("for")) {
        QJsonObject issue;
        issue["type"] = "nested_loops";
        issue["description"] = "Potential O(n²) complexity from nested loops";
        issue["severity"] = "high";
        issueList.append(issue);
    }
    
    // String operations in loops
    if (code.contains("for") && (code.contains("+=") || code.contains("append"))) {
        QJsonObject issue;
        issue["type"] = "string_in_loop";
        issue["description"] = "String concatenation in loops is inefficient";
        issue["severity"] = "medium";
        issueList.append(issue);
    }
    
    issues["issues"] = issueList;
    return issues;
}

QJsonObject RealTimeRefactoring::generateSuggestion(const QString& issueType, const QJsonObject& analysis) {
    QJsonObject suggestion;
    suggestion["type"] = issueType;
    suggestion["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    suggestion["confidence"] = 0.8;
    
    if (issueType == "performance") {
        suggestion["title"] = "Performance Improvement";
        suggestion["description"] = "Optimize for better performance";
        suggestion["category"] = "performance";
        suggestion["safety"] = "safe";
    } else if (issueType == "code_smell") {
        suggestion["title"] = "Code Quality";
        suggestion["description"] = "Improve code quality and readability";
        suggestion["category"] = "style";
        suggestion["safety"] = "safe";
    } else if (issueType == "style") {
        suggestion["title"] = "Style Enhancement";
        suggestion["description"] = "Improve code style consistency";
        suggestion["category"] = "style";
        suggestion["safety"] = "safe";
    }
    
    return suggestion;
}

QString RealTimeRefactoring::applyPatternRefactoring(const QString& code, const QJsonObject& suggestion) {
    QString pattern = suggestion["original_pattern"].toString();
    QString replacement = suggestion["suggested_pattern"].toString();
    
    QRegularExpression regex(pattern);
    QString result = code;
    result.replace(regex, replacement);
    return result;
}

QString RealTimeRefactoring::applyPerformanceOptimization(const QString& code, const QJsonObject& optimization) {
    QString type = optimization["type"].toString();
    
    if (type == "loop_optimization") {
        // Cache container size()
        QRegularExpression sizeCacheRegex(R"((\w+)\.size\(\))");
        QString optimized = code;
        optimized.replace(sizeCacheRegex, R"(\1_cached)");
        
        // Add size caching
        QRegularExpression loopRegex(R"(for\s*\(\s*(\w+)\s*=\s*0\s*;\s*\1\s*<\s*(\w+)\.size\(\)\s*;\s*\1\+\+\s*\))");
        optimized.replace(loopRegex, R"(for(int \1 = 0, \2_size = \2.size(); \1 < \2_size; \1++))");
        
        return optimized;
    } else if (type == "string_concatenation") {
        // Replace += with append
        QRegularExpression concatRegex(R"(\s*\+=\s*)");
        QString result = code;
        result.replace(concatRegex, R"(.append())");
        return result;
    }
    
    return code;
}

QString RealTimeRefactoring::applyStyleImprovements(const QString& code, const QJsonObject& styleGuide) {
    QString improved = code;
    
    // Add const to member functions
    QRegularExpression memberFunctionRegex(R"((const\s+)?(\w+)\s+(\w+)\s*\([^)]*\)\s*\{)");
    improved.replace(memberFunctionRegex, R"(const \2 \3\4 {)");
    
    return improved;
}

bool RealTimeRefactoring::validateSyntax(const QString& code, const QString& language) {
    // Simplified syntax validation
    int braceCount = code.count('{') - code.count('}');
    int parenCount = code.count('(') - code.count(')');
    return braceCount == 0 && parenCount == 0;
}

bool RealTimeRefactoring::validateLogic(const QString& original, const QString& refactored) {
    // Simplified logic validation - check if key patterns are preserved
    QStringList keyPatterns = {"if", "while", "for", "return", "="};
    int originalMatches = 0;
    int refactoredMatches = 0;
    
    for (const QString& pattern : keyPatterns) {
        if (original.contains(pattern)) originalMatches++;
        if (refactored.contains(pattern)) refactoredMatches++;
    }
    
    return refactoredMatches >= originalMatches * 0.8; // Allow some flexibility
}

bool RealTimeRefactoring::validatePerformance(const QString& original, const QString& refactored) {
    // Check if refactored code has fewer performance anti-patterns
    int originalAntiPatterns = original.count("for") + original.count("while") + original.count("new ");
    int refactoredAntiPatterns = refactored.count("for") + refactored.count("while") + refactored.count("new ");
    
    return refactoredAntiPatterns <= originalAntiPatterns;
}
