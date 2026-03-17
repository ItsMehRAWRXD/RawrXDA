#include "digestion_reverse_engineering_enterprise.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTextStream>
#include <QDateTime>
#include <QtConcurrent>
#include <QFutureSynchronizer>
#include <QSaveFile>
#include <QProgressBar>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QDebug>
#include <QProcess>
#include <QStack>
#include <QCryptographicHash>
#include <QThread>
#include <QThreadPool>
#include <cmath>

DigestionReverseEngineeringSystem::DigestionReverseEngineeringSystem(QObject *parent)
    : QObject(parent) {
    initializeLanguageProfiles();
    initializeFixTemplates();
    m_stats.parallelWorkers = QThreadPool::globalInstance()->maxThreadCount();
}

DigestionReverseEngineeringSystem::~DigestionReverseEngineeringSystem() {
    stop();
}

void DigestionReverseEngineeringSystem::initializeLanguageProfiles() {
    m_profiles.clear();
    
    // C++ Profile
    LanguageProfile cpp;
    cpp.name = "C++";
    cpp.extensions = QStringList{"cpp", "hpp", "h", "cc", "cxx", "c++", "inl"};
    cpp.singleLineComment = "//";
    cpp.multiLineCommentStart = "/*";
    cpp.multiLineCommentEnd = "*/";
    cpp.stubPatterns << QRegularExpression("\\bTODO\\s*:\\s*(.+)", QRegularExpression::CaseInsensitiveOption)
                    << QRegularExpression("\\bFIXME\\s*:\\s*(.+)", QRegularExpression::CaseInsensitiveOption)
                    << QRegularExpression("\\bSTUB\\s*:\\s*(.+)", QRegularExpression::CaseInsensitiveOption)
                    << QRegularExpression("\\bNOT_IMPLEMENTED\\b", QRegularExpression::CaseInsensitiveOption)
                    << QRegularExpression("throw\\s+std::(?:runtime_error|logic_error|exception)\\s*\\(\\s*[\"']Not implemented", QRegularExpression::CaseInsensitiveOption)
                    << QRegularExpression("return\\s+(?:false|0|nullptr|NULL)\\s*;\\s*//\\s*stub", QRegularExpression::CaseInsensitiveOption)
                    << QRegularExpression("Q_UNIMPLEMENTED\\(\\)", QRegularExpression::CaseInsensitiveOption)
                    << QRegularExpression("\\{\\s*\\}\\s*//\\s*TODO", QRegularExpression::CaseInsensitiveOption);
    cpp.complexityWeight = 2;
    m_profiles.append(cpp);
    m_profileMap["C++"] = cpp;
    
    // MASM Profile (x64)
    LanguageProfile masm;
    masm.name = "MASM";
    masm.extensions = QStringList{"asm", "inc", "masm"};
    masm.singleLineComment = ";";
    masm.multiLineCommentStart = "COMMENT !";
    masm.multiLineCommentEnd = "!";
    masm.stubPatterns << QRegularExpression(";\\s*TODO\\s*:\\s*(.+)", QRegularExpression::CaseInsensitiveOption)
                     << QRegularExpression(";\\s*STUB", QRegularExpression::CaseInsensitiveOption)
                     << QRegularExpression(";\\s*FIXME", QRegularExpression::CaseInsensitiveOption)
                     << QRegularExpression("invoke\\s+ExitProcess\\s*,\\s*0\\s*;\\s*stub", QRegularExpression::CaseInsensitiveOption)
                     << QRegularExpression("ret\\s*;\\s*unimplemented", QRegularExpression::CaseInsensitiveOption)
                     << QRegularExpression("db\\s+['\"]NOT_IMPLEMENTED['\"]", QRegularExpression::CaseInsensitiveOption)
                     << QRegularExpression(";\\s*PLACEHOLDER", QRegularExpression::CaseInsensitiveOption)
                     << QRegularExpression("xor\\s+(?:eax|rax)\\s*,\\s*(?:eax|rax)\\s*;\\s*stub return", QRegularExpression::CaseInsensitiveOption);
    masm.complexityWeight = 1;
    m_profiles.append(masm);
    m_profileMap["MASM"] = masm;
    
    // Python Profile
    LanguageProfile py;
    py.name = "Python";
    py.extensions = QStringList{"py", "pyw", "pyi"};
    py.singleLineComment = "#";
    py.multiLineCommentStart = "\"\"\"";
    py.multiLineCommentEnd = "\"\"\"";
    py.stubPatterns << QRegularExpression("#\\s*TODO\\s*:\\s*(.+)", QRegularExpression::CaseInsensitiveOption)
                   << QRegularExpression("#\\s*FIXME", QRegularExpression::CaseInsensitiveOption)
                   << QRegularExpression("pass\\s*#\\s*stub", QRegularExpression::CaseInsensitiveOption)
                   << QRegularExpression("raise\\s+NotImplementedError\\s*\\(\\s*['\"]", QRegularExpression::CaseInsensitiveOption)
                   << QRegularExpression("return\\s+None\\s*#\\s*stub", QRegularExpression::CaseInsensitiveOption)
                   << QRegularExpression("\\.\\.\\.", QRegularExpression::CaseInsensitiveOption);
    py.complexityWeight = 1;
    m_profiles.append(py);
    m_profileMap["Python"] = py;
    
    // CMake Profile
    LanguageProfile cmake;
    cmake.name = "CMake";
    cmake.extensions = QStringList{"cmake", "txt"};
    cmake.singleLineComment = "#";
    cmake.stubPatterns << QRegularExpression("#\\s*TODO", QRegularExpression::CaseInsensitiveOption)
                      << QRegularExpression("message\\s*\\(\\s*FATAL_ERROR\\s+['\"]Not implemented", QRegularExpression::CaseInsensitiveOption);
    cmake.complexityWeight = 1;
    m_profiles.append(cmake);
    m_profileMap["CMake"] = cmake;
    
    // JavaScript/TypeScript
    LanguageProfile js;
    js.name = "JavaScript";
    js.extensions = QStringList{"js", "jsx", "ts", "tsx", "mjs"};
    js.singleLineComment = "//";
    js.multiLineCommentStart = "/*";
    js.multiLineCommentEnd = "*/";
    js.stubPatterns << QRegularExpression("//\\s*TODO", QRegularExpression::CaseInsensitiveOption)
                   << QRegularExpression("//\\s*FIXME", QRegularExpression::CaseInsensitiveOption)
                   << QRegularExpression("throw\\s+new\\s+Error\\s*\\(\\s*['\"]Not implemented", QRegularExpression::CaseInsensitiveOption)
                   << QRegularExpression("return\\s+(?:null|undefined)\\s*;\\s*//\\s*stub", QRegularExpression::CaseInsensitiveOption);
    js.complexityWeight = 1;
    m_profiles.append(js);
    m_profileMap["JavaScript"] = js;
}

