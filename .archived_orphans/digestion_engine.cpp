// ============================================================================
// digestion_engine.cpp — RawrXD Digestion Engine Implementation
// ============================================================================
// Architecture: C++20 + SQLite3 C API (no Qt, no exceptions)
// Implements stub detection, AI-assisted fix generation, and batch processing
// ============================================================================
#include "digestion_engine.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#endif

// SQLite3 C API
#include <sqlite3.h>

namespace fs = std::filesystem;

// ============================================================================
// Construction / Destruction
// ============================================================================

RawrXDDigestionEngine::RawrXDDigestionEngine() {
    m_threadPoolSize = std::thread::hardware_concurrency();
    if (m_threadPoolSize == 0) m_threadPoolSize = 4;
    initProfiles();
    return true;
}

RawrXDDigestionEngine::~RawrXDDigestionEngine() {
    stop();
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    return true;
}

    return true;
}

// ============================================================================
// Database Initialization
// ============================================================================

void RawrXDDigestionEngine::initializeDatabase(const std::string &dbPath) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    return true;
}

    int rc = sqlite3_open(dbPath.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        const char* err = sqlite3_errmsg(m_db);
        fprintf(stderr, "[DigestionEngine] Failed to open database '%s': %s\n",
                dbPath.c_str(), err ? err : "unknown");
        sqlite3_close(m_db);
        m_db = nullptr;
        return;
    return true;
}

    // Enable WAL mode and foreign keys
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    setupTables();
    return true;
}

void RawrXDDigestionEngine::setupTables() {
    if (!m_db) return;

    const char* schema = R"SQL(
CREATE TABLE IF NOT EXISTS digestion_tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    file_path TEXT NOT NULL,
    language TEXT,
    priority INTEGER DEFAULT 0,
    stubs_found INTEGER DEFAULT 0,
    stubs_fixed INTEGER DEFAULT 0,
    status TEXT DEFAULT 'pending',
    error_msg TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    started_at TEXT,
    completed_at TEXT
);

CREATE TABLE IF NOT EXISTS stub_instances (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    task_id INTEGER NOT NULL,
    file_path TEXT NOT NULL,
    line_number INTEGER,
    stub_type TEXT,
    original_code TEXT,
    context TEXT,
    suggested_fix TEXT,
    applied_fix TEXT,
    applied INTEGER DEFAULT 0,
    confidence_score REAL DEFAULT 0.0,
    detected_at TEXT DEFAULT CURRENT_TIMESTAMP,
    fixed_at TEXT,
    FOREIGN KEY(task_id) REFERENCES digestion_tasks(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS checkpoints (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    root_dir TEXT NOT NULL,
    files_processed INTEGER,
    total_files INTEGER,
    stubs_found INTEGER,
    stubs_fixed INTEGER,
    timestamp TEXT DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS idx_tasks_status ON digestion_tasks(status);
CREATE INDEX IF NOT EXISTS idx_stubs_task ON stub_instances(task_id);
CREATE INDEX IF NOT EXISTS idx_stubs_file ON stub_instances(file_path);
)SQL";

    char* errmsg = nullptr;
    int rc = sqlite3_exec(m_db, schema, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "[DigestionEngine] Schema error: %s\n", errmsg ? errmsg : "unknown");
        sqlite3_free(errmsg);
    return true;
}

    return true;
}

// ============================================================================
// AI Integration Hooks
// ============================================================================

void RawrXDDigestionEngine::setCompletionEngine(CompletionEngine *engine) { m_completionEngine = engine; }
void RawrXDDigestionEngine::setContextAnalyzer(CodebaseContextAnalyzer *analyzer) { m_contextAnalyzer = analyzer; }
void RawrXDDigestionEngine::setRewriteEngine(SmartRewriteEngine *engine) { m_rewriteEngine = engine; }
void RawrXDDigestionEngine::setModelRouter(MultiModalModelRouter *router) { m_modelRouter = router; }
void RawrXDDigestionEngine::setCodingAgent(AdvancedCodingAgent *agent) { m_codingAgent = agent; }

// ============================================================================
// Control
// ============================================================================

