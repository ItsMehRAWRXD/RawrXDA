/**
 * Test program for StreamingGGUFLoader (C++20, Qt-free).
 */

#include "../streaming_gguf_loader.h"
#include <string>
#include <chrono>

static void printMemoryStats(RawrXD::StreamingGGUFLoader& loader) {
    uint64_t totalFile = loader.GetTotalFileSize();
    auto tensors = loader.GetAllTensorInfo();
    uint64_t memUsed = loader.GetCurrentMemoryUsage();
    (void)totalFile;
    (void)tensors;
    (void)memUsed;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return 1;
    }

    std::string modelPath = argv[1];

    RawrXD::StreamingGGUFLoader loader;

    auto startTime = std::chrono::high_resolution_clock::now();
    bool success = loader.Open(modelPath);
    auto endTime = std::chrono::high_resolution_clock::now();
    auto loadTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    (void)loadTime;

    if (!success) {
        return 1;
    }

    auto meta = loader.GetMetadata();
    auto tensors = loader.GetAllTensorInfo();
    (void)meta;
    (void)tensors;

    printMemoryStats(loader);

    loader.Close();
    return 0;
}
