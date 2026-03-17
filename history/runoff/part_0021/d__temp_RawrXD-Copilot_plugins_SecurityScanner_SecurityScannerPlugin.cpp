#include "SecurityScannerPlugin.h"
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTextStream>
#include <QDebug>

SecurityScannerPlugin::SecurityScannerPlugin(QObject *parent)
    : AgenticPlugin(parent)
    , m_initialized(false)
    , m_scanTimer(new QTimer(this))
    , m_severityThreshold(Medium)
    , m_autoFixEnabled(true)
{
    connect(m_scanTimer, &QTimer::timeout, this, &SecurityScannerPlugin::performScheduledScan);
}

SecurityScannerPlugin::~SecurityScannerPlugin()
{
    cleanup();
}

bool SecurityScannerPlugin::initialize(const QJsonObject& config)
{
    if (m_initialized) {
        return true;
    }
    
    // Load configuration
    if (config.contains("severityThreshold")) {
        m_severityThreshold = static_cast<Severity>(config["severityThreshold"].toInt());
    }
    
    if (config.contains("autoFix")) {
        m_autoFixEnabled = config["autoFix"].toBool();
    }
    
    if (config.contains("scanInterval")) {
        int interval = config["scanInterval"].toInt(300) * 1000; // Convert to ms
        m_scanTimer->setInterval(interval);
        m_scanTimer->start();
    }
    
    if (config.contains("excludePatterns")) {
        QJsonArray patterns = config["excludePatterns"].toArray();
        for (const QJsonValue& val : patterns) {
            m_excludePatterns.append(val.toString());
        }
    }
    
    // Initialize vulnerability patterns database
    initializePatterns();
    
    // Load custom patterns if specified
    if (config.contains("customPatternsFile")) {
        loadCustomPatterns(config["customPatternsFile"].toString());
    }
    
    m_initialized = true;
    
    emit pluginMessage("SecurityScanner initialized", QJsonObject{
        {"patterns", m_patterns.size()},
        {"severityThreshold", m_severityThreshold}
    });
    
    return true;
}

void SecurityScannerPlugin::cleanup()
{
    m_scanTimer->stop();
    m_patterns.clear();
    m_initialized = false;
}

QJsonObject SecurityScannerPlugin::executeAction(const QString& action, const QJsonObject& params)
{
    if (!m_initialized) {
        return createResult(false, {}, "Plugin not initialized");
    }
    
    if (action == "scanCode") {
        QString code = params["code"].toString();
        QString language = params["language"].toString("cpp");
        return scanCode(code, language);
        
    } else if (action == "scanFile") {
        QString filePath = params["filePath"].toString();
        return scanFile(filePath);
        
    } else if (action == "scanDirectory") {
        QString dirPath = params["dirPath"].toString();
        return scanDirectory(dirPath);
        
    } else if (action == "quickScan") {
        QString code = params["code"].toString();
        return quickScan(code);
        
    } else if (action == "setSeverityThreshold") {
        int threshold = params["threshold"].toInt(Medium);
        setSeverityThreshold(static_cast<Severity>(threshold));
        return createResult(true, {{"threshold", threshold}});
        
    } else if (action == "setAutoFix") {
        bool enabled = params["enabled"].toBool();
        setAutoFixEnabled(enabled);
        return createResult(true, {{"autoFix", enabled}});
    }
    
    return createResult(false, {}, "Unknown action: " + action);
}

QStringList SecurityScannerPlugin::getAvailableActions() const
{
    return {
        "scanCode",
        "scanFile",
        "scanDirectory",
        "quickScan",
        "setSeverityThreshold",
        "setAutoFix"
    };
}

void SecurityScannerPlugin::onCodeAnalyzed(const QString& code, const QString& context)
{
    // Automatically scan code when it's analyzed
    if (m_autoFixEnabled) {
        QJsonObject result = quickScan(code);
        
        int vulnCount = result["vulnerabilitiesFound"].toInt();
        if (vulnCount > 0) {
            emit pluginMessage("Vulnerabilities detected during code analysis", result);
        }
    }
}