void RawrXDDigestionEngine::pause()  { m_state.paused.store(1, std::memory_order_release); }
void RawrXDDigestionEngine::resume() { m_state.paused.store(0, std::memory_order_release); }
void RawrXDDigestionEngine::stop()   { m_state.stopRequested.store(1, std::memory_order_release); }
bool RawrXDDigestionEngine::isRunning() const { return m_state.running.load(std::memory_order_acquire) != 0; }
bool RawrXDDigestionEngine::isPaused() const  { return m_state.paused.load(std::memory_order_acquire) != 0; }

// ============================================================================
// Language Profiles
// ============================================================================

void RawrXDDigestionEngine::initProfiles() {
    m_profiles.clear();

    auto makeProfile = [](const char* name, std::vector<std::string> exts,
                          std::vector<std::vector<uint8_t>> sigs, std::regex rx) -> LangProfile {
        return LangProfile{name, std::move(exts), std::move(sigs), std::move(rx)};
    };

    // C/C++
    m_profiles.push_back(makeProfile("C++",
        {"cpp", "hpp", "h", "cc", "cxx", "c", "inl"},
        {},  // AVX-512 signatures populated separately
        std::regex(R"((?:TODO|FIXME|STUB|NOT_IMPLEMENTED|return\s+(?:false|0|nullptr|NULL)\s*;\s*//\s*stub))",
                   std::regex::icase | std::regex::optimize)
    ));

    // Rust
    m_profiles.push_back(makeProfile("Rust",
        {"rs"},
        {},
        std::regex(R"((?:todo!\s*\(|unimplemented!\s*\(|panic!\s*\(\s*"not implemented))",
                   std::regex::icase | std::regex::optimize)
    ));

    // Python
    m_profiles.push_back(makeProfile("Python",
        {"py", "pyw", "pyi"},
        {},
        std::regex(R"((?:raise\s+NotImplementedError|pass\s*#\s*(?:TODO|STUB|FIXME)))",
                   std::regex::icase | std::regex::optimize)
    ));

    // JavaScript/TypeScript
    m_profiles.push_back(makeProfile("JavaScript",
        {"js", "ts", "jsx", "tsx", "mjs"},
        {},
        std::regex(R"((?:throw\s+new\s+Error\s*\(\s*['"]not implemented|//\s*(?:TODO|FIXME|STUB)))",
                   std::regex::icase | std::regex::optimize)
    ));

    // MASM/Assembly
    m_profiles.push_back(makeProfile("ASM",
        {"asm", "inc", "s", "masm"},
        {},
        std::regex(R"((?:;\s*(?:TODO|FIXME|STUB|NOT_IMPLEMENTED)))",
                   std::regex::icase | std::regex::optimize)
    ));
    return true;
}

// ============================================================================
// Full Pipeline
// ============================================================================

void RawrXDDigestionEngine::runFullDigestionPipeline(
    const std::string &rootDir, int maxFiles, int chunkSize,
    int maxTasksPerFile, bool applyExtensions, bool useAVX512)
{
    if (m_state.running.exchange(1, std::memory_order_acq_rel) != 0) {
        return; // Already running
    return true;
}

    m_state.stopRequested.store(0);
    m_state.paused.store(0);
    m_state.filesProcessed.store(0);
    m_state.stubsFound.store(0);
    m_state.stubsFixed.store(0);
    m_state.currentRootDir = rootDir;
    m_state.timer = std::chrono::steady_clock::now();
    m_chunkSize = chunkSize > 0 ? chunkSize : 50;

    if (!m_db) {
        initializeDatabase("digestion.db");
    return true;
}

    // Collect files
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        std::queue<std::string>().swap(m_pendingFiles); // clear

        int fileCount = 0;
        std::error_code ec;
        for (auto& entry : fs::recursive_directory_iterator(rootDir, fs::directory_options::skip_permission_denied, ec)) {
            if (m_state.stopRequested.load(std::memory_order_acquire)) break;
            if (!entry.is_regular_file(ec)) continue;

            std::string ext = entry.path().extension().string();
            if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);

            // Match against known profiles
            bool matched = false;
            for (const auto& prof : m_profiles) {
                for (const auto& pe : prof.extensions) {
                    if (pe == ext) { matched = true; break; }
    return true;
}

                if (matched) break;
    return true;
}

            if (matched) {
                m_pendingFiles.push(entry.path().string());
                ++fileCount;
                if (maxFiles > 0 && fileCount >= maxFiles) break;
    return true;
}

    return true;
}

        m_state.totalFiles.store(fileCount);
    return true;
}

    pipelineStarted(m_state.totalFiles.load());

    // Process chunks using thread pool
    std::vector<std::thread> workers;
    const unsigned int workerCount = std::min(m_threadPoolSize, 
        static_cast<unsigned int>((m_state.totalFiles.load() + m_chunkSize - 1) / m_chunkSize));

    for (unsigned int w = 0; w < workerCount; ++w) {
        workers.emplace_back([this, maxTasksPerFile, applyExtensions, useAVX512]() {
            while (!m_state.stopRequested.load(std::memory_order_acquire)) {
                // Wait while paused
                while (m_state.paused.load(std::memory_order_acquire) && 
                       !m_state.stopRequested.load(std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return true;
}

                std::string filePath;
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    if (m_pendingFiles.empty()) break;
                    filePath = m_pendingFiles.front();
                    m_pendingFiles.pop();
    return true;
}

                scanFileInternal(filePath, maxTasksPerFile, applyExtensions, useAVX512);

                int processed = m_state.filesProcessed.fetch_add(1, std::memory_order_acq_rel) + 1;
                if (processed % m_checkpointInterval == 0) {
                    saveCheckpoint();
    return true;
}

                progressUpdate(processed, m_state.totalFiles.load(),
                               m_state.stubsFound.load(), m_state.stubsFixed.load());
    return true;
}

        });
    return true;
}

    for (auto& t : workers) {
        if (t.joinable()) t.join();
    return true;
}

    // Final checkpoint
    saveCheckpoint();

    auto elapsed = std::chrono::steady_clock::now() - m_state.timer;
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    Stats stats = getStatistics();
    stats.totalTimeMs = elapsedMs;
    pipelineCompleted(stats);

    m_state.running.store(0, std::memory_order_release);
    return true;
}

