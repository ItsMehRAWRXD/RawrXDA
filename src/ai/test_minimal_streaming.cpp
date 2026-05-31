/**
 * Minimal standalone test for StreamingGGUFLoader (C++20, Qt-free).
 */

#include "../streaming_gguf_loader.h"
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return 1;
    }

    std::string modelPath = argv[1];

    RawrXD::StreamingGGUFLoader loader;

    if (!loader.Open(modelPath)) {
        return 1;
    }

    auto meta = loader.GetMetadata();
    auto tensors = loader.GetAllTensorInfo();
    uint64_t fileSize = loader.GetTotalFileSize();
    uint64_t memUsed = loader.GetCurrentMemoryUsage();
    (void)meta;
    (void)tensors;
    (void)fileSize;
    (void)memUsed;

    loader.Close();
    return 0;
}
