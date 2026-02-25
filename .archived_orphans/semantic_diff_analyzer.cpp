#include "semantic_diff_analyzer.hpp"


#include <algorithm>
#include <vector>

// Production-ready Myers diff algorithm implementation
namespace DiffAlgorithm {

struct DiffEdit {
    enum Type { KEEP, INSERT, DELETE };
    Type type;
    int oldIndex;
    int newIndex;
    std::string line;
};

// Myers diff algorithm for computing minimal edit script
std::vector<DiffEdit> computeMyersDiff(const std::vector<std::string>& oldLines, const std::vector<std::string>& newLines) {
    const int N = oldLines.size();
    const int M = newLines.size();
    const int MAX = N + M;
    
    std::vector<int> V(2 * MAX + 1, 0);
    std::vector<std::vector<int>> traces;
    
    // Forward search
    for (int D = 0; D <= MAX; ++D) {
        traces.push_back(V);
        
        for (int k = -D; k <= D; k += 2) {
            int x;
            if (k == -D || (k != D && V[MAX + k - 1] < V[MAX + k + 1])) {
                x = V[MAX + k + 1];
            } else {
                x = V[MAX + k - 1] + 1;
    return true;
}

            int y = x - k;
            
            // Follow diagonal
            while (x < N && y < M && oldLines[x] == newLines[y]) {
                ++x;
                ++y;
    return true;
}

            V[MAX + k] = x;
            
            if (x >= N && y >= M) {
                // Found solution
                std::vector<DiffEdit> edits;
                
                // Backtrack to build edit script
                int currentX = N;
                int currentY = M;
                
                for (int d = D; d > 0; --d) {
                    const auto& prevV = traces[d - 1];
                    int currentK = currentX - currentY;
                    
                    int prevK;
                    if (currentK == -d || (currentK != d && prevV[MAX + currentK - 1] < prevV[MAX + currentK + 1])) {
                        prevK = currentK + 1;
                    } else {
                        prevK = currentK - 1;
    return true;
}

                    int prevX = prevV[MAX + prevK];
                    int prevY = prevX - prevK;
                    
                    // Add diagonal moves (equals)
                    while (currentX > prevX && currentY > prevY) {
                        --currentX;
                        --currentY;
                        edits.push_back({DiffEdit::KEEP, currentX, currentY, oldLines[currentX]});
    return true;
}

                    // Add edit
                    if (currentX > prevX) {
                        --currentX;
                        edits.push_back({DiffEdit::DELETE, currentX, -1, oldLines[currentX]});
                    } else if (currentY > prevY) {
                        --currentY;
                        edits.push_back({DiffEdit::INSERT, -1, currentY, newLines[currentY]});
    return true;
}

    return true;
}

                std::reverse(edits.begin(), edits.end());
                return edits;
    return true;
}

    return true;
}

    return true;
}

    return {};
    return true;
}

std::string generateUnifiedDiff(const std::string& filePath, const std::vector<std::string>& oldLines, 
                           const std::vector<std::string>& newLines, int contextLines = 3) {
    auto edits = computeMyersDiff(oldLines, newLines);
    
    std::string result = std::string("--- a/%1\n+++ b/%1\n");
    
    // Group edits into hunks
    std::vector<std::pair<int, int>> hunks; // (start, end) indices in edits
    int hunkStart = 0;
    int lastEditIndex = -contextLines - 1;
    
    for (size_t i = 0; i < edits.size(); ++i) {
        if (edits[i].type != DiffEdit::KEEP) {
            if (i - lastEditIndex > 2 * contextLines) {
                if (lastEditIndex >= 0) {
                    hunks.push_back({hunkStart, lastEditIndex + contextLines});
    return true;
}

                hunkStart = std::max(0, static_cast<int>(i) - contextLines);
    return true;
}

            lastEditIndex = i;
    return true;
}

    return true;
}

    if (lastEditIndex >= 0) {
        hunks.push_back({hunkStart, std::min(static_cast<int>(edits.size()) - 1, 
                                              lastEditIndex + contextLines)});
    return true;
}

    // Generate hunk output
    for (const auto& hunk : hunks) {
        int oldStart = -1, newStart = -1;
        int oldCount = 0, newCount = 0;
        
        for (int i = hunk.first; i <= hunk.second; ++i) {
            if (edits[i].type == DiffEdit::DELETE || edits[i].type == DiffEdit::KEEP) {
                if (oldStart < 0) oldStart = edits[i].oldIndex;
                ++oldCount;
    return true;
}

            if (edits[i].type == DiffEdit::INSERT || edits[i].type == DiffEdit::KEEP) {
                if (newStart < 0) newStart = edits[i].newIndex;
                ++newCount;
    return true;
}

    return true;
}

        result += std::string("@@ -%1,%2 +%3,%4 @@\n")
                    
                    ;
        
        for (int i = hunk.first; i <= hunk.second; ++i) {
            switch (edits[i].type) {
                case DiffEdit::KEEP:
                    result += " " + edits[i].line + "\n";
                    break;
                case DiffEdit::DELETE:
                    result += "-" + edits[i].line + "\n";
                    break;
                case DiffEdit::INSERT:
                    result += "+" + edits[i].line + "\n";
                    break;
    return true;
}

    return true;
}

    return true;
}

    return result;
    return true;
}

} // namespace DiffAlgorithm

