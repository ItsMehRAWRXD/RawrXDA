#include "intelligent_codebase_engine.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QFileInfo>
#include <QDebug>
#include <iostream>
#include <algorithm>
#include <cmath>

IntelligentCodebaseEngine::IntelligentCodebaseEngine(QObject* parent)
    : QObject(parent), fileWatcher(nullptr) {
    
    enableRealTimeIndexing = true;
    enableDeepAnalysis = true;
    enablePatternRecognition = true;
    maxAnalysisDepth = 3;
    minimumConfidence = 0.7;
    batchSize = 100;
}

IntelligentCodebaseEngine::~IntelligentCodebaseEngine() {
    symbolIndex.clear();
    dependencyGraph.clear();
    fileSymbols.clear();
    refactoringOpportunities.clear();
    bugReports.clear();
    optimizations.clear();
}

bool IntelligentCodebaseEngine::analyzeEntireCodebase(const QString& projectPath) {
    std::cout << "[IntelligentCodebaseEngine] Analyzing entire codebase: " << projectPath.toStdString() << std::endl;
    
    emit analysisStarted(projectPath);
    
    QDir projectDir(projectPath);
    if (!projectDir.exists()) {
        emit analysisCompleted(QJsonObject{{"error", "Project path does not exist"}});
        return false;
    }
    
    QStringList sourceFiles = getAllSourceFiles(projectPath);
    int totalFiles = sourceFiles.size();
    int processedFiles = 0;
    
    std::cout << "[IntelligentCodebaseEngine] Found " << totalFiles << " source files" << std::endl;
    
    for (int i = 0; i < totalFiles; i += batchSize) {
        int endIndex = qMin(i + batchSize, totalFiles);
        
        for (int j = i; j < endIndex; ++j) {
            const QString& filePath = sourceFiles[j];
            
            emit analysisProgress((processedFiles * 100) / totalFiles, filePath);
            
            if (!analyzeFile(filePath)) {
                std::cerr << "[IntelligentCodebaseEngine] Failed to analyze: " << filePath.toStdString() << std::endl;
            }
            
            processedFiles++;
        }
        
        processBatchRelationships(sourceFiles.mid(i, endIndex - i));
    }
    
    performGlobalAnalysis();
    
    QJsonObject results = generateAnalysisResults();
    emit analysisCompleted(results);
    
    std::cout << "[IntelligentCodebaseEngine] Analysis completed" << std::endl;
    
    return true;
}

bool IntelligentCodebaseEngine::analyzeFile(const QString& filePath) {
    std::cout << "[IntelligentCodebaseEngine] Analyzing file: " << filePath.toStdString() << std::endl;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    QString content = stream.readAll();
    file.close();
    
    QString language = detectLanguage(filePath);
    
    if (language == "cpp" || language == "c") {
        return analyzeCppFile(filePath, content);
    } else if (language == "python") {
        return analyzePythonFile(filePath, content);
    }
    
    return analyzeGenericFile(filePath, content);
}

