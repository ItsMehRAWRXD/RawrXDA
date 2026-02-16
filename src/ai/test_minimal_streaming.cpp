/**
 * Minimal standalone test for StreamingGGUFLoader (C++20, Qt-free).
 * Uses RawrXD::StreamingGGUFLoader from streaming_gguf_loader.h/cpp.
 */

#include "../streaming_gguf_loader.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "=== Minimal StreamingGGUFLoader Test (C++20) ===" << std::endl;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <gguf-file>" << std::endl;
        std::cerr << "Example: " << argv[0] << " phi-3-mini.gguf" << std::endl;
        return 1;
    }

    std::string modelPath = argv[1];
    std::cout << "Loading: " << modelPath << std::endl;

    RawrXD::StreamingGGUFLoader loader;

    if (!loader.Open(modelPath)) {
        std::cerr << "FAILED to open GGUF file." << std::endl;
        return 1;
    }

    auto meta = loader.GetMetadata();
    auto tensors = loader.GetAllTensorInfo();
    uint64_t fileSize = loader.GetTotalFileSize();
    uint64_t memUsed = loader.GetCurrentMemoryUsage();

    std::cout << "\nSUCCESS!" << std::endl;
    auto it = meta.kv_pairs.find("general.name");
    std::cout << "Model: " << (it != meta.kv_pairs.end() ? it->second : "(n/a)") << std::endl;
    it = meta.kv_pairs.find("general.architecture");
    std::cout << "Architecture: " << (it != meta.kv_pairs.end() ? it->second : std::to_string(meta.architecture_type)) << std::endl;
    std::cout << "Context length: " << meta.context_length << std::endl;
    std::cout << "Tensors: " << tensors.size() << std::endl;
    std::cout << "Metadata entries: " << meta.kv_pairs.size() << std::endl;
    std::cout << "File size: " << (fileSize / 1024 / 1024) << " MB" << std::endl;
    std::cout << "Current memory usage: " << (memUsed / 1024) << " KB" << std::endl;

    loader.Close();
    return 0;
}