void DigestionReverseEngineeringSystem::initializeFixTemplates() {
    auto &cpp = m_profileMap["C++"];
    cpp.fixTemplates["TODO"] = "// [AGENTIC-IMPL] ${date}\n// Original TODO: ${match}\n${indent}// Implementation required here\n${indent}throw std::runtime_error(\"Implementation pending: ${match}\");";
    cpp.fixTemplates["return false; // stub"] = "// [AGENTIC-IMPL] ${date}\n${indent}return true; // Default implementation - review required";
    cpp.fixTemplates["throw std::runtime_error(\"Not implemented"] = "// [AGENTIC-IMPL] ${date}\n${indent}// TODO: Implement proper logic\n${indent}return {}; // Default return";
    
    auto &masm = m_profileMap["MASM"];
    masm.fixTemplates["invoke ExitProcess, 0 ; stub"] = "; [AGENTIC-IMPL] ${date}\n${indent}mov rsp, rbp\n${indent}pop rbp\n${indent}ret ; Proper return instead of ExitProcess";
    masm.fixTemplates["ret ; unimplemented"] = "; [AGENTIC-IMPL] ${date}\n${indent}xor eax, eax ; Return 0\n${indent}ret";
    masm.fixTemplates["xor eax, eax ; stub return"] = "; [AGENTIC-IMPL] ${date}\n${indent}xor eax, eax ; Success code\n${indent}ret";
    
    auto &py = m_profileMap["Python"];
    py.fixTemplates["pass # stub"] = "# [AGENTIC-IMPL] ${date}\n${indent}raise NotImplementedError(\"${context}\")";
    py.fixTemplates["raise NotImplementedError"] = "# [AGENTIC-IMPL] ${date}\n${indent}pass # TODO: Implement ${context}";
}

