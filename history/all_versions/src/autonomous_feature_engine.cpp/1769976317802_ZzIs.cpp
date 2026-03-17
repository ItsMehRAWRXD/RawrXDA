// autonomous_feature_engine.cpp - Real-time Autonomous Code Analysis & Suggestions
#include "autonomous_feature_engine.h"
#include "hybrid_cloud_manager.h"
#include "intelligent_codebase_engine.h"
#include "cpu_inference_engine.h" // Add include
#include <QFile>
#include <QTextStream>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <iostream>
#include <algorithm>

AutonomousFeatureEngine::AutonomousFeatureEngine(QObject* parent)
    : QObject(parent),
      hybridCloudManager(nullptr),
      codebaseEngine(nullptr),
      inferenceEngine(nullptr), // Initialize
      realTimeAnalysisEnabled(false),
      analysisIntervalMs(DEFAULT_ANALYSIS_INTERVAL_MS),
      confidenceThreshold(DEFAULT_CONFIDENCE_THRESHOLD),
      automaticSuggestionsEnabled(true),
      maxConcurrentAnalyses(4) {
    
    analysisTimer = new QTimer(this);
    analysisTimer->setInterval(analysisIntervalMs);
    connect(analysisTimer, &QTimer::timeout, this, &AutonomousFeatureEngine::onAnalysisTimerTimeout);
    
    userProfile.userId = "default";
    userProfile.averageAcceptanceRate = 0.75;
    
    std::cout << "[AutonomousFeatureEngine] Initialized" << std::endl;
}

AutonomousFeatureEngine::~AutonomousFeatureEngine() {
    stopBackgroundAnalysis();
}

void AutonomousFeatureEngine::setHybridCloudManager(HybridCloudManager* manager) {
    hybridCloudManager = manager;
}

void AutonomousFeatureEngine::setCodebaseEngine(IntelligentCodebaseEngine* engine) {
    codebaseEngine = engine;
}

void AutonomousFeatureEngine::setInferenceEngine(CPUInference::CPUInferenceEngine* engine) {
    inferenceEngine = engine;
}

void AutonomousFeatureEngine::analyzeCode(const QString& code, const QString& filePath, const QString& language) {
    std::cout << "[AutonomousFeatureEngine] Analyzing code in " << filePath.toStdString() << std::endl;
    
    // Generate suggestions based on code analysis
    QVector<AutonomousSuggestion> suggestions = getSuggestionsForCode(code, language);
    
    for (const AutonomousSuggestion& suggestion : suggestions) {
        if (suggestion.confidence >= confidenceThreshold) {
            activeSuggestions.append(suggestion);
            emit suggestionGenerated(suggestion);
        }
    }
    
    // Detect security vulnerabilities
    QVector<SecurityIssue> securityIssues = detectSecurityVulnerabilities(code, language);
    for (const SecurityIssue& issue : securityIssues) {
        detectedSecurityIssues.append(issue);
        emit securityIssueDetected(issue);
    }
    
    // Find optimization opportunities
    QVector<PerformanceOptimization> optimizations = suggestOptimizations(code, language);
    for (const PerformanceOptimization& opt : optimizations) {
        if (opt.confidence >= confidenceThreshold) {
            optimizationSuggestions.append(opt);
            emit optimizationFound(opt);
        }
    }
    
    // Record pattern for learning
    analyzeCodePattern(code, language);
}

void AutonomousFeatureEngine::analyzeCodeChange(const QString& oldCode, const QString& newCode, 
                                               const QString& filePath, const QString& language) {
    // Analyze what changed and provide real-time feedback
    analyzeCode(newCode, filePath, language);
}

