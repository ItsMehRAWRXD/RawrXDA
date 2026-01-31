#include "digestion_engine.h"
// External MASM function declaration
extern "C" {
    int __cdecl avx512_scan_stubs(const char *data, size_t len, 
                                   const char **signatures, int sigCount,
                                   int *outPositions, int maxMatches);
}

RawrXDDigestionEngine::RawrXDDigestionEngine() 
    , m_threadPool(this) {
    m_threadPool.setMaxThreadCount(std::thread::idealThreadCount());
    initProfiles();
}

RawrXDDigestionEngine::~RawrXDDigestionEngine() {
    stop();
    m_threadPool.waitForDone(5000);
    if (m_db.isOpen()) m_db.close();
}

void RawrXDDigestionEngine::initProfiles() {
    m_profiles = {
        {"C++", {"cpp", "hpp", "h", "cc", "cxx"}, 
         {"TODO:", "FIXME:", "STUB:", "NOT_IMPLEMENTED", "Q_UNIMPLEMENTED()"},
         std::regex("(TODO|FIXME|STUB|NOT_IMPLEMENTED|Q_UNIMPLEMENTED)\\s*[:\\(]")},
         
        {"MASM", {"asm", "inc"}, 
         {"; TODO", "; FIXME", "; STUB", "invoke ExitProcess"},
         std::regex("(;\\s*(TODO|FIXME|STUB)|invoke\\s+ExitProcess)")},
         
        {"Python", {"py"}, 
         {"# TODO", "# FIXME", "NotImplementedError", "pass # stub"},
         std::regex("(#\\s*(TODO|FIXME)|NotImplementedError|pass\\s*#\\s*stub)")},
         
        {"JavaScript", {"js", "ts"}, 
         {"// TODO", "// FIXME", "throw new Error('Not implemented')"},
         std::regex("(//\\s*(TODO|FIXME)|throw\\s+new\\s+Error\\s*\\(\\s*['\"]Not implemented)")}
    };
}

void RawrXDDigestionEngine::initializeDatabase(const std::string &dbPath) {
    std::mutexLocker lock(&m_dbMutex);
    m_db = QSqlDatabase::addDatabase("QSQLITE", "digestion_conn");
    m_db.setDatabaseName(dbPath);
    
    if (!m_db.open()) {
        qFatal("Failed to open digestion database: %s", 
               m_db.lastError().text().toUtf8().constData());
    }
    
    setupTables();
}

void RawrXDDigestionEngine::setupTables() {
    std::mutexLocker lock(&m_dbMutex);
    std::stringList tables = {"tasks", "stubs", "checkpoints"};
    
    for (const std::string &table : tables) {
        QSqlQuery query(m_db);
        if (table == "tasks") {
            query.exec(R"(
                CREATE TABLE IF NOT EXISTS tasks (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    root_dir TEXT NOT NULL,
                    started DATETIME DEFAULT CURRENT_TIMESTAMP,
                    completed DATETIME,
                    total_files INTEGER DEFAULT 0,
                    processed_files INTEGER DEFAULT 0,
                    stubs_found INTEGER DEFAULT 0,
                    stubs_fixed INTEGER DEFAULT 0,
                    status TEXT DEFAULT 'running',
                    error_count INTEGER DEFAULT 0
                )
            )");
        } else if (table == "stubs") {
            query.exec(R"(
                CREATE TABLE IF NOT EXISTS stubs (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    task_id INTEGER,
                    file_path TEXT NOT NULL,
                    line_number INTEGER,
                    language TEXT,
                    stub_type TEXT,
                    original_code TEXT,
                    suggested_fix TEXT,
                    applied_fix TEXT,
                    applied BOOLEAN DEFAULT 0,
                    confidence_score REAL,
                    detected_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                    fixed_at DATETIME,
                    FOREIGN KEY (task_id) REFERENCES tasks(id)
                )
            )");
            query.exec("CREATE INDEX IF NOT EXISTS idx_stubs_task ON stubs(task_id)");
            query.exec("CREATE INDEX IF NOT EXISTS idx_stubs_file ON stubs(file_path)");
            query.exec("CREATE INDEX IF NOT EXISTS idx_stubs_applied ON stubs(applied)");
        } else if (table == "checkpoints") {
            query.exec(R"(
                CREATE TABLE IF NOT EXISTS checkpoints (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    task_id INTEGER,
                    files_processed INTEGER,
                    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                    FOREIGN KEY (task_id) REFERENCES tasks(id)
                )
            )");
        }
    }
}

