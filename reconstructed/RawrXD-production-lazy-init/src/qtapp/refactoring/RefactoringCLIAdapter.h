#ifndef REFACTORING_CLI_ADAPTER_H
#define REFACTORING_CLI_ADAPTER_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include "AdvancedRefactoring.h"

/**
 * @brief CLI adapter for the AdvancedRefactoring engine
 * 
 * Provides command-line interface to all refactoring operations.
 * Converts CLI arguments to refactoring engine calls.
 * 
 * CLI Commands:
 *   refactor extract-method <file> <start-line> <end-line> <method-name> [return-type]
 *   refactor extract-function <file> <start-line> <end-line> <function-name> [return-type]
 *   refactor inline-variable <file> <variable-name> <scope>
 *   refactor inline-method <file> <method-name> <scope>
 *   refactor inline-constant <file> <constant-name>
 *   refactor rename <file> <old-name> <new-name> [symbol-type]
 *   refactor move-class <source-file> <target-file> <class-name>
 *   refactor change-signature <file> <method-name> <class-name> <new-signature>
 *   refactor add-parameter <file> <method-name> <class-name> <param-type> <param-name> [default]
 *   refactor remove-parameter <file> <method-name> <class-name> <param-name>
 *   refactor reorder-parameters <file> <method-name> <class-name> <new-order>
 *   refactor convert-loop <file> <start-line> <end-line> <target-type>
 *   refactor convert-conditional <file> <start-line> <end-line> [--ternary]
 *   refactor optimize-includes <file>
 *   refactor remove-unused <file>
 *   refactor remove-dead-code <file>
 *   refactor extract-interface <file> <class-name> <interface-name> <methods>
 *   refactor extract-base-class <file> <class-name> <base-name> <members>
 *   refactor intro-param-object <file> <function-name> <object-name> <params>
 */
class RefactoringCLIAdapter : public QObject
{
    Q_OBJECT

public:
    explicit RefactoringCLIAdapter(QObject* parent = nullptr);
    ~RefactoringCLIAdapter();

    /**
     * @brief Execute refactoring command
     * @param command The refactoring command (e.g., "extract-method")
     * @param args Command arguments
     * @return JSON result with success status and details
     */
    QJsonObject executeCommand(const QString& command, const QStringList& args);

    /**
     * @brief Get available refactoring commands
     */
    QStringList getAvailableCommands() const;

    /**
     * @brief Get help for a specific command
     */
    QString getCommandHelp(const QString& command) const;

private:
    AdvancedRefactoring m_engine;

    // Command handlers
    QJsonObject handleExtractMethod(const QStringList& args);
    QJsonObject handleExtractFunction(const QStringList& args);
    QJsonObject handleInlineVariable(const QStringList& args);
    QJsonObject handleInlineMethod(const QStringList& args);
    QJsonObject handleInlineConstant(const QStringList& args);
    QJsonObject handleRename(const QStringList& args);
    QJsonObject handleMoveClass(const QStringList& args);
    QJsonObject handleChangeSignature(const QStringList& args);
    QJsonObject handleAddParameter(const QStringList& args);
    QJsonObject handleRemoveParameter(const QStringList& args);
    QJsonObject handleReorderParameters(const QStringList& args);
    QJsonObject handleConvertLoop(const QStringList& args);
    QJsonObject handleConvertConditional(const QStringList& args);
    QJsonObject handleOptimizeIncludes(const QStringList& args);
    QJsonObject handleRemoveUnused(const QStringList& args);
    QJsonObject handleRemoveDeadCode(const QStringList& args);
    QJsonObject handleExtractInterface(const QStringList& args);
    QJsonObject handleExtractBaseClass(const QStringList& args);
    QJsonObject handleIntroParamObject(const QStringList& args);

    // Helper methods
    QJsonObject resultToJson(const AdvancedRefactoring::RefactoringResult& result);
    QString getErrorMessage(const QString& command) const;
};

#endif // REFACTORING_CLI_ADAPTER_H