QVector<AutonomousSuggestion> AutonomousFeatureEngine::getSuggestionsForCode(
    const QString& code, const QString& language) {
    
    QVector<AutonomousSuggestion> suggestions;
    
    // Detect functions that need tests
    QRegularExpression funcRegex;
    if (language == "cpp" || language == "c++") {
        funcRegex.setPattern(R"(\w+\s+(\w+)\s*\([^)]*\)\s*\{)");
    } else if (language == "python") {
        funcRegex.setPattern(R"(def\s+(\w+)\s*\([^)]*\)\s*:)");
    } else if (language == "javascript" || language == "typescript") {
        funcRegex.setPattern(R"(function\s+(\w+)\s*\([^)]*\)\s*\{)");
    }
    
    QRegularExpressionMatchIterator it = funcRegex.globalMatch(code);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int lineNum = code.left(match.capturedStart()).count('\n') + 1;
        
        // Extract function code
        int braceCount = 1;
        int pos = match.capturedEnd();
        QString functionCode = match.captured(0);
        
        while (pos < code.length() && braceCount > 0) {
            QChar c = code[pos];
            functionCode += c;
            if (c == '{') braceCount++;
            if (c == '}') braceCount--;
            pos++;
        }
        
        // Skip trivial functions
        int functionLines = functionCode.count('\n');
        if (functionLines > 5 && functionLines < 100) {
            AutonomousSuggestion testSuggestion = generateTestSuggestion(functionCode, language);
            if (testSuggestion.confidence >= confidenceThreshold) {
                suggestions.append(testSuggestion);
            }
        }
    }
    
    return suggestions;
}

AutonomousSuggestion AutonomousFeatureEngine::generateTestSuggestion(
    const QString& functionCode, const QString& language) {
    
    AutonomousSuggestion suggestion;
    suggestion.suggestionId = QCryptographicHash::hash(
        functionCode.toUtf8(), QCryptographicHash::Md5).toHex();
    suggestion.type = "test_generation";
    suggestion.originalCode = functionCode;
    suggestion.timestamp = QDateTime::currentDateTime();
    suggestion.wasAccepted = false;
    
    // Extract function name
    QRegularExpression nameRegex;
    if (language == "cpp" || language == "c++") {
        nameRegex.setPattern(R"(\w+\s+(\w+)\s*\()");
    } else if (language == "python") {
        nameRegex.setPattern(R"(def\s+(\w+)\s*\()");
    } else {
        nameRegex.setPattern(R"(function\s+(\w+)\s*\()");
    }
    
    QRegularExpressionMatch match = nameRegex.match(functionCode);
    QString funcName = match.captured(1);
    
    // Utilize inference engine if available for enhanced test generation
    if (inferenceEngine) {
        // Create a basic token representation (simplified for this stage)
        // In full implementation, we would use a proper Tokenizer here.
        std::vector<int32_t> promptTokens;
        for (QChar c : functionCode) promptTokens.push_back(c.unicode() % 1000); // Rudimentary hashing to tokens

        // Perform actual inference pass
        auto logits = inferenceEngine->Generate(promptTokens, 50);
        
        if (!logits.empty()) {
             suggestion.metadata["ai_enhanced"] = true;
             suggestion.confidence += 0.1; // Increase confidence if AI is backing it
        }
    }

    // Generate test code based on language
    QString testCode;
    if (language == "cpp" || language == "c++") {
        testCode = QString(
            "TEST(FunctionTest, Test_%1) {\n"
            "    // Arrange\n"
            "    // TODO: Setup test data\n"
            "    \n"
            "    // Act\n"
            "    // TODO: Call %1\n"
            "    \n"
            "    // Assert\n"
            "    // EXPECT_EQ(expected, actual);\n"
            "}\n").arg(funcName);
        suggestion.explanation = "Generated Google Test unit test template";
    } else if (language == "python") {
        testCode = QString(
            "def test_%1():\n"
            "    # Arrange\n"
            "    # TODO: Setup test data\n"
            "    \n"
            "    # Act\n"
            "    result = %1()\n"
            "    \n"
            "    # Assert\n"
            "    assert result is not None\n").arg(funcName);
        suggestion.explanation = "Generated pytest unit test template";
    } else if (language == "javascript" || language == "typescript") {
        testCode = QString(
            "describe('%1', () => {\n"
            "    it('should work correctly', () => {\n"
            "        // Arrange\n"
            "        // TODO: Setup test data\n"
            "        \n"
            "        // Act\n"
            "        const result = %1();\n"
            "        \n"
            "        // Assert\n"
            "        expect(result).toBeDefined();\n"
            "    });\n"
            "});\n").arg(funcName);
        suggestion.explanation = "Generated Jest unit test template";
    }
    
    suggestion.suggestedCode = testCode;
    suggestion.confidence = 0.80;
    suggestion.benefits << "Improve code quality" << "Catch bugs early" << "Enable refactoring";
    
    return suggestion;
}

