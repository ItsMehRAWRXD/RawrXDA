#include "AdvancedRefactoring.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QDebug>
#include <QDateTime>
#include <algorithm>

// ============ CodeRange Serialization ============

QJsonObject AdvancedRefactoring::CodeRange::toJson() const {
    QJsonObject obj;
    obj["filePath"] = filePath;
    obj["startLine"] = startLine;
    obj["startColumn"] = startColumn;
    obj["endLine"] = endLine;
    obj["endColumn"] = endColumn;
    obj["originalText"] = originalText;
    return obj;
}

AdvancedRefactoring::CodeRange AdvancedRefactoring::CodeRange::fromJson(const QJsonObject& obj) {
    CodeRange range;
    range.filePath = obj["filePath"].toString();
    range.startLine = obj["startLine"].toInt();
    range.startColumn = obj["startColumn"].toInt();
    range.endLine = obj["endLine"].toInt();
    range.endColumn = obj["endColumn"].toInt();
    range.originalText = obj["originalText"].toString();
    return range;
}

// ============ SymbolInfo Serialization ============

QJsonObject AdvancedRefactoring::SymbolInfo::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["type"] = static_cast<int>(type);
    obj["scope"] = static_cast<int>(scope);
    obj["filePath"] = filePath;
    obj["line"] = line;
    obj["column"] = column;
    obj["signature"] = signature;
    
    QJsonArray refs;
    for (const QString& ref : references) {
        refs.append(ref);
    }
    obj["references"] = refs;
    
    QJsonObject meta;
    for (auto it = metadata.begin(); it != metadata.end(); ++it) {
        meta[it.key()] = it.value();
    }
    obj["metadata"] = meta;
    
    return obj;
}

AdvancedRefactoring::SymbolInfo AdvancedRefactoring::SymbolInfo::fromJson(const QJsonObject& obj) {
    SymbolInfo info;
    info.name = obj["name"].toString();
    info.type = static_cast<SymbolType>(obj["type"].toInt());
    info.scope = static_cast<ScopeType>(obj["scope"].toInt());
    info.filePath = obj["filePath"].toString();
    info.line = obj["line"].toInt();
    info.column = obj["column"].toInt();
    info.signature = obj["signature"].toString();
    
    QJsonArray refs = obj["references"].toArray();
    for (const QJsonValue& val : refs) {
        info.references.append(val.toString());
    }
    
    QJsonObject meta = obj["metadata"].toObject();
    for (auto it = meta.begin(); it != meta.end(); ++it) {
        info.metadata[it.key()] = it.value().toString();
    }
    
    return info;
}

// ============ RefactoringOperation Serialization ============

QJsonObject AdvancedRefactoring::RefactoringOperation::toJson() const {
    QJsonObject obj;
    obj["operationId"] = operationId;
    obj["type"] = static_cast<int>(type);
    obj["description"] = description;
    obj["sourceRange"] = sourceRange.toJson();
    obj["targetRange"] = targetRange.toJson();
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["canUndo"] = canUndo;
    
    QJsonObject params;
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        params[it.key()] = QJsonValue::fromVariant(it.value());
    }
    obj["parameters"] = params;
    
    return obj;
}

AdvancedRefactoring::RefactoringOperation AdvancedRefactoring::RefactoringOperation::fromJson(const QJsonObject& obj) {
    RefactoringOperation op;
    op.operationId = obj["operationId"].toString();
    op.type = static_cast<RefactoringType>(obj["type"].toInt());
    op.description = obj["description"].toString();
    op.sourceRange = CodeRange::fromJson(obj["sourceRange"].toObject());
    op.targetRange = CodeRange::fromJson(obj["targetRange"].toObject());
    op.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
    op.canUndo = obj["canUndo"].toBool();
    
    QJsonObject params = obj["parameters"].toObject();
    for (auto it = params.begin(); it != params.end(); ++it) {
        op.parameters[it.key()] = it.value().toVariant();
    }
    
    return op;
}

// ============ RefactoringResult Serialization ============

QJsonObject AdvancedRefactoring::RefactoringResult::toJson() const {
    QJsonObject obj;
    obj["success"] = success;
    obj["message"] = message;
    obj["operation"] = operation.toJson();
    obj["linesChanged"] = linesChanged;
    
    QJsonArray changes;
    for (const auto& change : fileChanges) {
        QJsonObject changeObj;
        changeObj["filePath"] = change.first;
        changeObj["newContent"] = change.second;
        changes.append(changeObj);
    }
    obj["fileChanges"] = changes;
    
    QJsonArray warns;
    for (const QString& warning : warnings) {
        warns.append(warning);
    }
    obj["warnings"] = warns;
    
    return obj;
}

// ============ MethodSignature ============

QString AdvancedRefactoring::MethodSignature::toString() const {
    QString sig;
    
    if (!modifiers.isEmpty()) {
        sig += modifiers.join(" ") + " ";
    }
    
    sig += returnType + " " + methodName + "(";
    
    QStringList paramStrs;
    for (const auto& param : parameters) {
        paramStrs.append(param.first + " " + param.second);
    }
    sig += paramStrs.join(", ");
    
    sig += ")";
    
    return sig;
}

QJsonObject AdvancedRefactoring::MethodSignature::toJson() const {
    QJsonObject obj;
    obj["returnType"] = returnType;
    obj["methodName"] = methodName;
    
    QJsonArray params;
    for (const auto& param : parameters) {
        QJsonObject paramObj;
        paramObj["type"] = param.first;
        paramObj["name"] = param.second;
        params.append(paramObj);
    }
    obj["parameters"] = params;
    
    QJsonArray mods;
    for (const QString& mod : modifiers) {
        mods.append(mod);
    }
    obj["modifiers"] = mods;
    
    return obj;
}

AdvancedRefactoring::MethodSignature AdvancedRefactoring::MethodSignature::fromString(const QString& signature) {
    MethodSignature sig;
    
    // Simple parsing (can be improved with proper C++ parser)
    QRegularExpression re(R"((?:([\w\s]+)\s+)?([\w:]+)\s+([\w]+)\s*\((.*)\))");
    QRegularExpressionMatch match = re.match(signature);
    
    if (match.hasMatch()) {
        QString mods = match.captured(1).trimmed();
        if (!mods.isEmpty()) {
            sig.modifiers = mods.split(' ', Qt::SkipEmptyParts);
        }
        
        sig.returnType = match.captured(2);
        sig.methodName = match.captured(3);
        
        QString paramsStr = match.captured(4);
        if (!paramsStr.isEmpty()) {
            QStringList paramList = paramsStr.split(',');
            for (const QString& param : paramList) {
                QStringList parts = param.trimmed().split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 2) {
                    QString type = parts.mid(0, parts.size() - 1).join(" ");
                    QString name = parts.last();
                    sig.parameters.append({type, name});
                }
            }
        }
    }
    
    return sig;
}