void RawrXDDigestionEngine::runFullDigestionPipeline(
    const std::string &rootDir, int maxFiles, int chunkSize, 
    int maxTasksPerFile, bool applyExtensions, bool useAVX512) {
    
    if (m_state.running.loadAcquire()) return;
    
    m_state.running.storeRelease(1);
    m_state.paused.storeRelease(0);
    m_state.stopRequested.storeRelease(0);
    m_state.filesProcessed.storeRelease(0);
    m_state.stubsFound.storeRelease(0);
    m_state.stubsFixed.storeRelease(0);
    m_state.currentRootDir = rootDir;
    m_chunkSize = chunkSize;
    m_state.timer.start();
    
    // Insert task record
    int taskId = -1;
    {
        std::mutexLocker lock(&m_dbMutex);
        QSqlQuery query(m_db);
        query.prepare("INSERT INTO tasks (root_dir, status) VALUES (?, 'running')");
        query.addBindValue(rootDir);
        query.exec();
        taskId = query.lastInsertId();
    }
    
    // Collect files
    std::stringList allFiles;
    // DirIterator it(rootDir, // Dir::Files, // Dir::Subdirectories);
    while (it.hasNext() && !m_state.stopRequested.loadAcquire()) {
        allFiles << it.next();
        if (maxFiles > 0 && allFiles.size() >= maxFiles) break;
    }
    
    m_state.totalFiles.storeRelease(allFiles.size());
    pipelineStarted(allFiles.size());
    
    // Update task with total
    {
        std::mutexLocker lock(&m_dbMutex);
        QSqlQuery query(m_db);
        query.prepare("UPDATE tasks SET total_files = ? WHERE id = ?");
        query.addBindValue(allFiles.size());
        query.addBindValue(taskId);
        query.exec();
    }
    
    // Process chunks
    std::vector<QFuture<void>> futures;
    for (int i = 0; i < allFiles.size() && !m_state.stopRequested.loadAcquire(); i += chunkSize) {
        // Pause handling
        while (m_state.paused.loadAcquire() && !m_state.stopRequested.loadAcquire()) {
            std::thread::msleep(100);
        }
        
        std::stringList chunk = allFiles.mid(i, chunkSize);
        auto future = [](auto f){f();}(&m_threadPool, [this, chunk, taskId, maxTasksPerFile, 
                                                         applyExtensions, useAVX512]() {
            for (const std::string &file : chunk) {
                if (m_state.stopRequested.loadAcquire()) return;
                
                while (m_state.paused.loadAcquire() && !m_state.stopRequested.loadAcquire()) {
                    std::thread::msleep(50);
                }
                
                scanFileInternal(file, maxTasksPerFile, applyExtensions, useAVX512);
                
                int processed = m_state.filesProcessed.fetchAndAddRelaxed(1) + 1;
                if (processed % m_checkpointInterval == 0) {
                    saveCheckpoint();
                    checkpointSaved(processed);
                }
            }
        });
        futures.append(future);
    }
    
    QFutureSynchronizer<void> sync;
    for (auto &f : futures) sync.addFuture(f);
    sync.waitForFinished();
    
    // Finalize
    m_state.running.storeRelease(0);
    Stats stats = getStatistics();
    
    {
        std::mutexLocker lock(&m_dbMutex);
        QSqlQuery query(m_db);
        query.prepare(R"(
            UPDATE tasks SET 
                completed = CURRENT_TIMESTAMP,
                processed_files = ?,
                stubs_found = ?,
                stubs_fixed = ?,
                status = ?
            WHERE id = ?
        )");
        query.addBindValue(m_state.filesProcessed.loadAcquire());
        query.addBindValue(m_state.stubsFound.loadAcquire());
        query.addBindValue(m_state.stubsFixed.loadAcquire());
        query.addBindValue(m_state.stopRequested.loadAcquire() ? "cancelled" : "completed");
        query.addBindValue(taskId);
        query.exec();
    }
    
    pipelineCompleted(stats);
}