void DigestionReverseEngineeringSystem::runFullDigestionPipeline(
    const QString &rootDir, int maxFiles, int chunkSize, 
    int maxTasksPerFile, bool applyExtensions, const QStringList &excludePatterns) {
    
    if (m_running.loadAcquire()) return;
    
    m_running.storeRelease(1);
    m_stopRequested.storeRelease(0);
    m_paused.storeRelease(0);
    m_rootDir = rootDir;
    
    {
        QMutexLocker lock(&m_mutex);
        m_stats = DigestionStats();
        m_results = QJsonArray();
        m_pendingTasks.clear();
    }
    
    m_timer.start();
    emit runningChanged(true);
    emit progressUpdate(0, 0, 0, 0);
    
    QStringList allFiles = collectFiles(rootDir, maxFiles, excludePatterns);
    
    {
        QMutexLocker lock(&m_mutex);
        m_stats.totalFiles.storeRelease(allFiles.size());
    }
    
    int totalChunks = (allFiles.size() + chunkSize - 1) / chunkSize;
    
    QFutureSynchronizer<void> synchronizer;
    for (int i = 0; i < allFiles.size() && !m_stopRequested.loadAcquire(); i += chunkSize) {
        QStringList chunk = allFiles.mid(i, chunkSize);
        int chunkId = i / chunkSize;
        
        QFuture<void> future = QtConcurrent::run([this, chunk, chunkId, totalChunks, maxTasksPerFile, applyExtensions]() {
            processChunk(chunk, chunkId, totalChunks, maxTasksPerFile, applyExtensions);
        });
        synchronizer.addFuture(future);
        
        if (synchronizer.futures().size() >= QThread::idealThreadCount()) {
            synchronizer.waitForFinished();
        }
    }
    
    synchronizer.waitForFinished();
    
    m_running.storeRelease(0);
    generateReport();
    emit runningChanged(false);
    emit pipelineFinished(m_lastReport, m_timer.elapsed());
}

QStringList DigestionReverseEngineeringSystem::collectFiles(const QString &rootDir, int maxFiles, const QStringList &excludes) {
    QStringList files;
    QDirIterator it(rootDir, QDir::Files, QDir::Subdirectories);
    
    while (it.hasNext()) {
        if (m_stopRequested.loadAcquire()) break;
        
        QString file = it.next();
        QFileInfo info(file);
        
        bool excluded = false;
        for (const QString &pattern : excludes) {
            if (file.contains(pattern) || info.fileName().matches(pattern, Qt::CaseInsensitive)) {
                excluded = true;
                break;
            }
        }
        if (excluded) continue;
        
        if (!detectLanguage(file).isEmpty()) {
            files << file;
            if (maxFiles > 0 && files.size() >= maxFiles) break;
        }
    }
    
    return files;
}

void DigestionReverseEngineeringSystem::processChunk(const QStringList &files, int chunkId, 
                                                     int totalChunks, int maxTasksPerFile, bool applyExtensions) {
    int stubsInChunk = 0;
    
    for (const QString &file : files) {
        if (m_stopRequested.loadAcquire()) return;
        
        while (m_paused.loadAcquire() && !m_stopRequested.loadAcquire()) {
            QThread::msleep(100);
        }
        
        scanFile(file, maxTasksPerFile, applyExtensions);
        
        int current = m_currentFileIndex.fetchAndAddRelaxed(1) + 1;
        int total = m_stats.totalFiles.loadAcquire();
        int percent = (current * 100) / qMax(total, 1);
        
        emit progressUpdate(current, total, m_stats.stubsFound.loadAcquire(), percent);
    }
    
    emit chunkCompleted(chunkId + 1, totalChunks, stubsInChunk);
}