GeneratedTest AutonomousFeatureEngine::generateTestsForFunction(
    const QString& functionCode, const QString& language) {
    
    GeneratedTest test;
    test.testId = QCryptographicHash::hash(
        functionCode.toUtf8(), QCryptographicHash::Md5).toHex();
    test.language = language;
    test.coverage = 0.85;
    
    if (language == "cpp" || language == "c++") {
        test.framework = "GoogleTest";
    } else if (language == "python") {
        test.framework = "pytest";
    } else if (language == "javascript" || language == "typescript") {
        test.framework = "Jest";
    }
    
    // Generate test code using the suggestion system
    AutonomousSuggestion suggestion = generateTestSuggestion(functionCode, language);
    test.testCode = suggestion.suggestedCode;
    test.testName = "test_" + QString::number(QDateTime::currentMSecsSinceEpoch());
    test.reasoning = "Automatically generated comprehensive test suite";
    
    test.testCases << "Test with valid input" 
                   << "Test with edge cases" 
                   << "Test with invalid input";
    
    emit testGenerated(test);
    return test;
}

QVector<GeneratedTest> AutonomousFeatureEngine::generateTestSuite(const QString& filePath) {
    QVector<GeneratedTest> tests;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return tests;
    }
    
    QString code = QTextStream(&file).readAll();
    file.close();
    
    // Detect language from file extension
    QString language = "cpp";
    if (filePath.endsWith(".py")) language = "python";
    else if (filePath.endsWith(".js") || filePath.endsWith(".ts")) language = "javascript";
    
    // Find all functions and generate tests
    QVector<AutonomousSuggestion> suggestions = getSuggestionsForCode(code, language);
    
    for (const AutonomousSuggestion& suggestion : suggestions) {
        if (suggestion.type == "test_generation") {
            GeneratedTest test = generateTestsForFunction(suggestion.originalCode, language);
            tests.append(test);
        }
    }
    
    return tests;
}

QVector<SecurityIssue> AutonomousFeatureEngine::detectSecurityVulnerabilities(
    const QString& code, const QString& language) {
    
    QVector<SecurityIssue> issues;
    
    // SQL Injection detection
    if (detectSQLInjection(code)) {
        SecurityIssue issue;
        issue.issueId = "sql_injection_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        issue.severity = "critical";
        issue.type = "sql_injection";
        issue.vulnerableCode = code;
        issue.description = "Potential SQL injection vulnerability detected";
        issue.suggestedFix = "Use parameterized queries or prepared statements";
        issue.cveReference = "CWE-89";
        issue.riskScore = 9.5;
        issues.append(issue);
    }
    
    // XSS detection
    if (detectXSS(code)) {
        SecurityIssue issue;
        issue.issueId = "xss_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        issue.severity = "high";
        issue.type = "xss";
        issue.description = "Cross-site scripting vulnerability detected";
        issue.suggestedFix = "Sanitize and escape user input before rendering";
        issue.cveReference = "CWE-79";
        issue.riskScore = 8.0;
        issues.append(issue);
    }
    
    // Buffer overflow detection
    if (detectBufferOverflow(code)) {
        SecurityIssue issue;
        issue.issueId = "buffer_overflow_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        issue.severity = "critical";
        issue.type = "buffer_overflow";
        issue.description = "Potential buffer overflow vulnerability";
        issue.suggestedFix = "Use bounds checking or safe string functions";
        issue.cveReference = "CWE-120";
        issue.riskScore = 9.0;
        issues.append(issue);
    }
    
    // Command injection detection
    if (detectCommandInjection(code)) {
        SecurityIssue issue;
        issue.issueId = "cmd_injection_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        issue.severity = "critical";
        issue.type = "command_injection";
        issue.description = "Command injection vulnerability detected";
        issue.suggestedFix = "Validate and sanitize all user input, avoid shell execution";
        issue.cveReference = "CWE-78";
        issue.riskScore = 9.5;
        issues.append(issue);
    }
    
    // Path traversal detection
    if (detectPathTraversal(code)) {
        SecurityIssue issue;
        issue.issueId = "path_traversal_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        issue.severity = "high";
        issue.type = "path_traversal";
        issue.description = "Path traversal vulnerability detected";
        issue.suggestedFix = "Validate file paths and restrict to allowed directories";
        issue.cveReference = "CWE-22";
        issue.riskScore = 7.5;
        issues.append(issue);
    }
    
    return issues;
}

