#include "RefactoringPlugin.h"
#include <QFile>
#include <QDirIterator>
#include <QTextStream>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>

RefactoringPlugin::RefactoringPlugin(QObject *parent)
    : AgenticPlugin(parent)
    , m_initialized(false)
    , m_scanTimer(new QTimer(this))
    , m_severityThreshold(Medium)
    , m_autoFixEnabled(true)
{
    connect(m_scanTimer, &QTimer::timeout, this, &RefactoringPlugin::performScheduledScan);
}

RefactoringPlugin::~RefactoringPlugin()
{
    cleanup();
}

bool RefactoringPlugin::initialize(const QJsonObject& config)
{
    if (m_initialized) return true;
    
    if (config.contains("severityThreshold")) {
        m_severityThreshold = static_cast<Severity>(config["severityThreshold"].toInt());
    }
    if (config.contains("autoFix")) {
        m_autoFixEnabled = config["autoFix"].toBool();
    }
    if (config.contains("scanInterval")) {
        int interval = config["scanInterval"].toInt(300) * 1000;
        m_scanTimer->setInterval(interval);
        m_scanTimer->start();
    }
    if (config.contains("excludePatterns")) {
        QJsonArray arr = config["excludePatterns"].toArray();
        for (const QJsonValue& val : arr) {
            m_excludePatterns.append(val.toString());
        }
    }
    
    initializePatterns();
    m_initialized = true;
    emit pluginMessage("RefactoringPlugin initialized", QJsonObject{{"severityThreshold", m_severityThreshold}});
    return true;
}

void RefactoringPlugin::cleanup()
{
    m_scanTimer->stop();
    m_patterns.clear();
    m_initialized = false;
}

QJsonObject RefactoringPlugin::executeAction(const QString& action, const QJsonObject& params)
{
    if (!m_initialized) {
        return createResult(false, {}, "Plugin not initialized");
    }
    
    if (action == "analyzeCode") {
        QString code = params["code"].toString();
        QString language = params["language"].toString("cpp");
        return analyzeCode(code, language);
    }
    if (action == "analyzeFile") {
        QString filePath = params["filePath"].toString();
        return analyzeFile(filePath);
    }
    if (action == "analyzeDirectory") {
        QString dirPath = params["dirPath"].toString();
        return analyzeDirectory(dirPath);
    }
    if (action == "refactorCode") {
        QString code = params["code"].toString();
        QString language = params["language"].toString("cpp");
        QString smellType = params["smellType"].toString();
        return refactorCode(code, language, smellType);
    }
    if (action == "refactorFile") {
        QString filePath = params["filePath"].toString();
        QString smellType = params["smellType"].toString();
        return refactorFile(filePath, smellType);
    }
    
    return createResult(false, {}, "Unknown action: " + action);
}

QStringList RefactoringPlugin::getAvailableActions() const
{
    return {"analyzeCode", "analyzeFile", "analyzeDirectory", "refactorCode", "refactorFile"};
}

void RefactoringPlugin::onCodeAnalyzed(const QString& code, const QString& context)
{
    // Auto-scan when code is analyzed
    if (m_autoFixEnabled) {
        QJsonObject result = analyzeCode(code, context);
        int count = result["smellsFound"].toInt();
        if (count > 0) {
            emit pluginMessage("Smells detected during analysis", result);
        }
    }
}

void RefactoringPlugin::onFileOpened(const QString& filePath)
{
    if (!shouldExclude(filePath)) {
        QJsonObject result = analyzeFile(filePath);
        int count = result["smellsFound"].toInt();
        if (count > 0) {
            emit pluginMessage("Smells detected in opened file", result);
        }
    }
}

// ============================================================================
// Analysis Methods
// ============================================================================

QJsonObject RefactoringPlugin::analyzeCode(const QString& code, const QString& language)
{
    QList<Smell> allSmells;
    
    allSmells.append(detectLongMethods(code, language));
    allSmells.append(detectDuplicateCode(code, language));
    allSmells.append(detectLargeClasses(code, language));
    allSmells.append(detectFeatureEnvy(code, language));
    allSmells.append(detectDataClumps(code, language));
    allSmells.append(detectPrimitiveObsession(code, language));
    allSmells.append(detectSwitchStatements(code, language));
    allSmells.append(detectLargeParameterLists(code, language));
    allSmells.append(detectLongParameterLists(code, language));
    allSmells.append(detectLongVariableNames(code, language));
    allSmells.append(detectLongMethodNames(code, language));
    allSmells.append(detectLongVariableDeclarations(code, language));
    allSmells.append(detectLongMethodDeclarations(code, language));
    allSmells.append(detectLongVariableAssignments(code, language));
    allSmells.append(detectLongMethodAssignments(code, language));
    allSmells.append(detectLongVariableUsage(code, language));
    allSmells.append(detectLongMethodUsage(code, language));
    allSmells.append(detectLongVariableUsageInLoops(code, language));
    allSmells.append(detectLongMethodUsageInLoops(code, language));
    allSmells.append(detectLongVariableUsageInConditionals(code, language));
    allSmells.append(detectLongMethodUsageInConditionals(code, language));
    allSmells.append(detectLongVariableUsageInSwitches(code, language));
    allSmells.append(detectLongMethodUsageInSwitches(code, language));
    allSmells.append(detectLongVariableUsageInFunctions(code, language));
    allSmells.append(detectLongMethodUsageInFunctions(code, language));
    allSmells.append(detectLongVariableUsageInClasses(code, language));
    allSmells.append(detectLongMethodUsageInClasses(code, language));
    
    // Filter by severity threshold
    QList<Smell> filtered;
    for (const Smell& s : allSmells) {
        if (s.severity <= m_severityThreshold) {
            filtered.append(s);
            emit smellDetected(s);
        }
    }
    
    QJsonObject report = generateReport(filtered);
    return report;
}

