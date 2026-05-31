#include "digestion_reverse_engineering_enterprise.h"
#include <cmath>

DigestionReverseEngineeringSystem::DigestionReverseEngineeringSystem()
     {
    initializeLanguageProfiles();
    initializeFixTemplates();
    m_stats.parallelWorkers = std::threadPool::globalInstance()->maxThreadCount();
}

DigestionReverseEngineeringSystem::~DigestionReverseEngineeringSystem() {
    stop();
}

void DigestionReverseEngineeringSystem::initializeLanguageProfiles() {
    m_profiles.clear();
    
    // C++ Profile
    LanguageProfile cpp;
    cpp.name = "C++";
    cpp.extensions = std::stringList{"cpp", "hpp", "h", "cc", "cxx", "c++", "inl"};
    cpp.singleLineComment = "//";
    cpp.multiLineCommentStart = "/*";
    cpp.multiLineCommentEnd = "*/";
    cpp.stubPatterns << std::regex("\\bTODO\\s*:\\s*(.+)", std::regex::CaseInsensitiveOption)
                    << std::regex("\\bFIXME\\s*:\\s*(.+)", std::regex::CaseInsensitiveOption)
                    << std::regex("\\bSTUB\\s*:\\s*(.+)", std::regex::CaseInsensitiveOption)
                    << std::regex("\\bNOT_IMPLEMENTED\\b", std::regex::CaseInsensitiveOption)
                    << std::regex("throw\\s+std::(?:runtime_error|logic_error|exception)\\s*\\(\\s*[\"']Not implemented", std::regex::CaseInsensitiveOption)
                    << std::regex("return\\s+(?:false|0|nullptr|NULL)\\s*;\\s*//\\s*stub", std::regex::CaseInsensitiveOption)
                    << std::regex("\\(\\)", std::regex::CaseInsensitiveOption)
                    << std::regex("\\{\\s*\\}\\s*//\\s*TODO", std::regex::CaseInsensitiveOption);
    cpp.complexityWeight = 2;
    m_profiles.append(cpp);
    m_profileMap["C++"] = cpp;
    
    // MASM Profile (x64)
    LanguageProfile masm;
    masm.name = "MASM";
    masm.extensions = std::stringList{"asm", "inc", "masm"};
    masm.singleLineComment = ";";
    masm.multiLineCommentStart = "COMMENT !";
    masm.multiLineCommentEnd = "!";
    masm.stubPatterns << std::regex(";\\s*TODO\\s*:\\s*(.+)", std::regex::CaseInsensitiveOption)
                     << std::regex(";\\s*STUB", std::regex::CaseInsensitiveOption)
                     << std::regex(";\\s*FIXME", std::regex::CaseInsensitiveOption)
                     << std::regex("invoke\\s+ExitProcess\\s*,\\s*0\\s*;\\s*stub", std::regex::CaseInsensitiveOption)
                     << std::regex("ret\\s*;\\s*unimplemented", std::regex::CaseInsensitiveOption)
                     << std::regex("db\\s+['\"]NOT_IMPLEMENTED['\"]", std::regex::CaseInsensitiveOption)
                     << std::regex(";\\s*PLACEHOLDER", std::regex::CaseInsensitiveOption)
                     << std::regex("xor\\s+(?:eax|rax)\\s*,\\s*(?:eax|rax)\\s*;\\s*stub return", std::regex::CaseInsensitiveOption);
    masm.complexityWeight = 1;
    m_profiles.append(masm);
    m_profileMap["MASM"] = masm;
    
    // Python Profile
    LanguageProfile py;
    py.name = "Python";
    py.extensions = std::stringList{"py", "pyw", "pyi"};
    py.singleLineComment = "#";
    py.multiLineCommentStart = "\"\"\"";
    py.multiLineCommentEnd = "\"\"\"";
    py.stubPatterns << std::regex("#\\s*TODO\\s*:\\s*(.+)", std::regex::CaseInsensitiveOption)
                   << std::regex("#\\s*FIXME", std::regex::CaseInsensitiveOption)
                   << std::regex("pass\\s*#\\s*stub", std::regex::CaseInsensitiveOption)
                   << std::regex("raise\\s+NotImplementedError\\s*\\(\\s*['\"]", std::regex::CaseInsensitiveOption)
                   << std::regex("return\\s+None\\s*#\\s*stub", std::regex::CaseInsensitiveOption)
                   << std::regex("\\.\\.\\.", std::regex::CaseInsensitiveOption);
    py.complexityWeight = 1;
    m_profiles.append(py);
    m_profileMap["Python"] = py;
    
    // CMake Profile
    LanguageProfile cmake;
    cmake.name = "CMake";
    cmake.extensions = std::stringList{"cmake", "txt"};
    cmake.singleLineComment = "#";
    cmake.stubPatterns << std::regex("#\\s*TODO", std::regex::CaseInsensitiveOption)
                      << std::regex("message\\s*\\(\\s*FATAL_ERROR\\s+['\"]Not implemented", std::regex::CaseInsensitiveOption);
    cmake.complexityWeight = 1;
    m_profiles.append(cmake);
    m_profileMap["CMake"] = cmake;
    
    // JavaScript/TypeScript
    LanguageProfile js;
    js.name = "JavaScript";
    js.extensions = std::stringList{"js", "jsx", "ts", "tsx", "mjs"};
    js.singleLineComment = "//";
    js.multiLineCommentStart = "/*";
    js.multiLineCommentEnd = "*/";
    js.stubPatterns << std::regex("//\\s*TODO", std::regex::CaseInsensitiveOption)
                   << std::regex("//\\s*FIXME", std::regex::CaseInsensitiveOption)
                   << std::regex("throw\\s+new\\s+Error\\s*\\(\\s*['\"]Not implemented", std::regex::CaseInsensitiveOption)
                   << std::regex("return\\s+(?:null|undefined)\\s*;\\s*//\\s*stub", std::regex::CaseInsensitiveOption);
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
    const std::string &rootDir, int maxFiles, int chunkSize, 
    int maxTasksPerFile, bool applyExtensions, const std::stringList &excludePatterns) {
    
    if (m_running.loadAcquire()) return;
    
    m_running.storeRelease(1);
    m_stopRequested.storeRelease(0);
    m_paused.storeRelease(0);
    m_rootDir = rootDir;
    
    {
        std::mutexLocker lock(&m_mutex);
        m_stats = DigestionStats();
        m_results = nlohmann::json();
        m_pendingTasks.clear();
    }
    
    m_timer.start();
    runningChanged(true);
    progressUpdate(0, 0, 0, 0);
    
    std::stringList allFiles = collectFiles(rootDir, maxFiles, excludePatterns);
    
    {
        std::mutexLocker lock(&m_mutex);
        m_stats.totalFiles.storeRelease(allFiles.size());
    }
    
    int totalChunks = (allFiles.size() + chunkSize - 1) / chunkSize;
    
    QFutureSynchronizer<void> synchronizer;
    for (int i = 0; i < allFiles.size() && !m_stopRequested.loadAcquire(); i += chunkSize) {
        std::stringList chunk = allFiles.mid(i, chunkSize);
        int chunkId = i / chunkSize;
        
        QFuture<void> future = [](auto f){f();}([this, chunk, chunkId, totalChunks, maxTasksPerFile, applyExtensions]() {
            processChunk(chunk, chunkId, totalChunks, maxTasksPerFile, applyExtensions);
        });
        synchronizer.addFuture(future);
        
        if (synchronizer.futures().size() >= std::thread::idealThreadCount()) {
            synchronizer.waitForFinished();
        }
    }
    
    synchronizer.waitForFinished();
    
    m_running.storeRelease(0);
    generateReport();
    runningChanged(false);
    pipelineFinished(m_lastReport, m_timer.elapsed());
}