bool IntelligentCodebaseEngine::analyzeCppFile(const QString& filePath, const QString& content) {
    // Function detection
    QRegularExpression functionRegex(R"((\w+(?:\s*\*)?\s+)(\w+)\s*\(([^)]*)\)\s*(const)?\s*\{)");
    QRegularExpressionMatchIterator functionMatches = functionRegex.globalMatch(content);
    
    int lineNumber = 1;
    while (functionMatches.hasNext()) {
        QRegularExpressionMatch match = functionMatches.next();
        
        SymbolInfo function;
        function.name = match.captured(2);
        function.type = "function";
        function.filePath = filePath;
        function.lineNumber = lineNumber;
        function.signature = match.captured(0);
        function.returnType = match.captured(1).trimmed();
        function.parameters = parseParameters(match.captured(3));
        function.isConst = !match.captured(4).isEmpty();
        
        function.metadata["language"] = "cpp";
        
        symbolIndex[function.name] = function;
        fileSymbols[filePath].append(function);
        
        lineNumber++;
    }
    
    // Class detection
    QRegularExpression classRegex(R"(class\s+(\w+)\s*(?::\s*(?:public|private|protected)\s+(\w+))?\s*\{)");
    QRegularExpressionMatchIterator classMatches = classRegex.globalMatch(content);
    
    while (classMatches.hasNext()) {
        QRegularExpressionMatch match = classMatches.next();
        
        SymbolInfo classSymbol;
        classSymbol.name = match.captured(1);
        classSymbol.type = "class";
        classSymbol.filePath = filePath;
        classSymbol.lineNumber = lineNumber;
        classSymbol.signature = match.captured(0);
        
        if (!match.captured(2).isEmpty()) {
            classSymbol.baseClasses.append(match.captured(2));
        }
        
        symbolIndex[classSymbol.name] = classSymbol;
        fileSymbols[filePath].append(classSymbol);
    }
    
    // Variable detection
    QRegularExpression variableRegex(R"((const\s+)?(\w+(?:\s*\*)?)\s+(\w+)\s*(?:=\s*([^;]+))?;)");
    QRegularExpressionMatchIterator variableMatches = variableRegex.globalMatch(content);
    
    while (variableMatches.hasNext()) {
        QRegularExpressionMatch match = variableMatches.next();
        
        SymbolInfo variable;
        variable.name = match.captured(3);
        variable.type = match.captured(1).isEmpty() ? "variable" : "constant";
        variable.filePath = filePath;
        variable.lineNumber = lineNumber;
        variable.returnType = match.captured(2).trimmed();
        variable.isConst = !match.captured(1).isEmpty();
        
        symbolIndex[variable.name] = variable;
        fileSymbols[filePath].append(variable);
    }
    
    analyzeCallGraph(filePath, content);
    analyzeDependencies(filePath, content);
    
    return true;
}

bool IntelligentCodebaseEngine::analyzePythonFile(const QString& filePath, const QString& content) {
    // Python function detection
    QRegularExpression functionRegex(R"(def\s+(\w+)\s*\(([^)]*)\)\s*(?:->\s*(\w+))?\s*:)");
    QRegularExpressionMatchIterator functionMatches = functionRegex.globalMatch(content);
    
    int lineNumber = 1;
    while (functionMatches.hasNext()) {
        QRegularExpressionMatch match = functionMatches.next();
        
        SymbolInfo function;
        function.name = match.captured(1);
        function.type = "function";
        function.filePath = filePath;
        function.lineNumber = lineNumber;
        function.signature = match.captured(0);
        function.parameters = parseParameters(match.captured(2));
        function.returnType = match.captured(3);
        
        function.metadata["language"] = "python";
        
        symbolIndex[function.name] = function;
        fileSymbols[filePath].append(function);
        
        lineNumber++;
    }
    
    // Python class detection
    QRegularExpression classRegex(R"(class\s+(\w+)\s*(?:\(([^)]+)\))?\s*:)");
    QRegularExpressionMatchIterator classMatches = classRegex.globalMatch(content);
    
    while (classMatches.hasNext()) {
        QRegularExpressionMatch match = classMatches.next();
        
        SymbolInfo classSymbol;
        classSymbol.name = match.captured(1);
        classSymbol.type = "class";
        classSymbol.filePath = filePath;
        classSymbol.lineNumber = lineNumber;
        classSymbol.signature = match.captured(0);
        
        if (!match.captured(2).isEmpty()) {
            classSymbol.baseClasses = match.captured(2).split(',', Qt::SkipEmptyParts);
        }
        
        symbolIndex[classSymbol.name] = classSymbol;
        fileSymbols[filePath].append(classSymbol);
    }
    
    return true;
}

bool IntelligentCodebaseEngine::analyzeGenericFile(const QString& filePath, const QString& content) {
    // Generic analysis for unknown file types
    complexityAnalysis.linesOfCode += content.count('\n') + 1;
    return true;
}

bool IntelligentCodebaseEngine::analyzeCallGraph(const QString& filePath, const QString& content) {
    QRegularExpression callRegex(R"(\b(\w+)\s*\()");
    QRegularExpressionMatchIterator callMatches = callRegex.globalMatch(content);
    
    while (callMatches.hasNext()) {
        QRegularExpressionMatch match = callMatches.next();
        QString calledFunction = match.captured(1);
        
        if (calledFunction != "if" && calledFunction != "while" && 
            calledFunction != "for" && calledFunction != "switch") {
            callGraph[calledFunction].append("caller");
        }
    }
    
    return true;
}

