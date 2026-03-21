#include "IDESemanticAnalysisEngine.hpp"
#include <QRegularExpression>
#include <QDebug>
#include <algorithm>

QList<InlineInsight> IDESemanticAnalysisEngine::analyzeAtCursor(const SemanticContext& context) {
    QList<InlineInsight> insights;
    
    insights.append(performSyntacticAnalysis(context));
    insights.append(performSemanticAnalysis(context));
    insights.append(performTypeAnalysis(context));
    insights.append(performFlowAnalysis(context));
    
    std::sort(insights.begin(), insights.end(), 
        [&context](const InlineInsight& a, const InlineInsight& b) {
            return calculateRelevanceScore(a, context) > calculateRelevanceScore(b, context);
        });
    
    return insights;
}

QList<ContextualRecommendation> IDESemanticAnalysisEngine::generateContextualRecommendations(
    const SemanticContext& context) {
    QList<ContextualRecommendation> recommendations;
    
    recommendations.append(generateRefactoringRecommendations(context));
    recommendations.append(generatePerformanceRecommendations(context));
    recommendations.append(generateSecurityRecommendations(context));
    recommendations.append(generateDesignPatternRecommendations(context));
    
    return recommendations;
}

QJsonObject IDESemanticAnalysisEngine::performRealTimeAnalysis(const QString& code, int cursorPosition) {
    QJsonObject analysis;
    
    int line = code.left(cursorPosition).count('\n') + 1;
    int column = cursorPosition - code.lastIndexOf('\n', cursorPosition - 1);
    
    SemanticContext context;
    context.cursorLine = line;
    context.cursorColumn = column;
    
    QStringList lines = code.split('\n');
    if (line - 1 < lines.size()) {
        context.lineContent = lines[line - 1];
    }
    
    QList<InlineInsight> insights = analyzeAtCursor(context);
    QJsonArray insightsArray;
    
    for (const auto& insight : insights) {
        QJsonObject obj;
        obj["line"] = insight.line;
        obj["column"] = insight.column;
        obj["message"] = insight.message;
        obj["severity"] = insight.severity;
        obj["relevance"] = insight.relevanceScore;
        insightsArray.append(obj);
    }
    
    analysis["insights"] = insightsArray;
    analysis["cursorLine"] = line;
    analysis["cursorColumn"] = column;
    
    return analysis;
}

QList<QString> IDESemanticAnalysisEngine::suggestRefactorings(const SemanticContext& context) {
    QList<QString> suggestions;
    
    if (context.lineContent.length() > 120) {
        suggestions.append("Line too long - consider breaking into multiple lines");
    }
    
    if (context.lineContent.count('{') > 2) {
        suggestions.append("High nesting level - consider extracting to separate function");
    }
    
    if (context.lineContent.contains(QRegularExpression(R"(\bif\s*\([^)]*\)\s*\{[^}]*\}\s*else\s*\{[^}]*\})"))) {
        suggestions.append("Consider using ternary operator for simple if-else");
    }
    
    return suggestions;
}

QJsonArray IDESemanticAnalysisEngine::buildSemanticIndex(const QString& code) {
    QJsonArray index;
    
    QRegularExpression classRegex(R"(\bclass\s+(\w+))");
    QRegularExpression funcRegex(R"(\b(?:void|int|double|float|bool|QString)\s+(\w+)\s*\()");
    QRegularExpression varRegex(R"(\b(?:int|float|double|char|bool|QString)\s+(\w+))");
    
    QRegularExpressionMatchIterator classMatches = classRegex.globalMatch(code);
    while (classMatches.hasNext()) {
        QJsonObject entry;
        entry["type"] = "class";
        entry["name"] = classMatches.next().captured(1);
        entry["line"] = code.left(classMatches.peekPrevious().capturedStart()).count('\n') + 1;
        index.append(entry);
    }
    
    QRegularExpressionMatchIterator funcMatches = funcRegex.globalMatch(code);
    while (funcMatches.hasNext()) {
        QJsonObject entry;
        entry["type"] = "function";
        entry["name"] = funcMatches.next().captured(1);
        entry["line"] = code.left(funcMatches.peekPrevious().capturedStart()).count('\n') + 1;
        index.append(entry);
    }
    
    return index;
}

QList<QString> IDESemanticAnalysisEngine::findRelatedSymbols(const SemanticContext& context) {
    QList<QString> related;
    
    QString symbol = extractSymbolAtCursor(context);
    if (symbol.isEmpty()) return related;
    
    QRegularExpression usage(QString("\\b%1\\b").arg(symbol));
    
    for (const auto& line : context.visibleLines) {
        if (usage.match(line).hasMatch()) {
            related.append(line.trimmed());
        }
    }
    
    return related;
}

