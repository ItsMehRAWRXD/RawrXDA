#include "RefactoringCLIAdapter.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

RefactoringCLIAdapter::RefactoringCLIAdapter(QObject* parent)
    : QObject(parent), m_engine(this)
{
    // Configure engine
    m_engine.setAutoDetectParameters(true);
    m_engine.setPreserveComments(true);
    m_engine.setAutoFormat(true);
    m_engine.setCreateBackups(true);
}

RefactoringCLIAdapter::~RefactoringCLIAdapter()
{
}

QJsonObject RefactoringCLIAdapter::executeCommand(const QString& command, const QStringList& args)
{
    if (command == "extract-method") {
        return handleExtractMethod(args);
    } else if (command == "extract-function") {
        return handleExtractFunction(args);
    } else if (command == "inline-variable") {
        return handleInlineVariable(args);
    } else if (command == "inline-method") {
        return handleInlineMethod(args);
    } else if (command == "inline-constant") {
        return handleInlineConstant(args);
    } else if (command == "rename") {
        return handleRename(args);
    } else if (command == "move-class") {
        return handleMoveClass(args);
    } else if (command == "change-signature") {
        return handleChangeSignature(args);
    } else if (command == "add-parameter") {
        return handleAddParameter(args);
    } else if (command == "remove-parameter") {
        return handleRemoveParameter(args);
    } else if (command == "reorder-parameters") {
        return handleReorderParameters(args);
    } else if (command == "convert-loop") {
        return handleConvertLoop(args);
    } else if (command == "convert-conditional") {
        return handleConvertConditional(args);
    } else if (command == "optimize-includes") {
        return handleOptimizeIncludes(args);
    } else if (command == "remove-unused") {
        return handleRemoveUnused(args);
    } else if (command == "remove-dead-code") {
        return handleRemoveDeadCode(args);
    } else if (command == "extract-interface") {
        return handleExtractInterface(args);
    } else if (command == "extract-base-class") {
        return handleExtractBaseClass(args);
    } else if (command == "intro-param-object") {
        return handleIntroParamObject(args);
    }

    QJsonObject error;
    error["success"] = false;
    error["error"] = "Unknown command: " + command;
    return error;
}

QStringList RefactoringCLIAdapter::getAvailableCommands() const
{
    return {
        "extract-method",
        "extract-function",
        "inline-variable",
        "inline-method",
        "inline-constant",
        "rename",
        "move-class",
        "change-signature",
        "add-parameter",
        "remove-parameter",
        "reorder-parameters",
        "convert-loop",
        "convert-conditional",
        "optimize-includes",
        "remove-unused",
        "remove-dead-code",
        "extract-interface",
        "extract-base-class",
        "intro-param-object"
    };
}

QString RefactoringCLIAdapter::getCommandHelp(const QString& command) const
{
    if (command == "extract-method") {
        return "Extract method from code selection\nUsage: refactor extract-method <file> <start-line> <end-line> <method-name> [return-type]";
    } else if (command == "rename") {
        return "Rename a symbol throughout codebase\nUsage: refactor rename <file> <old-name> <new-name> [symbol-type]";
    } else if (command == "optimize-includes") {
        return "Optimize #include statements\nUsage: refactor optimize-includes <file>";
    }
    return "No help available for: " + command;
}