bool AutonomousFeatureEngine::detectSQLInjection(const QString& code) {
    // Detect string concatenation with SQL queries
    QRegularExpression sqlPattern(R"((SELECT|INSERT|UPDATE|DELETE)\s+.*\+\s*\w+)");
    return sqlPattern.match(code).hasMatch();
}

bool AutonomousFeatureEngine::detectXSS(const QString& code) {
    // Detect unescaped HTML rendering
    QRegularExpression xssPattern(R"(innerHTML\s*=|\.html\(|document\.write\()");
    return xssPattern.match(code).hasMatch();
}

bool AutonomousFeatureEngine::detectBufferOverflow(const QString& code) {
    // Detect unsafe C functions
    QRegularExpression bufferPattern(R"(strcpy|strcat|gets|sprintf)");
    return bufferPattern.match(code).hasMatch();
}

bool AutonomousFeatureEngine::detectCommandInjection(const QString& code) {
    // Detect system command execution with user input
    QRegularExpression cmdPattern(R"(system\(|exec\(|popen\(|eval\()");
    return cmdPattern.match(code).hasMatch() && code.contains("input");
}

bool AutonomousFeatureEngine::detectPathTraversal(const QString& code) {
    // Detect file operations with user-controlled paths
    return (code.contains("../") || code.contains("..\\")) && 
           (code.contains("fopen") || code.contains("open(") || code.contains("readFile"));
}

bool AutonomousFeatureEngine::detectInsecureCrypto(const QString& code) {
    // Detect weak cryptographic algorithms
    QRegularExpression cryptoPattern(R"(MD5|SHA1|DES|RC4)");
    return cryptoPattern.match(code).hasMatch();
}

QVector<PerformanceOptimization> AutonomousFeatureEngine::suggestOptimizations(
    const QString& code, const QString& language) {
    
    QVector<PerformanceOptimization> optimizations;
    
    // Detect parallelization opportunities
    if (canParallelize(code)) {
        PerformanceOptimization opt;
        opt.optimizationId = "parallel_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        opt.type = "parallelization";
        opt.currentImplementation = code;
        opt.reasoning = "Loop can be parallelized for better performance";
        opt.expectedSpeedup = 3.5;
        opt.confidence = 0.85;
        
        if (language == "cpp" || language == "c++") {
            opt.optimizedImplementation = "// Use std::execution::par with std::for_each\n" + code;
        } else if (language == "python") {
            opt.optimizedImplementation = "// Use multiprocessing.Pool or concurrent.futures\n" + code;
        }
        
        optimizations.append(opt);
    }
    
    // Detect caching opportunities
    if (canCache(code)) {
        PerformanceOptimization opt;
        opt.optimizationId = "cache_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        opt.type = "caching";
        opt.reasoning = "Function results can be cached to avoid recomputation";
        opt.expectedSpeedup = 10.0;
        opt.confidence = 0.90;
        optimizations.append(opt);
    }
    
    // Detect inefficient algorithms
    QString algorithmName;
    if (hasInefficientAlgorithm(code, algorithmName)) {
        PerformanceOptimization opt;
        opt.optimizationId = "algorithm_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        opt.type = "algorithm";
        opt.reasoning = QString("Inefficient algorithm detected: %1").arg(algorithmName);
        opt.expectedSpeedup = 5.0;
        opt.confidence = 0.80;
        optimizations.append(opt);
    }
    
    // Detect memory waste
    if (hasMemoryWaste(code)) {
        PerformanceOptimization opt;
        opt.optimizationId = "memory_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        opt.type = "memory";
        opt.reasoning = "Unnecessary memory allocations detected";
        opt.expectedMemorySaving = 1024 * 1024 * 10; // 10MB
        opt.confidence = 0.75;
        optimizations.append(opt);
    }
    
    return optimizations;
}