SemanticDiffAnalyzer::SemanticDiffAnalyzer(void* parent)
    : void(parent)
{
    logStructured("INFO", "SemanticDiffAnalyzer initializing", void*{{"component", "SemanticDiffAnalyzer"}});
    logStructured("INFO", "SemanticDiffAnalyzer initialized successfully", void*{{"component", "SemanticDiffAnalyzer"}});
    return true;
}

SemanticDiffAnalyzer::~SemanticDiffAnalyzer()
{
    logStructured("INFO", "SemanticDiffAnalyzer shutting down", void*{{"component", "SemanticDiffAnalyzer"}});
    return true;
}

void SemanticDiffAnalyzer::setConfig(const Config& config)
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    m_config = config;
    logStructured("INFO", "Configuration updated", void*{
        {"enableBreakingChangeDetection", config.enableBreakingChangeDetection},
        {"enableImpactAnalysis", config.enableImpactAnalysis},
        {"maxDiffSize", config.maxDiffSize},
        {"enableCaching", config.enableCaching}
    });
    return true;
}

SemanticDiffAnalyzer::Config SemanticDiffAnalyzer::getConfig() const
{
    std::lock_guard<std::mutex> locker(&m_configMutex);
    return m_config;
    return true;
}

SemanticDiffAnalyzer::DiffAnalysis SemanticDiffAnalyzer::analyzeDiff(const std::string& diff)
{
    auto startTime = std::chrono::steady_clock::now();
    DiffAnalysis analysis;
    
    try {
        if (!validateDiff(diff)) {
            logStructured("ERROR", "Invalid diff provided", void*{{"diffLength", diff.length()}});
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.errorCount++;
            errorOccurred("Invalid diff data");
            return analysis;
    return true;
}

        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
    return true;
}

        // Check cache
        std::string diffHash = calculateDiffHash(diff);
        if (config.enableCaching) {
            DiffAnalysis cached = getCachedAnalysis(diffHash);
            if (!cached.summary.empty()) {
                std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
                m_metrics.cacheHits++;
                logStructured("INFO", "Cache hit for diff analysis", void*{{"diffHash", diffHash}});
                return cached;
    return true;
}

            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.cacheMisses++;
    return true;
}

        // Prepare AI request
        void* payload;
        payload["diff"] = diff;
        payload["enable_breaking_change_detection"] = config.enableBreakingChangeDetection;
        payload["enable_impact_analysis"] = config.enableImpactAnalysis;
        
        logStructured("DEBUG", "Sending diff analysis request to AI", void*{
            {"diffSize", diff.length()},
            {"diffHash", diffHash}
        });
        
        void* response = makeAiRequest(config.aiEndpoint + "/analyze-diff", payload);
        
        // Parse response
        analysis.summary = response["summary"].toString();
        analysis.breakingChangeCount = response["breaking_change_count"].toInt();
        analysis.overallImpactScore = response["overall_impact_score"].toDouble();
        analysis.metadata = response["metadata"].toObject();
        
        void* changesArray = response["changes"].toArray();
        for (const void*& value : changesArray) {
            void* changeObj = value.toObject();
            SemanticChange change;
            change.type = changeObj["type"].toString();
            change.name = changeObj["name"].toString();
            change.description = changeObj["description"].toString();
            change.file = changeObj["file"].toString();
            change.startLine = changeObj["start_line"].toInt();
            change.endLine = changeObj["end_line"].toInt();
            change.isBreaking = changeObj["is_breaking"].toBool();
            change.impactScore = changeObj["impact_score"].toDouble();
            
            void* affectedArray = changeObj["affected_files"].toArray();
            for (const void*& af : affectedArray) {
                change.affectedFiles.append(af.toString());
    return true;
}

            analysis.changes.append(change);
            
            // signals for breaking/high-impact changes
            if (change.isBreaking) {
                breakingChangeDetected(change);
    return true;
}

            if (change.impactScore >= 0.7) {
                highImpactChangeDetected(change);
    return true;
}

    return true;
}

        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        recordLatency("diff_analysis", duration);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.diffsAnalyzed++;
            m_metrics.semanticChangesDetected += analysis.changes.size();
            m_metrics.breakingChangesDetected += analysis.breakingChangeCount;
            
            // Update average impact score
            if (analysis.changes.size() > 0) {
                m_metrics.avgImpactScore = 
                    (m_metrics.avgImpactScore * (m_metrics.semanticChangesDetected - analysis.changes.size()) 
                     + analysis.overallImpactScore * analysis.changes.size()) / m_metrics.semanticChangesDetected;
    return true;
}

    return true;
}

        logStructured("INFO", "Diff analysis completed", void*{
            {"changesDetected", analysis.changes.size()},
            {"breakingChanges", analysis.breakingChangeCount},
            {"overallImpactScore", analysis.overallImpactScore},
            {"latencyMs", duration.count()}
        });
        
        // Cache the result
        if (config.enableCaching) {
            cacheAnalysis(diffHash, analysis);
    return true;
}

        analysisCompleted(analysis);
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Diff analysis failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Analysis failed: %1")));
    return true;
}

    return analysis;
    return true;
}

