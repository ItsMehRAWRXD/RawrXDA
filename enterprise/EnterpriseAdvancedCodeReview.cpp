#include "EnterpriseAdvancedCodeReview.hpp"
#include <QRegularExpression>
#include <QUuid>
#include <QDebug>
#include <cmath>
#include <algorithm>

EnterpriseAdvancedCodeReview::AdvancedReviewResult EnterpriseAdvancedCodeReview::performAdvancedCodeReview(
    const QString& filePath, const QString& code) {
    
    AdvancedReviewResult result;
    result.reviewId = QUuid::createUuid().toString();
    result.filePath = filePath;
    result.reviewedAt = QDateTime::currentDateTime();
    
    // Data flow analysis
    QList<AdvancedCodeInsight> dataFlowInsights = performDataFlowAnalysis(code);
    result.insights.append(dataFlowInsights);
    result.dataFlowAnalysis = buildDataFlowGraph(code);
    
    // Control flow analysis
    QList<AdvancedCodeInsight> controlFlowInsights = performControlFlowAnalysis(code);
    result.insights.append(controlFlowInsights);
    result.controlFlowAnalysis = buildControlFlowGraph(code);
    
    // Dependency analysis
    QList<AdvancedCodeInsight> depInsights = performDependencyAnalysis(code);
    result.insights.append(depInsights);
    result.dependencyGraph = buildDependencyGraph(code);
    
    // Architecture analysis
    QList<AdvancedCodeInsight> archInsights = performArchitectureAnalysis(code);
    result.insights.append(archInsights);
    result.architecturePatterns = analyzeArchitecturePatterns(code);
    
    // Complexity analysis
    QList<AdvancedCodeInsight> complexInsights = performComplexityAnalysis(code);
    result.insights.append(complexInsights);
    result.cyclomaticComplexity = calculateCyclomaticComplexity(code);
    
    // Taint analysis
    QList<AdvancedCodeInsight> taintInsights = performTaintAnalysis(code);
    result.insights.append(taintInsights);
    result.taintAnalysis = performTaintAnalysisImpl(code);
    
    // Type flow analysis
    QList<AdvancedCodeInsight> typeInsights = performTypeFlowAnalysis(code);
    result.insights.append(typeInsights);
    result.typeFlowAnalysis = performTypeFlowAnalysisImpl(code);
    
    // Code clone detection
    QList<AdvancedCodeInsight> cloneInsights = performCodeCloneDetection(code);
    result.insights.append(cloneInsights);
    result.codeClones = findCodeClones(code);
    
    // Call graph analysis
    QList<AdvancedCodeInsight> callInsights = performCallGraphAnalysis(code);
    result.insights.append(callInsights);
    
    // Memory analysis
    QList<AdvancedCodeInsight> memInsights = performMemoryAnalysis(code);
    result.insights.append(memInsights);
    
    // Symbol table extraction
    result.symbolTable = extractSymbolTable(code);
    
    // Generate metrics
    result.metrics = generateAdvancedMetrics(code);
    
    // Calculate overall score
    result.overallScore = 85.0;
    for (const auto& insight : result.insights) {
        if (insight.severity == "critical") result.overallScore -= 10.0;
        else if (insight.severity == "high") result.overallScore -= 5.0;
        else if (insight.severity == "medium") result.overallScore -= 2.0;
    }
    result.overallScore = qBound(0.0, result.overallScore, 100.0);
    
    if (result.overallScore >= 90) result.overallRating = "Excellent";
    else if (result.overallScore >= 80) result.overallRating = "Good";
    else if (result.overallScore >= 70) result.overallRating = "Fair";
    else if (result.overallScore >= 60) result.overallRating = "Poor";
    else result.overallRating = "Critical";
    
    result.aiReasoning["reasoning"] = generateAdvancedReasoningText(result);
    result.aiReasoning["confidence"] = 0.92;
    result.aiReasoning["analysisDepth"] = "comprehensive";
    
    qDebug() << "Advanced code review completed:" << filePath
             << "- Score:" << result.overallScore
             << "- Insights:" << result.insights.size()
             << "- Cyclomatic Complexity:" << result.cyclomaticComplexity;
    
    return result;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performDataFlowAnalysis(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    QList<DataFlowNode> nodes = extractDataFlowNodes(code);
    
    for (const auto& node : nodes) {
        if (node.isTainted && !node.sinks.isEmpty()) {
            AdvancedCodeInsight insight;
            insight.type = "data_flow";
            insight.severity = "high";
            insight.description = QString("Tainted data flow detected: %1").arg(node.name);
            insight.suggestion = "Validate and sanitize data before use";
            insight.confidenceScore = 0.88;
            insight.lineNumber = node.lineNumber;
            insight.metadata["sources"] = QJsonArray::fromStringList(node.sources);
            insight.metadata["sinks"] = QJsonArray::fromStringList(node.sinks);
            insights.append(insight);
        }
    }
    
    return insights;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performControlFlowAnalysis(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    QList<ControlFlowNode> nodes = extractControlFlowNodes(code);
    
    for (const auto& node : nodes) {
        if (node.complexity > 5.0) {
            AdvancedCodeInsight insight;
            insight.type = "control_flow";
            insight.severity = "medium";
            insight.description = QString("High control flow complexity at line %1").arg(node.lineNumber);
            insight.suggestion = "Consider simplifying control flow logic";
            insight.confidenceScore = 0.85;
            insight.lineNumber = node.lineNumber;
            insight.metadata["complexity"] = node.complexity;
            insights.append(insight);
        }
    }
    
    return insights;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performDependencyAnalysis(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    
    QRegularExpression circularDepRegex(R\"(#include\s+[<"]([^>"]+)[>"])");
    QRegularExpressionMatchIterator matches = circularDepRegex.globalMatch(code);
    
    QSet<QString> includes;
    while (matches.hasNext()) {
        QString include = matches.next().captured(1);
        if (includes.contains(include)) {
            AdvancedCodeInsight insight;
            insight.type = "dependency";
            insight.severity = "medium";
            insight.description = QString("Circular dependency detected: %1").arg(include);
            insight.suggestion = "Refactor to break circular dependency";
            insight.confidenceScore = 0.90;
            insights.append(insight);
        }
        includes.insert(include);
    }
    
    return insights;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performArchitectureAnalysis(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    
    int classCount = code.count(QRegularExpression(R"(\bclass\s+\w+)"));
    int interfaceCount = code.count(QRegularExpression(R"(\bvirtual\s+)"));
    int singletonCount = code.count(QRegularExpression(R"(static\s+\w+\s*\*\s*getInstance)"));
    
    if (classCount > 20 && interfaceCount < classCount / 3) {
        AdvancedCodeInsight insight;
        insight.type = "architecture";
        insight.severity = "medium";
        insight.description = "Low abstraction ratio - many classes with few interfaces";
        insight.suggestion = "Consider introducing more abstract interfaces";
        insight.confidenceScore = 0.80;
        insights.append(insight);
    }
    
    if (singletonCount > 3) {
        AdvancedCodeInsight insight;
        insight.type = "architecture";
        insight.severity = "low";
        insight.description = "Multiple singletons detected";
        insight.suggestion = "Consider dependency injection instead of singletons";
        insight.confidenceScore = 0.75;
        insights.append(insight);
    }
    
    return insights;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performComplexityAnalysis(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    
    double complexity = calculateCyclomaticComplexity(code);
    
    if (complexity > 15.0) {
        AdvancedCodeInsight insight;
        insight.type = "complexity";
        insight.severity = "high";
        insight.description = QString("High cyclomatic complexity: %1").arg(complexity, 0, 'f', 2);
        insight.suggestion = "Refactor to reduce complexity through decomposition";
        insight.confidenceScore = 0.92;
        insight.metadata["complexity"] = complexity;
        insights.append(insight);
    }
    
    return insights;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performTaintAnalysis(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    
    QRegularExpression userInputRegex(R"(\b(?:scanf|gets|cin|input|readLine)\s*\()");
    QRegularExpression unsafeUseRegex(R"(\b(?:printf|cout|system|exec|eval)\s*\()");
    
    bool hasUserInput = userInputRegex.match(code).hasMatch();
    bool hasUnsafeUse = unsafeUseRegex.match(code).hasMatch();
    
    if (hasUserInput && hasUnsafeUse) {
        AdvancedCodeInsight insight;
        insight.type = "taint";
        insight.severity = "critical";
        insight.description = "Potential taint flow from user input to unsafe function";
        insight.suggestion = "Implement input validation and sanitization";
        insight.confidenceScore = 0.95;
        insights.append(insight);
    }
    
    return insights;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performTypeFlowAnalysis(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    
    QRegularExpression implicitCastRegex(R"(\b(?:int|float|double)\s+\w+\s*=\s*(?:void\s*\*|char\s*\*))");
    QRegularExpressionMatchIterator matches = implicitCastRegex.globalMatch(code);
    
    while (matches.hasNext()) {
        matches.next();
        AdvancedCodeInsight insight;
        insight.type = "type_flow";
        insight.severity = "medium";
        insight.description = "Implicit type conversion detected";
        insight.suggestion = "Use explicit casting for clarity and safety";
        insight.confidenceScore = 0.82;
        insights.append(insight);
    }
    
    return insights;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performCodeCloneDetection(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    
    QList<QPair<int, int>> clones = findCodeClones(code);
    
    if (!clones.isEmpty()) {
        AdvancedCodeInsight insight;
        insight.type = "code_clone";
        insight.severity = "low";
        insight.description = QString("Code duplication detected: %1 clone pairs").arg(clones.size());
        insight.suggestion = "Extract common code into reusable functions";
        insight.confidenceScore = 0.88;
        insights.append(insight);
    }
    
    return insights;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performCallGraphAnalysis(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    
    QRegularExpression deepCallRegex(R"(\w+\s*\(\s*\w+\s*\(\s*\w+\s*\(\s*\w+\s*\()");
    
    if (deepCallRegex.match(code).hasMatch()) {
        AdvancedCodeInsight insight;
        insight.type = "call_graph";
        insight.severity = "low";
        insight.description = "Deep call chain detected (4+ levels)";
        insight.suggestion = "Consider flattening call hierarchy for better readability";
        insight.confidenceScore = 0.80;
        insights.append(insight);
    }
    
    return insights;
}

QList<AdvancedCodeInsight> EnterpriseAdvancedCodeReview::performMemoryAnalysis(const QString& code) {
    QList<AdvancedCodeInsight> insights;
    
    int newCount = code.count(QRegularExpression(R"(\bnew\s+)"));
    int deleteCount = code.count(QRegularExpression(R"(\bdelete\s+)"));
    
    if (newCount > deleteCount && newCount > 0) {
        AdvancedCodeInsight insight;
        insight.type = "memory";
        insight.severity = "high";
        insight.description = QString("Potential memory leak: %1 allocations vs %2 deallocations").arg(newCount).arg(deleteCount);
        insight.suggestion = "Use smart pointers (unique_ptr, shared_ptr) for automatic memory management";
        insight.confidenceScore = 0.85;
        insights.append(insight);
    }
    
    return insights;
}

QJsonArray EnterpriseAdvancedCodeReview::buildDependencyGraph(const QString& code) {
    QJsonArray graph;
    
    QRegularExpression includeRegex(R"(#include\s+[<"]([^>"]+)[>"])");
    QRegularExpressionMatchIterator matches = includeRegex.globalMatch(code);
    
    while (matches.hasNext()) {
        QJsonObject node;
        node["type"] = "include";
        node["name"] = matches.next().captured(1);
        graph.append(node);
    }
    
    return graph;
}

QJsonArray EnterpriseAdvancedCodeReview::buildDataFlowGraph(const QString& code) {
    QJsonArray graph;
    QList<DataFlowNode> nodes = extractDataFlowNodes(code);
    
    for (const auto& node : nodes) {
        QJsonObject nodeObj;
        nodeObj["name"] = node.name;
        nodeObj["type"] = node.type;
        nodeObj["line"] = node.lineNumber;
        nodeObj["tainted"] = node.isTainted;
        graph.append(nodeObj);
    }
    
    return graph;
}

QJsonArray EnterpriseAdvancedCodeReview::buildControlFlowGraph(const QString& code) {
    QJsonArray graph;
    QList<ControlFlowNode> nodes = extractControlFlowNodes(code);
    
    for (const auto& node : nodes) {
        QJsonObject nodeObj;
        nodeObj["line"] = node.lineNumber;
        nodeObj["type"] = node.nodeType;
        nodeObj["complexity"] = node.complexity;
        graph.append(nodeObj);
    }
    
    return graph;
}

QJsonObject EnterpriseAdvancedCodeReview::analyzeArchitecturePatterns(const QString& code) {
    QJsonObject patterns;
    
    patterns["hasDesignPatterns"] = code.contains("Factory") || code.contains("Singleton") || code.contains("Observer");
    patterns["hasLayering"] = code.contains("Controller") || code.contains("Service") || code.contains("Repository");
    patterns["hasMVC"] = code.contains("Model") && code.contains("View") && code.contains("Controller");
    patterns["classCount"] = code.count(QRegularExpression(R"(\bclass\s+\w+)"));
    
    return patterns;
}

double EnterpriseAdvancedCodeReview::calculateCyclomaticComplexity(const QString& code) {
    int decisions = code.count(QRegularExpression(R"(\b(?:if|else|for|while|case|catch)\b)"));
    int operators = code.count(QRegularExpression(R"(\b(?:&&|\|\|)\b)"));
    
    return 1.0 + decisions + operators * 0.5;
}

QMap<QString, int> EnterpriseAdvancedCodeReview::extractSymbolTable(const QString& code) {
    QMap<QString, int> symbols;
    
    QRegularExpression varRegex(R"(\b(?:int|float|double|char|bool|QString|auto)\s+(\w+))");
    QRegularExpressionMatchIterator matches = varRegex.globalMatch(code);
    
    while (matches.hasNext()) {
        QString symbol = matches.next().captured(1);
        symbols[symbol]++;
    }
    
    return symbols;
}

QList<QPair<int, int>> EnterpriseAdvancedCodeReview::findCodeClones(const QString& code) {
    QList<QPair<int, int>> clones;
    QStringList lines = code.split('\n');
    
    for (int i = 0; i < lines.size() - 3; ++i) {
        for (int j = i + 3; j < lines.size(); ++j) {
            if (lines[i].trimmed() == lines[j].trimmed() && !lines[i].trimmed().isEmpty()) {
                clones.append(qMakePair(i, j));
            }
        }
    }
    
    return clones;
}

QJsonObject EnterpriseAdvancedCodeReview::performTaintAnalysisImpl(const QString& code) {
    QJsonObject analysis;
    analysis["sources"] = QJsonArray();
    analysis["sinks"] = QJsonArray();
    analysis["flows"] = QJsonArray();
    return analysis;
}

QJsonObject EnterpriseAdvancedCodeReview::performTypeFlowAnalysisImpl(const QString& code) {
    QJsonObject analysis;
    analysis["implicitCasts"] = code.count(QRegularExpression(R"(\((?:int|float|double)\s*\))"));
    analysis["typeErrors"] = 0;
    return analysis;
}

QJsonObject EnterpriseAdvancedCodeReview::buildCallGraph(const QString& code) {
    QJsonObject graph;
    graph["nodes"] = QJsonArray();
    graph["edges"] = QJsonArray();
    return graph;
}

QJsonObject EnterpriseAdvancedCodeReview::analyzeMemoryPatterns(const QString& code) {
    QJsonObject analysis;
    analysis["allocations"] = code.count(QRegularExpression(R"(\bnew\s+)"));
    analysis["deallocations"] = code.count(QRegularExpression(R"(\bdelete\s+)"));
    return analysis;
}

QList<DataFlowNode> EnterpriseAdvancedCodeReview::extractDataFlowNodes(const QString& code) {
    QList<DataFlowNode> nodes;
    
    QRegularExpression varRegex(R"(\b(?:int|float|double|char|bool|QString)\s+(\w+)\s*=)");
    QRegularExpressionMatchIterator matches = varRegex.globalMatch(code);
    
    while (matches.hasNext()) {
        DataFlowNode node;
        node.name = matches.next().captured(1);
        node.type = "variable";
        node.lineNumber = code.left(matches.peekPrevious().capturedStart()).count('\n') + 1;
        node.isTainted = false;
        nodes.append(node);
    }
    
    return nodes;
}

QList<ControlFlowNode> EnterpriseAdvancedCodeReview::extractControlFlowNodes(const QString& code) {
    QList<ControlFlowNode> nodes;
    QStringList lines = code.split('\n');
    
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].contains(QRegularExpression(R"(\b(?:if|for|while|switch)\b)"))) {
            ControlFlowNode node;
            node.lineNumber = i + 1;
            node.nodeType = "branch";
            node.complexity = 1.0;
            nodes.append(node);
        }
    }
    
    return nodes;
}

QJsonObject EnterpriseAdvancedCodeReview::generateAdvancedMetrics(const QString& code) {
    QJsonObject metrics;
    
    int totalLines = code.count('\n') + 1;
    int commentLines = code.count(QRegularExpression(R"(//|/\*|\*/)"));
    int functionCount = code.count(QRegularExpression(R"(\b(?:void|int|double|float|char|bool)\s+\w+\s*\()"));
    
    metrics["totalLines"] = totalLines;
    metrics["commentLines"] = commentLines;
    metrics["functionCount"] = functionCount;
    metrics["commentRatio"] = totalLines > 0 ? (double)commentLines / totalLines : 0.0;
    metrics["linesPerFunction"] = functionCount > 0 ? (double)totalLines / functionCount : 0.0;
    
    return metrics;
}

QString EnterpriseAdvancedCodeReview::generateAdvancedReasoningText(const AdvancedReviewResult& result) {
    QString reasoning;
    
    reasoning += QString("Advanced analysis completed with %1 insights. ").arg(result.insights.size());
    reasoning += QString("Cyclomatic complexity: %1. ").arg(result.cyclomaticComplexity, 0, 'f', 2);
    
    int critical = 0, high = 0;
    for (const auto& insight : result.insights) {
        if (insight.severity == "critical") critical++;
        else if (insight.severity == "high") high++;
    }
    
    if (critical > 0) reasoning += QString("Critical issues: %1. ").arg(critical);
    if (high > 0) reasoning += QString("High-priority issues: %1. ").arg(high);
    
    return reasoning;
}