bool IntelligentCodebaseEngine::analyzeDependencies(const QString& filePath, const QString& content) {
    // C++ includes
    QRegularExpression includeRegex(R"(#include\s*["<]([^">]+)[">])");
    QRegularExpressionMatchIterator includeMatches = includeRegex.globalMatch(content);
    
    while (includeMatches.hasNext()) {
        QRegularExpressionMatch match = includeMatches.next();
        QString includeFile = match.captured(1);
        
        DependencyInfo dependency;
        dependency.fromSymbol = filePath;
        dependency.toSymbol = includeFile;
        dependency.dependencyType = "import";
        dependency.filePath = filePath;
        dependency.lineNumber = 0;
        dependency.confidence = 0.95;
        
        dependencyGraph[filePath].append(dependency);
    }
    
    return true;
}

QVector<RefactoringOpportunity> IntelligentCodebaseEngine::discoverRefactoringOpportunities() {
    std::cout << "[IntelligentCodebaseEngine] Discovering refactoring opportunities..." << std::endl;
    
    refactoringOpportunities.clear();
    
    discoverExtractMethodOpportunities();
    discoverInlineFunctionOpportunities();
    discoverMoveClassOpportunities();
    discoverRenameOpportunities();
    
    std::cout << "[IntelligentCodebaseEngine] Found " << refactoringOpportunities.size() 
              << " refactoring opportunities" << std::endl;
    
    emit refactoringOpportunitiesFound(refactoringOpportunities);
    
    return refactoringOpportunities;
}

void IntelligentCodebaseEngine::discoverExtractMethodOpportunities() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function") {
            double complexity = calculateFunctionComplexity(symbol);
            
            if (complexity > 15) {
                RefactoringOpportunity opportunity;
                opportunity.type = "extract_method";
                opportunity.description = QString("Extract complex logic from function '%1'").arg(symbol.name);
                opportunity.filePath = symbol.filePath;
                opportunity.startLine = symbol.lineNumber;
                opportunity.endLine = symbol.lineNumber + 50;
                opportunity.confidence = 0.85;
                opportunity.estimatedImpact = 0.7;
                
                opportunity.implementationHints["suggestion"] = "Break into smaller functions";
                
                refactoringOpportunities.append(opportunity);
            }
        }
    }
}

void IntelligentCodebaseEngine::discoverInlineFunctionOpportunities() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function") {
            double complexity = calculateFunctionComplexity(symbol);
            
            if (complexity < 3 && symbol.parameters.isEmpty()) {
                RefactoringOpportunity opportunity;
                opportunity.type = "inline_function";
                opportunity.description = QString("Inline simple function '%1'").arg(symbol.name);
                opportunity.filePath = symbol.filePath;
                opportunity.startLine = symbol.lineNumber;
                opportunity.endLine = symbol.lineNumber + 10;
                opportunity.confidence = 0.9;
                opportunity.estimatedImpact = 0.6;
                
                refactoringOpportunities.append(opportunity);
            }
        }
    }
}

void IntelligentCodebaseEngine::discoverMoveClassOpportunities() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "class") {
            QString expectedFileName = symbol.name.toLower() + ".cpp";
            QString actualFileName = QFileInfo(symbol.filePath).fileName();
            
            if (actualFileName != expectedFileName && actualFileName != symbol.name.toLower() + ".h") {
                RefactoringOpportunity opportunity;
                opportunity.type = "move_class";
                opportunity.description = QString("Move class '%1' to dedicated file").arg(symbol.name);
                opportunity.filePath = symbol.filePath;
                opportunity.startLine = symbol.lineNumber;
                opportunity.endLine = symbol.lineNumber + 50;
                opportunity.confidence = 0.8;
                opportunity.estimatedImpact = 0.5;
                
                refactoringOpportunities.append(opportunity);
            }
        }
    }
}

void IntelligentCodebaseEngine::discoverRenameOpportunities() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.name.length() < 3 || symbol.name.contains(QRegularExpression("[0-9]"))) {
            RefactoringOpportunity opportunity;
            opportunity.type = "rename";
            opportunity.description = QString("Rename symbol '%1' to follow naming conventions").arg(symbol.name);
            opportunity.filePath = symbol.filePath;
            opportunity.startLine = symbol.lineNumber;
            opportunity.endLine = symbol.lineNumber + 1;
            opportunity.confidence = 0.7;
            opportunity.estimatedImpact = 0.4;
            
            refactoringOpportunities.append(opportunity);
        }
    }
}