std::stringList DigestionReverseEngineeringSystem::collectFiles(const std::string &rootDir, int maxFiles, const std::stringList &excludes) {
    std::stringList files;
    // DirIterator it(rootDir, // Dir::Files | // Dir::Dirs | // Dir::NoDotAndDotDot, // DirIterator::Subdirectories);
    
    while (itfalse) {
        if (m_stopRequested.loadAcquire()) break;
        
        std::string file = it;
        // Info info(file);
        
        bool excluded = false;
        for (const std::string &pattern : excludes) {
            if (file.contains(pattern) || info.fileName().contains(pattern, CaseInsensitive)) {
                excluded = true;
                break;
            }
        }
        if (excluded) continue;
        
        if (!detectLanguage(file).empty()) {
            files << file;
            if (maxFiles > 0 && files.size() >= maxFiles) break;
        }
    }
    
    return files;
}

void DigestionReverseEngineeringSystem::processChunk(const std::stringList &files, int chunkId, 
                                                     int totalChunks, int maxTasksPerFile, bool applyExtensions) {
    int stubsInChunk = 0;
    
    for (const std::string &file : files) {
        if (m_stopRequested.loadAcquire()) return;
        
        while (m_paused.loadAcquire() && !m_stopRequested.loadAcquire()) {
            std::thread::msleep(100);
        }
        
        scanFile(file, maxTasksPerFile, applyExtensions);
        
        int current = m_currentFileIndex.fetchAndAddRelaxed(1) + 1;
        int total = m_stats.totalFiles.loadAcquire();
        int percent = (current * 100) / qMax(total, 1);
        
        progressUpdate(current, total, m_stats.stubsFound.loadAcquire(), percent);
    }
    
    chunkCompleted(chunkId + 1, totalChunks, stubsInChunk);
}