QJsonObject IDESemanticAnalysisEngine::analyzeCodePattern(const QString& code, int startLine, int endLine) {
    QJsonObject pattern;
    
    QStringList lines = code.split('\n');
    QString selectedCode;
    
    for (int i = startLine; i <= endLine && i < lines.size(); ++i) {
        selectedCode += lines[i] + "\n";
    }
    
    pattern["lines"] = endLine - startLine + 1;
    pattern["hasLoop"] = selectedCode.contains(QRegularExpression(R"(\b(?:for|while)\b)"));
    pattern["hasConditional"] = selectedCode.contains(QRegularExpression(R"(\b(?:if|switch)\b)"));
    pattern["hasFunctionCall"] = selectedCode.contains(QRegularExpression(R"(\w+\s*\()"));
    pattern["complexity"] = selectedCode.count(QRegularExpression(R"(\b(?:if|for|while|case)\b)"));
    
    return pattern;
}

QList<InlineInsight> IDESemanticAnalysisEngine::performSyntacticAnalysis(const SemanticContext& context) {
    QList<InlineInsight> insights;
    
    if (context.lineContent.count('(') != context.lineContent.count(')')) {
        InlineInsight insight;
        insight.line = context.cursorLine;
        insight.column = context.cursorColumn;
        insight.message = "Mismatched parentheses";
        insight.severity = "error";
        insight.actionType = "fix";
        insight.relevanceScore = 0.95;
        insights.append(insight);
    }
    
    if (context.lineContent.count('{') != context.lineContent.count('}')) {
        InlineInsight insight;
        insight.line = context.cursorLine;
        insight.column = context.cursorColumn;
        insight.message = "Mismatched braces";
        insight.severity = "error";
        insight.actionType = "fix";
        insight.relevanceScore = 0.95;
        insights.append(insight);
    }
    
    return insights;
}

QList<InlineInsight> IDESemanticAnalysisEngine::performSemanticAnalysis(const SemanticContext& context) {
    QList<InlineInsight> insights;
    
    QString symbol = extractSymbolAtCursor(context);
    if (!symbol.isEmpty()) {
        QList<QString> definitions = findSymbolDefinitions(symbol, context.lineContent);
        if (definitions.isEmpty()) {
            InlineInsight insight;
            insight.line = context.cursorLine;
            insight.column = context.cursorColumn;
            insight.message = QString("Undefined symbol: %1").arg(symbol);
            insight.severity = "warning";
            insight.actionType = "navigate";
            insight.relevanceScore = 0.90;
            insights.append(insight);
        }
    }
    
    return insights;
}

QList<InlineInsight> IDESemanticAnalysisEngine::performTypeAnalysis(const SemanticContext& context) {
    QList<InlineInsight> insights;
    
    QRegularExpression implicitCast(R"(\b(?:int|float|double)\s+\w+\s*=\s*(?:void\s*\*|char\s*\*))");
    
    if (implicitCast.match(context.lineContent).hasMatch()) {
        InlineInsight insight;
        insight.line = context.cursorLine;
        insight.column = context.cursorColumn;
        insight.message = "Implicit type conversion - use explicit cast";
        insight.severity = "warning";
        insight.actionType = "suggest";
        insight.relevanceScore = 0.80;
        insights.append(insight);
    }
    
    return insights;
}

QList<InlineInsight> IDESemanticAnalysisEngine::performFlowAnalysis(const SemanticContext& context) {
    QList<InlineInsight> insights;
    
    if (context.lineContent.contains("return") && context.lineContent.contains("=")) {
        InlineInsight insight;
        insight.line = context.cursorLine;
        insight.column = context.cursorColumn;
        insight.message = "Assignment before return - consider simplifying";
        insight.severity = "info";
        insight.actionType = "suggest";
        insight.relevanceScore = 0.70;
        insights.append(insight);
    }
    
    return insights;
}

QList<ContextualRecommendation> IDESemanticAnalysisEngine::generateRefactoringRecommendations(
    const SemanticContext& context) {
    QList<ContextualRecommendation> recommendations;
    
    if (context.lineContent.length() > 100) {
        ContextualRecommendation rec;
        rec.type = "refactoring";
        rec.title = "Extract Long Line";
        rec.description = "This line is too long and should be split";
        rec.confidence = 0.85;
        rec.rationale = "Improves readability and maintainability";
        recommendations.append(rec);
    }
    
    return recommendations;
}