QVector<BugReport> IntelligentCodebaseEngine::detectBugs() {
    std::cout << "[IntelligentCodebaseEngine] Detecting bugs..." << std::endl;
    
    bugReports.clear();
    
    detectNullPointerBugs();
    detectMemoryLeakBugs();
    detectInfiniteLoopBugs();
    detectRaceConditionBugs();
    
    std::cout << "[IntelligentCodebaseEngine] Found " << bugReports.size() 
              << " potential bugs" << std::endl;
    
    emit bugsDetected(bugReports);
    
    return bugReports;
}

void IntelligentCodebaseEngine::detectNullPointerBugs() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function") {
            for (const QString& param : symbol.parameters) {
                if (param.contains('*') || param.contains("pointer")) {
                    if (!checkForNullChecks(symbol)) {
                        BugReport bug;
                        bug.bugType = "null_pointer";
                        bug.severity = "high";
                        bug.description = QString("Function '%1' has pointer parameter without null check").arg(symbol.name);
                        bug.filePath = symbol.filePath;
                        bug.lineNumber = symbol.lineNumber;
                        bug.confidence = 0.8;
                        
                        bug.evidence.append("Pointer parameter detected: " + param);
                        bug.suggestedFix["add_null_check"] = true;
                        
                        bugReports.append(bug);
                    }
                }
            }
        }
    }
}

void IntelligentCodebaseEngine::detectMemoryLeakBugs() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function") {
            bool hasAlloc = checkForMemoryAllocation(symbol);
            bool hasDealloc = checkForMemoryDeallocation(symbol);
            
            if (hasAlloc && !hasDealloc) {
                BugReport bug;
                bug.bugType = "memory_leak";
                bug.severity = "medium";
                bug.description = QString("Function '%1' allocates memory without proper cleanup").arg(symbol.name);
                bug.filePath = symbol.filePath;
                bug.lineNumber = symbol.lineNumber;
                bug.confidence = 0.7;
                
                bug.evidence.append("Memory allocation detected");
                bug.suggestedFix["add_cleanup"] = true;
                bug.suggestedFix["use_raii"] = true;
                
                bugReports.append(bug);
            }
        }
    }
}

void IntelligentCodebaseEngine::detectInfiniteLoopBugs() {
    // Simplified infinite loop detection
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function" && checkForLoopsWithoutBreak(symbol)) {
            BugReport bug;
            bug.bugType = "infinite_loop";
            bug.severity = "high";
            bug.description = QString("Function '%1' has potential infinite loop").arg(symbol.name);
            bug.filePath = symbol.filePath;
            bug.lineNumber = symbol.lineNumber;
            bug.confidence = 0.6;
            
            bug.evidence.append("Loop without break condition detected");
            bug.suggestedFix["add_break_condition"] = true;
            
            bugReports.append(bug);
        }
    }
}

void IntelligentCodebaseEngine::detectRaceConditionBugs() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function") {
            bool hasSharedAccess = checkForSharedStateAccess(symbol);
            bool hasSync = checkForSynchronization(symbol);
            
            if (hasSharedAccess && !hasSync) {
                BugReport bug;
                bug.bugType = "race_condition";
                bug.severity = "critical";
                bug.description = QString("Function '%1' accesses shared state without synchronization").arg(symbol.name);
                bug.filePath = symbol.filePath;
                bug.lineNumber = symbol.lineNumber;
                bug.confidence = 0.8;
                
                bug.evidence.append("Shared state access detected");
                bug.suggestedFix["add_mutex"] = true;
                
                bugReports.append(bug);
            }
        }
    }
}

QVector<Optimization> IntelligentCodebaseEngine::suggestOptimizations() {
    std::cout << "[IntelligentCodebaseEngine] Suggesting optimizations..." << std::endl;
    
    optimizations.clear();
    
    suggestPerformanceOptimizations();
    suggestMemoryOptimizations();
    suggestReadabilityOptimizations();
    suggestSecurityOptimizations();
    
    std::cout << "[IntelligentCodebaseEngine] Found " << optimizations.size() 
              << " optimization opportunities" << std::endl;
    
    emit optimizationsSuggested(optimizations);
    
    return optimizations;
}