QJsonObject RefactoringCLIAdapter::handleExtractMethod(const QStringList& args)
{
    if (args.size() < 4) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for extract-method";
        return error;
    }

    QString filePath = args[0];
    int startLine = args[1].toInt();
    int endLine = args[2].toInt();
    QString methodName = args[3];
    QString returnType = args.size() > 4 ? args[4] : "void";

    AdvancedRefactoring::CodeRange range;
    range.filePath = filePath;
    range.startLine = startLine;
    range.endLine = endLine;
    range.startColumn = 0;
    range.endColumn = 0;

    auto result = m_engine.extractMethod(range, methodName, returnType);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleExtractFunction(const QStringList& args)
{
    if (args.size() < 4) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for extract-function";
        return error;
    }

    QString filePath = args[0];
    int startLine = args[1].toInt();
    int endLine = args[2].toInt();
    QString functionName = args[3];
    QString returnType = args.size() > 4 ? args[4] : "void";

    AdvancedRefactoring::CodeRange range;
    range.filePath = filePath;
    range.startLine = startLine;
    range.endLine = endLine;

    auto result = m_engine.extractFunction(range, functionName, returnType);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleInlineVariable(const QStringList& args)
{
    if (args.size() < 2) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for inline-variable";
        return error;
    }

    QString filePath = args[0];
    QString variableName = args[1];

    AdvancedRefactoring::CodeRange scope;
    scope.filePath = filePath;

    auto result = m_engine.inlineVariable(variableName, scope);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleInlineMethod(const QStringList& args)
{
    if (args.size() < 2) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for inline-method";
        return error;
    }

    QString filePath = args[0];
    QString methodName = args[1];

    AdvancedRefactoring::CodeRange scope;
    scope.filePath = filePath;

    auto result = m_engine.inlineMethod(methodName, scope);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleInlineConstant(const QStringList& args)
{
    if (args.size() < 1) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for inline-constant";
        return error;
    }

    QString constantName = args[0];
    auto result = m_engine.inlineConstant(constantName);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleRename(const QStringList& args)
{
    if (args.size() < 3) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for rename";
        return error;
    }

    QString filePath = args[0];
    QString oldName = args[1];
    QString newName = args[2];
    AdvancedRefactoring::SymbolType type = AdvancedRefactoring::SymbolType::Variable;

    if (args.size() > 3) {
        QString typeStr = args[3];
        if (typeStr == "class") type = AdvancedRefactoring::SymbolType::Class;
        else if (typeStr == "function") type = AdvancedRefactoring::SymbolType::Function;
        else if (typeStr == "method") type = AdvancedRefactoring::SymbolType::Method;
    }

    AdvancedRefactoring::CodeRange scope;
    scope.filePath = filePath;

    auto result = m_engine.renameSymbol(oldName, newName, type, scope);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleMoveClass(const QStringList& args)
{
    if (args.size() < 3) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for move-class";
        return error;
    }

    QString sourceFile = args[0];
    QString targetFile = args[1];
    QString className = args[2];

    auto result = m_engine.moveClass(className, sourceFile, targetFile);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleChangeSignature(const QStringList& args)
{
    if (args.size() < 4) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for change-signature";
        return error;
    }

    QString filePath = args[0];
    QString methodName = args[1];
    QString className = args[2];
    QString newSig = args[3];

    auto sig = AdvancedRefactoring::MethodSignature::fromString(newSig);
    auto result = m_engine.changeMethodSignature(methodName, className, sig);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleAddParameter(const QStringList& args)
{
    if (args.size() < 5) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for add-parameter";
        return error;
    }

    QString filePath = args[0];
    QString methodName = args[1];
    QString className = args[2];
    QString paramType = args[3];
    QString paramName = args[4];
    QString defaultValue = args.size() > 5 ? args[5] : "";

    auto result = m_engine.addParameter(methodName, className, paramType, paramName, defaultValue);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleRemoveParameter(const QStringList& args)
{
    if (args.size() < 3) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for remove-parameter";
        return error;
    }

    QString filePath = args[0];
    QString methodName = args[1];
    QString className = args[2];
    QString paramName = args[3];

    auto result = m_engine.removeParameter(methodName, className, paramName);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleReorderParameters(const QStringList& args)
{
    if (args.size() < 4) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for reorder-parameters";
        return error;
    }

    QString filePath = args[0];
    QString methodName = args[1];
    QString className = args[2];
    QStringList newOrder = args.mid(3);

    auto result = m_engine.reorderParameters(methodName, className, newOrder);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleConvertLoop(const QStringList& args)
{
    if (args.size() < 4) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for convert-loop";
        return error;
    }

    QString filePath = args[0];
    int startLine = args[1].toInt();
    int endLine = args[2].toInt();
    QString targetType = args[3];

    AdvancedRefactoring::CodeRange range;
    range.filePath = filePath;
    range.startLine = startLine;
    range.endLine = endLine;

    auto result = m_engine.convertLoopType(range, targetType);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleConvertConditional(const QStringList& args)
{
    if (args.size() < 3) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for convert-conditional";
        return error;
    }

    QString filePath = args[0];
    int startLine = args[1].toInt();
    int endLine = args[2].toInt();
    bool useTernary = args.size() > 3 && args[3] == "--ternary";

    AdvancedRefactoring::CodeRange range;
    range.filePath = filePath;
    range.startLine = startLine;
    range.endLine = endLine;

    auto result = m_engine.convertConditional(range, useTernary);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleOptimizeIncludes(const QStringList& args)
{
    if (args.size() < 1) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for optimize-includes";
        return error;
    }

    QString filePath = args[0];
    auto result = m_engine.optimizeIncludes(filePath);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleRemoveUnused(const QStringList& args)
{
    if (args.size() < 1) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for remove-unused";
        return error;
    }

    QString filePath = args[0];
    auto result = m_engine.removeUnusedCode(filePath);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleRemoveDeadCode(const QStringList& args)
{
    if (args.size() < 1) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for remove-dead-code";
        return error;
    }

    QString filePath = args[0];
    auto result = m_engine.removeDeadCode(filePath);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleExtractInterface(const QStringList& args)
{
    if (args.size() < 4) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for extract-interface";
        return error;
    }

    QString filePath = args[0];
    QString className = args[1];
    QString interfaceName = args[2];
    QStringList methodNames = args.mid(3);

    auto result = m_engine.extractInterface(className, interfaceName, methodNames);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleExtractBaseClass(const QStringList& args)
{
    if (args.size() < 4) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for extract-base-class";
        return error;
    }

    QString filePath = args[0];
    QString className = args[1];
    QString baseName = args[2];
    QStringList memberNames = args.mid(3);

    auto result = m_engine.extractBaseClass(className, baseName, memberNames);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::handleIntroParamObject(const QStringList& args)
{
    if (args.size() < 4) {
        QJsonObject error;
        error["success"] = false;
        error["error"] = "Insufficient arguments for intro-param-object";
        return error;
    }

    QString filePath = args[0];
    QString functionName = args[1];
    QString objectName = args[2];
    QStringList parameterNames = args.mid(3);

    auto result = m_engine.introduceParameterObject(functionName, parameterNames, objectName);
    return resultToJson(result);
}

QJsonObject RefactoringCLIAdapter::resultToJson(const AdvancedRefactoring::RefactoringResult& result)
{
    QJsonObject json;
    json["success"] = result.success;
    json["message"] = result.message;
    json["linesChanged"] = result.linesChanged;

    if (!result.warnings.isEmpty()) {
        QJsonArray warningsArray;
        for (const QString& warning : result.warnings) {
            warningsArray.append(warning);
        }
        json["warnings"] = warningsArray;
    }

    if (!result.fileChanges.isEmpty()) {
        QJsonArray changesArray;
        for (const auto& change : result.fileChanges) {
            QJsonObject changeObj;
            changeObj["file"] = change.first;
            changeObj["linesModified"] = change.second.count('\n');
            changesArray.append(changeObj);
        }
        json["fileChanges"] = changesArray;
    }

    return json;
}

QString RefactoringCLIAdapter::getErrorMessage(const QString& command) const
{
    return QString("Error executing command: %1").arg(command);
}
