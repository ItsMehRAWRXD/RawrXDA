/**
 * @file RawrXD_WorkspaceIndexer.hpp
 * @brief Zero-Qt high-performance Win32 Workspace Indexer
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <memory>
#include <windows.h>
#include "RawrXD_AgentKernel.hpp"
#include "RawrXD_ModelInvoker.hpp"

namespace RawrXD::Agent {

struct CodeChunk {
    std::string filePath;
    int startLine{0};
    int endLine{0};
    std::string content;
    std::vector<float> embedding;
    std::string language;
    std::string symbol;
};

struct SearchResult {
    CodeChunk chunk;
    float similarity{0.0f};
    std::string preview;
};

class WorkspaceIndexer {
public:
    explicit WorkspaceIndexer(ModelInvoker& invoker);
    ~WorkspaceIndexer();

    void SetEmbeddingModel(const std::string& model);
    void IndexWorkspace(const std::string& workspacePath);
    void IndexFile(const std::string& filePath);
    void RemoveFile(const std::string& filePath);

    std::vector<SearchResult> Search(const std::string& query, int topK = 10);
    std::string GetRelevantContext(const std::string& query, int maxTokens = 2000);

    bool IsIndexing() const { return m_isIndexing; }
    int GetTotalChunks() const;

private:
    std::vector<CodeChunk> ChunkCode(const std::string& filePath, const std::string& content);
    std::vector<float> GenerateEmbedding(const std::string& text);
    float CosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2);
    bool ShouldIndexFile(const std::string& filePath);

    ModelInvoker& m_modelInvoker;
    std::string m_embeddingModel;
    std::string m_workspacePath;
    
    std::map<std::string, std::vector<CodeChunk>> m_fileChunks;
    std::vector<CodeChunk*> m_allChunks;
    
    mutable std::mutex m_mutex;
    std::atomic<bool> m_isIndexing{false};
};

} // namespace RawrXD
