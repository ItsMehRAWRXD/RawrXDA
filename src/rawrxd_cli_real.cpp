// rawrxd_cli_real.cpp — Real CLI entry point for TITAN inference engine
// This is the standalone test harness for the restored engine.
// Usage: RawrXD_CLI_Real.exe --model path/to/model.gguf --chat "prompt"
#include "cpu_inference_engine.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <iostream>

int main(int argc, char* argv[]) {
    printf("[TITAN] RawrXD CLI — Real Engine\n");
    printf("[TITAN] Engine: %s\n", "CPUInferenceEngine (Restored)");

    std::string model_path;
    std::string prompt;
    int max_tokens = 64;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--model") == 0 && i + 1 < argc) {
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--chat") == 0 && i + 1 < argc) {
            prompt = argv[++i];
        } else if (strcmp(argv[i], "--max-tokens") == 0 && i + 1 < argc) {
            max_tokens = atoi(argv[++i]);
        }
    }

    auto* engine = RawrXD::CPUInferenceEngine::getInstance();
    printf("[TITAN] Singleton: %p\n", (void*)engine);

    if (model_path.empty()) {
        // Dry run - test that the engine initializes without a model
        printf("[TITAN] No --model specified. Engine status:\n");
        printf("  IsModelLoaded: %s\n", engine->IsModelLoaded() ? "yes" : "no");
        printf("  GetEngineName: %s\n", engine->GetEngineName());
        printf("  VocabSize: %d\n", engine->GetVocabSize());
        printf("  EmbeddingDim: %d\n", engine->GetEmbeddingDim());
        printf("  NumLayers: %d\n", engine->GetNumLayers());
        printf("  NumHeads: %d\n", engine->GetNumHeads());
        printf("[TITAN] Dry run complete. Pass --model <path.gguf> to load.\n");
        return 0;
    }

    printf("[TITAN] Loading model: %s\n", model_path.c_str());
    if (!engine->LoadModel(model_path)) {
        fprintf(stderr, "[TITAN] ERROR: Failed to load model: %s\n", model_path.c_str());
        return 1;
    }

    printf("[TITAN] Model loaded. VocabSize=%d EmbDim=%d Layers=%d Heads=%d\n",
           engine->GetVocabSize(), engine->GetEmbeddingDim(),
           engine->GetNumLayers(), engine->GetNumHeads());

    if (!prompt.empty()) {
        printf("[TITAN] Tokenizing: \"%s\"\n", prompt.c_str());
        auto tokens = engine->Tokenize(prompt);
        printf("[TITAN] Token count: %zu\n", tokens.size());
        printf("[TITAN] Tokens: [");
        for (size_t i = 0; i < tokens.size(); i++) {
            if (i > 0) printf(", ");
            printf("%d", tokens[i]);
        }
        printf("]\n");

        printf("[TITAN] Detokenize: \"%s\"\n", engine->Detokenize(tokens).c_str());

        printf("[TITAN] Generating response (max %d tokens)...\n", max_tokens);
        // Use streaming to show tokens as they arrive
        auto tok32 = std::vector<int32_t>(tokens.begin(), tokens.end());
        engine->GenerateStreaming(tok32, max_tokens,
            [](const std::string& piece) { printf("%s", piece.c_str()); fflush(stdout); },
            []() { printf("\n[TITAN] Generation complete.\n"); },
            nullptr
        );
    } else {
        // Interactive REPL
        printf("[TITAN] Interactive mode. Type 'exit' to quit.\n");
        std::string line;
        while (printf(">>> "), std::getline(std::cin, line)) {
            if (line == "exit" || line == "quit") break;
            auto tokens = engine->Tokenize(line);
            auto tok32 = std::vector<int32_t>(tokens.begin(), tokens.end());
            engine->GenerateStreaming(tok32, 128,
                [](const std::string& piece) { printf("%s", piece.c_str()); fflush(stdout); },
                []() { printf("\n"); },
                nullptr
            );
        }
    }

    return 0;
}