void DigestionReverseEngineeringSystem::scanFile(const QString &filePath, int maxTasksPerFile, bool applyExtensions) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit errorOccurred(filePath, "Cannot open file: " + file.errorString(), true);
        m_stats.errors.fetchAndAddRelaxed(1);
        return;
    }
    
    QByteArray rawContent = file.readAll();
    file.close();
    
    QString content = QString::fromUtf8(rawContent);
    if (content.contains(QChar::ReplacementCharacter)) {
        content = QString::fromLatin1(rawContent);
    }
    
    QString langName = detectLanguage(filePath);
    if (langName.isEmpty()) return;
    
    LanguageProfile lang = m_profileMap[langName];
    auto tasks = findStubs(content, lang, filePath, maxTasksPerFile);
    
    if (!tasks.isEmpty()) {
        m_stats.stubsByLanguage[langName] += tasks.size();
        for (const auto &task : tasks) {
            m_stats.stubsByType[task.stubType]++;
        }
    }
    
    m_stats.scannedFiles.fetchAndAddRelaxed(1);
    m_stats.stubsFound.fetchAndAddRelaxed(tasks.size());
    
    emit fileScanned(filePath, tasks.size(), langName);
    
    {
        QMutexLocker lock(&m_mutex);
        for (const auto &task : tasks) {
            m_pendingTasks.append(task);
        }
    }
    
    if (applyExtensions && !tasks.isEmpty()) {
        QString modifiedContent = content;
        bool modified = false;
        
        QList<AgenticTask> sortedTasks = tasks;
        std::sort(sortedTasks.begin(), sortedTasks.end(), 
                  [](const AgenticTask &a, const AgenticTask &b) { return a.lineNumber > b.lineNumber; });
        
        for (auto &task : sortedTasks) {
            if (m_stopRequested.loadAcquire()) break;
            
            QElapsedTimer taskTimer;
            taskTimer.start();
            
            if (applyAgenticExtension(filePath, task)) {
                m_stats.extensionsApplied.fetchAndAddRelaxed(1);
                emit extensionApplied(filePath, task.lineNumber, task.stubType);
                modified = true;
            } else {
                emit extensionFailed(filePath, task.lineNumber, "Apply failed");
            }
        }
    }
    
    CodeMetrics metrics = calculateMetrics(content, lang);
    emit metricsCalculated(filePath, metrics);
    
    QJsonObject fileResult;
    fileResult["file"] = filePath;
    fileResult["language"] = langName;
    fileResult["stubs_found"] = tasks.size();
    fileResult["lines_of_code"] = metrics.linesOfCode;
    fileResult["complexity"] = metrics.cyclomaticComplexity;
    
    QJsonArray taskArray;
    for (const auto &task : tasks) {
        QJsonObject t;
        t["line"] = task.lineNumber;
        t["column"] = task.column;
        t["type"] = task.stubType;
        t["severity"] = task.severity;
        t["context"] = task.contextBefore + "\n>>> " + task.originalCode + " <<<\n" + task.contextAfter;
        t["suggested_fix"] = task.suggestedFix;
        t["applied"] = task.applied;
        taskArray.append(t);
    }
    fileResult["tasks"] = taskArray;
    
    {
        QMutexLocker lock(&m_mutex);
        m_results.append(fileResult);
    }
    
    emitProgress();
}

QList<AgenticTask> DigestionReverseEngineeringSystem::findStubs(
    const QString &content, const LanguageProfile &lang, const QString &filePath, int maxTasks) {
    
    QList<AgenticTask> tasks;
    QStringList lines = content.split('\n');
    
    for (int i = 0; i < lines.size(); ++i) {
        if (maxTasks > 0 && tasks.size() >= maxTasks) break;
        
        const QString &line = lines[i];
        
        for (const QRegularExpression &pattern : lang.stubPatterns) {
            QRegularExpressionMatch match = pattern.match(line);
            if (match.hasMatch()) {
                AgenticTask task;
                task.filePath = filePath;
                task.lineNumber = i + 1;
                task.column = match.capturedStart();
                task.stubType = match.captured(0);
                task.originalCode = line;
                
                if (task.stubType.contains("TODO", Qt::CaseInsensitive)) task.severity = "info";
                else if (task.stubType.contains("FIXME", Qt::CaseInsensitive)) task.severity = "warning";
                else if (task.stubType.contains("STUB", Qt::CaseInsensitive) || 
                         task.stubType.contains("NotImplemented", Qt::CaseInsensitive)) {
                    task.severity = "critical";
                    m_stats.criticals.fetchAndAddRelaxed(1);
                } else {
                    task.severity = "warning";
                }
                
                int startLine = qMax(0, i - 5);
                int endLine = qMin(lines.size() - 1, i + 5);
                task.contextBefore = lines.mid(startLine, i - startLine).join('\n');
                task.contextAfter = lines.mid(i + 1, endLine - i).join('\n');
                task.fullFunctionContext = extractFunctionContext(content, i);
                
                task.suggestedFix = generateFix(task, lang);
                
                tasks.append(task);
                emit agenticTaskGenerated(task);
                break;
            }
        }
    }
    
    return tasks;
}