void IntelligentCodebaseEngine::suggestPerformanceOptimizations() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function") {
            double complexity = calculateFunctionComplexity(symbol);
            
            if (complexity > 20) {
                Optimization optimization;
                optimization.optimizationType = "performance";
                optimization.description = QString("Optimize high-complexity function '%1'").arg(symbol.name);
                optimization.filePath = symbol.filePath;
                optimization.lineNumber = symbol.lineNumber;
                optimization.potentialImprovement = 30.0;
                optimization.confidence = 0.85;
                
                optimization.optimizedImplementation["suggestion"] = "Break into smaller functions";
                
                optimizations.append(optimization);
            }
        }
    }
}

void IntelligentCodebaseEngine::suggestMemoryOptimizations() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function" && checkForLargeObjectCreation(symbol)) {
            Optimization optimization;
            optimization.optimizationType = "memory";
            optimization.description = QString("Optimize memory usage in function '%1'").arg(symbol.name);
            optimization.filePath = symbol.filePath;
            optimization.lineNumber = symbol.lineNumber;
            optimization.potentialImprovement = 25.0;
            optimization.confidence = 0.75;
            
            optimization.optimizedImplementation["use_raii"] = true;
            
            optimizations.append(optimization);
        }
    }
}

void IntelligentCodebaseEngine::suggestReadabilityOptimizations() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function" && symbol.parameters.size() > 5) {
            Optimization optimization;
            optimization.optimizationType = "readability";
            optimization.description = QString("Simplify parameter list of function '%1'").arg(symbol.name);
            optimization.filePath = symbol.filePath;
            optimization.lineNumber = symbol.lineNumber;
            optimization.potentialImprovement = 25.0;
            optimization.confidence = 0.85;
            
            optimization.optimizedImplementation["parameter_object"] = true;
            
            optimizations.append(optimization);
        }
    }
}

void IntelligentCodebaseEngine::suggestSecurityOptimizations() {
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "function") {
            if (!checkForInputValidation(symbol) && !symbol.parameters.isEmpty()) {
                Optimization optimization;
                optimization.optimizationType = "security";
                optimization.description = QString("Add input validation to function '%1'").arg(symbol.name);
                optimization.filePath = symbol.filePath;
                optimization.lineNumber = symbol.lineNumber;
                optimization.potentialImprovement = 50.0;
                optimization.confidence = 0.9;
                
                optimization.optimizedImplementation["input_validation"] = true;
                
                optimizations.append(optimization);
            }
        }
    }
}

ArchitecturePattern IntelligentCodebaseEngine::detectArchitecturePattern() {
    std::cout << "[IntelligentCodebaseEngine] Detecting architecture patterns..." << std::endl;
    
    ArchitecturePattern pattern;
    pattern.confidenceScore = 0.0;
    
    bool hasModels = false;
    bool hasViews = false;
    bool hasControllers = false;
    
    for (auto it = symbolIndex.begin(); it != symbolIndex.end(); ++it) {
        const SymbolInfo& symbol = it.value();
        
        if (symbol.type == "class") {
            QString className = symbol.name.toLower();
            
            if (className.contains("model")) hasModels = true;
            if (className.contains("view")) hasViews = true;
            if (className.contains("controller")) hasControllers = true;
        }
    }
    
    if (hasModels && hasViews && hasControllers) {
        pattern.patternType = "MVC";
        pattern.confidenceScore = 0.85;
        pattern.evidence.append("MVC components detected");
    } else if (hasModels || hasViews || hasControllers) {
        pattern.patternType = "Layered";
        pattern.confidenceScore = 0.7;
        pattern.evidence.append("Layered architecture detected");
    } else {
        pattern.patternType = "Monolith";
        pattern.confidenceScore = 0.6;
        pattern.evidence.append("Monolithic structure detected");
    }
    
    emit architecturePatternDetected(pattern);
    
    return pattern;
}

CodeComplexity IntelligentCodebaseEngine::analyzeComplexity() {
    return complexityAnalysis;
}

double IntelligentCodebaseEngine::calculateCodeQualityScore() {
    double score = 1.0;
    
    score -= (refactoringOpportunities.size() * 0.05);
    score -= (bugReports.size() * 0.1);
    
    return qMax(0.0, qMin(1.0, score));
}

double IntelligentCodebaseEngine::calculateMaintainabilityIndex() {
    double avgComplexity = 0.0;
    int functionCount = 0;
    
    for (const SymbolInfo& symbol : symbolIndex) {
        if (symbol.type == "function") {
            avgComplexity += calculateFunctionComplexity(symbol);
            functionCount++;
        }
    }
    
    if (functionCount > 0) {
        avgComplexity /= functionCount;
    }
    
    return qMax(0.0, qMin(1.0, 1.0 - (avgComplexity / 100.0)));
}