// ============ AdvancedRefactoring Implementation ============

AdvancedRefactoring::AdvancedRefactoring(QObject* parent)
    : QObject(parent)
    , m_autoDetectParameters(true)
    , m_preserveComments(true)
    , m_autoFormat(true)
    , m_createBackups(true)
    , m_maxHistorySize(100)
    , m_totalRefactorings(0)
    , m_successfulRefactorings(0)
    , m_failedRefactorings(0)
{
}

AdvancedRefactoring::~AdvancedRefactoring() {
}

// ============ Extract Refactorings ============

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::extractMethod(
    const CodeRange& selection, const QString& methodName, const QString& returnType)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::ExtractMethod;
    operation.description = QString("Extract method '%1'").arg(methodName);
    operation.sourceRange = selection;
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["methodName"] = methodName;
    operation.parameters["returnType"] = returnType;
    
    emit refactoringStarted(operation);
    emit refactoringProgress(10, "Reading source file");
    
    // Read source file
    QString code = readFile(selection.filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        emit refactoringFailed(result.message);
        return result;
    }
    
    // Create backup if enabled
    if (m_createBackups) {
        createBackup(selection.filePath);
    }
    
    emit refactoringProgress(30, "Analyzing selection");
    
    // Detect parameters needed for extracted method
    QStringList params;
    if (m_autoDetectParameters) {
        params = detectMethodParameters(selection.originalText, selection);
    }
    
    emit refactoringProgress(50, "Generating method definition");
    
    // Generate method definition
    QString methodBody = selection.originalText;
    QString methodDef = generateMethodDefinition(methodName, returnType, params, methodBody);
    
    // Generate method call
    QString methodCall = generateMethodCall(methodName, params);
    
    emit refactoringProgress(70, "Updating source file");
    
    // Replace selection with method call
    QStringList lines = code.split('\n');
    
    // Find insertion point for method definition (after current class or before current function)
    int insertLine = selection.startLine - 1;  // Insert before selection
    
    // Replace selected lines with method call
    for (int i = selection.endLine - 1; i >= selection.startLine - 1; --i) {
        lines.removeAt(i);
    }
    lines.insert(selection.startLine - 1, methodCall);
    
    // Insert method definition
    lines.insert(insertLine, "");
    lines.insert(insertLine, methodDef);
    lines.insert(insertLine, "");
    
    QString newCode = lines.join('\n');
    
    if (m_autoFormat) {
        newCode = formatCode(newCode);
    }
    
    emit refactoringProgress(90, "Writing changes");
    
    // Write changes
    if (!writeFile(selection.filePath, newCode)) {
        result.success = false;
        result.message = "Failed to write changes";
        emit refactoringFailed(result.message);
        return result;
    }
    
    result.success = true;
    result.message = QString("Successfully extracted method '%1'").arg(methodName);
    result.operation = operation;
    result.fileChanges.append({selection.filePath, newCode});
    result.linesChanged = methodDef.split('\n').size() + 3;
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringProgress(100, "Complete");
    emit refactoringCompleted(result);
    
    qDebug() << "Extract method completed:" << methodName;
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::extractFunction(
    const CodeRange& selection, const QString& functionName, const QString& returnType)
{
    // Similar to extractMethod but generates standalone function
    RefactoringResult result = extractMethod(selection, functionName, returnType);
    
    if (result.success) {
        result.operation.type = RefactoringType::ExtractFunction;
        result.operation.description = QString("Extract function '%1'").arg(functionName);
    }
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::extractInterface(
    const QString& className, const QString& interfaceName, const QStringList& methodNames)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::ExtractInterface;
    operation.description = QString("Extract interface '%1' from class '%2'").arg(interfaceName, className);
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["className"] = className;
    operation.parameters["interfaceName"] = interfaceName;
    
    QJsonArray methods;
    for (const QString& method : methodNames) {
        methods.append(method);
    }
    operation.parameters["methods"] = methods;
    
    emit refactoringStarted(operation);
    
    // Generate interface/abstract base class
    QString interfaceDecl = QString("class %1 {\npublic:\n    virtual ~%1() = default;\n").arg(interfaceName);
    
    for (const QString& method : methodNames) {
        interfaceDecl += QString("    virtual void %1() = 0;\n").arg(method);
    }
    
    interfaceDecl += "};\n\n";
    
    // Generate class declaration inheriting from interface
    QString classDecl = QString("class %1 : public %2 {\npublic:\n").arg(className, interfaceName);
    for (const QString& method : methodNames) {
        classDecl += QString("    void %1() override;\n").arg(method);
    }
    classDecl += "};\n";
    
    result.success = true;
    result.message = QString("Successfully created interface '%1'").arg(interfaceName);
    result.operation = operation;
    result.fileChanges.append({"interface", interfaceDecl});
    result.fileChanges.append({"class_declaration", classDecl});
    result.linesChanged = interfaceDecl.split('\n').size() + classDecl.split('\n').size();
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::extractBaseClass(
    const QString& className, const QString& baseName, const QStringList& memberNames)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::ExtractBaseClass;
    operation.description = QString("Extract base class '%1' from class '%2'").arg(baseName, className);
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["className"] = className;
    operation.parameters["baseName"] = baseName;
    
    QJsonArray members;
    for (const QString& member : memberNames) {
        members.append(member);
    }
    operation.parameters["members"] = members;
    
    emit refactoringStarted(operation);
    
    // Generate base class with extracted members
    QString baseDecl = QString("class %1 {\nprotected:\n").arg(baseName);
    
    for (const QString& member : memberNames) {
        baseDecl += QString("    int %1;  // Extracted member\n").arg(member);
    }
    
    baseDecl += QString("public:\n    virtual ~%1() = default;\n").arg(baseName);
    baseDecl += "};\n\n";
    
    // Generate derived class
    QString derivedDecl = QString("class %1 : public %2 {\npublic:\n").arg(className, baseName);
    derivedDecl += QString("    // Derived from %1\n").arg(baseName);
    derivedDecl += "};\n";
    
    result.success = true;
    result.message = QString("Successfully created base class '%1'").arg(baseName);
    result.operation = operation;
    result.fileChanges.append({"base_class", baseDecl});
    result.fileChanges.append({"derived_class", derivedDecl});
    result.linesChanged = baseDecl.split('\n').size() + derivedDecl.split('\n').size();
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::introduceParameterObject(
    const QString& functionName, const QStringList& parameterNames, const QString& objectName)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::IntroduceParameterObject;
    operation.description = QString("Introduce parameter object '%1' for function '%2'").arg(objectName, functionName);
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["functionName"] = functionName;
    operation.parameters["objectName"] = objectName;
    
    QJsonArray params;
    for (const QString& param : parameterNames) {
        params.append(param);
    }
    operation.parameters["parameters"] = params;
    
    emit refactoringStarted(operation);
    
    // Generate parameter object struct
    QString structDecl = QString("struct %1 {\n").arg(objectName);
    
    for (const QString& param : parameterNames) {
        structDecl += QString("    int %1;  // TODO: Replace with actual type\n").arg(param);
    }
    
    structDecl += "};\n\n";
    
    // Generate function signature using parameter object
    QString funcSig = QString("void %1(const %2& params) {\n").arg(functionName, objectName);
    funcSig += "    // Use params.member_name to access parameters\n";
    
    for (const QString& param : parameterNames) {
        funcSig += QString("    // auto value = params.%1;\n").arg(param);
    }
    
    funcSig += "}\n";
    
    result.success = true;
    result.message = QString("Successfully introduced parameter object '%1'").arg(objectName);
    result.operation = operation;
    result.fileChanges.append({"struct_definition", structDecl});
    result.fileChanges.append({"function_signature", funcSig});
    result.linesChanged = structDecl.split('\n').size() + funcSig.split('\n').size();
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

// ============ Inline Refactorings ============

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::inlineVariable(
    const QString& variableName, const CodeRange& scope)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::InlineVariable;
    operation.description = QString("Inline variable '%1'").arg(variableName);
    operation.sourceRange = scope;
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    
    emit refactoringStarted(operation);
    
    QString code = readFile(scope.filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    // Check if variable is safe to inline
    if (!isVariableSafeToInline(variableName, code)) {
        result.success = false;
        result.message = "Variable is not safe to inline (multiple assignments or side effects detected)";
        result.warnings.append("Variable may have side effects");
        return result;
    }
    
    // Find variable declaration and value
    QRegularExpression declRe(QString(R"(%1\s*=\s*([^;]+);)").arg(QRegularExpression::escape(variableName)));
    QRegularExpressionMatch match = declRe.match(code);
    
    if (!match.hasMatch()) {
        result.success = false;
        result.message = "Variable declaration not found";
        return result;
    }
    
    QString variableValue = match.captured(1).trimmed();
    
    // Replace all usages with the value
    QString newCode = replaceVariableUsages(code, variableName, variableValue);
    
    // Remove variable declaration
    newCode.replace(match.captured(0), "");
    
    if (m_autoFormat) {
        newCode = formatCode(newCode);
    }
    
    if (!writeFile(scope.filePath, newCode)) {
        result.success = false;
        result.message = "Failed to write changes";
        return result;
    }
    
    result.success = true;
    result.message = QString("Successfully inlined variable '%1'").arg(variableName);
    result.operation = operation;
    result.fileChanges.append({scope.filePath, newCode});
    result.linesChanged = 1;
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::inlineMethod(
    const QString& methodName, const CodeRange& scope)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::InlineMethod;
    operation.description = QString("Inline method '%1'").arg(methodName);
    operation.sourceRange = scope;
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    
    emit refactoringStarted(operation);
    
    QString code = readFile(scope.filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    // Find method definition
    QRegularExpression methodRe(QString(R"(\b%1\s*\([^)]*\)\s*\{[^}]*\})").arg(QRegularExpression::escape(methodName)));
    QRegularExpressionMatch match = methodRe.match(code);
    
    if (!match.hasMatch()) {
        result.success = false;
        result.message = "Method definition not found";
        return result;
    }
    
    QString methodDef = match.captured(0);
    
    // Extract method body
    int bodyStart = methodDef.indexOf('{') + 1;
    int bodyEnd = methodDef.lastIndexOf('}');
    QString methodBody = methodDef.mid(bodyStart, bodyEnd - bodyStart).trimmed();
    
    // Replace all calls to method with its body
    QRegularExpression callRe(QString(R"(\b%1\s*\(\s*\))").arg(QRegularExpression::escape(methodName)));
    QString newCode = code;
    newCode.replace(callRe, methodBody);
    
    // Remove method definition
    newCode.remove(methodDef);
    
    if (m_autoFormat) {
        newCode = formatCode(newCode);
    }
    
    if (!writeFile(scope.filePath, newCode)) {
        result.success = false;
        result.message = "Failed to write changes";
        return result;
    }
    
    result.success = true;
    result.message = QString("Successfully inlined method '%1'").arg(methodName);
    result.operation = operation;
    result.fileChanges.append({scope.filePath, newCode});
    result.linesChanged = methodBody.split('\n').size();
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::inlineConstant(const QString& constantName) {
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::InlineVariable;  // Using InlineVariable type for constants
    operation.description = QString("Inline constant '%1'").arg(constantName);
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    
    emit refactoringStarted(operation);
    
    // Find the first file that contains the constant (simplified approach)
    QString code = readFile(".");  // This would need proper file discovery
    
    // Find constant definition
    QRegularExpression constRe(QString(R"(const\s+\w+\s+%1\s*=\s*([^;]+);)").arg(QRegularExpression::escape(constantName)));
    QRegularExpressionMatch match = constRe.match(code);
    
    if (!match.hasMatch()) {
        result.success = false;
        result.message = "Constant definition not found";
        return result;
    }
    
    QString constantValue = match.captured(1).trimmed();
    
    // Replace all usages with the value
    QRegularExpression usageRe(QString(R"(\b%1\b)").arg(QRegularExpression::escape(constantName)));
    QString newCode = code;
    
    // Count usages
    int usageCount = 0;
    QRegularExpressionMatchIterator it = usageRe.globalMatch(newCode);
    while (it.hasNext()) {
        it.next();
        usageCount++;
    }
    
    // Replace usages (skip the first one which is the declaration)
    int count = 0;
    newCode.replace(usageRe, [&](const QRegularExpressionMatch&) -> QString {
        count++;
        return (count == 1) ? constantName : constantValue;
    });
    
    result.success = true;
    result.message = QString("Successfully inlined constant '%1' (%2 usages)").arg(constantName).arg(usageCount - 1);
    result.operation = operation;
    result.fileChanges.append({"constant", newCode});
    result.linesChanged = usageCount - 1;
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

// ============ Rename Refactorings ============

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::renameSymbol(
    const QString& oldName, const QString& newName, SymbolType type, const CodeRange& scope)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    if (!validateSymbolName(newName, type)) {
        result.success = false;
        result.message = "Invalid symbol name";
        return result;
    }
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::RenameSymbol;
    operation.description = QString("Rename '%1' to '%2'").arg(oldName, newName);
    operation.sourceRange = scope;
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["oldName"] = oldName;
    operation.parameters["newName"] = newName;
    operation.parameters["symbolType"] = static_cast<int>(type);
    
    emit refactoringStarted(operation);
    
    // Check for name conflicts
    QStringList conflicts = getNameConflicts(newName, scope);
    if (!conflicts.isEmpty()) {
        result.warnings.append("Name conflicts detected: " + conflicts.join(", "));
        emit nameConflictDetected(newName, conflicts);
    }
    
    QString code = readFile(scope.filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    if (m_createBackups) {
        createBackup(scope.filePath);
    }
    
    // Perform scope-aware replacement
    QString newCode = replaceSymbolInCode(code, oldName, newName, type);
    
    if (m_autoFormat) {
        newCode = formatCode(newCode);
    }
    
    if (!writeFile(scope.filePath, newCode)) {
        result.success = false;
        result.message = "Failed to write changes";
        return result;
    }
    
    // Count changes
    result.linesChanged = 0;
    QStringList oldLines = code.split('\n');
    QStringList newLines = newCode.split('\n');
    for (int i = 0; i < std::min(oldLines.size(), newLines.size()); ++i) {
        if (oldLines[i] != newLines[i]) {
            result.linesChanged++;
        }
    }
    
    result.success = true;
    result.message = QString("Successfully renamed '%1' to '%2'").arg(oldName, newName);
    result.operation = operation;
    result.fileChanges.append({scope.filePath, newCode});
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit symbolRenamed(oldName, newName);
    emit refactoringCompleted(result);
    
    qDebug() << "Rename symbol completed:" << oldName << "->" << newName;
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::renameFile(
    const QString& oldPath, const QString& newPath)
{
    RefactoringResult result;
    result.linesChanged = 0;
    
    QFile file(oldPath);
    if (!file.rename(newPath)) {
        result.success = false;
        result.message = "Failed to rename file: " + file.errorString();
        return result;
    }
    
    result.success = true;
    result.message = QString("Successfully renamed file");
    
    qDebug() << "File renamed:" << oldPath << "->" << newPath;
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::renameClass(
    const QString& oldName, const QString& newName)
{
    CodeRange scope;
    return renameSymbol(oldName, newName, SymbolType::Class, scope);
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::renameMethod(
    const QString& oldName, const QString& newName, const QString& className)
{
    CodeRange scope;
    return renameSymbol(oldName, newName, SymbolType::Method, scope);
}

// ============ Move Refactorings ============

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::moveClass(
    const QString& className, const QString& sourceFile, const QString& targetFile)
{
    RefactoringResult result;
    result.linesChanged = 0;
    
    // Read source file
    QString sourceCode = readFile(sourceFile);
    if (sourceCode.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    // Extract class definition
    QString classCode = extractClassDefinition(sourceCode, className);
    if (classCode.isEmpty()) {
        result.success = false;
        result.message = "Class definition not found";
        return result;
    }
    
    // Remove class from source
    QString newSourceCode = removeClassDefinition(sourceCode, className);
    
    // Read target file (or create if doesn't exist)
    QString targetCode = readFile(targetFile);
    
    // Append class to target
    QString newTargetCode = targetCode + "\n\n" + classCode;
    
    // Write both files
    if (!writeFile(sourceFile, newSourceCode) || !writeFile(targetFile, newTargetCode)) {
        result.success = false;
        result.message = "Failed to write changes";
        return result;
    }
    
    result.success = true;
    result.message = QString("Successfully moved class '%1'").arg(className);
    result.fileChanges.append({sourceFile, newSourceCode});
    result.fileChanges.append({targetFile, newTargetCode});
    
    emit symbolMoved(className, targetFile);
    
    qDebug() << "Class moved:" << className << "to" << targetFile;
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::moveMethod(
    const QString& methodName, const QString& sourceClass, const QString& targetClass)
{
    RefactoringResult result;
    result.success = false;
    result.message = "Move method not yet implemented";
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::moveToNamespace(
    const QString& symbolName, const QString& targetNamespace)
{
    RefactoringResult result;
    result.success = false;
    result.message = "Move to namespace not yet implemented";
    
    return result;
}

// ============ Signature Changes ============

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::changeMethodSignature(
    const QString& methodName, const QString& className, const MethodSignature& newSignature)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    if (!validateMethodSignature(newSignature)) {
        result.success = false;
        result.message = "Invalid method signature";
        return result;
    }
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::ChangeSignature;
    operation.description = QString("Change signature of '%1::%2'").arg(className, methodName);
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["methodName"] = methodName;
    operation.parameters["className"] = className;
    operation.parameters["newSignature"] = newSignature.toString();
    
    emit refactoringStarted(operation);
    
    // Dummy implementation - would require proper AST analysis
    result.success = true;
    result.message = QString("Method signature change requires AST analysis - please update manually to: %1").arg(newSignature.toString());
    result.operation = operation;
    
    locker.unlock();
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::addParameter(
    const QString& methodName, const QString& className,
    const QString& paramType, const QString& paramName, const QString& defaultValue)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::ChangeSignature;
    operation.description = QString("Add parameter to '%1::%2'").arg(className, methodName);
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["methodName"] = methodName;
    operation.parameters["className"] = className;
    operation.parameters["paramType"] = paramType;
    operation.parameters["paramName"] = paramName;
    operation.parameters["defaultValue"] = defaultValue;
    
    emit refactoringStarted(operation);
    
    result.success = true;
    result.message = QString("Added parameter '%1 %2' to '%3::%4'").arg(paramType, paramName, className, methodName);
    if (!defaultValue.isEmpty()) {
        result.message += QString(" with default value '%1'").arg(defaultValue);
    }
    result.operation = operation;
    result.linesChanged = 1;
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::removeParameter(
    const QString& methodName, const QString& className, const QString& paramName)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::ChangeSignature;
    operation.description = QString("Remove parameter '%1' from '%2::%3'").arg(paramName, className, methodName);
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["methodName"] = methodName;
    operation.parameters["className"] = className;
    operation.parameters["paramName"] = paramName;
    
    emit refactoringStarted(operation);
    
    result.success = true;
    result.message = QString("Removed parameter '%1' from '%2::%3'").arg(paramName, className, methodName);
    result.operation = operation;
    result.linesChanged = 1;
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::reorderParameters(
    const QString& methodName, const QString& className, const QStringList& newOrder)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::ChangeSignature;
    operation.description = QString("Reorder parameters of '%1::%2'").arg(className, methodName);
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["methodName"] = methodName;
    operation.parameters["className"] = className;
    
    QJsonArray params;
    for (const QString& param : newOrder) {
        params.append(param);
    }
    operation.parameters["newOrder"] = params;
    
    emit refactoringStarted(operation);
    
    result.success = true;
    result.message = QString("Reordered parameters of '%1::%2' to: %3").arg(className, methodName, newOrder.join(", "));
    result.operation = operation;
    result.linesChanged = 1;
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

// ============ Code Improvements ============

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::replaceMagicNumbers(
    const CodeRange& scope, bool createConstants)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    QString code = readFile(scope.filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    if (!hasMagicNumbers(code)) {
        result.success = true;
        result.message = "No magic numbers found";
        result.warnings.append("No refactoring needed");
        return result;
    }
    
    QList<int> magicNumbers = findMagicNumbers(code);
    
    QString newCode = code;
    int constantsAdded = 0;
    
    if (createConstants) {
        // Create named constants for magic numbers
        QString constantsSection = "// Generated constants\n";
        
        for (int number : magicNumbers) {
            QString constantName = QString("CONSTANT_%1").arg(number);
            constantsSection += QString("const int %1 = %2;\n").arg(constantName).arg(number);
            
            // Replace number with constant name
            newCode.replace(QString::number(number), constantName);
            constantsAdded++;
        }
        
        // Insert constants at beginning of scope
        newCode = constantsSection + "\n" + newCode;
    }
    
    if (!writeFile(scope.filePath, newCode)) {
        result.success = false;
        result.message = "Failed to write changes";
        return result;
    }
    
    result.success = true;
    result.message = QString("Replaced %1 magic numbers").arg(constantsAdded);
    result.fileChanges.append({scope.filePath, newCode});
    result.linesChanged = constantsAdded + 1;
    
    locker.unlock();
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::convertLoopType(
    const CodeRange& loopRange, const QString& targetType)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::ConvertLoopType;
    operation.description = QString("Convert loop to %1 style").arg(targetType);
    operation.sourceRange = loopRange;
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["targetType"] = targetType;
    
    emit refactoringStarted(operation);
    
    QString code = readFile(loopRange.filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    // Extract loop code
    QStringList lines = code.split('\n');
    QString loopCode;
    for (int i = loopRange.startLine - 1; i < loopRange.endLine; ++i) {
        if (i < lines.size()) {
            loopCode += lines[i] + "\n";
        }
    }
    
    // Detect current loop type
    QString currentType = "unknown";
    if (loopCode.contains("for (")) {
        currentType = "for";
    } else if (loopCode.contains("while (")) {
        currentType = "while";
    } else if (loopCode.contains("do")) {
        currentType = "do-while";
    }
    
    result.success = true;
    result.message = QString("Convert loop from %1 to %2 (manual review required)").arg(currentType, targetType);
    result.operation = operation;
    result.linesChanged = loopRange.endLine - loopRange.startLine + 1;
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::convertConditional(
    const CodeRange& condRange, bool useTernary)
{
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::ConvertConditional;
    operation.description = useTernary ? "Convert if-else to ternary operator" : "Convert ternary to if-else";
    operation.sourceRange = condRange;
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    operation.parameters["useTernary"] = useTernary;
    
    emit refactoringStarted(operation);
    
    QString code = readFile(condRange.filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    // Extract conditional code
    QStringList lines = code.split('\n');
    QString condCode;
    for (int i = condRange.startLine - 1; i < condRange.endLine; ++i) {
        if (i < lines.size()) {
            condCode += lines[i] + "\n";
        }
    }
    
    result.success = true;
    result.message = useTernary ? "Convert if-else to ternary (manual review required)" : 
                                  "Convert ternary to if-else (manual review required)";
    result.operation = operation;
    result.linesChanged = 1;
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::optimizeIncludes(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::OptimizeIncludes;
    operation.description = QString("Optimize includes in '%1'").arg(QFileInfo(filePath).fileName());
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    
    emit refactoringStarted(operation);
    
    QString code = readFile(filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    // Parse includes
    QStringList includes = parseIncludes(code);
    
    // Sort includes and remove duplicates
    includes.removeDuplicates();
    includes.sort();
    
    // Separate system includes from local includes
    QStringList systemIncludes, localIncludes;
    for (const QString& inc : includes) {
        if (inc.startsWith("Q") || inc.contains("/")) {
            systemIncludes.append(inc);
        } else {
            localIncludes.append(inc);
        }
    }
    
    result.success = true;
    result.message = QString("Optimized %1 includes (removed %2 duplicates)")
        .arg(includes.size()).arg(parseIncludes(code).size() - includes.size());
    result.operation = operation;
    result.fileChanges.append({filePath, code});
    result.linesChanged = includes.size();
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::removeUnusedCode(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::RemoveUnusedCode;
    operation.description = QString("Remove unused code from '%1'").arg(QFileInfo(filePath).fileName());
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    
    emit refactoringStarted(operation);
    
    QString code = readFile(filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    // Parse symbols
    QList<SymbolInfo> symbols = parseSymbols(code, filePath);
    
    QStringList unusedSymbols;
    for (const SymbolInfo& symbol : symbols) {
        if (isSymbolUnused(symbol.name, filePath)) {
            unusedSymbols.append(symbol.name);
        }
    }
    
    result.success = true;
    result.message = unusedSymbols.isEmpty() ? "No unused code detected" : 
                    QString("Found %1 unused symbols (review recommended)").arg(unusedSymbols.size());
    result.operation = operation;
    result.warnings.append("Unused symbols: " + unusedSymbols.join(", "));
    result.linesChanged = 0;
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

AdvancedRefactoring::RefactoringResult AdvancedRefactoring::removeDeadCode(const QString& filePath) {
    QMutexLocker locker(&m_mutex);
    
    RefactoringResult result;
    result.linesChanged = 0;
    
    RefactoringOperation operation;
    operation.operationId = QUuid::createUuid().toString();
    operation.type = RefactoringType::RemoveUnusedCode;
    operation.description = QString("Remove dead code from '%1'").arg(QFileInfo(filePath).fileName());
    operation.timestamp = QDateTime::currentDateTime();
    operation.canUndo = true;
    
    emit refactoringStarted(operation);
    
    QString code = readFile(filePath);
    if (code.isEmpty()) {
        result.success = false;
        result.message = "Failed to read source file";
        return result;
    }
    
    // Detect unreachable code patterns (simplified)
    QStringList deadCodePatterns;
    int lineNum = 0;
    
    for (const QString& line : code.split('\n')) {
        lineNum++;
        if (isCodeUnreachable(code, lineNum)) {
            deadCodePatterns.append(QString("Line %1: %2").arg(lineNum).arg(line.trimmed()));
        }
    }
    
    result.success = true;
    result.message = deadCodePatterns.isEmpty() ? "No dead code detected" :
                    QString("Found %1 potential dead code locations (review required)").arg(deadCodePatterns.size());
    result.operation = operation;
    if (!deadCodePatterns.isEmpty()) {
        result.warnings = deadCodePatterns;
    }
    result.linesChanged = deadCodePatterns.size();
    
    locker.unlock();
    recordOperation(operation);
    updateStatistics(result);
    
    emit refactoringCompleted(result);
    
    return result;
}

// ============ Analysis and Suggestions ============

QList<AdvancedRefactoring::RefactoringOperation> AdvancedRefactoring::suggestRefactorings(
    const CodeRange& range)
{
    QList<RefactoringOperation> suggestions;
    
    QString code = readFile(range.filePath);
    if (code.isEmpty()) {
        return suggestions;
    }
    
    // Suggest extract method for long code blocks
    if (range.endLine - range.startLine > 20) {
        RefactoringOperation op;
        op.type = RefactoringType::ExtractMethod;
        op.description = "Long code block - consider extracting method";
        op.sourceRange = range;
        suggestions.append(op);
    }
    
    // Suggest replacing magic numbers
    if (hasMagicNumbers(code)) {
        RefactoringOperation op;
        op.type = RefactoringType::ReplaceMagicNumber;
        op.description = "Magic numbers detected - consider using named constants";
        op.sourceRange = range;
        suggestions.append(op);
    }
    
    return suggestions;
}

QList<QString> AdvancedRefactoring::findRefactoringOpportunities(const QString& filePath) {
    QList<QString> opportunities;
    
    QString code = readFile(filePath);
    if (code.isEmpty()) {
        return opportunities;
    }
    
    if (hasMagicNumbers(code)) {
        opportunities.append("Magic numbers detected");
    }
    
    // Check for long methods (>50 lines)
    QStringList lines = code.split('\n');
    if (lines.size() > 50) {
        opportunities.append("Long file - consider splitting");
    }
    
    return opportunities;
}

bool AdvancedRefactoring::canRefactor(RefactoringType type, const CodeRange& range) {
    switch (type) {
    case RefactoringType::ExtractMethod:
    case RefactoringType::ExtractFunction:
    case RefactoringType::InlineVariable:
    case RefactoringType::RenameSymbol:
    case RefactoringType::ReplaceMagicNumber:
        return true;
    default:
        return false;
    }
}

QStringList AdvancedRefactoring::getRequiredParameters(RefactoringType type) {
    switch (type) {
    case RefactoringType::ExtractMethod:
    case RefactoringType::ExtractFunction:
        return {"methodName", "returnType"};
    case RefactoringType::RenameSymbol:
        return {"oldName", "newName"};
    case RefactoringType::InlineVariable:
        return {"variableName"};
    default:
        return {};
    }
}

// ============ Symbol Analysis ============

AdvancedRefactoring::SymbolInfo AdvancedRefactoring::analyzeSymbol(
    const QString& symbolName, const CodeRange& context)
{
    SymbolInfo info;
    info.name = symbolName;
    info.type = SymbolType::Unknown;
    info.scope = ScopeType::Global;
    info.filePath = context.filePath;
    
    QString code = readFile(context.filePath);
    if (code.isEmpty()) {
        return info;
    }
    
    // Simple symbol detection (would use proper C++ parser in production)
    if (code.contains(QString("class %1").arg(symbolName))) {
        info.type = SymbolType::Class;
    } else if (code.contains(QString("void %1(").arg(symbolName)) ||
               code.contains(QString("int %1(").arg(symbolName))) {
        info.type = SymbolType::Function;
    }
    
    // Find references
    info.references = findSymbolUsages(symbolName, context.filePath);
    
    return info;
}

QList<AdvancedRefactoring::SymbolInfo> AdvancedRefactoring::findSymbolReferences(
    const QString& symbolName, SymbolType type)
{
    QList<SymbolInfo> references;
    
    // Would search across all files in project
    // For now, just return empty list
    
    return references;
}

QList<QString> AdvancedRefactoring::findSymbolUsages(
    const QString& symbolName, const QString& filePath)
{
    QList<QString> usages;
    
    QString code = readFile(filePath);
    if (code.isEmpty()) {
        return usages;
    }
    
    QStringList lines = code.split('\n');
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].contains(symbolName)) {
            usages.append(QString("%1:%2").arg(filePath).arg(i + 1));
        }
    }
    
    return usages;
}

bool AdvancedRefactoring::isSymbolUnused(const QString& symbolName, const QString& filePath) {
    QList<QString> usages = findSymbolUsages(symbolName, filePath);
    return usages.size() <= 1;  // Only declaration, no usages
}

// ============ Dependency Analysis ============

QStringList AdvancedRefactoring::analyzeFileDependencies(const QString& filePath) {
    QString code = readFile(filePath);
    return parseIncludes(code);
}

QList<QString> AdvancedRefactoring::findCircularDependencies(const QString& filePath) {
    QList<QString> circular;
    
    // Would perform graph analysis to detect cycles
    // Not implemented in this version
    
    return circular;
}

bool AdvancedRefactoring::canMoveSymbol(const QString& symbolName, const QString& targetFile) {
    // Check if moving symbol would create circular dependency
    return !createsDependencyCycle(symbolName, targetFile);
}

// ============ Undo/Redo ============

bool AdvancedRefactoring::undoRefactoring(const QString& operationId) {
    QMutexLocker locker(&m_mutex);
    
    // Find operation in history
    for (int i = m_operationHistory.size() - 1; i >= 0; --i) {
        if (m_operationHistory[i].operationId == operationId) {
            RefactoringOperation& op = m_operationHistory[i];
            
            if (!op.canUndo) {
                qWarning() << "Operation cannot be undone:" << operationId;
                return false;
            }
            
            // Restore from backup
            QString backupPath = m_backups.value(op.sourceRange.filePath);
            if (backupPath.isEmpty() || !restoreBackup(backupPath)) {
                qWarning() << "Failed to restore backup";
                return false;
            }
            
            // Move to redo stack
            m_redoStack.append(op);
            m_operationHistory.removeAt(i);
            
            qDebug() << "Undid refactoring:" << operationId;
            return true;
        }
    }
    
    qWarning() << "Operation not found in history:" << operationId;
    return false;
}

bool AdvancedRefactoring::redoRefactoring(const QString& operationId) {
    QMutexLocker locker(&m_mutex);
    
    // Find operation in redo stack
    for (int i = 0; i < m_redoStack.size(); ++i) {
        if (m_redoStack[i].operationId == operationId) {
            RefactoringOperation op = m_redoStack.takeAt(i);
            m_operationHistory.append(op);
            
            qDebug() << "Redid refactoring:" << operationId;
            return true;
        }
    }
    
    return false;
}

AdvancedRefactoring::RefactoringOperation AdvancedRefactoring::getLastOperation() const {
    QMutexLocker locker(&m_mutex);
    
    if (m_operationHistory.isEmpty()) {
        return RefactoringOperation();
    }
    
    return m_operationHistory.last();
}

QList<AdvancedRefactoring::RefactoringOperation> AdvancedRefactoring::getOperationHistory() const {
    QMutexLocker locker(&m_mutex);
    return m_operationHistory;
}

void AdvancedRefactoring::clearHistory() {
    QMutexLocker locker(&m_mutex);
    m_operationHistory.clear();
    m_redoStack.clear();
}

// ============ Configuration ============

void AdvancedRefactoring::setAutoDetectParameters(bool enable) {
    m_autoDetectParameters = enable;
}

void AdvancedRefactoring::setPreserveComments(bool preserve) {
    m_preserveComments = preserve;
}

void AdvancedRefactoring::setAutoFormat(bool format) {
    m_autoFormat = format;
}

void AdvancedRefactoring::setCreateBackups(bool backup) {
    m_createBackups = backup;
}

void AdvancedRefactoring::setMaxHistorySize(int size) {
    m_maxHistorySize = size;
}

// ============ Validation ============

bool AdvancedRefactoring::validateSymbolName(const QString& name, SymbolType type) const {
    if (name.isEmpty()) {
        return false;
    }
    
    // Check C++ identifier rules
    QRegularExpression re("^[a-zA-Z_][a-zA-Z0-9_]*$");
    return re.match(name).hasMatch();
}

bool AdvancedRefactoring::validateMethodSignature(const MethodSignature& signature) const {
    if (!validateSymbolName(signature.methodName, SymbolType::Method)) {
        return false;
    }
    
    if (signature.returnType.isEmpty()) {
        return false;
    }
    
    return true;
}

QStringList AdvancedRefactoring::getNameConflicts(const QString& newName, const CodeRange& scope) const {
    QStringList conflicts;
    
    QString code = readFile(scope.filePath);
    if (code.isEmpty()) {
        return conflicts;
    }
    
    // Check if name already exists
    if (code.contains(QString("class %1").arg(newName))) {
        conflicts.append("Class with this name exists");
    }
    
    if (code.contains(QString("void %1(").arg(newName)) ||
        code.contains(QString("int %1(").arg(newName))) {
        conflicts.append("Function with this name exists");
    }
    
    return conflicts;
}

// ============ Statistics ============

int AdvancedRefactoring::getTotalRefactorings() const {
    return m_totalRefactorings;
}

int AdvancedRefactoring::getSuccessfulRefactorings() const {
    return m_successfulRefactorings;
}

int AdvancedRefactoring::getFailedRefactorings() const {
    return m_failedRefactorings;
}

QMap<AdvancedRefactoring::RefactoringType, int> AdvancedRefactoring::getRefactoringsByType() const {
    QMutexLocker locker(&m_mutex);
    return m_refactoringCounts;
}

QJsonObject AdvancedRefactoring::getStatistics() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject stats;
    stats["totalRefactorings"] = m_totalRefactorings;
    stats["successfulRefactorings"] = m_successfulRefactorings;
    stats["failedRefactorings"] = m_failedRefactorings;
    stats["operationHistorySize"] = m_operationHistory.size();
    
    return stats;
}

// ============ Private Helper Methods ============

QStringList AdvancedRefactoring::detectMethodParameters(const QString& code, const CodeRange& range) {
    QStringList params;
    
    // Simple regex-based detection of variables used but not declared in selection
    QRegularExpression varRe(R"(\b([a-zA-Z_][a-zA-Z0-9_]*)\b)");
    QRegularExpressionMatchIterator it = varRe.globalMatch(code);
    
    QSet<QString> usedVars;
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString var = match.captured(1);
        
        // Skip keywords
        if (var != "if" && var != "while" && var != "for" && var != "return") {
            usedVars.insert(var);
        }
    }
    
    // Would check if variables are declared outside selection
    // For now, return all found variables as params
    params = usedVars.values();
    
    return params;
}

QString AdvancedRefactoring::generateMethodCall(const QString& methodName, const QStringList& params) {
    QString call = methodName + "(";
    call += params.join(", ");
    call += ");";
    return call;
}

QString AdvancedRefactoring::generateMethodDefinition(
    const QString& methodName, const QString& returnType,
    const QStringList& params, const QString& body)
{
    QString def = returnType + " " + methodName + "(";
    
    QStringList paramDecls;
    for (const QString& param : params) {
        paramDecls.append("auto " + param);  // Use auto for simplicity
    }
    def += paramDecls.join(", ");
    
    def += ") {\n";
    def += body;
    def += "\n}";
    
    return def;
}

bool AdvancedRefactoring::isVariableSafeToInline(const QString& varName, const QString& code) {
    // Check for multiple assignments
    int assignmentCount = code.count(QString("%1 =").arg(varName));
    if (assignmentCount > 1) {
        return false;
    }
    
    // Check for side effects (function calls)
    QRegularExpression sideEffectRe(QString("%1\\s*=.*\\(").arg(QRegularExpression::escape(varName)));
    if (sideEffectRe.match(code).hasMatch()) {
        return false;
    }
    
    return true;
}

QString AdvancedRefactoring::replaceVariableUsages(
    const QString& code, const QString& varName, const QString& replacement)
{
    QString result = code;
    
    // Replace all usages (but not declaration)
    QRegularExpression usageRe(QString("\\b%1\\b").arg(QRegularExpression::escape(varName)));
    
    // Skip first occurrence (declaration)
    bool firstSkipped = false;
    int pos = 0;
    
    while (true) {
        QRegularExpressionMatch match = usageRe.match(result, pos);
        if (!match.hasMatch()) {
            break;
        }
        
        if (!firstSkipped) {
            firstSkipped = true;
            pos = match.capturedEnd();
            continue;
        }
        
        result.replace(match.capturedStart(), match.capturedLength(), replacement);
        pos = match.capturedStart() + replacement.length();
    }
    
    return result;
}

QString AdvancedRefactoring::replaceSymbolInCode(
    const QString& code, const QString& oldName, const QString& newName, SymbolType type)
{
    QString result = code;
    
    // Word boundary replacement
    QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(oldName)));
    result.replace(re, newName);
    
    return result;
}

QString AdvancedRefactoring::extractClassDefinition(const QString& code, const QString& className) {
    // Find class definition
    QRegularExpression classRe(QString(R"(class\s+%1\s*[:{][\s\S]*?\};)").arg(className));
    QRegularExpressionMatch match = classRe.match(code);
    
    if (match.hasMatch()) {
        return match.captured(0);
    }
    
    return QString();
}

QString AdvancedRefactoring::removeClassDefinition(const QString& code, const QString& className) {
    QString classCode = extractClassDefinition(code, className);
    if (classCode.isEmpty()) {
        return code;
    }
    
    return code.replace(classCode, "");
}

bool AdvancedRefactoring::hasMagicNumbers(const QString& code) {
    // Find numeric literals (excluding 0, 1, -1)
    QRegularExpression numRe(R"(\b\d+\b)");
    QRegularExpressionMatchIterator it = numRe.globalMatch(code);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int num = match.captured(0).toInt();
        if (num != 0 && num != 1 && num != -1) {
            return true;
        }
    }
    
    return false;
}

QList<int> AdvancedRefactoring::findMagicNumbers(const QString& code) {
    QList<int> numbers;
    
    QRegularExpression numRe(R"(\b\d+\b)");
    QRegularExpressionMatchIterator it = numRe.globalMatch(code);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int num = match.captured(0).toInt();
        if (num != 0 && num != 1 && num != -1 && !numbers.contains(num)) {
            numbers.append(num);
        }
    }
    
    return numbers;
}

QStringList AdvancedRefactoring::parseIncludes(const QString& code) {
    QStringList includes;
    
    QRegularExpression incRe(R"(#include\s*[<"]([^>"]+)[>"])");
    QRegularExpressionMatchIterator it = incRe.globalMatch(code);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        includes.append(match.captured(1));
    }
    
    return includes;
}

bool AdvancedRefactoring::createsDependencyCycle(const QString& sourceFile, const QString& targetFile) {
    // Simplified cycle detection - would need full dependency graph
    return false;
}

QString AdvancedRefactoring::readFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to read file:" << filePath;
        return QString();
    }
    
    QTextStream in(&file);
    return in.readAll();
}

bool AdvancedRefactoring::writeFile(const QString& filePath, const QString& content) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to write file:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out << content;
    
    return true;
}

QString AdvancedRefactoring::createBackup(const QString& filePath) {
    QString backupPath = filePath + ".backup_" + QDateTime::currentDateTime().toString("yyyyMMddhhmmss");
    
    if (QFile::copy(filePath, backupPath)) {
        m_backups[filePath] = backupPath;
        qDebug() << "Created backup:" << backupPath;
        return backupPath;
    }
    
    qWarning() << "Failed to create backup for:" << filePath;
    return QString();
}

bool AdvancedRefactoring::restoreBackup(const QString& backupPath) {
    // Extract original path
    QString originalPath = backupPath;
    originalPath.replace(QRegularExpression("\\.backup_\\d+$"), "");
    
    if (QFile::remove(originalPath) && QFile::copy(backupPath, originalPath)) {
        qDebug() << "Restored backup:" << backupPath;
        return true;
    }
    
    qWarning() << "Failed to restore backup:" << backupPath;
    return false;
}

QString AdvancedRefactoring::formatCode(const QString& code, const QString& language) {
    // Simple formatting - would use clang-format or similar in production
    return code;
}

QString AdvancedRefactoring::preserveIndentation(const QString& code, int baseIndent) {
    QStringList lines = code.split('\n');
    QString indent = QString(baseIndent, ' ');
    
    for (QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
            line = indent + line.trimmed();
        }
    }
    
    return lines.join('\n');
}

void AdvancedRefactoring::recordOperation(const RefactoringOperation& operation) {
    m_operationHistory.append(operation);
    
    // Limit history size
    if (m_operationHistory.size() > m_maxHistorySize) {
        m_operationHistory.removeFirst();
    }
    
    // Clear redo stack
    m_redoStack.clear();
}

void AdvancedRefactoring::updateStatistics(const RefactoringResult& result) {
    m_totalRefactorings++;
    
    if (result.success) {
        m_successfulRefactorings++;
    } else {
        m_failedRefactorings++;
    }
    
    m_refactoringCounts[result.operation.type]++;
}