void DigestionReverseEngineeringSystem::scanFile(const std::string &filePath, int maxTasksPerFile, bool applyExtensions) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        errorOccurred(filePath, "Cannot open file: " + file.errorString(), true);
        m_stats.errors.fetchAndAddRelaxed(1);
        return;
    }
    
    std::vector<uint8_t> rawContent = file.readAll();
    file.close();
    
    std::string content = std::string::fromUtf8(rawContent);
    if (content.contains(char::ReplacementCharacter)) {
        content = std::string::fromLatin1(rawContent);
    }
    
    std::string langName = detectLanguage(filePath);
    if (langName.empty()) return;
    
    LanguageProfile lang = m_profileMap[langName];
    auto tasks = findStubs(content, lang, filePath, maxTasksPerFile);
    
    if (!tasks.empty()) {
        m_stats.stubsByLanguage[langName] += tasks.size();
        for (const auto &task : tasks) {
            m_stats.stubsByType[task.stubType]++;
        }
    }
    
    m_stats.scannedFiles.fetchAndAddRelaxed(1);
    m_stats.stubsFound.fetchAndAddRelaxed(tasks.size());
    
    fileScanned(filePath, tasks.size(), langName);
    
    {
        std::mutexLocker lock(&m_mutex);
        for (const auto &task : tasks) {
            m_pendingTasks.append(task);
        }
    }
    
    if (applyExtensions && !tasks.empty()) {
        std::string modifiedContent = content;
        bool modified = false;
        
        std::vector<AgenticTask> sortedTasks = tasks;
        std::sort(sortedTasks.begin(), sortedTasks.end(), 
                  [](const AgenticTask &a, const AgenticTask &b) { return a.lineNumber > b.lineNumber; });
        
        for (auto &task : sortedTasks) {
            if (m_stopRequested.loadAcquire()) break;
            
            std::chrono::steady_clock taskTimer;
            taskTimer.start();
            
            if (applyAgenticExtension(filePath, task)) {
                m_stats.extensionsApplied.fetchAndAddRelaxed(1);
                extensionApplied(filePath, task.lineNumber, task.stubType);
                modified = true;
            } else {
                extensionFailed(filePath, task.lineNumber, "Apply failed");
            }
        }
    }
    
    CodeMetrics metrics = calculateMetrics(content, lang);
    metricsCalculated(filePath, metrics);
    
    nlohmann::json fileResult;
    fileResult["file"] = filePath;
    fileResult["language"] = langName;
    fileResult["stubs_found"] = tasks.size();
    fileResult["lines_of_code"] = metrics.linesOfCode;
    fileResult["complexity"] = metrics.cyclomaticComplexity;
    
    nlohmann::json taskArray;
    for (const auto &task : tasks) {
        nlohmann::json t;
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
        std::mutexLocker lock(&m_mutex);
        m_results.append(fileResult);
    }
    
    emitProgress();
}