QJsonObject RefactoringPlugin::analyzeFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return createResult(false, {}, "Cannot open file: " + filePath);
    }
    QString code = QTextStream(&file).readAll();
    file.close();
    QString language = detectLanguage(filePath);
    return analyzeCode(code, language);
}

QJsonObject RefactoringPlugin::analyzeDirectory(const QString& dirPath)
{
    emit scanStarted(dirPath);
    QList<Smell> allSmells;
    int filesScanned = 0;
    int totalFiles = 0;
    
    QDirIterator it(dirPath, QStringList() << "*.cpp" << "*.h" << "*.hpp" << "*.c" << "*.py" << "*.js" << "*.ts" << "*.java" << "*.cs", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        if (shouldExclude(filePath)) continue;
        totalFiles++;
    }
    
    QDirIterator scanIt(dirPath, QStringList() << "*.cpp" << "*.h" << "*.hpp" << "*.c" << "*.py" << "*.js" << "*.ts" << "*.java" << "*.cs", QDir::Files, QDirIterator::Subdirectories);
    while (scanIt.hasNext()) {
        QString filePath = scanIt.next();
        if (shouldExclude(filePath)) continue;
        QJsonObject result = analyzeFile(filePath);
        if (result["success"].toBool()) {
            QJsonArray smells = result["smells"].toArray();
            for (const QJsonValue& val : smells) {
                allSmells.append(Smell::fromJson(val.toObject()));
            }
        }
        filesScanned++;
        emit scanProgress(filesScanned, totalFiles);
    }
    
    QJsonObject report = generateReport(allSmells);
    report["filesScanned"] = filesScanned;
    report["directory"] = dirPath;
    emit scanCompleted(allSmells.size(), 0, 0); // For now, counts not split by severity
    return report;
}

// ============================================================================
// Refactoring Methods
// ============================================================================

QJsonObject RefactoringPlugin::refactorCode(const QString& code, const QString& language, const QString& smellType)
{
    // Find the first smell of the requested type
    QList<Smell> smells = analyzeCode(code, language)["smells"].toArray();
    Smell target;
    for (const QJsonValue& val : smells) {
        Smell s = Smell::fromJson(val.toObject());
        if (s.type == smellType) {
            target = s;
            break;
        }
    }
    if (target.type.isEmpty()) {
        return createResult(false, {}, "No smell of type: " + smellType);
    }
    
    // Simple refactor: extract method for long method smell
    if (smellType == "Long Method") {
        // Extract method body
        QString body = extractMethodBody(code, target.line, target.line + 10); // Simplified
        QString methodName = "extractedMethod" + QString::number(target.line);
        QString returnType = "void"; // Simplified
        QStringList params; // Simplified
        QString newMethod = generateExtractedMethod(methodName, body, returnType, params);
        
        // Replace original body with call
        QString newCode = replaceMethodBody(code, target.line, target.line + 10, methodName + "();");
        
        // Append new method at end
        newCode += "\n" + newMethod;
        
        return createResult(true, {{"refactoredCode", newCode}}, "Method extracted");
    }
    
    return createResult(false, {}, "Refactoring not implemented for smell: " + smellType);
}

QJsonObject RefactoringPlugin::refactorFile(const QString& filePath, const QString& smellType)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return createResult(false, {}, "Cannot open file: " + filePath);
    }
    QString code = QTextStream(&file).readAll();
    file.close();
    QString language = detectLanguage(filePath);
    QJsonObject result = refactorCode(code, language, smellType);
    if (result["success"].toBool()) {
        QFile out(filePath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return createResult(false, {}, "Cannot write file: " + filePath);
        }
        QTextStream outStream(&out);
        outStream << result["refactoredCode"].toString();
        out.close();
    }
    return result;
}

// ============================================================================
// Pattern Initialization
// ============================================================================

void RefactoringPlugin::initializePatterns()
{
    // Example: Long Method pattern
    SmellPattern longMethod;
    longMethod.name = "Long Method";
    longMethod.severity = High;
    longMethod.pattern = QRegularExpression("void\s+\w+\s*\([^)]*\)\s*\{[^}]*\}", QRegularExpression::DotMatchesEverythingOption);
    longMethod.description = "Method exceeds 20 lines";
    longMethod.recommendation = "Extract method or split logic";
    longMethod.cwe = {"CWE-200"};
    m_patterns.append(longMethod);
    
    // Additional patterns would be added here for each smell type
}