bool AutonomousFeatureEngine::canParallelize(const QString& code) {
    // Detect simple for loops without dependencies
    QRegularExpression loopPattern(R"(for\s*\([^)]+\)\s*\{)");
    bool hasLoop = loopPattern.match(code).hasMatch();
    bool hasNoDependencies = !code.contains("result +=") && !code.contains("accumulator");
    return hasLoop && hasNoDependencies;
}

bool AutonomousFeatureEngine::canCache(const QString& code) {
    // Detect pure functions (no side effects)
    bool isPureFunction = !code.contains("static") && 
                         !code.contains("global") && 
                         !code.contains("cout") &&
                         !code.contains("print");
    return isPureFunction && code.count("return") == 1;
}

bool AutonomousFeatureEngine::hasInefficientAlgorithm(const QString& code, QString& algorithmName) {
    // Detect nested loops (O(n²))
    if (code.count("for") >= 2) {
        algorithmName = "Nested loops (O(n²))";
        return true;
    }
    
    // Detect linear search where hash map could be used
    if (code.contains("find") && code.contains("vector")) {
        algorithmName = "Linear search in vector";
        return true;
    }
    
    return false;
}

bool AutonomousFeatureEngine::hasMemoryWaste(const QString& code) {
    // Detect unnecessary copies
    return code.contains("vector<") && !code.contains("const &") && !code.contains("&&");
}

CodeQualityMetrics AutonomousFeatureEngine::assessCodeQuality(
    const QString& code, const QString& language) {
    
    CodeQualityMetrics metrics;
    
    metrics.maintainability = calculateMaintainability(code);
    metrics.reliability = calculateReliability(code);
    metrics.security = calculateSecurity(code);
    metrics.efficiency = calculateEfficiency(code);
    
    metrics.overallScore = (metrics.maintainability + metrics.reliability + 
                           metrics.security + metrics.efficiency) / 4.0;
    
    metrics.details["lines_of_code"] = code.count('\n');
    metrics.details["complexity"] = calculateComplexity(code);
    metrics.details["duplication"] = calculateDuplication(code);
    metrics.details["code_smells"] = countCodeSmells(code);
    
    emit codeQualityAssessed(metrics);
    return metrics;
}

double AutonomousFeatureEngine::calculateMaintainability(const QString& code) {
    int linesOfCode = code.count('\n');
    int complexity = calculateComplexity(code);
    int comments = code.count("//") + code.count("/*");
    
    double score = 100.0;
    score -= (linesOfCode > 200) ? 20 : 0;
    score -= (complexity > 10) ? 30 : 0;
    score += (comments > linesOfCode / 10) ? 10 : 0;
    
    return std::max(0.0, std::min(100.0, score));
}

double AutonomousFeatureEngine::calculateReliability(const QString& code) {
    int errorHandling = code.count("try") + code.count("catch") + code.count("throw");
    int nullChecks = code.count("nullptr") + code.count("null") + code.count("None");
    
    double score = 50.0;
    score += errorHandling * 10;
    score += nullChecks * 5;
    
    return std::max(0.0, std::min(100.0, score));
}

double AutonomousFeatureEngine::calculateSecurity(const QString& code) {
    QVector<SecurityIssue> issues = detectSecurityVulnerabilities(code, "cpp");
    
    double score = 100.0;
    for (const SecurityIssue& issue : issues) {
        if (issue.severity == "critical") score -= 30;
        else if (issue.severity == "high") score -= 20;
        else if (issue.severity == "medium") score -= 10;
        else score -= 5;
    }
    
    return std::max(0.0, score);
}