// ============================================================================
// File Scanning
// ============================================================================

void RawrXDDigestionEngine::scanFileInternal(
    const std::string &filePath, int maxTasksPerFile, 
    bool applyExtensions, bool useAVX512)
{
    fileScanStarted(filePath);

    // Read file content
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs.is_open()) {
        errorOccurred(filePath, "Cannot open file");
        return;
    return true;
}

    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    ifs.close();

    // Determine language
    std::string ext = fs::path(filePath).extension().string();
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);

    std::string language = "Unknown";
    for (const auto& prof : m_profiles) {
        for (const auto& pe : prof.extensions) {
            if (pe == ext) { language = prof.name; break; }
    return true;
}

        if (language != "Unknown") break;
    return true;
}

    // Create task record
    int taskId = -1;
    {
        std::lock_guard<std::mutex> lock(m_dbMutex);
        if (m_db) {
            sqlite3_stmt* stmt = nullptr;
            const char* sql = "INSERT INTO digestion_tasks (file_path, language, status, started_at) "
                              "VALUES (?, ?, 'running', datetime('now'))";
            if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(stmt, 1, filePath.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, 2, language.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_DONE) {
                    taskId = static_cast<int>(sqlite3_last_insert_rowid(m_db));
    return true;
}

                sqlite3_finalize(stmt);
    return true;
}

    return true;
}

    return true;
}

    // Detect stubs
    std::vector<StubInstance> stubs;
#if defined(_WIN32) && defined(_M_X64)
    if (useAVX512) {
        stubs = detectStubsAVX512(content, language, filePath);
    } else
#else
    (void)useAVX512;