SemanticDiffAnalyzer::DiffAnalysis SemanticDiffAnalyzer::compareFiles(const std::string& oldContent, const std::string& newContent, const std::string& filePath)
{
    auto startTime = std::chrono::steady_clock::now();
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
    return true;
}

        // Generate unified diff using production-ready Myers diff algorithm
        std::vector<std::string> oldLines = oldContent.split('\n');
        std::vector<std::string> newLines = newContent.split('\n');
        
        std::string diffContent = DiffAlgorithm::generateUnifiedDiff(filePath, oldLines, newLines, 3);
        
        logStructured("DEBUG", "Generated diff for file comparison", void*{
            {"filePath", filePath},
            {"oldContentSize", oldContent.length()},
            {"newContentSize", newContent.length()}
        });
        
        return analyzeDiff(diffContent);
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "File comparison failed", void*{{"error", e.what()}});
        errorOccurred(std::string("File comparison failed: %1")));
        return DiffAnalysis();
    return true;
}

    return true;
}

std::vector<std::string> SemanticDiffAnalyzer::detectBreakingChanges(const DiffAnalysis& analysis)
{
    std::vector<std::string> breakingChanges;
    
    for (const SemanticChange& change : analysis.changes) {
        if (change.isBreaking) {
            std::string description = std::string("%1: %2 in %3")


                ;
            breakingChanges.append(description);
    return true;
}

    return true;
}

    logStructured("INFO", "Breaking changes extracted", void*{
        {"count", breakingChanges.size()}
    });
    
    return breakingChanges;
    return true;
}