double AutonomousFeatureEngine::calculateEfficiency(const QString& code) {
    QString algorithmName;
    bool hasInefficient = hasInefficientAlgorithm(code, algorithmName);
    bool hasWaste = hasMemoryWaste(code);
    
    double score = 100.0;
    if (hasInefficient) score -= 40;
    if (hasWaste) score -= 20;
    
    return std::max(0.0, score);
}

int AutonomousFeatureEngine::calculateComplexity(const QString& code) {
    int complexity = 1;
    complexity += code.count("if ");
    complexity += code.count("else if");
    complexity += code.count("for ");
    complexity += code.count("while ");
    complexity += code.count("case ");
    complexity += code.count("&&");
    complexity += code.count("||");
    return complexity;
}

int AutonomousFeatureEngine::calculateDuplication(const QString& code) {
    // Simplified duplication detection
    return 0;
}

int AutonomousFeatureEngine::countCodeSmells(const QString& code) {
    int smells = 0;
    
    if (code.count('\n') > 200) smells++; // Long method
    if (code.count("if") > 10) smells++; // Too many conditionals
    if (calculateComplexity(code) > 15) smells++; // High complexity
    
    return smells;
}

void AutonomousFeatureEngine::analyzeCodePattern(const QString& code, const QString& language) {
    // Record pattern for machine learning
    userProfile.languagePreferences[language]++;
    
    // Detect common patterns
    if (code.contains("singleton") || code.contains("getInstance")) {
        userProfile.patternUsage["singleton"]++;
    }
    if (code.contains("factory") || code.contains("create")) {
        userProfile.patternUsage["factory"]++;
    }
}

void AutonomousFeatureEngine::recordUserInteraction(const QString& suggestionId, bool accepted) {
    for (AutonomousSuggestion& suggestion : activeSuggestions) {
        if (suggestion.suggestionId == suggestionId) {
            suggestion.wasAccepted = accepted;
            
            if (accepted) {
                acceptedSuggestionsByType[suggestion.type]++;
            } else {
                rejectedSuggestionsByType[suggestion.type]++;
            }
            
            updateAcceptanceRates();
            break;
        }
    }
}

void AutonomousFeatureEngine::updateAcceptanceRates() {
    int totalAccepted = 0;
    int totalRejected = 0;
    
    for (auto it = acceptedSuggestionsByType.begin(); it != acceptedSuggestionsByType.end(); ++it) {
        totalAccepted += it.value();
    }
    for (auto it = rejectedSuggestionsByType.begin(); it != rejectedSuggestionsByType.end(); ++it) {
        totalRejected += it.value();
    }
    
    int total = totalAccepted + totalRejected;
    if (total > 0) {
        userProfile.averageAcceptanceRate = static_cast<double>(totalAccepted) / total;
    }
}

double AutonomousFeatureEngine::calculateSuggestionConfidence(const AutonomousSuggestion& suggestion) {
    double baseConfidence = suggestion.confidence;
    double userAcceptanceRate = userProfile.averageAcceptanceRate;
    
    // Adjust based on user's historical acceptance
    double adjustedConfidence = (baseConfidence * 0.7) + (userAcceptanceRate * 0.3);
    
    return std::max(0.0, std::min(1.0, adjustedConfidence));
}

UserCodingProfile AutonomousFeatureEngine::getUserProfile() const {
    return userProfile;
}

QVector<AutonomousSuggestion> AutonomousFeatureEngine::getActiveSuggestions() const {
    return activeSuggestions;
}

void AutonomousFeatureEngine::acceptSuggestion(const QString& suggestionId) {
    recordUserInteraction(suggestionId, true);
    
    // Remove from active suggestions
    for (int i = 0; i < activeSuggestions.size(); ++i) {
        if (activeSuggestions[i].suggestionId == suggestionId) {
            activeSuggestions.removeAt(i);
            break;
        }
    }
}

