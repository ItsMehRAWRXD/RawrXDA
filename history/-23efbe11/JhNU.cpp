#include "RawrXD_WorkspaceIndexer.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

namespace RawrXD {

WorkspaceIndexer::WorkspaceIndexer(ModelInvoker& invoker)
    : m_modelInvoker(invoker)
    , m_embeddingModel("nomic-embed-text")
{
}

WorkspaceIndexer::~WorkspaceIndexer() = default;

void WorkspaceIndexer::SetEmbeddingModel(const std::string& model) {
    m_embeddingModel = model;
}

void WorkspaceIndexer::IndexWorkspace(const std::string& workspacePath) {
    if (m_isIndexing) return;

    m_workspacePath = workspacePath;
    m_isIndexing = true;

    AgentKernel::Instance().RunAsyncTask([this, workspacePath]() {
        std::vector<std::string> files;
        
        // Win32 directory walk
        std::string searchPath = workspacePath + "\\*";
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
        
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    std::string fullPath = workspacePath + "\\" + findData.c_strFileName;
                    if (ShouldIndexFile(fullPath)) {
                        files.push_back(fullPath);
                    }
                }
            } while (FindNextFileA(hFind, &findData));
            FindClose(hFind);
        }

        for (const auto& file : files) {
            IndexFile(file);
        }

        m_isIndexing = false;
    });
}

void WorkspaceIndexer::IndexFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) return;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto chunks = ChunkCode(filePath, content);
    for (auto& chunk : chunks) {
        chunk.embedding = m_modelInvoker.GenerateEmbedding(m_embeddingModel, chunk.content);
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fileChunks[filePath] = chunks;
        
        m_allChunks.clear();
        for (auto& entry : m_fileChunks) {
            for (auto& chunk : entry.second) {
                m_allChunks.push_back(&chunk);
            }
        }
    }
}

void WorkspaceIndexer::RemoveFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fileChunks.erase(filePath);
    
    m_allChunks.clear();
    for (auto& entry : m_fileChunks) {
        for (auto& chunk : entry.second) {
            m_allChunks.push_back(&chunk);
        }
    }
}

std::vector<SearchResult> WorkspaceIndexer::Search(const std::string& query, int topK) {
    auto queryEmbedding = m_modelInvoker.GenerateEmbedding(m_embeddingModel, query);
    if (queryEmbedding.empty()) return {};

    std::vector<SearchResult> results;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto* chunk : m_allChunks) {
            float sim = CosineSimilarity(queryEmbedding, chunk->embedding);
            if (sim > 0.3f) {
                SearchResult res;
                res.chunk = *chunk;
                res.similarity = sim;
                res.preview = chunk->content.substr(0, 200);
                results.push_back(res);
            }
        }
    }

    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b) {
        return a.similarity > b.similarity;
    });

    if (results.size() > (size_t)topK) {
        results.resize(topK);
    }

    return results;
}

std::string WorkspaceIndexer::GetRelevantContext(const std::string& query, int maxTokens) {
    auto results = Search(query, 10);
    std::string context;
    for (const auto& res : results) {
        context += "File: " + res.chunk.filePath + "\n";
        context += res.chunk.content + "\n\n";
    }
    return context;
}

std::vector<CodeChunk> WorkspaceIndexer::ChunkCode(const std::string& filePath, const std::string& content) {
    // Basic chunking by line count for now
    std::vector<CodeChunk> chunks;
    std::stringstream ss(content);
    std::string line;
    std::string currentChunk;
    int lineCount = 0;
    int startLine = 1;

    while (std::getline(ss, line)) {
        currentChunk += line + "\n";
        lineCount++;
        if (lineCount >= 50) {
            CodeChunk c;
            c.filePath = filePath;
            c.content = currentChunk;
            c.startLine = startLine;
            c.endLine = startLine + lineCount;
            chunks.push_back(c);
            
            currentChunk = "";
            startLine += lineCount;
            lineCount = 0;
        }
    }
    
    if (!currentChunk.empty()) {
        CodeChunk c;
        c.filePath = filePath;
        c.content = currentChunk;
        c.startLine = startLine;
        c.endLine = startLine + lineCount;
        chunks.push_back(c);
    }

    return chunks;
}

float WorkspaceIndexer::CosineSimilarity(const std::vector<float>& v1, const std::vector<float>& v2) {
    if (v1.size() != v2.size() || v1.empty()) return 0.0f;
    double dot = 0, n1 = 0, n2 = 0;
    for (size_t i = 0; i < v1.size(); i++) {
        dot += v1[i] * v2[i];
        n1 += v1[i] * v1[i];
        n2 += v2[i] * v2[i];
    }
    return (float)(dot / (sqrt(n1) * sqrt(n2)));
}

bool WorkspaceIndexer::ShouldIndexFile(const std::string& filePath) {
    const char* ext = PathFindExtensionA(filePath.c_str());
    if (!ext) return false;
    std::string s_ext = ext;
    return s_ext == ".cpp" || s_ext == ".hpp" || s_ext == ".h" || s_ext == ".c" || s_ext == ".asm" || s_ext == ".txt" || s_ext == ".md";
}

int WorkspaceIndexer::GetTotalChunks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (int)m_allChunks.size();
}

} // namespace RawrXD
