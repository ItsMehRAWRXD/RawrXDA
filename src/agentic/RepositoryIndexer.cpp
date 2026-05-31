/*
 * RepositoryIndexer.cpp
 * Scans repository files and feeds semantic vectors into RawrXDVectorIndex
 * for agent context awareness.
 */

#include <filesystem>
#include <fstream>
#include <string>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <windows.h>
#include "runtime/RawrXDVectorIndex.h"

namespace fs = std::filesystem;

namespace RawrXD::Agentic {

class RepositoryIndexer {
public:
    static RepositoryIndexer& instance() {
        static RepositoryIndexer indexer;
        return indexer;
    }

    // Main entry point: scan repo and populate vector index
    size_t indexRepository(const std::string& rootPath, size_t maxFiles = 1000) {
        std::cout << "[RepositoryIndexer] Starting scan of " << rootPath << " (max " << maxFiles << " files)\n";
        
        size_t filesIndexed = 0;
        size_t filesScanned = 0;
        
        try {
            for (auto it = fs::recursive_directory_iterator(rootPath,
                     fs::directory_options::skip_permission_denied);
                 it != fs::recursive_directory_iterator(); ++it) {
                
                if (filesIndexed >= maxFiles) break;
                if (filesScanned > maxFiles * 5) break;  // Safety limit on scans
                
                if (!it->is_regular_file()) continue;
                filesScanned++;
                
                // Skip uninteresting files
                std::string filename = it->path().filename().string();
                if (isIgnoredFile(filename)) continue;
                
                try {
                    auto fsize = it->file_size();
                    if (fsize == 0 || fsize > (10 * 1024 * 1024)) continue;  // Skip 10MB+ files
                    
                    // Read file
                    std::ifstream file(it->path(), std::ios::binary);
                    if (!file.is_open()) continue;
                    
                    std::string content((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
                    file.close();
                    
                    if (isLikelyBinary(content)) continue;
                    
                    // Extract snippets and create vector entries
                    std::string fullPath = it->path().string();
                    std::vector<CodeSnippet> snippets = extractSnippets(content, fullPath);
                    
                    filesIndexed += indexSnippets(snippets);
                    
                } catch (const std::exception& ex) {
                    // Skip files that can't be read
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "[RepositoryIndexer] Error: " << ex.what() << "\n";
        }
        
        std::cout << "[RepositoryIndexer] Indexed " << filesIndexed << " snippets from " 
                  << filesScanned << " files\n";
        return filesIndexed;
    }

private:
    struct CodeSnippet {
        std::string filePath;
        int lineStart;
        int lineEnd;
        std::string content;
        std::string language;
    };

    bool isIgnoredFile(const std::string& filename) {
        static const std::string ignored[] = {
            "node_modules", ".git", ".vscode", "build", "dist", ".dll", ".exe",
            ".obj", ".pdb", ".lib", ".o", ".so", ".a", ".pyc", "__pycache__",
            ".o", ".out", ".class", ".jar"
        };
        
        std::string lower = filename;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        for (const auto& pattern : ignored) {
            if (lower.find(pattern) != std::string::npos) return true;
        }
        
        return false;
    }

    bool isLikelyBinary(const std::string& content) {
        // Simple heuristic: if more than 30% non-printable, likely binary
        int nonPrintable = 0;
        for (unsigned char c : content) {
            if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
                nonPrintable++;
            }
        }
        return (nonPrintable * 100) / (content.size() + 1) > 30;
    }

    std::vector<CodeSnippet> extractSnippets(const std::string& content, const std::string& filepath) {
        std::vector<CodeSnippet> snippets;
        
        // Extract function/class definitions (simplified)
        std::istringstream iss(content);
        std::string line;
        int lineNum = 0;
        std::string buffer;
        int bufferStartLine = 1;
        const int SNIPPET_SIZE = 20;  // Lines per snippet
        
        while (std::getline(iss, line)) {
            lineNum++;
            buffer += line + "\n";
            
            if (buffer.size() > 500 || lineNum % SNIPPET_SIZE == 0) {
                // Create snippet
                if (!buffer.empty() && buffer.length() > 50) {  // Skip tiny snippets
                    CodeSnippet snippet;
                    snippet.filePath = filepath;
                    snippet.lineStart = bufferStartLine;
                    snippet.lineEnd = lineNum;
                    snippet.content = buffer.substr(0, 500);  // Limit to 500 chars
                    snippet.language = inferLanguage(filepath);
                    snippets.push_back(snippet);
                }
                buffer.clear();
                bufferStartLine = lineNum + 1;
            }
        }

        if (!buffer.empty() && buffer.length() > 50) {
            CodeSnippet snippet;
            snippet.filePath = filepath;
            snippet.lineStart = bufferStartLine;
            snippet.lineEnd = lineNum;
            snippet.content = buffer.substr(0, 500);
            snippet.language = inferLanguage(filepath);
            snippets.push_back(snippet);
        }
        
        // Don't index huge files in one go
        if (snippets.size() > 100) {
            snippets.resize(50);  // Subsample large files
        }
        
        return snippets;
    }

    std::string inferLanguage(const std::string& filepath) {
        std::string lower = filepath;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        
        if (lower.find(".cpp") != std::string::npos) return "cpp";
        if (lower.find(".h") != std::string::npos) return "cpp";
        if (lower.find(".c") != std::string::npos) return "c";
        if (lower.find(".py") != std::string::npos) return "python";
        if (lower.find(".js") != std::string::npos) return "javascript";
        if (lower.find(".rs") != std::string::npos) return "rust";
        if (lower.find(".java") != std::string::npos) return "java";
        return "text";
    }

    // Generates deterministic embedding from content hash for similarity search
    std::vector<float> generateEmbeddingPlaceholder(const std::string& content) {
        std::vector<float> embedding(768, 0.0f);
        
        // Simple hash-based pseudo-embedding for now
        // (will be replaced with real embedding model later)
        uint32_t seed = 2166136261u;
        for (char c : content) {
            seed ^= static_cast<unsigned char>(c);
            seed *= 16777619u;
        }
        
        // Distribute hash into 768-dim space
        for (size_t i = 0; i < 768; ++i) {
            seed = seed * 1664525 + 1013904223;  // LCG
            embedding[i] = (static_cast<float>(seed) / 4294967296.0f) * 2.0f - 1.0f;
        }
        
        // Normalize to unit vector
        float norm = 0.0f;
        for (float v : embedding) norm += v * v;
        norm = std::sqrt(norm);
        if (norm > 0.0f) {
            for (auto& v : embedding) v /= norm;
        }
        
        return embedding;
    }

    size_t indexSnippets(const std::vector<CodeSnippet>& snippets) {
        auto& vecIndex = RawrXD::Runtime::RawrXDVectorIndex::instance();
        size_t inserted = 0;

        for (const auto& snippet : snippets) {
            if (snippet.content.size() < 16) {
                continue;
            }

            auto embedding = generateEmbeddingPlaceholder(snippet.content);
            std::string meta = snippet.filePath + ":" +
                               std::to_string(snippet.lineStart) + "-" +
                               std::to_string(snippet.lineEnd) + "|" +
                               snippet.language;

            vecIndex.addVector(embedding, meta, 0);
            ++inserted;
        }

        return inserted;
    }
};

} // namespace RawrXD::Agentic

// Export for C access if needed
extern "C" {
    DWORD __stdcall RepositoryIndexer_ScanRepository(const wchar_t* rootPath, size_t maxFiles) {
        // Convert wide to UTF-8
        int len = WideCharToMultiByte(CP_UTF8, 0, rootPath, -1, nullptr, 0, nullptr, nullptr);
        std::string path(len, 0);
        WideCharToMultiByte(CP_UTF8, 0, rootPath, -1, &path[0], len, nullptr, nullptr);
        path.pop_back();  // Remove null terminator
        
        return static_cast<DWORD>(
            RawrXD::Agentic::RepositoryIndexer::instance().indexRepository(path, maxFiles)
        );
    }
}
