#ifndef REFACTORINGPLUGIN_H
#define REFACTORINGPLUGIN_H

#include "../plugins/AgenticPlugin.h"
#include <QTimer>
#include <QMap>
#include <QRegularExpression>

/**
 * @brief RefactoringPlugin
 *
 * Detects common code smells and offers automated refactorings.
 * Supports C++, Python, JavaScript, TypeScript, Java.
 */
class RefactoringPlugin : public AgenticPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.rawrxd.AgenticPlugin/1.0" FILE "RefactoringPlugin.json")
    Q_INTERFACES(AgenticPlugin)

public:
    enum Severity {
        Critical = 0,
        High = 1,
        Medium = 2,
        Low = 3,
        Info = 4
    };

    struct Smell {
        QString type;           // e.g., "Long Method"
        Severity severity;
        QString description;
        QString location;       // file + line
        int line;
        int column;
        QString codeSnippet;
        QString recommendation;
        QStringList cwe;
        
        QJsonObject toJson() const;
        static Smell fromJson(const QJsonObject& obj);
    };

    explicit RefactoringPlugin(QObject *parent = nullptr);
    ~RefactoringPlugin();

    // Plugin metadata
    QString name() const override { return "RefactoringPlugin"; }
    QString version() const override { return "1.0.0"; }
    QString description() const override { return "Detects code smells and offers automated refactorings"; }
    QStringList capabilities() const override { return {"refactoring", "smell-detection"}; }

    // Core plugin methods
    bool initialize(const QJsonObject& config) override;
    void cleanup() override;
    QJsonObject executeAction(const QString& action, const QJsonObject& params) override;
    QStringList getAvailableActions() const override;

    // Event hooks
    void onCodeAnalyzed(const QString& code, const QString& context) override;
    void onFileOpened(const QString& filePath) override;

public slots:
    // Analysis methods
    QJsonObject analyzeCode(const QString& code, const QString& language);
    QJsonObject analyzeFile(const QString& filePath);
    QJsonObject analyzeDirectory(const QString& dirPath);

    // Refactoring methods
    QJsonObject refactorCode(const QString& code, const QString& language, const QString& smellType);
    QJsonObject refactorFile(const QString& filePath, const QString& smellType);

private:
    bool m_initialized;
    QTimer* m_scanTimer;
    Severity m_severityThreshold;
    bool m_autoFixEnabled;
    QStringList m_excludePatterns;

    // Smell detection patterns
    struct SmellPattern {
        QString name;
        Severity severity;
        QRegularExpression pattern;
        QString description;
        QString recommendation;
        QStringList cwe;
    };
    QList<SmellPattern> m_patterns;

    // Helper methods
    void initializePatterns();
    QList<Smell> detectLongMethods(const QString& code, const QString& language);
    QList<Smell> detectDuplicateCode(const QString& code, const QString& language);
    QList<Smell> detectLargeClasses(const QString& code, const QString& language);
    QList<Smell> detectFeatureEnvy(const QString& code, const QString& language);
    QList<Smell> detectDataClumps(const QString& code, const QString& language);
    QList<Smell> detectPrimitiveObsession(const QString& code, const QString& language);
    QList<Smell> detectSwitchStatements(const QString& code, const QString& language);
    QList<Smell> detectLargeParameterLists(const QString& code, const QString& language);
    QList<Smell> detectLongParameterLists(const QString& code, const QString& language);
    QList<Smell> detectLongVariableNames(const QString& code, const QString& language);
    QList<Smell> detectLongMethodNames(const QString& code, const QString& language);
    QList<Smell> detectLongVariableDeclarations(const QString& code, const QString& language);
    QList<Smell> detectLongMethodDeclarations(const QString& code, const QString& language);
    QList<Smell> detectLongVariableAssignments(const QString& code, const QString& language);
    QList<Smell> detectLongMethodAssignments(const QString& code, const QString& language);
    QList<Smell> detectLongVariableUsage(const QString& code, const QString& language);
    QList<Smell> detectLongMethodUsage(const QString& code, const QString& language);
    QList<Smell> detectLongVariableUsageInLoops(const QString& code, const QString& language);
    QList<Smell> detectLongMethodUsageInLoops(const QString& code, const QString& language);
    QList<Smell> detectLongVariableUsageInConditionals(const QString& code, const QString& language);
    QList<Smell> detectLongMethodUsageInConditionals(const QString& code, const QString& language);
    QList<Smell> detectLongVariableUsageInSwitches(const QString& code, const QString& language);
    QList<Smell> detectLongMethodUsageInSwitches(const QString& code, const QString& language);
    QList<Smell> detectLongVariableUsageInFunctions(const QString& code, const QString& language);
    QList<Smell> detectLongMethodUsageInFunctions(const QString& code, const QString& language);
    QList<Smell> detectLongVariableUsageInClasses(const QString& code, const QString& language);
    QList<Smell> detectLongMethodUsageInClasses(const QString& code, const QString& language);
    
    // Auto-fix helpers
    QString extractMethodBody(const QString& code, int startLine, int endLine);
    QString extractMethodSignature(const QString& code, int line);
    QString generateExtractedMethod(const QString& methodName, const QString& methodBody, const QString& returnType, const QStringList& parameters);
    QString replaceMethodBody(const QString& code, int startLine, int endLine, const QString& newBody);
    
    // Utility
    bool shouldExclude(const QString& path);
    QString detectLanguage(const QString& filePath);
    QJsonObject generateReport(const QList<Smell>& smells);
};

#endif // REFACTORINGPLUGIN_H