QList<ContextualRecommendation> IDESemanticAnalysisEngine::generatePerformanceRecommendations(
    const SemanticContext& context) {
    QList<ContextualRecommendation> recommendations;
    
    if (context.lineContent.contains(QRegularExpression(R"(\bstrcpy\b)"))) {
        ContextualRecommendation rec;
        rec.type = "performance";
        rec.title = "Use Safe String Functions";
        rec.description = "strcpy is unsafe and slow - use strncpy or std::string";
        rec.confidence = 0.95;
        rec.rationale = "Prevents buffer overflows and improves performance";
        recommendations.append(rec);
    }
    
    return recommendations;
}

QList<ContextualRecommendation> IDESemanticAnalysisEngine::generateSecurityRecommendations(
    const SemanticContext& context) {
    QList<ContextualRecommendation> recommendations;
    
    if (context.lineContent.contains(QRegularExpression(R"(\bscanf\b|\bgets\b)"))) {
        ContextualRecommendation rec;
        rec.type = "security";
        rec.title = "Validate User Input";
        rec.description = "User input functions require validation and sanitization";
        rec.confidence = 0.98;
        rec.rationale = "Prevents injection attacks and buffer overflows";
        recommendations.append(rec);
    }
    
    return recommendations;
}

QList<ContextualRecommendation> IDESemanticAnalysisEngine::generateDesignPatternRecommendations(
    const SemanticContext& context) {
    QList<ContextualRecommendation> recommendations;
    
    if (context.lineContent.contains("new") && !context.lineContent.contains("unique_ptr")) {
        ContextualRecommendation rec;
        rec.type = "design_pattern";
        rec.title = "Use Smart Pointers";
        rec.description = "Consider using unique_ptr or shared_ptr instead of raw new";
        rec.confidence = 0.90;
        rec.rationale = "Automatic memory management prevents leaks";
        recommendations.append(rec);
    }
    
    return recommendations;
}

QString IDESemanticAnalysisEngine::extractSymbolAtCursor(const SemanticContext& context) {
    QRegularExpression symbolRegex(R"(\b\w+\b)");
    QRegularExpressionMatchIterator matches = symbolRegex.globalMatch(context.lineContent);
    
    while (matches.hasNext()) {
        QRegularExpressionMatch match = matches.next();
        if (match.capturedStart() <= context.cursorColumn && 
            context.cursorColumn <= match.capturedEnd()) {
            return match.captured(0);
        }
    }
    
    return QString();
}

QList<QString> IDESemanticAnalysisEngine::findSymbolDefinitions(const QString& symbol, const QString& code) {
    QList<QString> definitions;
    
    QRegularExpression defRegex(QString("\\b(?:class|struct|void|int|double|float|bool)\\s+%1\\b").arg(symbol));
    QRegularExpressionMatchIterator matches = defRegex.globalMatch(code);
    
    while (matches.hasNext()) {
        definitions.append(matches.next().captured(0));
    }
    
    return definitions;
}

QList<QString> IDESemanticAnalysisEngine::findSymbolUsages(const QString& symbol, const QString& code) {
    QList<QString> usages;
    
    QRegularExpression usageRegex(QString("\\b%1\\b").arg(symbol));
    QRegularExpressionMatchIterator matches = usageRegex.globalMatch(code);
    
    while (matches.hasNext()) {
        usages.append(matches.next().captured(0));
    }
    
    return usages;
}

QJsonObject IDESemanticAnalysisEngine::buildCallHierarchy(const QString& code) {
    QJsonObject hierarchy;
    hierarchy["nodes"] = QJsonArray();
    hierarchy["edges"] = QJsonArray();
    return hierarchy;
}

QJsonObject IDESemanticAnalysisEngine::buildInheritanceHierarchy(const QString& code) {
    QJsonObject hierarchy;
    hierarchy["nodes"] = QJsonArray();
    hierarchy["edges"] = QJsonArray();
    return hierarchy;
}

double IDESemanticAnalysisEngine::calculateRelevanceScore(const InlineInsight& insight, 
                                                         const SemanticContext& context) {
    double score = insight.relevanceScore;
    
    if (insight.line == context.cursorLine) {
        score *= 1.5;
    }
    
    if (insight.severity == "error") {
        score *= 1.3;
    } else if (insight.severity == "warning") {
        score *= 1.1;
    }
    
    return score;
}
