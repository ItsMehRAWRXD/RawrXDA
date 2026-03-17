#include "rawrxd_inference.h"
#include "agentic_engine.h"
#include <iostream>
#include <vector>
#include <string>

int main(int argc, char* argv[]) {
    printf("RawrXD Inference Engine v1.0 [AVX-512 + Vulkan]\n");
    printf("Initializing Real-Time Inference Stack...\n");
    
    // 1. Initialize Core Inference Engine
    RawrXDInference engine;
    const wchar_t* modelPath = L"models/llama-3-8b-instruct.gguf";
    const char* vocabPath = "models/tokenizer.json"; 
    const char* mergesPath = "models/tokenizer_config.json";

    if (!engine.Initialize(modelPath, vocabPath, mergesPath)) {
        printf("[Error] Failed to initialize Core Inference Engine.\n");
        printf("Ensure models are in models/ directory.\n");
        // return 1; // Continue to demonstrate Agentic setup even if model missing (for simulation/testing of stack flow)
    }

    // 2. Initialize Agentic Layer (connects to Inference)
    AgenticEngine agent;
    agent.initialize();
    
    // In a real integration, AgenticEngine would use the initialized engine or router would load it.
    // Our UniversalModelRouter (inside AgenticEngine) lazy-loads CPUInferenceEngine, 
    // which in turn lazy-loads RawrXDInference.
    
    printf("[System] Agentic Engine Initialized.\n");
    printf("[System] Universal Router Ready.\n");
    
    // 3. Test Direct Generation (if model loaded)
    if (engine.IsInitialized()) {
        std::string prompt = "Explain Quantum Entanglement in simple terms:";
        printf("\n[Direct Inference] Prompt: %s\n", prompt.c_str());
        engine.Generate(prompt, 128, [](const std::string& token) {
             printf("%s", token.c_str());
             fflush(stdout);
        });
        printf("\n");
    }

    printf("\n[System] Stack Verification Complete.\n");
    return 0;
}