void SecurityScannerPlugin::onFileOpened(const QString& filePath)
{
    // Automatically scan file when opened
    if (!shouldExclude(filePath)) {
        QJsonObject result = scanFile(filePath);
        
        int vulnCount = result["vulnerabilitiesFound"].toInt();
        if (vulnCount > 0) {
            emit pluginMessage("Vulnerabilities detected in opened file", result);
        }
    }
}

// ============================================================================
// Scanning Methods
// ============================================================================

QJsonObject SecurityScannerPlugin::scanCode(const QString& code, const QString& language)
{
    emit scanStarted("code snippet");
    
    QList<Vulnerability> vulnerabilities;
    
    // Run all detection methods
    vulnerabilities.append(detectInjectionVulnerabilities(code, language));
    vulnerabilities.append(detectXSSVulnerabilities(code, language));
    vulnerabilities.append(detectHardcodedSecrets(code));
    vulnerabilities.append(detectInsecureCrypto(code));
    vulnerabilities.append(detectPathTraversal(code));
    vulnerabilities.append(detectCommandInjection(code));
    vulnerabilities.append(detectBufferOverflows(code));
    vulnerabilities.append(detectInsecureDeserialization(code));
    vulnerabilities.append(detectBrokenAuthentication(code));
    vulnerabilities.append(detectSensitiveDataExposure(code));
    
    // Filter by severity threshold
    QList<Vulnerability> filtered;
    for (const Vulnerability& vuln : vulnerabilities) {
        if (vuln.severity <= m_severityThreshold) {
            filtered.append(vuln);
            emit vulnerabilityFound(vuln);
        }
    }
    
    QJsonObject report = generateReport(filtered);
    
    emit scanCompleted(
        filtered.size(),
        report["criticalCount"].toInt(),
        report["highCount"].toInt()
    );
    
    return report;
}

QJsonObject SecurityScannerPlugin::scanFile(const QString& filePath)
{
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return createResult(false, {}, "Cannot open file: " + filePath);
    }
    
    QString code = QTextStream(&file).readAll();
    file.close();
    
    QString language = detectLanguage(filePath);
    
    QJsonObject result = scanCode(code, language);
    result["filePath"] = filePath;
    result["language"] = language;
    
    return result;
}

QJsonObject SecurityScannerPlugin::scanDirectory(const QString& dirPath)
{
    emit scanStarted(dirPath);
    
    QStringList codeExtensions = {
        "*.cpp", "*.h", "*.c", "*.hpp", "*.cc",
        "*.py", "*.js", "*.ts", "*.java", "*.cs",
        "*.php", "*.rb", "*.go", "*.rs"
    };
    
    QList<Vulnerability> allVulnerabilities;
    int filesScanned = 0;
    int totalFiles = 0;
    
    // Count total files first
    QDirIterator countIt(dirPath, codeExtensions, QDir::Files, QDirIterator::Subdirectories);
    while (countIt.hasNext()) {
        countIt.next();
        if (!shouldExclude(countIt.filePath())) {
            totalFiles++;
        }
    }
    
    // Scan all files
    QDirIterator it(dirPath, codeExtensions, QDir::Files, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        QString filePath = it.next();
        
        if (shouldExclude(filePath)) {
            continue;
        }
        
        QJsonObject result = scanFile(filePath);
        
        if (result["success"].toBool()) {
            QJsonArray vulns = result["vulnerabilities"].toArray();
            for (const QJsonValue& val : vulns) {
                allVulnerabilities.append(Vulnerability::fromJson(val.toObject()));
            }
        }
        
        filesScanned++;
        emit scanProgress(filesScanned, totalFiles);
    }
    
    QJsonObject report = generateReport(allVulnerabilities);
    report["filesScanned"] = filesScanned;
    report["directory"] = dirPath;
    
    emit scanCompleted(
        allVulnerabilities.size(),
        report["criticalCount"].toInt(),
        report["highCount"].toInt()
    );
    
    return report;
}