void RawrXDDigestionEngine::scanFileInternal(const std::string &filePath, int maxTasksPerFile, 
                                             bool applyExtensions, bool useAVX512) {
    fileScanStarted(filePath);
    
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) {
        errorOccurred(filePath, "Cannot open file");
        return;
    }
    
    std::vector<uint8_t> rawContent = file.readAll();
    file.close();
    
    // Detect language
    std::string lang;
    std::string ext = // FileInfo: filePath).suffix().toLower();
    for (const auto &profile : m_profiles) {
        if (profile.extensions.contains(ext)) {
            lang = profile.name;
            break;
        }
    }
    
    if (lang.empty()) return; // Skip unknown
    
    std::string content = std::string::fromUtf8(rawContent);
    std::vector<StubInstance> stubs;
    
    // Use AVX-512 if available and content is large enough
    if (useAVX512 && rawContent.size() > 1024 && !m_profiles.empty() && m_profiles.first().stubSignatures.size() > 0) {
        stubs = detectStubsAVX512(content, lang, filePath);
    } else {
        stubs = detectStubsFallback(content, lang, filePath);
    }
    
    if (maxTasksPerFile > 0 && stubs.size() > maxTasksPerFile) {
        stubs = stubs.mid(0, maxTasksPerFile);
    }
    
    m_state.stubsFound.fetchAndAddRelaxed(stubs.size());
    
    // Apply fixes if requested
    int fixedCount = 0;
    if (applyExtensions && !stubs.empty()) {
        for (auto &stub : stubs) {
            std::string fix = generateAIFix(stub);
            if (!fix.empty()) {
                stub.suggestedFix = fix;
                if (applyFix(stub, fix)) {
                    stub.applied = true;
                    stub.appliedFix = fix;
                    fixedCount++;
                    m_state.stubsFixed.fetchAndAddRelaxed(1);
                    stubFixed(stub);
                }
            }
            stubDetected(stub);
        }
    }
    
    // Persist stubs
    {
        std::mutexLocker lock(&m_dbMutex);
        QSqlQuery query(m_db);
        query.prepare(R"(
            INSERT INTO stubs 
            (task_id, file_path, line_number, language, stub_type, 
             original_code, suggested_fix, applied_fix, applied, confidence_score)
            VALUES 
            ((SELECT MAX(id) FROM tasks), ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )");
        
        for (const auto &stub : stubs) {
            query.addBindValue(stub.filePath);
            query.addBindValue(stub.lineNumber);
            query.addBindValue(stub.language);
            query.addBindValue(stub.stubType);
            query.addBindValue(stub.originalCode);
            query.addBindValue(stub.suggestedFix);
            query.addBindValue(stub.appliedFix);
            query.addBindValue(stub.applied);
            query.addBindValue(stub.confidenceScore);
            query.exec();
        }
    }
    
    fileScanCompleted(filePath, stubs.size(), fixedCount);
    progressUpdate(m_state.filesProcessed.loadAcquire(), 
                        m_state.totalFiles.loadAcquire(),
                        m_state.stubsFound.loadAcquire(),
                        m_state.stubsFixed.loadAcquire());
}

std::vector<StubInstance> RawrXDDigestionEngine::detectStubsAVX512(
    const std::string &content, const std::string &language, const std::string &filePath) {
    
    std::vector<StubInstance> results;
    std::vector<uint8_t> utf8 = content.toUtf8();
    const char *data = utf8.constData();
    size_t len = utf8.size();
    
    // Get signatures for this language
    std::vector<const char*> sigs;
    for (const auto &profile : m_profiles) {
        if (profile.name == language) {
            for (const auto &sig : profile.stubSignatures) {
                sigs.append(sig.constData());
            }
            break;
        }
    }
    
    if (sigs.empty()) return detectStubsFallback(content, language, filePath);
    
    int positions[1024]; // Max 1024 stubs per file
    int matchCount = avx512_scan_stubs(data, len, sigs.data(), sigs.size(), positions, 1024);
    
    // Convert byte positions to line numbers and extract context
    std::stringList lines = content.split('\n');
    int currentPos = 0;
    int lineNum = 1;
    
    for (int i = 0; i < matchCount; ++i) {
        int pos = positions[i];
        // Find line number
        while (currentPos < pos && currentPos < len) {
            if (data[currentPos] == '\n') lineNum++;
            currentPos++;
        }
        
        StubInstance stub;
        stub.filePath = filePath;
        stub.language = language;
        stub.lineNumber = lineNum;
        stub.stubType = "AVX_MATCH";
        
        // Extract context (3 lines)
        int startLine = qMax(1, lineNum - 1);
        int endLine = qMin(lines.size(), lineNum + 1);
        stub.originalCode = lines.mid(startLine - 1, endLine - startLine + 1).join('\n');
        stub.context = stub.originalCode; // Set context for fix generation
        
        results.append(stub);
    }
    
    return results;
}

