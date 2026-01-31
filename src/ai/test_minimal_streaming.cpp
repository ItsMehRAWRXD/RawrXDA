/**
 * Minimal standalone test for StreamingGGUFLoaderQt
 * Compiles with: cl /EHsc /std:c++20 /I. test_minimal_streaming.cpp memory_mapped_file.cpp streaming_gguf_loader_qt.cpp /Fe:test_streaming.exe
 */

#include "streaming_gguf_loader_qt.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {


    if (argc < 2) {


        return 1;
    }
    
    std::string modelPath = argv[1];


    StreamingGGUFLoaderQt loader;
    
    if (!loader.loadModel(modelPath)) {
        
        return 1;
    }


    auto stats = loader.getMemoryStats();


    return 0;
}