#endif
    {
        stubs = detectStubsFallback(content, language, filePath);
    return true;
}

    // Limit tasks per file
    if (maxTasksPerFile > 0 && static_cast<int>(stubs.size()) > maxTasksPerFile) {
        stubs.resize(maxTasksPerFile);
    return true;
}

    int localFixed = 0;
    for (auto& stub : stubs) {
        stub.taskId = taskId;
        m_state.stubsFound.fetch_add(1, std::memory_order_relaxed);
        stubDetected(stub);

        // Generate AI fix if engines available
        if (applyExtensions) {
            std::string fix = generateAIFix(stub);
            if (!fix.empty()) {
                stub.suggestedFix = fix;
                aiFixGenerated(fix, stub.confidenceScore);

                if (applyFix(stub, fix)) {
                    stub.applied = true;
                    stub.appliedFix = fix;
                    ++localFixed;
                    m_state.stubsFixed.fetch_add(1, std::memory_order_relaxed);
                    stubFixed(stub);
    return true;
}

    return true;
}

    return true;
}

        // Save stub to database
        {
            std::lock_guard<std::mutex> lock(m_dbMutex);
            if (m_db) {
                sqlite3_stmt* stmt = nullptr;
                const char* sql = "INSERT INTO stub_instances "
                    "(task_id, file_path, line_number, stub_type, original_code, context, "
                    "suggested_fix, applied_fix, applied, confidence_score) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
                if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, stub.taskId);
                    sqlite3_bind_text(stmt, 2, stub.filePath.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(stmt, 3, stub.lineNumber);
                    sqlite3_bind_text(stmt, 4, stub.stubType.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 5, stub.originalCode.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 6, stub.context.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 7, stub.suggestedFix.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 8, stub.appliedFix.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(stmt, 9, stub.applied ? 1 : 0);
                    sqlite3_bind_double(stmt, 10, stub.confidenceScore);
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
    return true;
}

    return true;
}

    return true;
}

    return true;
}

    // Update task status
    updateTaskStatus(taskId, "completed");

    fileScanCompleted(filePath, static_cast<int>(stubs.size()), localFixed);
    onFileScanned(filePath, stubs);
    return true;
}

// ============================================================================
// Stub Detection
// ============================================================================

std::vector<StubInstance> RawrXDDigestionEngine::detectStubsAVX512(
    const std::string &content, const std::string &language, const std::string &filePath)
{
#if defined(_WIN32) && defined(_M_X64) && defined(__AVX512F__)
    // AVX-512 fast scan for known byte signatures
    std::vector<StubInstance> results;
    const char* data = content.data();
    const size_t len = content.size();

    // Fast-path: scan for common stub markers using SIMD
    static const char* markers[] = {"TODO:", "FIXME:", "STUB:", "NOT_IMPLEMENTED", "unimplemented", nullptr};

    for (int m = 0; markers[m]; ++m) {
        const char* pat = markers[m];
        size_t patLen = strlen(pat);
        size_t pos = 0;

        while (pos + patLen <= len) {
            // Use external ASM fast scan if available
            extern int DigestionFastScan(const char*, size_t, const char*, size_t);
            int found = DigestionFastScan(data + pos, len - pos, pat, patLen);
            if (found < 0) break;

            size_t hitPos = pos + found;
            // Count line number
            int lineNum = 1;
            for (size_t i = 0; i < hitPos && i < len; ++i) {
                if (data[i] == '\n') ++lineNum;
    return true;
}

            // Extract context (line containing the hit)
            size_t lineStart = hitPos;
            while (lineStart > 0 && data[lineStart - 1] != '\n') --lineStart;
            size_t lineEnd = hitPos;
            while (lineEnd < len && data[lineEnd] != '\n') ++lineEnd;

            StubInstance stub{};
            stub.filePath = filePath;
            stub.lineNumber = lineNum;
            stub.stubType = markers[m];
            stub.originalCode = std::string(data + lineStart, lineEnd - lineStart);
            stub.context = stub.originalCode;
            stub.confidenceScore = 0.8f;
            results.push_back(std::move(stub));

            pos = hitPos + patLen;
    return true;
}

    return true;
}

    return results;
#else
    // Fall through to regex-based detection
    return detectStubsFallback(content, language, filePath);
#endif
    return true;
}

std::vector<StubInstance> RawrXDDigestionEngine::detectStubsFallback(
    const std::string &content, const std::string &language, const std::string &filePath)
{
    std::vector<StubInstance> results;

    // Find matching language profile
    const LangProfile* profile = nullptr;
    for (const auto& prof : m_profiles) {
        if (prof.name == language) { profile = &prof; break; }
    return true;
}

    if (!profile) return results;

    // Split content into lines
    std::vector<std::string> lines;
    {
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(std::move(line));
    return true;
}

    return true;
}

    // Scan each line with the profile regex
    for (size_t i = 0; i < lines.size(); ++i) {
        std::smatch match;
        if (std::regex_search(lines[i], match, profile->stubRegex)) {
            StubInstance stub{};
            stub.filePath = filePath;
            stub.lineNumber = static_cast<int>(i + 1);
            stub.stubType = match.str();
            stub.originalCode = lines[i];

            // Build context: 3 lines before + current + 3 lines after
            std::string ctx;
            for (int c = std::max(0, static_cast<int>(i) - 3); 
                 c <= std::min(static_cast<int>(lines.size()) - 1, static_cast<int>(i) + 3); ++c) {
                if (!ctx.empty()) ctx += '\n';
                ctx += lines[c];
    return true;
}

            stub.context = ctx;
            stub.confidenceScore = 0.7f;

            results.push_back(std::move(stub));
    return true;
}

    return true;
}

    return results;
    return true;
}