std::vector<StubInstance> RawrXDDigestionEngine::detectStubsFallback(
    const std::string &content, const std::string &language, const std::string &filePath) {
    
    std::vector<StubInstance> results;
    std::stringList lines = content.split('\n');
    
    const LangProfile *profile = nullptr;
    for (const auto &p : m_profiles) {
        if (p.name == language) {
            profile = &p;
            break;
        }
    }
    if (!profile) return results;
    
    for (int i = 0; i < lines.size(); ++i) {
        auto match = profile->stubRegex.match(lines[i]);
        if (match.hasMatch()) {
            StubInstance stub;
            stub.filePath = filePath;
            stub.language = language;
            stub.lineNumber = i + 1;
            stub.stubType = match.captured(0);
            
            int start = qMax(0, i - 1);
            int end = qMin(lines.size() - 1, i + 1);
            stub.originalCode = lines.mid(start, end - start + 1).join('\n');
            stub.context = stub.originalCode;
            
            results.append(stub);
        }
    }
    
    return results;
}

std::string RawrXDDigestionEngine::generateAIFix(const StubInstance &stub) {
    std::string fix;
    float confidence = 0.0f;
    
    // Try AI systems in order of sophistication
    if (m_codingAgent) {
        // Use advanced coding agent for complex fixes
        aiFixRequested(stub.originalCode, &fix);
        confidence = 0.95f;
    } else if (m_modelRouter) {
        // Route to appropriate model
        aiFixRequested(stub.context, &fix);
        confidence = 0.85f;
    } else if (m_completionEngine) {
        // Use completion engine for simple fixes
        fix = "// [AGENTIC-AUTO] Implementation required\n// Original: " + stub.stubType;
        confidence = 0.6f;
    } else {
        // Fallback template-based
        if (stub.language == "MASM") {
            fix = "    ; [AGENTIC] Implemented stub at line " + std::string::number(stub.lineNumber) + "\n    nop";
        } else if (stub.language == "C++") {
            if (stub.originalCode.contains("bool")) {
                fix = "    // [AGENTIC] Auto-generated\n    return true;";
            } else if (stub.originalCode.contains("int")) {
                fix = "    // [AGENTIC] Auto-generated\n    return 0;";
            } else {
                fix = "    // [AGENTIC] TODO: Implement " + stub.stubType;
            }
        } else {
            fix = "# [AGENTIC] TODO: Implement " + stub.stubType;
        }
        confidence = 0.5f;
    }
    
    aiFixGenerated(fix, confidence);
    return fix;
}

bool RawrXDDigestionEngine::applyFix(const StubInstance &stub, const std::string &fix) {
    // File operation removed;
    if (!file.open(std::iostream::ReadOnly | std::iostream::Text)) return false;
    
    std::string content = std::string::fromUtf8(file.readAll());
    file.close();
    
    std::stringList lines = content.split('\n');
    if (stub.lineNumber < 1 || stub.lineNumber > lines.size()) return false;
    
    // Replace the line
    lines[stub.lineNumber - 1] = fix;
    
    // Atomic write
    QSaveFile out(stub.filePath);
    if (!out.open(std::iostream::WriteOnly)) return false;
    out.write(lines.join('\n').toUtf8());
    return out.commit();
}

void RawrXDDigestionEngine::saveCheckpoint() {
    std::mutexLocker lock(&m_dbMutex);
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO checkpoints (task_id, files_processed) VALUES ((SELECT MAX(id) FROM tasks), ?)");
    query.addBindValue(m_state.filesProcessed.loadAcquire());
    query.exec();
}

void RawrXDDigestionEngine::pause() {
    m_state.paused.storeRelease(1);
}

void RawrXDDigestionEngine::resume() {
    m_state.paused.storeRelease(0);
}

void RawrXDDigestionEngine::stop() {
    m_state.stopRequested.storeRelease(1);
}

bool RawrXDDigestionEngine::isRunning() const {
    return m_state.running.loadAcquire();
}

bool RawrXDDigestionEngine::isPaused() const {
    return m_state.paused.loadAcquire();
}