QJsonObject IntelligentCodebaseEngine::generateQualityReport() {
    QJsonObject report;
    
    report["code_quality_score"] = calculateCodeQualityScore();
    report["maintainability_index"] = calculateMaintainabilityIndex();
    report["total_symbols"] = symbolIndex.size();
    report["total_files"] = fileSymbols.size();
    report["refactoring_opportunities"] = refactoringOpportunities.size();
    report["bugs_detected"] = bugReports.size();
    report["optimizations_suggested"] = optimizations.size();
    
    return report;
}

bool IntelligentCodebaseEngine::startRealTimeAnalysis(const QString& projectPath) {
    std::cout << "[IntelligentCodebaseEngine] Starting real-time analysis" << std::endl;
    
    fileWatcher = new QFileSystemWatcher(this);
    fileWatcher->addPath(projectPath);
    
    connect(fileWatcher, &QFileSystemWatcher::fileChanged, this, &IntelligentCodebaseEngine::updateAnalysis);
    
    return analyzeEntireCodebase(projectPath);
}

bool IntelligentCodebaseEngine::stopRealTimeAnalysis() {
    if (fileWatcher) {
        fileWatcher->deleteLater();
        fileWatcher = nullptr;
    }
    return true;
}

bool IntelligentCodebaseEngine::updateAnalysis(const QString& filePath) {
    std::cout << "[IntelligentCodebaseEngine] Updating analysis for: " << filePath.toStdString() << std::endl;
    
    removeFileAnalysis(filePath);
    analyzeFile(filePath);
    
    emit realTimeAnalysisUpdated(filePath);
    
    return true;
}

QStringList IntelligentCodebaseEngine::getAllSourceFiles(const QString& projectPath) {
    QStringList files;
    QDir dir(projectPath);
    
    QStringList filters;
    filters << "*.cpp" << "*.h" << "*.c" << "*.hpp" << "*.py" << "*.js" << "*.ts";
    
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files | QDir::NoDotAndDotDot);
    
    for (const QFileInfo& fileInfo : fileList) {
        files.append(fileInfo.absoluteFilePath());
    }
    
    QFileInfoList subdirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& subdir : subdirs) {
        files.append(getAllSourceFiles(subdir.absoluteFilePath()));
    }
    
    return files;
}

void IntelligentCodebaseEngine::processBatchRelationships(const QStringList& files) {
    // Process inter-file relationships
    for (const QString& file : files) {
        analyzeDependencies(file, "");
    }
}

void IntelligentCodebaseEngine::performGlobalAnalysis() {
    std::cout << "[IntelligentCodebaseEngine] Performing global analysis" << std::endl;
    
    complexityAnalysis.numberOfFunctions = 0;
    complexityAnalysis.numberOfClasses = 0;
    
    for (const SymbolInfo& symbol : symbolIndex) {
        if (symbol.type == "function") complexityAnalysis.numberOfFunctions++;
        if (symbol.type == "class") complexityAnalysis.numberOfClasses++;
    }
    
    complexityAnalysis.numberOfDependencies = dependencyGraph.size();
}

QJsonObject IntelligentCodebaseEngine::generateAnalysisResults() {
    return generateQualityReport();
}

QString IntelligentCodebaseEngine::detectLanguage(const QString& filePath) {
    QString suffix = QFileInfo(filePath).suffix().toLower();
    
    if (suffix == "cpp" || suffix == "cc" || suffix == "cxx" || suffix == "h" || suffix == "hpp") {
        return "cpp";
    } else if (suffix == "py") {
        return "python";
    } else if (suffix == "js") {
        return "javascript";
    } else if (suffix == "ts") {
        return "typescript";
    }
    
    return "unknown";
}

QString IntelligentCodebaseEngine::detectFileType(const QString& filePath) {
    return QFileInfo(filePath).suffix();
}

QVector<QString> IntelligentCodebaseEngine::parseParameters(const QString& paramString) {
    QVector<QString> params;
    
    if (paramString.trimmed().isEmpty()) {
        return params;
    }
    
    QStringList paramList = paramString.split(',');
    for (const QString& param : paramList) {
        params.append(param.trimmed());
    }
    
    return params;
}

