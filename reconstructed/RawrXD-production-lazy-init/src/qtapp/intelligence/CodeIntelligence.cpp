#include "CodeIntelligence.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <algorithm>

QJsonObject CodeIntelligence::CallGraphNode::toJson() const {
    QJsonObject obj;
    obj["functionName"] = functionName;
    obj["filePath"] = filePath;
    obj["line"] = line;
    QJsonArray callersArr, calleesArr;
    for (const QString& c : callers) callersArr.append(c);
    for (const QString& c : callees) calleesArr.append(c);
    obj["callers"] = callersArr;
    obj["callees"] = calleesArr;
    obj["depth"] = depth;
    return obj;
}

QJsonObject CodeIntelligence::DependencyInfo::toJson() const {
    QJsonObject obj;
    obj["sourceFile"] = sourceFile;
    obj["targetFile"] = targetFile;
    obj["dependencyType"] = dependencyType;
    obj["isCircular"] = isCircular;
    return obj;
}

QJsonObject CodeIntelligence::ComplexityMetrics::toJson() const {
    QJsonObject obj;
    obj["functionName"] = functionName;
    obj["filePath"] = filePath;
    obj["cyclomaticComplexity"] = cyclomaticComplexity;
    obj["cognitiveComplexity"] = cognitiveComplexity;
    obj["lineCount"] = lineCount;
    obj["parameterCount"] = parameterCount;
    obj["maintainabilityIndex"] = maintainabilityIndex;
    return obj;
}

CodeIntelligence::CodeIntelligence(QObject* parent) : QObject(parent) {}
CodeIntelligence::~CodeIntelligence() {}

QStringList CodeIntelligence::semanticSearch(const QString& query, const QString& scope) {
    QMutexLocker locker(&m_mutex);
    QStringList results;
    // Semantic search implementation (would use embeddings/ML in production)
    emit progressUpdated(100, "Search complete");
    emit analysisCompleted(AnalysisType::SemanticSearch, QJsonObject());
    return results;
}

QList<QPair<QString, int>> CodeIntelligence::findSimilarCode(const QString& code, double threshold) {
    QList<QPair<QString, int>> results;
    // Code similarity analysis
    return results;
}

QStringList CodeIntelligence::findByPattern(const QString& pattern, bool useRegex) {
    QStringList results;
    QRegularExpression re(useRegex ? pattern : QRegularExpression::escape(pattern));
    // Pattern matching across codebase
    return results;
}

CodeIntelligence::CallGraphNode CodeIntelligence::buildCallGraph(const QString& functionName, const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    CallGraphNode node;
    node.functionName = functionName;
    node.filePath = filePath;
    node.line = 0;
    node.depth = 0;
    
    QString code = readFile(filePath);
    if (code.isEmpty()) return node;
    
    // Parse function calls
    QRegularExpression callRe(R"((\w+)\s*\()");
    QRegularExpressionMatchIterator it = callRe.globalMatch(code);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString callee = match.captured(1);
        if (!node.callees.contains(callee)) {
            node.callees.append(callee);
        }
    }
    
    m_callGraphCache[functionName].append(node);
    emit analysisCompleted(AnalysisType::CallGraph, node.toJson());
    
    return node;
}

QList<CodeIntelligence::CallGraphNode> CodeIntelligence::getCallers(const QString& functionName) {
    QMutexLocker locker(&m_mutex);
    return m_callGraphCache.value(functionName);
}

QList<CodeIntelligence::CallGraphNode> CodeIntelligence::getCallees(const QString& functionName) {
    QList<CallGraphNode> callees;
    // Find all functions called by functionName
    return callees;
}

QList<QString> CodeIntelligence::findUnusedFunctions(const QString& filePath) {
    QStringList unused;
    QString code = readFile(filePath);
    QStringList symbols = parseSymbols(code);
    
    for (const QString& symbol : symbols) {
        int usageCount = code.count(QRegularExpression(QString("\\b%1\\b").arg(symbol)));
        if (usageCount <= 1) {  // Only declaration
            unused.append(symbol);
        }
    }
    
    return unused;
}

QList<CodeIntelligence::DependencyInfo> CodeIntelligence::analyzeDependencies(const QString& filePath) {
    QList<DependencyInfo> deps;
    
    QString code = readFile(filePath);
    QRegularExpression incRe(R"(#include\s*[<"]([^>"]+)[>"])");
    QRegularExpressionMatchIterator it = incRe.globalMatch(code);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        DependencyInfo dep;
        dep.sourceFile = filePath;
        dep.targetFile = match.captured(1);
        dep.dependencyType = "include";
        dep.isCircular = false;
        deps.append(dep);
    }
    
    emit analysisCompleted(AnalysisType::Dependencies, QJsonObject());
    return deps;
}

QList<QStringList> CodeIntelligence::findCircularDependencies(const QString& projectRoot) {
    QList<QStringList> circular;
    // Graph-based cycle detection
    return circular;
}

QMap<QString, QStringList> CodeIntelligence::buildDependencyMap(const QString& projectRoot) {
    QMap<QString, QStringList> depMap;
    
    QDir dir(projectRoot);
    QStringList files = dir.entryList(QStringList() << "*.cpp" << "*.h", QDir::Files);
    
    for (const QString& file : files) {
        QString fullPath = dir.absoluteFilePath(file);
        QList<DependencyInfo> deps = analyzeDependencies(fullPath);
        
        QStringList targets;
        for (const DependencyInfo& dep : deps) {
            targets.append(dep.targetFile);
        }
        depMap[fullPath] = targets;
    }
    
    return depMap;
}