QString DigestionReverseEngineeringSystem::extractFunctionContext(const QString &content, int lineNumber) {
    QStringList lines = content.split('\n');
    int braceCount = 0;
    int start = lineNumber;
    int end = lineNumber;
    bool inFunction = false;
    
    for (int i = lineNumber; i >= 0; --i) {
        QString line = lines[i];
        if (line.contains('{')) {
            braceCount++;
            inFunction = true;
        }
        if (line.contains('}')) braceCount--;
        
        if (inFunction && braceCount == 0) {
            start = i;
            break;
        }
        
        if (line.contains(QRegularExpression("\\b(?:def|void|int|bool|QString|function)\\s+\\w+\\s*\\("))) {
            start = i;
            break;
        }
    }
    
    braceCount = 0;
    for (int i = lineNumber; i < lines.size(); ++i) {
        QString line = lines[i];
        if (line.contains('{')) braceCount++;
        if (line.contains('}')) {
            braceCount--;
            if (braceCount == 0) {
                end = i;
                break;
            }
        }
    }
    
    return lines.mid(start, end - start + 1).join('\n');
}

bool DigestionReverseEngineeringSystem::applyAgenticExtension(const QString &filePath, const AgenticTask &task) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    QStringList lines = content.split('\n');
    if (task.lineNumber < 1 || task.lineNumber > lines.size()) return false;
    
    int idx = task.lineNumber - 1;
    QString indent = task.originalCode.left(task.originalCode.length() - task.originalCode.trimmed().length());
    
    QString replacement = task.suggestedFix;
    if (replacement.isEmpty()) {
        if (filePath.endsWith(".asm") || filePath.endsWith(".inc")) {
            replacement = indent + "; [AGENTIC-AUTO] " + QDateTime::currentDateTime().toString(Qt::ISODate) + "\n" +
                         indent + "; Original: " + task.stubType + "\n" +
                         indent + "nop ; Placeholder implementation";
        } else {
            replacement = indent + "// [AGENTIC-AUTO] " + QDateTime::currentDateTime().toString(Qt::ISODate) + "\n" +
                         indent + task.originalCode.trimmed().replace(
                             QRegularExpression("throw.*|return.*false.*|pass.*|TODO.*"), 
                             "/* Implementation required */");
        }
    }
    
    lines[idx] = replacement;
    
    QSaveFile out(filePath);
    if (!out.open(QIODevice::WriteOnly)) return false;
    
    out.write(lines.join('\n').toUtf8());
    bool success = out.commit();
    
    if (success) {
        m_stats.extensionsApplied.fetchAndAddRelaxed(1);
    }
    
    return success;
}

QString DigestionReverseEngineeringSystem::generateFix(const AgenticTask &task, const LanguageProfile &lang) {
    QString templateStr = lang.fixTemplates.value(task.stubType, QString());
    if (templateStr.isEmpty()) {
        templateStr = "// [AGENTIC] ${date}\n${indent}// TODO: Replace stub\n${indent}${original}";
    }
    
    QString result = templateStr;
    result.replace("${date}", QDateTime::currentDateTime().toString(Qt::ISODate));
    result.replace("${match}", task.stubType);
    result.replace("${context}", task.fullFunctionContext.left(50).replace('\n', ' '));
    result.replace("${original}", task.originalCode.trimmed());
    
    QString indent = task.originalCode.left(task.originalCode.length() - task.originalCode.trimmed().length());
    result.replace("${indent}", indent);
    
    return result;
}