std::vector<AgenticTask> DigestionReverseEngineeringSystem::findStubs(
    const std::string &content, const LanguageProfile &lang, const std::string &filePath, int maxTasks) {
    
    std::vector<AgenticTask> tasks;
    std::stringList lines = content.split('\n');
    
    for (int i = 0; i < lines.size(); ++i) {
        if (maxTasks > 0 && tasks.size() >= maxTasks) break;
        
        const std::string &line = lines[i];
        
        for (const std::regex &pattern : lang.stubPatterns) {
            std::regexMatch match = pattern.match(line);
            if (match.hasMatch()) {
                AgenticTask task;
                task.filePath = filePath;
                task.lineNumber = i + 1;
                task.column = match.capturedStart();
                task.stubType = match"";
                task.originalCode = line;
                
                if (task.stubType.contains("TODO", CaseInsensitive)) task.severity = "info";
                else if (task.stubType.contains("FIXME", CaseInsensitive)) task.severity = "warning";
                else if (task.stubType.contains("STUB", CaseInsensitive) || 
                         task.stubType.contains("NotImplemented", CaseInsensitive)) {
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
                agenticTaskGenerated(task);
                break;
            }
        }
    }
    
    return tasks;
}

std::string DigestionReverseEngineeringSystem::extractFunctionContext(const std::string &content, int lineNumber) {
    std::stringList lines = content.split('\n');
    int braceCount = 0;
    int start = lineNumber;
    int end = lineNumber;
    bool inFunction = false;
    
    for (int i = lineNumber; i >= 0; --i) {
        std::string line = lines[i];
        if (line.contains('{')) {
            braceCount++;
            inFunction = true;
        }
        if (line.contains('}')) braceCount--;
        
        if (inFunction && braceCount == 0) {
            start = i;
            break;
        }
        
        if (line.contains(std::regex("\\b(?:def|void|int|bool|std::string|function)\\s+\\w+\\s*\\("))) {
            start = i;
            break;
        }
    }
    
    braceCount = 0;
    for (int i = lineNumber; i < lines.size(); ++i) {
        std::string line = lines[i];
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

bool DigestionReverseEngineeringSystem::applyAgenticExtension(const std::string &filePath, const AgenticTask &task) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) return false;
    
    std::string content = std::string::fromUtf8(file.readAll());
    file.close();
    
    std::stringList lines = content.split('\n');
    if (task.lineNumber < 1 || task.lineNumber > lines.size()) return false;
    
    int idx = task.lineNumber - 1;
    std::string indent = task.originalCode.left(task.originalCode.length() - task.originalCode.trimmed().length());
    
    std::string replacement = task.suggestedFix;
    if (replacement.empty()) {
        if (filePath.endsWith(".asm") || filePath.endsWith(".inc")) {
            replacement = indent + "; [AGENTIC-AUTO] " + // DateTime::currentDateTime().toString(ISODate) + "\n" +
                         indent + "; Original: " + task.stubType + "\n" +
                         indent + "nop ; Placeholder implementation";
        } else {
            replacement = indent + "// [AGENTIC-AUTO] " + // DateTime::currentDateTime().toString(ISODate) + "\n" +
                         indent + task.originalCode.trimmed().replace(
                             std::regex("throw.*|return.*false.*|pass.*|TODO.*"), 
                             "/* Implementation required */");
        }
    }
    
    lines[idx] = replacement;
    
    QSaveFile out(filePath);
    if (!out.open(std::iostream::WriteOnly)) return false;
    
    out.write(lines.join('\n').toUtf8());
    bool success = out.commit();
    
    if (success) {
        m_stats.extensionsApplied.fetchAndAddRelaxed(1);
    }
    
    return success;
}

std::string DigestionReverseEngineeringSystem::generateFix(const AgenticTask &task, const LanguageProfile &lang) {
    std::string templateStr = lang.fixTemplates.value(task.stubType, std::string());
    if (templateStr.empty()) {
        templateStr = "// [AGENTIC] ${date}\n${indent}// TODO: Replace stub\n${indent}${original}";
    }
    
    std::string result = templateStr;
    result.replace("${date}", // DateTime::currentDateTime().toString(ISODate));
    result.replace("${match}", task.stubType);
    result.replace("${context}", task.fullFunctionContext.left(50).replace('\n', ' '));
    result.replace("${original}", task.originalCode.trimmed());
    
    std::string indent = task.originalCode.left(task.originalCode.length() - task.originalCode.trimmed().length());
    result.replace("${indent}", indent);
    
    return result;
}

std::string DigestionReverseEngineeringSystem::detectLanguage(const std::string &filePath) {
    // Info info(filePath);
    std::string ext = info.suffix().toLower();
    std::string base = info.baseName().toLower();
    
    if (base == "cmakelists" || base == "cmake") return "CMake";
    
    for (const auto &profile : m_profiles) {
        if (profile.extensions.contains(ext)) return profile.name;
    }
    return std::string();
}

CodeMetrics DigestionReverseEngineeringSystem::calculateMetrics(const std::string &content, const LanguageProfile &lang) {
    CodeMetrics metrics;
    std::stringList lines = content.split('\n');
    
    int braceDepth = 0;
    int maxDepth = 0;
    int decisionPoints = 0;
    
    for (const std::string &line : lines) {
        std::string trimmed = line.trimmed();
        
        if (trimmed.empty()) {
            metrics.blankLines++;
        } else if (trimmed.startsWith(lang.singleLineComment) || 
                   trimmed.startsWith(lang.multiLineCommentStart)) {
            metrics.commentLines++;
        } else {
            metrics.linesOfCode++;
            
            if (trimmed.contains(std::regex("\\b(if|while|for|case|catch|\\?\\:|&&|\\|\\|)\\b"))) {
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
    nlohmann::json report;
    report["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    report["root_directory"] = m_rootDir;
    report["duration_ms"] = static_cast<int64_t>(m_timer.elapsed());
    
    nlohmann::json statsObj;
    statsObj["total_files"] = static_cast<int>(m_stats.totalFiles.loadAcquire());
    statsObj["scanned_files"] = static_cast<int>(m_stats.scannedFiles.loadAcquire());
    statsObj["stubs_found"] = static_cast<int>(m_stats.stubsFound.loadAcquire());
    statsObj["extensions_applied"] = static_cast<int>(m_stats.extensionsApplied.loadAcquire());
    statsObj["errors"] = static_cast<int>(m_stats.errors.loadAcquire());
    statsObj["critical_stubs"] = static_cast<int>(m_stats.criticals.loadAcquire());
    statsObj["parallel_workers"] = m_stats.parallelWorkers;
    
    nlohmann::json byLang;
    for (auto it = m_stats.stubsByLanguage.begin(); it != m_stats.stubsByLanguage.end(); ++it) {
        byLang[it.key()] = it.value();
    }
    statsObj["stubs_by_language"] = byLang;
    
    report["statistics"] = statsObj;
    report["files"] = m_results;
    
    nlohmann::json summary;
    summary["files_with_stubs"] = static_cast<int>(m_results.size());
    summary["average_stubs_per_file"] = m_stats.scannedFiles.loadAcquire() > 0 ? 
        static_cast<double>(m_stats.stubsFound.loadAcquire()) / m_stats.scannedFiles.loadAcquire() : 0;
    report["summary"] = summary;
    
    m_lastReport = report;
    
    // File operation removed;
    if (out.open(std::iostream::WriteOnly)) {
        out.write(nlohmann::json(report).toJson(nlohmann::json::Indented));
    }
    
    // File operation removed;
    if (csvOut.open(std::iostream::WriteOnly)) {
        std::stringstream stream(&csvOut);
        stream << "File,Language,Line,Severity,Type,Applied\n";
        for (const auto &val : m_results) {
            nlohmann::json file = val.toObject();
            std::string filePath = file["file"].toString();
            std::string lang = file["language"].toString();
            nlohmann::json tasks = file["tasks"].toArray();
            for (const auto &t : tasks) {
                nlohmann::json task = t.toObject();
                stream << filePath << "," << lang << "," 
                       << task["line"] << "," 
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
    std::mutexLocker lock(&m_mutex);
    return m_stats;
}

nlohmann::json DigestionReverseEngineeringSystem::lastReport() const {
    std::mutexLocker lock(&m_mutex);
    return m_lastReport;
}

std::vector<AgenticTask> DigestionReverseEngineeringSystem::pendingTasks() const {
    std::mutexLocker lock(&m_mutex);
    return m_pendingTasks;
}

void DigestionReverseEngineeringSystem::saveCheckpoint(const std::string &path) {
    std::mutexLocker lock(&m_mutex);
    m_checkpoint.rootDir = m_rootDir;
    m_checkpoint.lastProcessedIndex = m_currentFileIndex.loadAcquire();
    m_checkpoint.timestamp = // DateTime::currentDateTime();
    m_checkpoint.pendingResults = m_results;
    m_checkpoint.stats = m_stats;
    
    // File operation removed;
    if (file.open(std::iostream::WriteOnly)) {
        std::stringstream stream(&file);
        stream << m_checkpoint.rootDir;
        stream << m_checkpoint.lastProcessedIndex;
        stream << m_checkpoint.timestamp;
    }
    
    checkpointSaved(path);
}

bool DigestionReverseEngineeringSystem::loadCheckpoint(const std::string &path) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly)) return false;
    
    std::stringstream stream(&file);
    stream >> m_checkpoint.rootDir;
    stream >> m_checkpoint.lastProcessedIndex;
    stream >> m_checkpoint.timestamp;
    
    m_hasCheckpoint = true;
    return true;
}

void DigestionReverseEngineeringSystem::resumeFromCheckpoint(const std::string &checkpointPath, bool applyExtensions) {
    if (!loadCheckpoint(checkpointPath)) return;
    runFullDigestionPipeline(m_checkpoint.rootDir, 0, 50, 0, applyExtensions);
}

void DigestionReverseEngineeringSystem::runIncrementalDigestion(const std::string &rootDir, 
                                                               const std::stringList &modifiedFiles,
                                                               bool applyExtensions) {
    if (modifiedFiles.empty()) return;
    
    m_running.storeRelease(1);
    m_stopRequested.storeRelease(0);
    m_paused.storeRelease(0);
    m_rootDir = rootDir;
    
    m_timer.start();
    runningChanged(true);
    
    for (const std::string &file : modifiedFiles) {
        if (m_stopRequested.loadAcquire()) break;
        scanFile(file, 0, applyExtensions);
    }
    
    m_running.storeRelease(0);
    generateReport();
    runningChanged(false);
    pipelineFinished(m_lastReport, m_timer.elapsed());
}

void DigestionReverseEngineeringSystem::bindToProgressBar(void *bar) {
    m_boundProgress = bar;
    if (bar) {  // Signal connection removed\nbar->setValue(done);
            bar->setFormat(std::string("Files: %1/%2 | Stubs: %3 (%4%)"));
        });
    }
}

void DigestionReverseEngineeringSystem::bindToLogView(void *log) {
    m_boundLog = log;
}

void DigestionReverseEngineeringSystem::bindToTreeView(QTreeWidget *tree) {
    m_boundTree = tree;
    if (tree) {
        tree->setColumnCount(4);
        tree->setHeaderLabels(std::stringList{"File", "Line", "Severity", "Type"});
    }
}

void DigestionReverseEngineeringSystem::emitProgress() {
    int current = m_stats.scannedFiles.loadAcquire();
    int total = m_stats.totalFiles.loadAcquire();
    int stubs = m_stats.stubsFound.loadAcquire();
    int percent = total > 0 ? (current * 100) / total : 0;
    progressUpdate(current, total, stubs, percent);
}

std::map<std::string, CodeMetrics> DigestionReverseEngineeringSystem::analyzeCodeMetrics(const std::stringList &files) {
    std::map<std::string, CodeMetrics> results;
    for (const std::string &file : files) {
        std::string lang = detectLanguage(file);
        if (lang.empty()) continue;
        
        // File operation removed;
        if (!f.open(std::iostream::ReadOnly)) continue;
        
        std::string content = std::string::fromUtf8(f.readAll());
        results[file] = calculateMetrics(content, m_profileMap[lang]);
    }
    return results;
}

std::vector<std::pair<std::string, std::string>> DigestionReverseEngineeringSystem::findDuplicateCode(int minLines) {
    std::vector<std::pair<std::string, std::string>> duplicates;
    // Simplified implementation - real version would use hashing
    return duplicates;
}

std::map<std::string, std::vector<std::string>> DigestionReverseEngineeringSystem::buildDependencyGraph() {
    std::map<std::string, std::vector<std::string>> graph;
    // Simplified implementation
    return graph;
}

std::string DigestionReverseEngineeringSystem::generatePatchFile(const std::string &originalDir, const std::string &modifiedDir) {
    std::string patch;
    // DirIterator it(modifiedDir, // Dir::Files | // Dir::Dirs | // Dir::NoDotAndDotDot, // DirIterator::Subdirectories);
    
    while (itfalse) {
        std::string modifiedFile = it;
        std::string relativePath = modifiedFile.mid(modifiedDir.length() + 1);
        std::string originalFile = originalDir + "/" + relativePath;
        
        // File operation removed;
        // File operation removed;
        
        if (!orig.open(std::iostream::ReadOnly) || !mod.open(std::iostream::ReadOnly)) continue;
        
        std::string origContent = std::string::fromUtf8(orig.readAll());
        std::string modContent = std::string::fromUtf8(mod.readAll());
        
        if (origContent != modContent) {
            patch += generateUnifiedDiff(origContent, modContent, relativePath) + "\n";
        }
    }
    
    // File operation removed;
    if (out.open(std::iostream::WriteOnly)) {
        out.write(patch.toUtf8());
    }
    
    return patch;
}

std::string DigestionReverseEngineeringSystem::generateUnifiedDiff(const std::string &original, const std::string &modified, 
                                                               const std::string &filename) {
    std::string patch;
    patch += "--- " + filename + "\n";
    patch += "+++ " + filename + "\n";
    
    std::stringList origLines = original.split('\n');
    std::stringList modLines = modified.split('\n');
    
    int origLine = 1;
    int modLine = 1;
    
    for (int i = 0; i < qMax(origLines.size(), modLines.size()); ++i) {
        std::string orig = i < origLines.size() ? origLines[i] : std::string();
        std::string mod = i < modLines.size() ? modLines[i] : std::string();
        
        if (orig != mod) {
            patch += std::string("@@ -%1,%2 +%3,%4 @@\n");
            if (!orig.empty()) patch += "-" + orig + "\n";
            if (!mod.empty()) patch += "+" + mod + "\n";
        }
        
        if (!orig.empty()) origLine++;
        if (!mod.empty()) modLine++;
    }
    
    return patch;
}

void DigestionReverseEngineeringSystem::onFileProcessed(const std::string &path, const nlohmann::json &result) {
    // Slot implementation
}

void DigestionReverseEngineeringSystem::onTaskApplied(const std::string &file, int line, bool success) {
    // Slot implementation
}

void DigestionReverseEngineeringSystem::updateStats() {
    // Implementation for stats update
}