QJsonObject SecurityScannerPlugin::quickScan(const QString& code)
{
    // Quick scan focuses on critical and high severity issues only
    Severity originalThreshold = m_severityThreshold;
    m_severityThreshold = High;
    
    QJsonObject result = scanCode(code, "cpp");
    
    m_severityThreshold = originalThreshold;
    
    return result;
}

// ============================================================================
// Detection Methods
// ============================================================================

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectInjectionVulnerabilities(
    const QString& code, const QString& language)
{
    QList<Vulnerability> vulnerabilities;
    
    // SQL Injection patterns
    QStringList sqlPatterns = {
        R"(query\s*\+\s*["\'])",           // String concatenation in queries
        R"(execute\s*\(\s*["\'].*\+)",     // Direct query execution
        R"(exec\s*\(\s*["\'].*\+)",
        R"(QString\s*sql\s*=.*\+)",        // Qt SQL injection
    };
    
    for (const QString& patternStr : sqlPatterns) {
        QRegularExpression regex(patternStr, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "SQL Injection";
            vuln.severity = Critical;
            vuln.description = "Potential SQL injection vulnerability detected through string concatenation";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1;
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = extractCodeSnippet(code, vuln.line);
            vuln.recommendation = "Use parameterized queries or prepared statements instead of string concatenation";
            vuln.owaspCategory = "A03:2021 - Injection";
            vuln.cwe = {"CWE-89"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectXSSVulnerabilities(
    const QString& code, const QString& language)
{
    QList<Vulnerability> vulnerabilities;
    
    if (language != "html" && language != "javascript" && language != "php") {
        return vulnerabilities; // XSS primarily affects web languages
    }
    
    QStringList xssPatterns = {
        R"(innerHTML\s*=)",
        R"(document\.write\s*\()",
        R"(eval\s*\()",
        R"(\$\{.*\})",  // Template literals without sanitization
    };
    
    for (const QString& patternStr : xssPatterns) {
        QRegularExpression regex(patternStr);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "Cross-Site Scripting (XSS)";
            vuln.severity = High;
            vuln.description = "Potential XSS vulnerability through unsafe DOM manipulation";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1;
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = extractCodeSnippet(code, vuln.line);
            vuln.recommendation = "Sanitize user input and use textContent instead of innerHTML";
            vuln.owaspCategory = "A03:2021 - Injection";
            vuln.cwe = {"CWE-79"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectHardcodedSecrets(const QString& code)
{
    QList<Vulnerability> vulnerabilities;
    
    QStringList secretPatterns = {
        R"((password|passwd|pwd)\s*=\s*["\'][^"\']{3,}["\'])",
        R"((api[_-]?key|apikey)\s*=\s*["\'][^"\']{10,}["\'])",
        R"((secret|token)\s*=\s*["\'][^"\']{10,}["\'])",
        R"((aws|azure|gcp)[_-]?(access|secret)[_-]?key\s*=\s*["\'])",
        R"(["\'][0-9a-f]{32,}["\'])",  // Possible hash/key
    };
    
    for (const QString& patternStr : secretPatterns) {
        QRegularExpression regex(patternStr, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "Hardcoded Credentials";
            vuln.severity = Critical;
            vuln.description = "Hardcoded credentials or secrets detected in source code";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1;
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = "*** REDACTED FOR SECURITY ***";
            vuln.recommendation = "Use environment variables or secure configuration management";
            vuln.owaspCategory = "A07:2021 - Identification and Authentication Failures";
            vuln.cwe = {"CWE-798"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectInsecureCrypto(const QString& code)
{
    QList<Vulnerability> vulnerabilities;
    
    QStringList insecureAlgorithms = {
        R"(\bMD5\b)",
        R"(\bSHA1\b)",
        R"(\bDES\b)",
        R"(\bRC4\b)",
        R"(\bECB\b)",  // Electronic Codebook mode
    };
    
    for (const QString& patternStr : insecureAlgorithms) {
        QRegularExpression regex(patternStr);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "Insecure Cryptography";
            vuln.severity = High;
            vuln.description = "Use of weak or deprecated cryptographic algorithm detected";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1;
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = extractCodeSnippet(code, vuln.line);
            vuln.recommendation = "Use SHA-256/SHA-3 for hashing, AES-256-GCM for encryption";
            vuln.owaspCategory = "A02:2021 - Cryptographic Failures";
            vuln.cwe = {"CWE-327"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectPathTraversal(const QString& code)
{
    QList<Vulnerability> vulnerabilities;
    
    QStringList pathTraversalPatterns = {
        R"(fopen\s*\([^)]*\.\.[^)]*\))",
        R"(QFile\s*\([^)]*\.\.[^)]*\))",
        R"(readFile\s*\([^)]*\+)",
        R"(openFile\s*\([^)]*\+)",
    };
    
    for (const QString& patternStr : pathTraversalPatterns) {
        QRegularExpression regex(patternStr);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "Path Traversal";
            vuln.severity = High;
            vuln.description = "Potential path traversal vulnerability detected";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1;
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = extractCodeSnippet(code, vuln.line);
            vuln.recommendation = "Validate and sanitize file paths, use allowlist of permitted paths";
            vuln.owaspCategory = "A01:2021 - Broken Access Control";
            vuln.cwe = {"CWE-22"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectCommandInjection(const QString& code)
{
    QList<Vulnerability> vulnerabilities;
    
    QStringList commandInjectionPatterns = {
        R"(system\s*\([^)]*\+)",
        R"(exec\s*\([^)]*\+)",
        R"(QProcess.*start\s*\([^)]*\+)",
        R"(popen\s*\([^)]*\+)",
    };
    
    for (const QString& patternStr : commandInjectionPatterns) {
        QRegularExpression regex(patternStr);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "Command Injection";
            vuln.severity = Critical;
            vuln.description = "Potential command injection vulnerability detected";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1;
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = extractCodeSnippet(code, vuln.line);
            vuln.recommendation = "Use parameterized command execution, validate and sanitize all inputs";
            vuln.owaspCategory = "A03:2021 - Injection";
            vuln.cwe = {"CWE-78"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectBufferOverflows(const QString& code)
{
    QList<Vulnerability> vulnerabilities;
    
    QStringList bufferOverflowPatterns = {
        R"(strcpy\s*\()",
        R"(strcat\s*\()",
        R"(sprintf\s*\()",
        R"(gets\s*\()",
    };
    
    for (const QString& patternStr : bufferOverflowPatterns) {
        QRegularExpression regex(patternStr);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "Buffer Overflow";
            vuln.severity = High;
            vuln.description = "Use of unsafe function that may cause buffer overflow";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1);
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = extractCodeSnippet(code, vuln.line);
            vuln.recommendation = "Use safer alternatives: strncpy, strncat, snprintf, fgets";
            vuln.owaspCategory = "A04:2021 - Insecure Design";
            vuln.cwe = {"CWE-120"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectInsecureDeserialization(const QString& code)
{
    QList<Vulnerability> vulnerabilities;
    
    QStringList deserializationPatterns = {
        R"(pickle\.loads\s*\()",
        R"(unserialize\s*\()",
        R"(readObject\s*\()",
        R"(QDataStream.*>>)",  // Qt serialization
    };
    
    for (const QString& patternStr : deserializationPatterns) {
        QRegularExpression regex(patternStr);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "Insecure Deserialization";
            vuln.severity = High;
            vuln.description = "Potential insecure deserialization vulnerability";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1;
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = extractCodeSnippet(code, vuln.line);
            vuln.recommendation = "Validate serialized data, use safe serialization formats like JSON";
            vuln.owaspCategory = "A08:2021 - Software and Data Integrity Failures";
            vuln.cwe = {"CWE-502"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectBrokenAuthentication(const QString& code)
{
    QList<Vulnerability> vulnerabilities;
    
    // Detect weak password policies, missing authentication, etc.
    QStringList authPatterns = {
        R"(login\s*\([^)]*\)\s*\{[^}]*return\s+true)",  // Always returns true
        R"(authenticate\s*\([^)]*\)\s*\{[^}]*//\s*TODO)",
        R"(if\s*\(\s*password\s*==\s*["\'])",  // Hardcoded password check
    };
    
    for (const QString& patternStr : authPatterns) {
        QRegularExpression regex(patternStr, QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "Broken Authentication";
            vuln.severity = Critical;
            vuln.description = "Weak or missing authentication mechanism detected";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1;
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = extractCodeSnippet(code, vuln.line);
            vuln.recommendation = "Implement proper authentication with password hashing and secure session management";
            vuln.owaspCategory = "A07:2021 - Identification and Authentication Failures";
            vuln.cwe = {"CWE-287"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

QList<SecurityScannerPlugin::Vulnerability> SecurityScannerPlugin::detectSensitiveDataExposure(const QString& code)
{
    QList<Vulnerability> vulnerabilities;
    
    QStringList sensitiveDataPatterns = {
        R"(console\.log\s*\([^)]*password)",
        R"(qDebug\(\)\s*<<[^;]*password)",
        R"(print\s*\([^)]*password)",
        R"(System\.out\.println\s*\([^)]*password)",
    };
    
    for (const QString& patternStr : sensitiveDataPatterns) {
        QRegularExpression regex(patternStr, QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator it = regex.globalMatch(code);
        
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            
            Vulnerability vuln;
            vuln.type = "Sensitive Data Exposure";
            vuln.severity = Medium;
            vuln.description = "Sensitive data may be exposed through logging";
            vuln.location = QString("Line %1").arg(code.left(match.capturedStart()).count('\n') + 1);
            vuln.line = code.left(match.capturedStart()).count('\n') + 1;
            vuln.column = match.capturedStart() - code.lastIndexOf('\n', match.capturedStart());
            vuln.codeSnippet = extractCodeSnippet(code, vuln.line);
            vuln.recommendation = "Remove logging of sensitive data or redact before logging";
            vuln.owaspCategory = "A02:2021 - Cryptographic Failures";
            vuln.cwe = {"CWE-532"};
            
            vulnerabilities.append(vuln);
        }
    }
    
    return vulnerabilities;
}

// ============================================================================
// Helper Methods
// ============================================================================

void SecurityScannerPlugin::initializePatterns()
{
    // This method is called to initialize the vulnerability patterns database
    // Patterns are defined in the detection methods above
    
    qDebug() << "[SecurityScanner] Initialized vulnerability detection patterns";
}

void SecurityScannerPlugin::loadCustomPatterns(const QString& filePath)
{
    QFile file(filePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        emit pluginError("Cannot load custom patterns file: " + filePath);
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isArray()) {
        emit pluginError("Invalid custom patterns file format");
        return;
    }
    
    // TODO: Parse and add custom patterns
    
    qDebug() << "[SecurityScanner] Loaded custom patterns from:" << filePath;
}

QString SecurityScannerPlugin::extractCodeSnippet(const QString& code, int line, int contextLines)
{
    QStringList lines = code.split('\n');
    
    int startLine = qMax(0, line - contextLines - 1);
    int endLine = qMin(lines.size() - 1, line + contextLines - 1);
    
    QStringList snippet;
    for (int i = startLine; i <= endLine; ++i) {
        QString prefix = (i == line - 1) ? " >>> " : "     ";
        snippet.append(QString("%1%2: %3").arg(prefix).arg(i + 1).arg(lines[i]));
    }
    
    return snippet.join('\n');
}

QString SecurityScannerPlugin::detectLanguage(const QString& filePath)
{
    if (filePath.endsWith(".cpp") || filePath.endsWith(".cc") || filePath.endsWith(".cxx")) return "cpp";
    if (filePath.endsWith(".h") || filePath.endsWith(".hpp")) return "cpp";
    if (filePath.endsWith(".c")) return "c";
    if (filePath.endsWith(".py")) return "python";
    if (filePath.endsWith(".js")) return "javascript";
    if (filePath.endsWith(".ts")) return "typescript";
    if (filePath.endsWith(".java")) return "java";
    if (filePath.endsWith(".cs")) return "csharp";
    if (filePath.endsWith(".php")) return "php";
    if (filePath.endsWith(".rb")) return "ruby";
    if (filePath.endsWith(".go")) return "go";
    if (filePath.endsWith(".rs")) return "rust";
    
    return "unknown";
}

bool SecurityScannerPlugin::shouldExclude(const QString& path)
{
    for (const QString& pattern : m_excludePatterns) {
        if (path.contains(pattern)) {
            return true;
        }
    }
    
    return false;
}

QJsonObject SecurityScannerPlugin::generateReport(const QList<Vulnerability>& vulnerabilities)
{
    QJsonObject report;
    
    int criticalCount = 0;
    int highCount = 0;
    int mediumCount = 0;
    int lowCount = 0;
    int infoCount = 0;
    
    QJsonArray vulnArray;
    
    for (const Vulnerability& vuln : vulnerabilities) {
        vulnArray.append(vuln.toJson());
        
        switch (vuln.severity) {
            case Critical: criticalCount++; break;
            case High: highCount++; break;
            case Medium: mediumCount++; break;
            case Low: lowCount++; break;
            case Info: infoCount++; break;
        }
    }
    
    report["success"] = true;
    report["vulnerabilitiesFound"] = vulnerabilities.size();
    report["criticalCount"] = criticalCount;
    report["highCount"] = highCount;
    report["mediumCount"] = mediumCount;
    report["lowCount"] = lowCount;
    report["infoCount"] = infoCount;
    report["vulnerabilities"] = vulnArray;
    report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return report;
}

void SecurityScannerPlugin::setSeverityThreshold(Severity threshold)
{
    m_severityThreshold = threshold;
    emit pluginMessage("Severity threshold updated", QJsonObject{{"threshold", threshold}});
}

void SecurityScannerPlugin::setAutoFixEnabled(bool enabled)
{
    m_autoFixEnabled = enabled;
    emit pluginMessage("Auto-fix setting updated", QJsonObject{{"enabled", enabled}});
}

void SecurityScannerPlugin::setExcludePatterns(const QStringList& patterns)
{
    m_excludePatterns = patterns;
    emit pluginMessage("Exclude patterns updated", QJsonObject{{"count", patterns.size()}});
}

void SecurityScannerPlugin::performScheduledScan()
{
    emit pluginMessage("Scheduled scan triggered");
    // TODO: Implement scheduled scanning logic
}

// ============================================================================
// Serialization
// ============================================================================

QJsonObject SecurityScannerPlugin::Vulnerability::toJson() const
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
    obj["owaspCategory"] = owaspCategory;
    
    QJsonArray cweArray;
    for (const QString& id : cwe) {
        cweArray.append(id);
    }
    obj["cwe"] = cweArray;
    
    return obj;
}

SecurityScannerPlugin::Vulnerability SecurityScannerPlugin::Vulnerability::fromJson(const QJsonObject& obj)
{
    Vulnerability vuln;
    vuln.type = obj["type"].toString();
    vuln.severity = static_cast<Severity>(obj["severity"].toInt());
    vuln.description = obj["description"].toString();
    vuln.location = obj["location"].toString();
    vuln.line = obj["line"].toInt();
    vuln.column = obj["column"].toInt();
    vuln.codeSnippet = obj["codeSnippet"].toString();
    vuln.recommendation = obj["recommendation"].toString();
    vuln.owaspCategory = obj["owaspCategory"].toString();
    
    QJsonArray cweArray = obj["cwe"].toArray();
    for (const QJsonValue& val : cweArray) {
        vuln.cwe.append(val.toString());
    }
    
    return vuln;
}