CodeIntelligence::ComplexityMetrics CodeIntelligence::analyzeComplexity(const QString& functionName, const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    ComplexityMetrics metrics;
    metrics.functionName = functionName;
    metrics.filePath = filePath;
    
    QString code = readFile(filePath);
    if (code.isEmpty()) return metrics;
    
    metrics.cyclomaticComplexity = calculateCyclomaticComplexity(code);
    metrics.cognitiveComplexity = metrics.cyclomaticComplexity;  // Simplified
    metrics.lineCount = code.split('\n').size();
    metrics.parameterCount = 0;  // Would parse function signature
    metrics.maintainabilityIndex = 100.0 - (metrics.cyclomaticComplexity * 2.0);
    
    m_metricsCache[functionName] = metrics;
    
    return metrics;
}

QList<CodeIntelligence::ComplexityMetrics> CodeIntelligence::getHighComplexityFunctions(int threshold) {
    QMutexLocker locker(&m_mutex);
    
    QList<ComplexityMetrics> high;
    for (const ComplexityMetrics& metrics : m_metricsCache) {
        if (metrics.cyclomaticComplexity >= threshold) {
            high.append(metrics);
        }
    }
    
    return high;
}

double CodeIntelligence::calculateMaintainabilityIndex(const QString& filePath) {
    QString code = readFile(filePath);
    if (code.isEmpty()) return 0.0;
    
    int complexity = calculateCyclomaticComplexity(code);
    int loc = code.split('\n').size();
    
    // Simplified maintainability index
    double mi = 171.0 - 5.2 * std::log(loc) - 0.23 * complexity;
    return std::max(0.0, std::min(100.0, mi));
}

QStringList CodeIntelligence::findDeadCode(const QString& filePath) {
    QStringList dead = findUnusedFunctions(filePath);
    dead.append(findUnusedVariables(filePath));
    return dead;
}

QStringList CodeIntelligence::findUnreachableCode(const QString& filePath) {
    QStringList unreachable;
    QString code = readFile(filePath);
    QStringList lines = code.split('\n');
    
    bool afterReturn = false;
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        
        if (line.startsWith("return")) {
            afterReturn = true;
        } else if (afterReturn && !line.isEmpty() && !line.startsWith("}")) {
            unreachable.append(QString("Line %1: %2").arg(i + 1).arg(line));
        }
        
        if (line.startsWith("}")) {
            afterReturn = false;
        }
    }
    
    return unreachable;
}

QStringList CodeIntelligence::findUnusedVariables(const QString& filePath) {
    QStringList unused;
    QString code = readFile(filePath);
    
    QRegularExpression varDeclRe(R"(\b(\w+)\s+(\w+)\s*;)");
    QRegularExpressionMatchIterator it = varDeclRe.globalMatch(code);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(2);
        
        // Count usages
        int usageCount = code.count(QRegularExpression(QString("\\b%1\\b").arg(varName)));
        if (usageCount <= 1) {
            unused.append(varName);
        }
    }
    
    return unused;
}

QJsonObject CodeIntelligence::analyzeCodeQuality(const QString& filePath) {
    QJsonObject quality;
    
    quality["maintainabilityIndex"] = calculateMaintainabilityIndex(filePath);
    quality["deadCodeCount"] = findDeadCode(filePath).size();
    quality["complexFunctionCount"] = getHighComplexityFunctions(10).size();
    
    QStringList smells = detectCodeSmells(filePath);
    QJsonArray smellsArr;
    for (const QString& smell : smells) smellsArr.append(smell);
    quality["codeSmells"] = smellsArr;
    
    return quality;
}

QStringList CodeIntelligence::detectCodeSmells(const QString& filePath) {
    QStringList smells;
    
    QString code = readFile(filePath);
    int loc = code.split('\n').size();
    
    if (loc > 500) {
        smells.append("Long file (>500 lines)");
    }
    
    if (calculateCyclomaticComplexity(code) > 20) {
        smells.append("High complexity");
    }
    
    if (findDeadCode(filePath).size() > 0) {
        smells.append("Dead code detected");
    }
    
    for (const QString& smell : smells) {
        emit codeSmellDetected(filePath, smell);
    }
    
    return smells;
}

int CodeIntelligence::calculateTechnicalDebt(const QString& filePath) {
    // Technical debt estimation in minutes
    int debt = 0;
    
    QStringList smells = detectCodeSmells(filePath);
    debt += smells.size() * 30;  // 30 min per smell
    
    QList<ComplexityMetrics> complex = getHighComplexityFunctions(10);
    debt += complex.size() * 60;  // 1 hour per complex function
    
    return debt;
}

QString CodeIntelligence::readFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QTextStream in(&file);
    return in.readAll();
}

QStringList CodeIntelligence::parseSymbols(const QString& code) {
    QStringList symbols;
    QRegularExpression funcRe(R"((\w+)\s*\([^)]*\)\s*\{)");
    QRegularExpressionMatchIterator it = funcRe.globalMatch(code);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        symbols.append(match.captured(1));
    }
    
    return symbols;
}

int CodeIntelligence::calculateCyclomaticComplexity(const QString& code) {
    int complexity = 1;  // Base complexity
    
    // Count decision points
    complexity += code.count("if");
    complexity += code.count("else");
    complexity += code.count("for");
    complexity += code.count("while");
    complexity += code.count("case");
    complexity += code.count("&&");
    complexity += code.count("||");
    complexity += code.count("?");
    
    return complexity;
}

bool CodeIntelligence::isCodeReachable(const QString& code, int line) {
    QStringList lines = code.split('\n');
    if (line >= lines.size()) return false;
    
    // Check if line is after return statement in same block
    for (int i = line - 1; i >= 0; --i) {
        QString l = lines[i].trimmed();
        if (l.startsWith("return")) return false;
        if (l.contains("{")) break;  // Different block
    }
    
    return true;
}