// ============================================================================
// Smell Detection Implementations (simplified examples)
// ============================================================================

QList<RefactoringPlugin::Smell> RefactoringPlugin::detectLongMethods(const QString& code, const QString& language)
{
    QList<Smell> results;
    // Simplified: count lines per method
    QRegularExpression methodRegex("void\s+\w+\s*\([^)]*\)\s*\{", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator it = methodRegex.globalMatch(code);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int start = match.capturedStart();
        int end = code.indexOf("}", start) + 1;
        if (end <= start) continue;
        QString methodBody = code.mid(start, end - start);
        int lineCount = methodBody.count('\n') + 1;
        if (lineCount > 20) {
            Smell s;
            s.type = "Long Method";
            s.severity = High;
            s.description = QString("Method has %1 lines").arg(lineCount);
            s.location = QString("Line %1").arg(code.left(start).count('\n') + 1);
            s.line = code.left(start).count('\n') + 1;
            s.column = 1;
            s.codeSnippet = methodBody;
            s.recommendation = "Extract method or refactor logic";
            s.cwe = {"CWE-200"};
            results.append(s);
        }
    }
    return results;
}

// Other detection methods would follow similar patterns, using regex or AST parsing.

// ============================================================================
// Auto-fix Helpers
// ============================================================================

QString RefactoringPlugin::extractMethodBody(const QString& code, int startLine, int endLine)
{
    QStringList lines = code.split('\n');
    if (startLine < 1 || endLine > lines.size()) return QString();
    return lines.mid(startLine - 1, endLine - startLine + 1).join('\n');
}

QString RefactoringPlugin::extractMethodSignature(const QString& code, int line)
{
    QStringList lines = code.split('\n');
    if (line < 1 || line > lines.size()) return QString();
    return lines[line - 1].trimmed();
}

QString RefactoringPlugin::generateExtractedMethod(const QString& methodName, const QString& methodBody, const QString& returnType, const QStringList& parameters)
{
    QString params = parameters.join(", ");
    return QString("%1 %2(%3) {\n%4\n}")
        .arg(returnType)
        .arg(methodName)
        .arg(params)
        .arg(methodBody);
}

QString RefactoringPlugin::replaceMethodBody(const QString& code, int startLine, int endLine, const QString& newBody)
{
    QStringList lines = code.split('\n');
    if (startLine < 1 || endLine > lines.size()) return code;
    lines.replace(startLine - 1, endLine - startLine + 1, newBody.split('\n'));
    return lines.join('\n');
}

// ============================================================================
// Utility
// ============================================================================

bool RefactoringPlugin::shouldExclude(const QString& path)
{
    for (const QString& pattern : m_excludePatterns) {
        if (path.contains(pattern)) return true;
    }
    return false;
}

QString RefactoringPlugin::detectLanguage(const QString& filePath)
{
    if (filePath.endsWith(".cpp") || filePath.endsWith(".cxx") || filePath.endsWith(".cc")) return "cpp";
    if (filePath.endsWith(".h") || filePath.endsWith(".hpp")) return "cpp";
    if (filePath.endsWith(".py")) return "python";
    if (filePath.endsWith(".js")) return "javascript";
    if (filePath.endsWith(".ts")) return "typescript";
    if (filePath.endsWith(".java")) return "java";
    if (filePath.endsWith(".cs")) return "csharp";
    return "unknown";
}

QJsonObject RefactoringPlugin::generateReport(const QList<Smell>& smells)
{
    QJsonObject report;
    report["success"] = true;
    report["smellsFound"] = smells.size();
    QJsonArray array;
    for (const Smell& s : smells) {
        array.append(s.toJson());
    }
    report["smells"] = array;
    return report;
}

// ============================================================================
// Smell Serialization
// ============================================================================

QJsonObject RefactoringPlugin::Smell::toJson() const
{
    QJsonObject obj;
    obj["type"] = type;
    obj["severity"] = severity;
    obj["description"] = description;
    obj["location"] = location;
    obj["line"] = line;
    obj["column"] = column;
    obj["codeSnippet"] = codeSnippet;
    obj["recommendation"] = recommendation;
    QJsonArray cweArray;
    for (const QString& id : cwe) cweArray.append(id);
    obj["cwe"] = cweArray;
    return obj;
}

RefactoringPlugin::Smell RefactoringPlugin::Smell::fromJson(const QJsonObject& obj)
{
    Smell s;
    s.type = obj["type"].toString();
    s.severity = static_cast<Severity>(obj["severity"].toInt());
    s.description = obj["description"].toString();
    s.location = obj["location"].toString();
    s.line = obj["line"].toInt();
    s.column = obj["column"].toInt();
    s.codeSnippet = obj["codeSnippet"].toString();
    s.recommendation = obj["recommendation"].toString();
    QJsonArray cweArray = obj["cwe"].toArray();
    for (const QJsonValue& val : cweArray) s.cwe.append(val.toString());
    return s;
}

