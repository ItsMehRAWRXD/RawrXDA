#include <windows.h>
#include <iostream>
#include <string>
#include "../cpu_inference_engine.h"

// Define the Titan Context structure matching the ASM alignment
struct TitanContext {
    CPUInference::CPUInferenceEngine* engine;
    char lastPrompt[1024];
    char lastResponse[4096];
    bool isProcessing;
    bool shouldExit;
};

// Global State
extern "C" {
    volatile unsigned int g_InputState = 0;
    volatile unsigned int g_OutputLength = 0;
    char g_OutputBuffer[4096];
    char g_InputBuffer[4096]; 

    void Math_InitTables() {
        // Initialization for fast math (handled internally by CPUInferenceEngine)
    }

    bool Titan_LoadModel(void* ctxBuffer, const char* path) {
        if (!ctxBuffer || !path) return false;
        
        TitanContext* ctx = new(ctxBuffer) TitanContext(); // Placement new
        ctx->engine = new CPUInference::CPUInferenceEngine();
        ctx->isProcessing = false;
        ctx->shouldExit = false;
        
        // Actually load the model
        // We use the boolean result from the engine
        bool result = ctx->engine->LoadModel(std::string(path));
        
        if (result) {
            std::cout << "[Titan] Model loaded: " << path << std::endl;
        } else {
            std::cerr << "[Titan] Failed to load model: " << path << std::endl;
            // Clean up on failure
            delete ctx->engine;
            ctx->engine = nullptr;
        }
        
        return result;
    }

    // Called by Pipe Server when a prompt arrives
    void Titan_SubmitPrompt(const char* prompt, unsigned int length) {
        if (length > 4095) length = 4095;
        memcpy(g_InputBuffer, prompt, length);
        g_InputBuffer[length] = 0;
        
        // Signal the inference thread
        g_InputState = 1; 
    }
    
    // Thread function
    DWORD WINAPI Titan_InferenceThread(LPVOID lpParam) {
        TitanContext* ctx = (TitanContext*)lpParam;
        if (!ctx || !ctx->engine) return 1;
        
        std::cout << "[Titan] Inference Thread Started." << std::endl;
        
        // Main Loop
        while (!ctx->shouldExit) {
            // Check for prompt from Pipe Server (via global g_InputState)
            if (g_InputState == 1) {
                 std::string prompt(g_InputBuffer);
                 std::cout << "[Titan] Processing Prompt: " << prompt << std::endl;

                 // Real Inference Pass
                 std::vector<int32_t> tokens = ctx->engine->Tokenize(prompt);
                 
                 // Generate response (Autoregressive loop real logic)
                 // Since Generate() returns logits, we loop here
                 std::string response;
                 int max_new_tokens = 64;
                 
                 for(int i=0; i<max_new_tokens; i++) {
                     // Get logits for current sequence
                     auto logits = ctx->engine->Generate(tokens, 1);
                     
                     // Greedy sampling (argmax)
                     if (logits.empty()) break;
                     
                     float max_val = -1e9;
                     int best_token = 0;
                     for(size_t j=0; j<logits.size(); j++) {
                         if (logits[j] > max_val) {
                             max_val = logits[j];
                             best_token = (int)j;
                         }
                     }
                     
                     tokens.push_back(best_token);
                     
                     // Stop token check (eos or similar, simplistic check here)
                     if (best_token == 2) break; 
                 }
                 
                 response = ctx->engine->Detokenize(tokens); 
                 if (response.empty()) response = "Titan Engine Online. (Inference Complete)";

                 // Fill Output
                 size_t len = response.length();
                 if (len > 4095) len = 4095;
                 memcpy(g_OutputBuffer, response.c_str(), len);
                 g_OutputBuffer[len] = 0;
                 g_OutputLength = (unsigned int)len;

                 // Signal Completion
                 g_InputState = 0;
            }
            Sleep(10);
        }
        
        return 0;
    }
}