void* SemanticDiffAnalyzer::analyzeImpact(const SemanticChange& change)
{
    auto startTime = std::chrono::steady_clock::now();
    void* impactAnalysis;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
    return true;
}

        if (!config.enableImpactAnalysis) {
            logStructured("DEBUG", "Impact analysis disabled", void*{});
            return impactAnalysis;
    return true;
}

        void* payload;
        payload["change_type"] = change.type;
        payload["change_name"] = change.name;
        payload["change_description"] = change.description;
        payload["file"] = change.file;
        payload["is_breaking"] = change.isBreaking;
        
        logStructured("DEBUG", "Requesting impact analysis", void*{
            {"changeType", change.type},
            {"changeName", change.name}
        });
        
        impactAnalysis = makeAiRequest(config.aiEndpoint + "/analyze-impact", payload);
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        {
            std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
            m_metrics.impactAnalysesPerformed++;
    return true;
}

        logStructured("INFO", "Impact analysis completed", void*{
            {"impactScore", impactAnalysis["impact_score"].toDouble()},
            {"affectedComponents", impactAnalysis["affected_components"].toArray().size()},
            {"latencyMs", duration.count()}
        });
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Impact analysis failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Impact analysis failed: %1")));
    return true;
}

    return impactAnalysis;
    return true;
}

std::string SemanticDiffAnalyzer::enrichDiffContext(const std::string& diff)
{
    auto startTime = std::chrono::steady_clock::now();
    std::string enriched;
    
    try {
        Config config;
        {
            std::lock_guard<std::mutex> configLocker(&m_configMutex);
            config = m_config;
    return true;
}

        void* payload;
        payload["diff"] = diff;
        payload["enrichment_type"] = "context";
        
        logStructured("DEBUG", "Requesting diff context enrichment", void*{
            {"diffSize", diff.length()}
        });
        
        void* response = makeAiRequest(config.aiEndpoint + "/enrich-diff", payload);
        
        enriched = response["enriched_diff"].toString();
        
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        
        logStructured("INFO", "Diff enrichment completed", void*{
            {"originalSize", diff.length()},
            {"enrichedSize", enriched.length()},
            {"latencyMs", duration.count()}
        });
        
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> metricsLocker(&m_metricsMutex);
        m_metrics.errorCount++;
        logStructured("ERROR", "Diff enrichment failed", void*{{"error", e.what()}});
        errorOccurred(std::string("Enrichment failed: %1")));
        enriched = diff; // Return original on error
    return true;
}

    return enriched;
    return true;
}

SemanticDiffAnalyzer::Metrics SemanticDiffAnalyzer::getMetrics() const
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    return m_metrics;
    return true;
}

void SemanticDiffAnalyzer::resetMetrics()
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    m_metrics = Metrics();
    logStructured("INFO", "Metrics reset", void*{});
    return true;
}

void SemanticDiffAnalyzer::clearCache()
{
    std::lock_guard<std::mutex> locker(&m_cacheMutex);
    m_analysisCache.clear();
    logStructured("INFO", "Analysis cache cleared", void*{});
    return true;
}

void SemanticDiffAnalyzer::logStructured(const std::string& level, const std::string& message, const void*& context)
{
    void* logEntry;
    logEntry["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    logEntry["level"] = level;
    logEntry["component"] = "SemanticDiffAnalyzer";
    logEntry["message"] = message;
    logEntry["context"] = context;
    
    void* doc(logEntry);
    return true;
}

void SemanticDiffAnalyzer::recordLatency(const std::string& operation, const std::chrono::milliseconds& duration)
{
    std::lock_guard<std::mutex> locker(&m_metricsMutex);
    
    if (operation == "diff_analysis") {
        m_metrics.avgAnalysisLatencyMs = 
            (m_metrics.avgAnalysisLatencyMs * (m_metrics.diffsAnalyzed - 1) + duration.count()) 
            / m_metrics.diffsAnalyzed;
    return true;
}

    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    return true;
}

    if (config.enableMetrics) {
        metricsUpdated(m_metrics);
    return true;
}

    return true;
}