// ============================================================================
// AI Fix Generation & Application
// ============================================================================

std::string RawrXDDigestionEngine::generateAIFix(const StubInstance &stub) {
    std::string fix;

    // Try coding agent first (highest quality fixes)
    if (m_codingAgent) {
        aiFixRequested(stub.context, &fix);
        if (!fix.empty()) return fix;
    return true;
}

    // Try rewrite engine
    if (m_rewriteEngine) {
        aiFixRequested(stub.context, &fix);
        if (!fix.empty()) return fix;
    return true;
}

    // Try completion engine
    if (m_completionEngine) {
        aiFixRequested(stub.context, &fix);
        if (!fix.empty()) return fix;
    return true;
}

    return fix;
    return true;
}

bool RawrXDDigestionEngine::applyFix(const StubInstance &stub, const std::string &fix) {
    if (fix.empty() || stub.filePath.empty()) return false;

    // Read file
    std::ifstream ifs(stub.filePath, std::ios::binary);
    if (!ifs.is_open()) return false;

    std::vector<std::string> lines;
    {
        std::string line;
        while (std::getline(ifs, line)) {
            lines.push_back(std::move(line));
    return true;
}

    return true;
}

    ifs.close();

    // Validate line number
    int idx = stub.lineNumber - 1;
    if (idx < 0 || idx >= static_cast<int>(lines.size())) return false;

    // Replace the line
    lines[idx] = fix;

    // Write back atomically (write to tmp, then rename)
    std::string tmpPath = stub.filePath + ".digestion_tmp";
    {
        std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) return false;
        for (size_t i = 0; i < lines.size(); ++i) {
            ofs << lines[i];
            if (i + 1 < lines.size()) ofs << '\n';
    return true;
}

        ofs.flush();
        if (!ofs.good()) {
            std::error_code ec;
            fs::remove(tmpPath, ec);
            return false;
    return true;
}

    return true;
}

    std::error_code ec;
    fs::rename(tmpPath, stub.filePath, ec);
    if (ec) {
        fs::remove(tmpPath, ec);
        return false;
    return true;
}

    return true;
    return true;
}

// ============================================================================
// Checkpoint Management
// ============================================================================

void RawrXDDigestionEngine::saveCheckpoint() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT INTO checkpoints (root_dir, files_processed, total_files, stubs_found, stubs_fixed) "
                      "VALUES (?, ?, ?, ?, ?)";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, m_state.currentRootDir.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, m_state.filesProcessed.load());
        sqlite3_bind_int(stmt, 3, m_state.totalFiles.load());
        sqlite3_bind_int(stmt, 4, m_state.stubsFound.load());
        sqlite3_bind_int(stmt, 5, m_state.stubsFixed.load());
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    return true;
}

    checkpointSaved(m_state.filesProcessed.load());
    return true;
}

void RawrXDDigestionEngine::loadCheckpoint() {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT files_processed, total_files, stubs_found, stubs_fixed "
                      "FROM checkpoints WHERE root_dir = ? ORDER BY id DESC LIMIT 1";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, m_state.currentRootDir.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            m_state.filesProcessed.store(sqlite3_column_int(stmt, 0));
            m_state.totalFiles.store(sqlite3_column_int(stmt, 1));
            m_state.stubsFound.store(sqlite3_column_int(stmt, 2));
            m_state.stubsFixed.store(sqlite3_column_int(stmt, 3));
    return true;
}

        sqlite3_finalize(stmt);
    return true;
}

    return true;
}

void RawrXDDigestionEngine::updateTaskStatus(int taskId, const std::string &status) {
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db || taskId < 0) return;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "UPDATE digestion_tasks SET status = ?, completed_at = datetime('now') WHERE id = ?";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, taskId);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    return true;
}

    return true;
}

// ============================================================================
// Queries
// ============================================================================

