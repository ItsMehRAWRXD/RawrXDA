#include "RepoMemory.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTextStream>
#include <cmath>
#include <numeric>

namespace mem {

RepoMemory::RepoMemory() = default;

RepoMemory::~RepoMemory() {
    // Cleanup ggml context if needed
}

bool RepoMemory::load(const QString& ggufPath) {
    // Build side-car path
    QFileInfo info(ggufPath);
    QString sidecarPath = info.absolutePath() + "/" + 
                         info.baseName() + ".memory.gguf";

    if (!QFile::exists(sidecarPath)) {
        qDebug("Side-car not found at %s, starting fresh", sidecarPath.toStdString().c_str());
        return false;
    }

    return loadFromSidecar(sidecarPath);
}

bool RepoMemory::isLoaded() const {
    return meta != nullptr || !embeddings.empty();
}

std::vector<RepoMemory::CodeChunk> RepoMemory::retrieve(const QString& query, int topK) const {
    if (embeddings.empty()) {
        return {};
    }

    // Generate query embedding
    auto queryVec = embed(query);
    if (queryVec.empty()) {
        return {};
    }

    // Compute similarities
    std::vector<std::pair<CodeChunk, double>> results;
    for (const auto& [filePath, fileEmbedding] : embeddings) {
        double sim = cosineSimilarity(queryVec, fileEmbedding);
        results.push_back({
            CodeChunk{.filePath = QString::fromStdString(filePath), .similarity = sim},
            sim
        });
    }

    // Sort by similarity descending
    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Extract topK
    std::vector<CodeChunk> topResults;
    for (int i = 0; i < std::min(topK, static_cast<int>(results.size())); ++i) {
        topResults.push_back(results[i].first);
    }

    return topResults;
}

std::vector<RepoMemory::CodeChunk> RepoMemory::retrieveFromFile(const QString& filePath, 
                                                               int topK) const {
    std::vector<CodeChunk> results;
    
    // In a real implementation, this would query chunks stored for specific file
    auto it = embeddings.find(filePath.toStdString());
    if (it != embeddings.end()) {
        results.push_back(CodeChunk{.filePath = filePath});
    }

    return results;
}

bool RepoMemory::indexFile(const QString& filePath, const QString& content) {
    // Generate embedding for file
    auto embedding = embed(content);
    if (embedding.empty()) {
        return false;
    }

    embeddings[filePath.toStdString()] = embedding;
    summaries[filePath.toStdString()] = generateSummary(content);
    dirty = true;
    return true;
}

bool RepoMemory::saveIndex() {
    if (!dirty) return true;

    // Find a suitable save location
    // In production, would be: models/<model>.memory.gguf
    QString savePath = QDir::temp().filePath("repo.memory.gguf");
    bool ok = saveToSidecar(savePath);
    if (ok) {
        dirty = false;
    }
    return ok;
}

QString RepoMemory::getFileSummary(const QString& filePath, int maxLen) const {
    auto it = summaries.find(filePath.toStdString());
    if (it != summaries.end()) {
        QString summary = it->second;
        if (summary.length() > maxLen) {
            summary.truncate(maxLen);
            summary.append("...");
        }
        return summary;
    }
    return {};
}

QString RepoMemory::getFolderSummary(const QString& folderPath, int maxLen) const {
    QString summary;
    int count = 0;

    // Summarize all files in folder
    for (const auto& [filepath, summaryText] : summaries) {
        QString fp = QString::fromStdString(filepath);
        if (fp.startsWith(folderPath)) {
            summary += fp + ": " + summaryText + "\n";
            count++;
            if (summary.length() > maxLen) break;
        }
    }

    return summary;
}

int RepoMemory::rebuildIndex(const QString& workspacePath, const QString& filePattern) {
    int indexed = 0;

    QDirIterator it(workspacePath, filePattern.split(";"), QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QFile file(filePath);
        
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream stream(&file);
        QString content = stream.readAll();
        file.close();

        if (indexFile(filePath, content)) {
            indexed++;
        }
    }

    saveIndex();
    return indexed;
}

void RepoMemory::clear() {
    embeddings.clear();
    summaries.clear();
    dirty = false;
}

std::unordered_map<std::string, int64_t> RepoMemory::getMemoryStats() const {
    std::unordered_map<std::string, int64_t> stats;
    stats["embedding_count"] = embeddings.size();
    stats["summary_count"] = summaries.size();
    
    // Rough byte estimation: 256 floats × 4 bytes per embedding
    stats["embeddings_bytes"] = embeddings.size() * 256 * 4;
    
    return stats;
}

std::vector<float> RepoMemory::embed(const QString& text) const {
    // Simplified embedding: hash-based to 256-d
    // In production: would use actual embedding model (GGML or external)
    
    std::vector<float> vec(256, 0.0f);
    
    QByteArray bytes = text.toUtf8();
    for (int i = 0; i < bytes.size(); ++i) {
        uint8_t byte = bytes[i];
        int idx = (byte * i) % 256;
        vec[idx] += static_cast<float>(byte) / 255.0f;
    }

    // Normalize
    float norm = 0.0f;
    for (float v : vec) {
        norm += v * v;
    }
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (auto& v : vec) {
            v /= norm;
        }
    }

    return vec;
}

double RepoMemory::cosineSimilarity(const std::vector<float>& a,
                                   const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) {
        return 0.0;
    }

    double dotProduct = 0.0;
    double normA = 0.0;
    double normB = 0.0;

    for (size_t i = 0; i < a.size(); ++i) {
        dotProduct += a[i] * b[i];
        normA += a[i] * a[i];
        normB += b[i] * b[i];
    }

    normA = std::sqrt(normA);
    normB = std::sqrt(normB);

    if (normA > 0.0 && normB > 0.0) {
        return dotProduct / (normA * normB);
    }

    return 0.0;
}

QString RepoMemory::generateSummary(const QString& content) {
    // Very simple summarization: first 200 chars
    if (content.length() > 200) {
        return content.left(200) + "...";
    }
    return content;
}

bool RepoMemory::loadFromSidecar(const QString& sidecarPath) {
    // TODO: Load ggml tensors from GGUF file
    // For now: placeholder
    qDebug("Would load from: %s", sidecarPath.toStdString().c_str());
    return true;
}

bool RepoMemory::saveToSidecar(const QString& sidecarPath) {
    // TODO: Save ggml tensors to GGUF file
    // For now: placeholder
    qDebug("Would save to: %s", sidecarPath.toStdString().c_str());
    return true;
}

} // namespace mem