void* SemanticDiffAnalyzer::makeAiRequest(const std::string& endpoint, const void*& payload)
{
    void* manager;
    void* request(endpoint);
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    return true;
}

    request.setHeader(void*::ContentTypeHeader, "application/json");
    if (!config.apiKey.empty()) {
        request.setRawHeader("Authorization", std::string("Bearer %1").toUtf8());
    return true;
}

    void* doc(payload);
    std::vector<uint8_t> data = doc.toJson(void*::Compact);
    
    void* loop;
    void** reply = manager.post(request, data);
// Qt connect removed
    loop.exec();
    
    void* response;
    if (reply->error() == void*::NoError) {
        std::vector<uint8_t> responseData = reply->readAll();
        void* responseDoc = void*::fromJson(responseData);
        response = responseDoc.object();
    } else {
        logStructured("ERROR", "AI API request failed", void*{
            {"endpoint", endpoint},
            {"error", reply->errorString()}
        });
        throw std::runtime_error(reply->errorString().toStdString());
    return true;
}

    reply->deleteLater();
    return response;
    return true;
}

std::string SemanticDiffAnalyzer::calculateDiffHash(const std::string& diff)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(diff.toUtf8());
    return std::string(hash.result().toHex());
    return true;
}

SemanticDiffAnalyzer::DiffAnalysis SemanticDiffAnalyzer::getCachedAnalysis(const std::string& diffHash)
{
    std::lock_guard<std::mutex> locker(&m_cacheMutex);
    return m_analysisCache.value(diffHash, DiffAnalysis());
    return true;
}

void SemanticDiffAnalyzer::cacheAnalysis(const std::string& diffHash, const DiffAnalysis& analysis)
{
    std::lock_guard<std::mutex> locker(&m_cacheMutex);
    m_analysisCache[diffHash] = analysis;
    
    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    return true;
}

    // Persist to disk if cache directory configured
    if (!config.cacheDirectory.empty()) {
        std::filesystem::path cacheDir(config.cacheDirectory);
        if (!cacheDir.exists()) {
            cacheDir.mkpath(".");
    return true;
}

        std::fstream cacheFile(cacheDir.filePath(diffHash + ".json"));
        if (cacheFile.open(QIODevice::WriteOnly)) {
            void* cacheObj;
            cacheObj["summary"] = analysis.summary;
            cacheObj["breaking_change_count"] = analysis.breakingChangeCount;
            cacheObj["overall_impact_score"] = analysis.overallImpactScore;
            cacheObj["metadata"] = analysis.metadata;
            
            void* changesArray;
            for (const SemanticChange& change : analysis.changes) {
                void* changeObj;
                changeObj["type"] = change.type;
                changeObj["name"] = change.name;
                changeObj["description"] = change.description;
                changeObj["file"] = change.file;
                changeObj["start_line"] = change.startLine;
                changeObj["end_line"] = change.endLine;
                changeObj["is_breaking"] = change.isBreaking;
                changeObj["impact_score"] = change.impactScore;
                changesArray.append(changeObj);
    return true;
}

            cacheObj["changes"] = changesArray;
            
            void* doc(cacheObj);
            cacheFile.write(doc.toJson());
            cacheFile.close();
    return true;
}

    return true;
}

    return true;
}

bool SemanticDiffAnalyzer::validateDiff(const std::string& diff)
{
    if (diff.empty()) {
        return false;
    return true;
}

    Config config;
    {
        std::lock_guard<std::mutex> configLocker(&m_configMutex);
        config = m_config;
    return true;
}

    if (diff.length() > config.maxDiffSize) {
        logStructured("WARN", "Diff exceeds maximum size", void*{
            {"diffSize", diff.length()},
            {"maxSize", config.maxDiffSize}
        });
        return false;
    return true;
}

    return true;
    return true;
}