QString DigestionReverseEngineeringSystem::detectLanguage(const QString &filePath) {
    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    QString base = info.baseName().toLower();
    
    if (base == "cmakelists" || base == "cmake") return "CMake";
    
    for (const auto &profile : m_profiles) {
        if (profile.extensions.contains(ext)) return profile.name;
    }
    return QString();
}

CodeMetrics DigestionReverseEngineeringSystem::calculateMetrics(const QString &content, const LanguageProfile &lang) {
    CodeMetrics metrics;
    QStringList lines = content.split('\n');
    
    int braceDepth = 0;
    int maxDepth = 0;
    int decisionPoints = 0;
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        
        if (trimmed.isEmpty()) {
            metrics.blankLines++;
        } else if (trimmed.startsWith(lang.singleLineComment) || 
                   trimmed.startsWith(lang.multiLineCommentStart)) {
            metrics.commentLines++;
        } else {
            metrics.linesOfCode++;
            
            if (trimmed.contains(QRegularExpression("\\b(if|while|for|case|catch|\\?\\:|&&|\\|\\|)\\b"))) {
                decisionPoints++;
            }
        }
        
        braceDepth += trimmed.count('{') + trimmed.count('(');
        braceDepth -= trimmed.count('}') + trimmed.count(')');
        maxDepth = qMax(maxDepth, braceDepth);
    }
    
    metrics.cyclomaticComplexity = decisionPoints + 1;
    metrics.maxNestingDepth = maxDepth;
    
    double avgLines = static_cast<double>(metrics.linesOfCode) / qMax(lines.size(), 1);
    metrics.maintainabilityIndex = 171.0 - 5.2 * std::log(metrics.linesOfCode + 1) 
                                   - 0.23 * metrics.cyclomaticComplexity 
                                   - 16.2 * avgLines;
    
    return metrics;
}

void DigestionReverseEngineeringSystem::generateReport() {
    QJsonObject report;
    report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    report["root_directory"] = m_rootDir;
    report["duration_ms"] = static_cast<qint64>(m_timer.elapsed());
    
    QJsonObject statsObj;
    statsObj["total_files"] = static_cast<int>(m_stats.totalFiles.loadAcquire());
    statsObj["scanned_files"] = static_cast<int>(m_stats.scannedFiles.loadAcquire());
    statsObj["stubs_found"] = static_cast<int>(m_stats.stubsFound.loadAcquire());
    statsObj["extensions_applied"] = static_cast<int>(m_stats.extensionsApplied.loadAcquire());
    statsObj["errors"] = static_cast<int>(m_stats.errors.loadAcquire());
    statsObj["critical_stubs"] = static_cast<int>(m_stats.criticals.loadAcquire());
    statsObj["parallel_workers"] = m_stats.parallelWorkers;
    
    QJsonObject byLang;
    for (auto it = m_stats.stubsByLanguage.begin(); it != m_stats.stubsByLanguage.end(); ++it) {
        byLang[it.key()] = it.value();
    }
    statsObj["stubs_by_language"] = byLang;
    
    report["statistics"] = statsObj;
    report["files"] = m_results;
    
    QJsonObject summary;
    summary["files_with_stubs"] = static_cast<int>(m_results.size());
    summary["average_stubs_per_file"] = m_stats.scannedFiles.loadAcquire() > 0 ? 
        static_cast<double>(m_stats.stubsFound.loadAcquire()) / m_stats.scannedFiles.loadAcquire() : 0;
    report["summary"] = summary;
    
    m_lastReport = report;
    
    QFile out("digestion_report.json");
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    }
    
    QFile csvOut("digestion_report.csv");
    if (csvOut.open(QIODevice::WriteOnly)) {
        QTextStream stream(&csvOut);
        stream << "File,Language,Line,Severity,Type,Applied\n";
        for (const auto &val : m_results) {
            QJsonObject file = val.toObject();
            QString filePath = file["file"].toString();
            QString lang = file["language"].toString();
            QJsonArray tasks = file["tasks"].toArray();
            for (const auto &t : tasks) {
                QJsonObject task = t.toObject();
                stream << filePath << "," << lang << "," 
                       << task["line"].toInt() << "," 
                       << task["severity"].toString() << ","
                       << "\"" << task["type"].toString() << "\","
                       << (task["applied"].toBool() ? "Yes" : "No") << "\n";
            }
        }
    }
}