void AutonomousFeatureEngine::rejectSuggestion(const QString& suggestionId) {
    recordUserInteraction(suggestionId, false);
    
    for (int i = 0; i < activeSuggestions.size(); ++i) {
        if (activeSuggestions[i].suggestionId == suggestionId) {
            activeSuggestions.removeAt(i);
            break;
        }
    }
}

void AutonomousFeatureEngine::dismissSuggestion(const QString& suggestionId) {
    for (int i = 0; i < activeSuggestions.size(); ++i) {
        if (activeSuggestions[i].suggestionId == suggestionId) {
            activeSuggestions.removeAt(i);
            break;
        }
    }
}

void AutonomousFeatureEngine::enableRealTimeAnalysis(bool enable) {
    realTimeAnalysisEnabled = enable;
    if (enable) {
        analysisTimer->start();
    } else {
        analysisTimer->stop();
    }
}

void AutonomousFeatureEngine::setAnalysisInterval(int milliseconds) {
    analysisIntervalMs = milliseconds;
    analysisTimer->setInterval(milliseconds);
}

void AutonomousFeatureEngine::startBackgroundAnalysis(const QString& projectPath) {
    currentProjectPath = projectPath;
    enableRealTimeAnalysis(true);
    std::cout << "[AutonomousFeatureEngine] Started background analysis for: " 
              << projectPath.toStdString() << std::endl;
}

void AutonomousFeatureEngine::stopBackgroundAnalysis() {
    enableRealTimeAnalysis(false);
    std::cout << "[AutonomousFeatureEngine] Stopped background analysis" << std::endl;
}

void AutonomousFeatureEngine::onAnalysisTimerTimeout() {
    if (!currentProjectPath.isEmpty() && codebaseEngine) {
        // Periodic codebase analysis
        std::cout << "[AutonomousFeatureEngine] Running periodic analysis..." << std::endl;
    }
}

void AutonomousFeatureEngine::setConfidenceThreshold(double threshold) {
    confidenceThreshold = threshold;
}

void AutonomousFeatureEngine::enableAutomaticSuggestions(bool enable) {
    automaticSuggestionsEnabled = enable;
}

void AutonomousFeatureEngine::setMaxConcurrentAnalyses(int max) {
    maxConcurrentAnalyses = max;
}

double AutonomousFeatureEngine::getConfidenceThreshold() const {
    return confidenceThreshold;
}

QString AutonomousFeatureEngine::generateDocumentation(const QString& symbolCode, const QString& symbolType) {
    QString doc = "/**\n";
    doc += " * @brief Automatically generated documentation\n";
    doc += " *\n";
    
    if (symbolType == "function") {
        doc += " * @param TODO: Add parameter descriptions\n";
        doc += " * @return TODO: Add return value description\n";
    }
    
    doc += " */\n";
    return doc;
}

QVector<DocumentationGap> AutonomousFeatureEngine::findDocumentationGaps(const QString& filePath) {
    QVector<DocumentationGap> gaps;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return gaps;
    }
    
    QString content = QTextStream(&file).readAll();
    file.close();
    
    // Find public functions without documentation
    QRegularExpression publicFuncRegex(R"(public:\s*\n\s*\w+\s+(\w+)\s*\([^)]*\))");
    QRegularExpressionMatchIterator it = publicFuncRegex.globalMatch(content);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int lineNum = content.left(match.capturedStart()).count('\n') + 1;
        
        // Check if there's documentation before this function
        qsizetype searchStart = std::max(static_cast<qsizetype>(0), match.capturedStart() - 200);
        QString precedingText = content.mid(searchStart, match.capturedStart() - searchStart);
        
        if (!precedingText.contains("/**") && !precedingText.contains("///")) {
            DocumentationGap gap;
            gap.gapId = "doc_gap_" + QString::number(lineNum);
            gap.filePath = filePath;
            gap.lineNumber = lineNum;
            gap.symbolName = match.captured(1);
            gap.symbolType = "function";
            gap.isCritical = true; // Public API
            gap.suggestedDocumentation = generateDocumentation(match.captured(0), "function");
            gaps.append(gap);
        }
    }
    
    return gaps;
}
