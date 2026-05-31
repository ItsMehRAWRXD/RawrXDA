#pragma once

#include <filesystem>
#include <string>
#include <cstdint>
#include <fstream>

// Compatibility shim for legacy tool targets that include
// src/inference/InferenceEngine.hpp and expect this simple API.
class InferenceEngine {
public:
    // Accepts a null or typed config pointer; ignored in this shim.
    explicit InferenceEngine(void* /*config*/) {}
public:
    // Alias expected by test targets that call Initialize instead of loadModel.
    bool Initialize(const std::string& modelPath) { return loadModel(modelPath); }
    int GetVocabSize() const { return m_vocabSize; }
    int GetEmbeddingDim() const { return m_embeddingDim; }

    bool loadModel(const std::string& modelPath) {
        m_modelPath = modelPath;
        if (m_modelPath.empty()) {
            return false;
        }

        std::error_code ec;
        if (!std::filesystem::exists(std::filesystem::path(m_modelPath), ec)) {
            return false;
        }

        // Attempt to read real dimensions from GGUF metadata
        loadModelMetadata(m_modelPath);
        return true;
    }

private:
    // Minimal GGUF header parser to extract vocab_size and embedding_dim
    // from the model file itself, falling back to sensible defaults.
    void loadModelMetadata(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) return;

        // GGUF magic: 0x46554747 ('GGUF') at offset 0
        uint32_t magic = 0;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic != 0x46554747) return; // Not a GGUF file

        // Version (uint32)
        uint32_t version = 0;
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != 2 && version != 3) return; // Unsupported version

        // tensor_count (uint64), metadata_kv_count (uint64)
        uint64_t tensorCount = 0, metaCount = 0;
        file.read(reinterpret_cast<char*>(&tensorCount), sizeof(tensorCount));
        file.read(reinterpret_cast<char*>(&metaCount), sizeof(metaCount));

        // Scan metadata key-value pairs for vocab_size and embedding_dim
        for (uint64_t i = 0; i < metaCount && file; ++i) {
            uint64_t keyLen = 0;
            file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
            if (!file) break;

            std::string key(keyLen, '\0');
            file.read(key.data(), static_cast<std::streamsize>(keyLen));
            if (!file) break;

            // Value type (uint32)
            uint32_t valType = 0;
            file.read(reinterpret_cast<char*>(&valType), sizeof(valType));
            if (!file) break;

            // We only care about uint32 / uint64 scalar values for these keys
            if (key == "tokenizer.ggml.tokens" || key == "vocab_size") {
                if (valType == 4) { // uint32
                    uint32_t v = 0;
                    file.read(reinterpret_cast<char*>(&v), sizeof(v));
                    m_vocabSize = static_cast<int>(v);
                } else if (valType == 5) { // uint64
                    uint64_t v = 0;
                    file.read(reinterpret_cast<char*>(&v), sizeof(v));
                    m_vocabSize = static_cast<int>(v);
                } else {
                    skipValue(file, valType);
                }
            } else if (key == "llama.embedding_length" || key == "embedding_dim" || key == "n_embd") {
                if (valType == 4) { // uint32
                    uint32_t v = 0;
                    file.read(reinterpret_cast<char*>(&v), sizeof(v));
                    m_embeddingDim = static_cast<int>(v);
                } else if (valType == 5) { // uint64
                    uint64_t v = 0;
                    file.read(reinterpret_cast<char*>(&v), sizeof(v));
                    m_embeddingDim = static_cast<int>(v);
                } else {
                    skipValue(file, valType);
                }
            } else {
                skipValue(file, valType);
            }
        }
    }

    void skipValue(std::ifstream& file, uint32_t valType) {
        // GGUF value types: 0=uint8, 1=int8, 2=uint16, 3=int16, 4=uint32, 5=uint64,
        // 6=int32, 7=int64, 8=float32, 9=float64, 10=bool, 11=string, 12=array
        switch (valType) {
            case 0: case 1: file.seekg(1, std::ios::cur); break;
            case 2: case 3: file.seekg(2, std::ios::cur); break;
            case 4: case 6: case 10: file.seekg(4, std::ios::cur); break;
            case 5: case 7: case 9: file.seekg(8, std::ios::cur); break;
            case 8: file.seekg(4, std::ios::cur); break;
            case 11: {
                uint64_t len = 0;
                file.read(reinterpret_cast<char*>(&len), sizeof(len));
                file.seekg(static_cast<std::streamoff>(len), std::ios::cur);
                break;
            }
            case 12: {
                uint32_t arrType = 0;
                uint64_t arrLen = 0;
                file.read(reinterpret_cast<char*>(&arrType), sizeof(arrType));
                file.read(reinterpret_cast<char*>(&arrLen), sizeof(arrLen));
                for (uint64_t j = 0; j < arrLen; ++j) skipValue(file, arrType);
                break;
            }
            default: break;
        }
    }

    std::string m_modelPath;
    int m_vocabSize = 50257;      // GPT-2 default
    int m_embeddingDim = 768;     // GPT-2 small default
};

    std::string generate(const std::string& prompt, int maxTokens) {
        if (m_modelPath.empty() || maxTokens <= 0) {
            return {};
        }

        // Deterministic local text expansion for CLI tool smoke/bench flows.
        const std::string seed = prompt.empty() ? "RawrXD" : prompt;
        std::string output;
        output.reserve(static_cast<size_t>(maxTokens) * 8);

        for (int i = 0; i < maxTokens; ++i) {
            output += seed;
            if (i + 1 < maxTokens) {
                output.push_back(' ');
            }
        }
        return output;
    }

    void unloadModel() {
        m_modelPath.clear();
    }

private:
    std::string m_modelPath;
};