void DigestionReverseEngineeringSystem::stop() {
    m_stopRequested.storeRelease(1);
    m_paused.storeRelease(0);
    m_pauseSemaphore.release(100);
}

void DigestionReverseEngineeringSystem::pause() {
    m_paused.storeRelease(1);
}

void DigestionReverseEngineeringSystem::resume() {
    m_paused.storeRelease(0);
    m_pauseSemaphore.release(100);
}

bool DigestionReverseEngineeringSystem::isRunning() const {
    return m_running.loadAcquire();
}

bool DigestionReverseEngineeringSystem::isPaused() const {
    return m_paused.loadAcquire();
}

DigestionStats DigestionReverseEngineeringSystem::stats() const {
    QMutexLocker lock(&m_mutex);
    return m_stats;
}

QJsonObject DigestionReverseEngineeringSystem::lastReport() const {
    QMutexLocker lock(&m_mutex);
    return m_lastReport;
}

QVector<AgenticTask> DigestionReverseEngineeringSystem::pendingTasks() const {
    QMutexLocker lock(&m_mutex);
    return m_pendingTasks;
}

void DigestionReverseEngineeringSystem::saveCheckpoint(const QString &path) {
    QMutexLocker lock(&m_mutex);
    m_checkpoint.rootDir = m_rootDir;
    m_checkpoint.lastProcessedIndex = m_currentFileIndex.loadAcquire();
    m_checkpoint.timestamp = QDateTime::currentDateTime();
    m_checkpoint.pendingResults = m_results;
    m_checkpoint.stats = m_stats;
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QDataStream stream(&file);
        stream << m_checkpoint.rootDir;
        stream << m_checkpoint.lastProcessedIndex;
        stream << m_checkpoint.timestamp;
    }
    
    emit checkpointSaved(path);
}

bool DigestionReverseEngineeringSystem::loadCheckpoint(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    
    QDataStream stream(&file);
    stream >> m_checkpoint.rootDir;
    stream >> m_checkpoint.lastProcessedIndex;
    stream >> m_checkpoint.timestamp;
    
    m_hasCheckpoint = true;
    return true;
}

void DigestionReverseEngineeringSystem::resumeFromCheckpoint(const QString &checkpointPath, bool applyExtensions) {
    if (!loadCheckpoint(checkpointPath)) return;
    runFullDigestionPipeline(m_checkpoint.rootDir, 0, 50, 0, applyExtensions);
}

void DigestionReverseEngineeringSystem::runIncrementalDigestion(const QString &rootDir, 
                                                               const QStringList &modifiedFiles,
                                                               bool applyExtensions) {
    if (modifiedFiles.isEmpty()) return;
    
    m_running.storeRelease(1);
    m_stopRequested.storeRelease(0);
    m_paused.storeRelease(0);
    m_rootDir = rootDir;
    
    m_timer.start();
    emit runningChanged(true);
    
    for (const QString &file : modifiedFiles) {
        if (m_stopRequested.loadAcquire()) break;
        scanFile(file, 0, applyExtensions);
    }
    
    m_running.storeRelease(0);
    generateReport();
    emit runningChanged(false);
    emit pipelineFinished(m_lastReport, m_timer.elapsed());
}

void DigestionReverseEngineeringSystem::bindToProgressBar(QProgressBar *bar) {
    m_boundProgress = bar;
    if (bar) {
        connect(this, &DigestionReverseEngineeringSystem::progressUpdate, 
                [bar](int done, int total, int stubs, int percent) {
            bar->setMaximum(total);
            bar->setValue(done);
            bar->setFormat(QString("Files: %1/%2 | Stubs: %3 (%4%)").arg(done).arg(total).arg(stubs).arg(percent));
        });
    }
}