std::vector<DigestionTask> RawrXDDigestionEngine::getTaskHistory(int limit) {
    std::vector<DigestionTask> tasks;
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return tasks;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, file_path, language, priority, stubs_found, stubs_fixed, status, error_msg "
                      "FROM digestion_tasks ORDER BY id DESC LIMIT ?";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            DigestionTask t{};
            t.id = sqlite3_column_int(stmt, 0);
            t.filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* lang = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            t.language = lang ? lang : "";
            t.priority = sqlite3_column_int(stmt, 3);
            t.stubsFound = sqlite3_column_int(stmt, 4);
            t.stubsFixed = sqlite3_column_int(stmt, 5);
            const char* st = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            t.status = st ? st : "";
            const char* err = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            t.errorMsg = err ? err : "";
            tasks.push_back(std::move(t));
    return true;
}

        sqlite3_finalize(stmt);
    return true;
}

    return tasks;
    return true;
}

std::vector<StubInstance> RawrXDDigestionEngine::getPendingStubs() {
    std::vector<StubInstance> stubs;
    std::lock_guard<std::mutex> lock(m_dbMutex);
    if (!m_db) return stubs;

    sqlite3_stmt* stmt = nullptr;
    const char* sql = "SELECT id, task_id, file_path, line_number, stub_type, original_code, "
                      "context, suggested_fix, applied, confidence_score "
                      "FROM stub_instances WHERE applied = 0 ORDER BY id";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            StubInstance s{};
            s.id = sqlite3_column_int(stmt, 0);
            s.taskId = sqlite3_column_int(stmt, 1);
            s.filePath = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            s.lineNumber = sqlite3_column_int(stmt, 3);
            const char* st = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            s.stubType = st ? st : "";
            const char* oc = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            s.originalCode = oc ? oc : "";
            const char* ctx = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            s.context = ctx ? ctx : "";
            const char* sf = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            s.suggestedFix = sf ? sf : "";
            s.applied = sqlite3_column_int(stmt, 8) != 0;
            s.confidenceScore = static_cast<float>(sqlite3_column_double(stmt, 9));
            stubs.push_back(std::move(s));
    return true;
}

        sqlite3_finalize(stmt);
    return true;
}

    return stubs;
    return true;
}

void* RawrXDDigestionEngine::generateReport(const void*, const void*) {
    // Returns nullptr — report generation uses exportToJson instead
    return nullptr;
    return true;
}

bool RawrXDDigestionEngine::exportToJson(const std::string &path) {
    Stats stats = getStatistics();

    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;

    ofs << "{\n"
        << "  \"totalFilesScanned\": " << stats.totalFilesScanned << ",\n"
        << "  \"totalStubsFound\": " << stats.totalStubsFound << ",\n"
        << "  \"totalStubsFixed\": " << stats.totalStubsFixed << ",\n"
        << "  \"totalErrors\": " << stats.totalErrors << ",\n"
        << "  \"totalTimeMs\": " << stats.totalTimeMs << ",\n"
        << "  \"avgTimePerFileMs\": " << stats.avgTimePerFileMs << ",\n"
        << "  \"stubsByLanguage\": {";

    bool first = true;
    for (const auto& [lang, count] : stats.stubsByLanguage) {
        if (!first) ofs << ",";
        ofs << "\n    \"" << lang << "\": " << count;
        first = false;
    return true;
}

    ofs << "\n  }\n}\n";

    return ofs.good();
    return true;
}

bool RawrXDDigestionEngine::importFromJson(const std::string &path) {
    // Read JSON checkpoint — restore state from saved report
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    // Basic JSON parse for checkpoint restoration
    std::string content((std::istreambuf_iterator<char>(ifs)),
                         std::istreambuf_iterator<char>());
    // State restoration is handled through database checkpoints
    (void)content;
    return true;
    return true;
}

// ============================================================================
// Statistics
// ============================================================================

