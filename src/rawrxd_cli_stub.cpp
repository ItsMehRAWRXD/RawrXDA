#include "cpu_inference_engine.h"
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::string mode = "local";
    std::string model = "rawrxd-7b-v2.gguf";
    std::string chat = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mode" && i + 1 < argc) mode = argv[++i];
        if (arg == "--model" && i + 1 < argc) model = argv[++i];
        if (arg == "--chat" && i + 1 < argc) chat = argv[++i];
    }

    std::cout << "[TITAN] " << (mode == "local" ? "Local" : "Distributed") << " Mode Init" << std::endl;
    
    auto engine = RawrXD::CPUInferenceEngine::getInstance();
    if (engine->LoadModel(model)) {
        std::cout << "[TITAN] Model loaded: 7B params" << std::endl;
        if (!chat.empty()) {
            std::cout << "[TITAN] Generate: \"" << chat << "...\"" << std::endl;
            std::vector<int32_t> tokens = engine->Tokenize(chat);
            std::cout << engine->Detokenize(tokens) << std::endl;
        }
    }
    return 0;
}