void DigestionReverseEngineeringSystem::bindToLogView(QTextEdit *log) {
    m_boundLog = log;
}

void DigestionReverseEngineeringSystem::bindToTreeView(QTreeWidget *tree) {
    m_boundTree = tree;
    if (tree) {
        tree->setColumnCount(4);
        tree->setHeaderLabels(QStringList{"File", "Line", "Severity", "Type"});
    }
}

void DigestionReverseEngineeringSystem::emitProgress() {
    int current = m_stats.scannedFiles.loadAcquire();
    int total = m_stats.totalFiles.loadAcquire();
    int stubs = m_stats.stubsFound.loadAcquire();
    int percent = total > 0 ? (current * 100) / total : 0;
    emit progressUpdate(current, total, stubs, percent);
}

QHash<QString, CodeMetrics> DigestionReverseEngineeringSystem::analyzeCodeMetrics(const QStringList &files) {
    QHash<QString, CodeMetrics> results;
    for (const QString &file : files) {
        QString lang = detectLanguage(file);
        if (lang.isEmpty()) continue;
        
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly)) continue;
        
        QString content = QString::fromUtf8(f.readAll());
        results[file] = calculateMetrics(content, m_profileMap[lang]);
    }
    return results;
}

QVector<QPair<QString, QString>> DigestionReverseEngineeringSystem::findDuplicateCode(int minLines) {
    QVector<QPair<QString, QString>> duplicates;
    // Simplified implementation - real version would use hashing
    return duplicates;
}

QHash<QString, QVector<QString>> DigestionReverseEngineeringSystem::buildDependencyGraph() {
    QHash<QString, QVector<QString>> graph;
    // Simplified implementation
    return graph;
}

QString DigestionReverseEngineeringSystem::generatePatchFile(const QString &originalDir, const QString &modifiedDir) {
    QString patch;
    QDirIterator it(modifiedDir, QDir::Files, QDir::Subdirectories);
    
    while (it.hasNext()) {
        QString modifiedFile = it.next();
        QString relativePath = modifiedFile.mid(modifiedDir.length() + 1);
        QString originalFile = originalDir + "/" + relativePath;
        
        QFile orig(originalFile);
        QFile mod(modifiedFile);
        
        if (!orig.open(QIODevice::ReadOnly) || !mod.open(QIODevice::ReadOnly)) continue;
        
        QString origContent = QString::fromUtf8(orig.readAll());
        QString modContent = QString::fromUtf8(mod.readAll());
        
        if (origContent != modContent) {
            patch += generateUnifiedDiff(origContent, modContent, relativePath) + "\n";
        }
    }
    
    QFile out("changes.patch");
    if (out.open(QIODevice::WriteOnly)) {
        out.write(patch.toUtf8());
    }
    
    return patch;
}

QString DigestionReverseEngineeringSystem::generateUnifiedDiff(const QString &original, const QString &modified, 
                                                               const QString &filename) {
    QString patch;
    patch += "--- " + filename + "\n";
    patch += "+++ " + filename + "\n";
    
    QStringList origLines = original.split('\n');
    QStringList modLines = modified.split('\n');
    
    int origLine = 1;
    int modLine = 1;
    
    for (int i = 0; i < qMax(origLines.size(), modLines.size()); ++i) {
        QString orig = i < origLines.size() ? origLines[i] : QString();
        QString mod = i < modLines.size() ? modLines[i] : QString();
        
        if (orig != mod) {
            patch += QString("@@ -%1,%2 +%3,%4 @@\n").arg(origLine).arg(1).arg(modLine).arg(1);
            if (!orig.isEmpty()) patch += "-" + orig + "\n";
            if (!mod.isEmpty()) patch += "+" + mod + "\n";
        }
        
        if (!orig.isEmpty()) origLine++;
        if (!mod.isEmpty()) modLine++;
    }
    
    return patch;
}

void DigestionReverseEngineeringSystem::onFileProcessed(const QString &path, const QJsonObject &result) {
    // Slot implementation
}

void DigestionReverseEngineeringSystem::onTaskApplied(const QString &file, int line, bool success) {
    // Slot implementation
}

void DigestionReverseEngineeringSystem::updateStats() {
    // Implementation for stats update
}
