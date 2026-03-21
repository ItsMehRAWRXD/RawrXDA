#ifndef IDE_CONTEXT_MENU_SYSTEM_HPP
#define IDE_CONTEXT_MENU_SYSTEM_HPP

#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QVariant>

struct ContextMenuAction {
    QString id;
    QString label;
    QString category;
    QString icon;
    QString description;
    QString shortcut;
    bool enabled;
    int priority;
    QList<QString> applicableLanguages;
    QJsonObject metadata;
};

struct ContextMenuSection {
    QString name;
    QList<ContextMenuAction> actions;
    int displayOrder;
};

struct CodeSelection {
    QString selectedText;
    int startLine;
    int endLine;
    int startColumn;
    int endColumn;
    QString filePath;
    QString fileLanguage;
    QString lineContent;
    QList<QString> selectedLines;
    QString symbolAtCursor;
    QJsonObject semanticContext;
};

class IDEContextMenuSystem {
public:
    static QList<ContextMenuSection> buildContextMenu(const CodeSelection& selection);
    static QJsonObject executeAction(const QString& actionId, const CodeSelection& selection);
    
    // Analysis Actions
    static QJsonObject analyzeSelection(const CodeSelection& selection);
    static QJsonObject findAllReferences(const CodeSelection& selection);
    static QJsonObject findDefinition(const CodeSelection& selection);
    static QJsonObject showCallHierarchy(const CodeSelection& selection);
    static QJsonObject showTypeHierarchy(const CodeSelection& selection);
    
    // Refactoring Actions
    static QJsonObject extractMethod(const CodeSelection& selection);
    static QJsonObject extractVariable(const CodeSelection& selection);
    static QJsonObject inlineVariable(const CodeSelection& selection);
    static QJsonObject renameSymbol(const CodeSelection& selection);
    static QJsonObject changeSignature(const CodeSelection& selection);
    static QJsonObject moveToNewFile(const CodeSelection& selection);
    
    // Code Generation Actions
    static QJsonObject generateDocumentation(const CodeSelection& selection);
    static QJsonObject generateUnitTest(const CodeSelection& selection);
    static QJsonObject generateImplementation(const CodeSelection& selection);
    static QJsonObject generateGettersSetters(const CodeSelection& selection);
    
    // Security Actions
    static QJsonObject performSecurityAnalysis(const CodeSelection& selection);
    static QJsonObject suggestSecurityFixes(const CodeSelection& selection);
    static QJsonObject validateInput(const CodeSelection& selection);
    static QJsonObject checkTaintFlow(const CodeSelection& selection);
    
    // Performance Actions
    static QJsonObject analyzePerformance(const CodeSelection& selection);
    static QJsonObject suggestOptimizations(const CodeSelection& selection);
    static QJsonObject profileComplexity(const CodeSelection& selection);
    static QJsonObject detectBottlenecks(const CodeSelection& selection);
    
    // Code Quality Actions
    static QJsonObject checkCodeSmells(const CodeSelection& selection);
    static QJsonObject suggestRefactorings(const CodeSelection& selection);
    static QJsonObject calculateMetrics(const CodeSelection& selection);
    static QJsonObject checkCompliance(const CodeSelection& selection);
    
    // Navigation Actions
    static QJsonObject navigateToImplementation(const CodeSelection& selection);
    static QJsonObject navigateToDeclaration(const CodeSelection& selection);
    static QJsonObject navigateToUsages(const CodeSelection& selection);
    static QJsonObject showDependencies(const CodeSelection& selection);
    
    // Formatting Actions
    static QJsonObject formatSelection(const CodeSelection& selection);
    static QJsonObject sortLines(const CodeSelection& selection);
    static QJsonObject alignCode(const CodeSelection& selection);
    static QJsonObject normalizeWhitespace(const CodeSelection& selection);
    
    // Advanced Analysis Actions
    static QJsonObject performDataFlowAnalysis(const CodeSelection& selection);
    static QJsonObject performControlFlowAnalysis(const CodeSelection& selection);
    static QJsonObject detectCodeClones(const CodeSelection& selection);
    static QJsonObject buildDependencyGraph(const CodeSelection& selection);
    
    // AI-Powered Actions
    static QJsonObject explainCode(const CodeSelection& selection);
    static QJsonObject suggestImprovements(const CodeSelection& selection);
    static QJsonObject generateAlternatives(const CodeSelection& selection);
    static QJsonObject findSimilarPatterns(const CodeSelection& selection);
    
private:
    static QList<ContextMenuSection> buildAnalysisSection(const CodeSelection& selection);
    static QList<ContextMenuSection> buildRefactoringSection(const CodeSelection& selection);
    static QList<ContextMenuSection> buildGenerationSection(const CodeSelection& selection);
    static QList<ContextMenuSection> buildSecuritySection(const CodeSelection& selection);
    static QList<ContextMenuSection> buildPerformanceSection(const CodeSelection& selection);
    static QList<ContextMenuSection> buildQualitySection(const CodeSelection& selection);
    static QList<ContextMenuSection> buildNavigationSection(const CodeSelection& selection);
    static QList<ContextMenuSection> buildFormattingSection(const CodeSelection& selection);
    static QList<ContextMenuSection> buildAdvancedSection(const CodeSelection& selection);
    static QList<ContextMenuSection> buildAISection(const CodeSelection& selection);
    
    static bool isSymbolSelected(const CodeSelection& selection);
    static bool isMultilineSelected(const CodeSelection& selection);
    static bool isFunctionSelected(const CodeSelection& selection);
    static bool isClassSelected(const CodeSelection& selection);
    static bool isLoopSelected(const CodeSelection& selection);
    static bool isConditionalSelected(const CodeSelection& selection);
};

#endif // IDE_CONTEXT_MENU_SYSTEM_HPP