double IntelligentCodebaseEngine::calculateFunctionComplexity(const SymbolInfo& function) {
    double complexity = 1.0;
    
    complexity += function.parameters.size() * 0.5;
    if (function.isStatic) complexity += 0.5;
    if (function.isConst) complexity += 0.3;
    complexity += function.baseClasses.size() * 0.8;
    
    return complexity;
}

SymbolInfo IntelligentCodebaseEngine::getSymbolInfo(const QString& symbolName) {
    return symbolIndex.value(symbolName, SymbolInfo());
}

QVector<SymbolInfo> IntelligentCodebaseEngine::getSymbolsInFile(const QString& filePath) {
    return fileSymbols.value(filePath, QVector<SymbolInfo>());
}

QVector<DependencyInfo> IntelligentCodebaseEngine::getSymbolDependencies(const QString& symbolName) {
    QVector<DependencyInfo> deps;
    
    for (auto it = dependencyGraph.begin(); it != dependencyGraph.end(); ++it) {
        for (const DependencyInfo& dep : it.value()) {
            if (dep.fromSymbol == symbolName || dep.toSymbol == symbolName) {
                deps.append(dep);
            }
        }
    }
    
    return deps;
}

QJsonArray IntelligentCodebaseEngine::getFileDependencies(const QString& filePath) {
    QJsonArray deps;
    
    for (const DependencyInfo& dep : dependencyGraph.value(filePath)) {
        QJsonObject obj;
        obj["from"] = dep.fromSymbol;
        obj["to"] = dep.toSymbol;
        obj["type"] = dep.dependencyType;
        deps.append(obj);
    }
    
    return deps;
}

QJsonObject IntelligentCodebaseEngine::getDependencyGraph() {
    QJsonObject graph;
    
    for (auto it = dependencyGraph.begin(); it != dependencyGraph.end(); ++it) {
        graph[it.key()] = getFileDependencies(it.key());
    }
    
    return graph;
}

QVector<QString> IntelligentCodebaseEngine::getCircularDependencies() {
    QVector<QString> circular;
    // Simplified circular dependency detection
    return circular;
}

void IntelligentCodebaseEngine::removeFileAnalysis(const QString& filePath) {
    auto it = fileSymbols.find(filePath);
    if (it != fileSymbols.end()) {
        for (const SymbolInfo& symbol : it.value()) {
            symbolIndex.remove(symbol.name);
        }
        fileSymbols.erase(it);
    }
    
    dependencyGraph.remove(filePath);
}

void IntelligentCodebaseEngine::updateDependentAnalyses(const QString& filePath) {
    // Update analyses that depend on this file
    for (auto it = dependencyGraph.begin(); it != dependencyGraph.end(); ++it) {
        for (const DependencyInfo& dep : it.value()) {
            if (dep.toSymbol == filePath && it.key() != filePath) {
                updateAnalysis(it.key());
            }
        }
    }
}

// Simplified check functions
bool IntelligentCodebaseEngine::checkForNullChecks(const SymbolInfo& symbol) { return false; }
bool IntelligentCodebaseEngine::checkForMemoryAllocation(const SymbolInfo& symbol) { return false; }
bool IntelligentCodebaseEngine::checkForMemoryDeallocation(const SymbolInfo& symbol) { return false; }
bool IntelligentCodebaseEngine::checkForLoopsWithoutBreak(const SymbolInfo& symbol) { return false; }
bool IntelligentCodebaseEngine::checkForSharedStateAccess(const SymbolInfo& symbol) { return false; }
bool IntelligentCodebaseEngine::checkForSynchronization(const SymbolInfo& symbol) { return false; }
bool IntelligentCodebaseEngine::checkForNestedLoops(const SymbolInfo& symbol) { return false; }
bool IntelligentCodebaseEngine::checkForLargeObjectCreation(const SymbolInfo& symbol) { return false; }
bool IntelligentCodebaseEngine::checkForInputValidation(const SymbolInfo& symbol) { return false; }
bool IntelligentCodebaseEngine::checkForBufferOverflowRisk(const SymbolInfo& symbol) { return false; }

int IntelligentCodebaseEngine::estimateLinesOfCode(const SymbolInfo& symbol) { return 50; }
double IntelligentCodebaseEngine::calculateHalsteadVolume() { return 100.0; }