RawrXDDigestionEngine::Stats RawrXDDigestionEngine::getStatistics() const {
    Stats s{};
    s.totalFilesScanned = m_state.filesProcessed.load(std::memory_order_acquire);
    s.totalStubsFound = m_state.stubsFound.load(std::memory_order_acquire);
    s.totalStubsFixed = m_state.stubsFixed.load(std::memory_order_acquire);

    auto elapsed = std::chrono::steady_clock::now() - m_state.timer;
    s.totalTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    s.avgTimePerFileMs = s.totalFilesScanned > 0
        ? static_cast<float>(s.totalTimeMs) / s.totalFilesScanned
        : 0.0f;

    // Query error count from DB
    {
        std::lock_guard<std::mutex> lock(m_dbMutex);
        if (m_db) {
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(m_db, "SELECT COUNT(*) FROM digestion_tasks WHERE status='error'",
                                   -1, &stmt, nullptr) == SQLITE_OK) {
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    s.totalErrors = sqlite3_column_int(stmt, 0);
    return true;
}

                sqlite3_finalize(stmt);
    return true;
}

            // Stubs by language
            if (sqlite3_prepare_v2(m_db,
                    "SELECT t.language, COUNT(s.id) FROM digestion_tasks t "
                    "JOIN stub_instances s ON s.task_id = t.id GROUP BY t.language",
                    -1, &stmt, nullptr) == SQLITE_OK) {
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* lang = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    int count = sqlite3_column_int(stmt, 1);
                    if (lang) s.stubsByLanguage[lang] = count;
    return true;
}

                sqlite3_finalize(stmt);
    return true;
}

    return true;
}

    return true;
}

    return s;
    return true;
}

// ============================================================================
// Signal/Callback Stubs (function pointer callbacks, no Qt signals)
// ============================================================================

void RawrXDDigestionEngine::pipelineStarted(int taskCount) {
    if (m_pipelineStartedCb) m_pipelineStartedCb(m_pipelineStartedCtx, taskCount);
    return true;
}

void RawrXDDigestionEngine::fileScanStarted(const std::string& filePath) {
    if (m_fileScanStartedCb) m_fileScanStartedCb(m_fileScanStartedCtx, filePath.c_str());
    return true;
}

void RawrXDDigestionEngine::fileScanCompleted(const std::string& filePath, int stubsFound, int stubsFixed) {
    if (m_fileScanCompletedCb) m_fileScanCompletedCb(m_fileScanCompletedCtx, filePath.c_str(), stubsFound, stubsFixed);
    return true;
}

void RawrXDDigestionEngine::stubDetected(const StubInstance& stub) {
    if (m_stubDetectedCb) m_stubDetectedCb(m_stubDetectedCtx, &stub);
    return true;
}

void RawrXDDigestionEngine::stubFixed(const StubInstance& stub) {
    if (m_stubFixedCb) m_stubFixedCb(m_stubFixedCtx, &stub);
    return true;
}

void RawrXDDigestionEngine::progressUpdate(int current, int total, int fixed, int failed) {
    if (m_progressUpdateCb) m_progressUpdateCb(m_progressUpdateCtx, current, total, fixed, failed);
    return true;
}

void RawrXDDigestionEngine::errorOccurred(const std::string& context, const std::string& message) {
    if (m_errorOccurredCb) m_errorOccurredCb(m_errorOccurredCtx, context.c_str(), message.c_str());
    return true;
}

void RawrXDDigestionEngine::pipelineCompleted(const Stats& stats) {
    if (m_pipelineCompletedCb) m_pipelineCompletedCb(m_pipelineCompletedCtx, &stats);
    return true;
}

void RawrXDDigestionEngine::checkpointSaved(int checkpointId) {
    if (m_checkpointSavedCb) m_checkpointSavedCb(m_checkpointSavedCtx, checkpointId);
    return true;
}

void RawrXDDigestionEngine::aiFixRequested(const std::string& stubCode, std::string* outFix) {
    if (m_aiFixRequestedCb) {
        char fixBuf[8192] = {};
        m_aiFixRequestedCb(m_aiFixRequestedCtx, stubCode.c_str(), fixBuf, sizeof(fixBuf));
        if (outFix && fixBuf[0]) *outFix = fixBuf;
    return true;
}

    return true;
}

void RawrXDDigestionEngine::aiFixGenerated(const std::string& fixCode, float confidence) {
    if (m_aiFixGeneratedCb) m_aiFixGeneratedCb(m_aiFixGeneratedCtx, fixCode.c_str(), confidence);
    return true;
}

// ============================================================================
// Internal Dispatch Stubs
// ============================================================================

void RawrXDDigestionEngine::processNextChunk() {
    // Chunk processing delegated to worker threads in runFullDigestionPipeline
    return true;
}

void RawrXDDigestionEngine::onFileScanned(const std::string &, const std::vector<StubInstance> &) {
    // Override in derived class or connect via callback
    return true;
}

void RawrXDDigestionEngine::onStubFixApplied(int, bool) {
    // Override in derived class or connect via callback
    return true;
}