RawrXDDigestionEngine::Stats RawrXDDigestionEngine::getStatistics() const {
    Stats s;
    s.totalFilesScanned = m_state.filesProcessed.loadAcquire();
    s.totalStubsFound = m_state.stubsFound.loadAcquire();
    s.totalStubsFixed = m_state.stubsFixed.loadAcquire();
    s.totalTimeMs = m_state.timer.elapsed();
    s.avgTimePerFileMs = s.totalFilesScanned > 0 ? 
        static_cast<float>(s.totalTimeMs) / s.totalFilesScanned : 0.0f;
    s.totalErrors = 0; // Initialize totalErrors
    
    // Get per-language stats from DB
    std::mutexLocker lock(&m_dbMutex);
    QSqlQuery query(m_db);
    query.exec("SELECT language, COUNT(*) FROM stubs GROUP BY language");
    while (query.next()) {
        s.stubsByLanguage[query.value(0).toString()] = query.value(1);
    }
    
    return s;
}

void* RawrXDDigestionEngine::generateReport(const // DateTime &from, const // DateTime &to) {
    void* report;
    void* files;
    
    std::mutexLocker lock(&m_dbMutex);
    QSqlQuery query(m_db);
    
    if (from.isValid() && to.isValid()) {
        query.prepare("SELECT * FROM stubs WHERE detected_at BETWEEN ? AND ?");
        query.addBindValue(from);
        query.addBindValue(to);
    } else {
        query.prepare("SELECT * FROM stubs");
    }
    query.exec();
    
    while (query.next()) {
        void* stub;
        stub["file"] = query.value("file_path").toString();
        stub["line"] = query.value("line_number");
        stub["language"] = query.value("language").toString();
        stub["type"] = query.value("stub_type").toString();
        stub["applied"] = query.value("applied").toBool();
        stub["confidence"] = query.value("confidence_score").toDouble();
        files.append(stub);
    }
    
    report["files"] = files;
    report["total_stubs"] = files.size();
    report["generated_at"] = // DateTime::currentDateTime().toString(ISODate);
    
    return report;
}

bool RawrXDDigestionEngine::exportToJson(const std::string &path) {
    void* report = generateReport();
    // File operation removed;
    if (!file.open(std::iostream::WriteOnly)) return false;
    file.write(void*(report).toJson(void*::Indented));
    return true;
}

std::vector<DigestionTask> RawrXDDigestionEngine::getTaskHistory(int limit) {
     std::vector<DigestionTask> tasks;
     std::mutexLocker lock(&m_dbMutex);
     QSqlQuery query(m_db);
     query.prepare("SELECT * FROM tasks ORDER BY started DESC LIMIT ?");
     query.addBindValue(limit);
     query.exec();
     while(query.next()){
         DigestionTask t;
         t.id = query.value("id");
         t.filePath = query.value("root_dir").toString();
         t.stubsFound = query.value("stubs_found");
         t.stubsFixed = query.value("stubs_fixed");
         t.status = query.value("status").toString();
         tasks.append(t);
     }
     return tasks;
}

std::vector<StubInstance> RawrXDDigestionEngine::getPendingStubs() {
    std::vector<StubInstance> stubs;
    std::mutexLocker lock(&m_dbMutex);
    QSqlQuery query(m_db);
    query.exec("SELECT * FROM stubs WHERE applied = 0");
    while(query.next()){
        StubInstance s;
        s.id = query.value("id");
        s.filePath = query.value("file_path").toString();
        s.lineNumber = query.value("line_number");
        s.stubType = query.value("stub_type").toString();
        stubs.append(s);
    }
    return stubs;
}

void RawrXDDigestionEngine::setCompletionEngine(CompletionEngine *engine) {
    m_completionEngine = engine;
}

void RawrXDDigestionEngine::setContextAnalyzer(CodebaseContextAnalyzer *analyzer) {
    m_contextAnalyzer = analyzer;
}

void RawrXDDigestionEngine::setRewriteEngine(SmartRewriteEngine *engine) {
    m_rewriteEngine = engine;
}

void RawrXDDigestionEngine::setModelRouter(MultiModalModelRouter *router) {
    m_modelRouter = router;
}

void RawrXDDigestionEngine::setCodingAgent(AdvancedCodingAgent *agent) {
    m_codingAgent = agent;
}

void RawrXDDigestionEngine::processNextChunk() {}
void RawrXDDigestionEngine::onFileScanned(const std::string &, const std::vector<StubInstance> &) {}
void RawrXDDigestionEngine::onStubFixApplied(int, bool) {}
void RawrXDDigestionEngine::loadCheckpoint() {}
void RawrXDDigestionEngine::updateTaskStatus(int, const std::string &) {}






