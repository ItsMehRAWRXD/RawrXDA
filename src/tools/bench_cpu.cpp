#include "cpu_inference_engine.h"

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace RawrXD;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: bench_cpu <model_path>" << std::endl;
        return 1;
    }

    const std::string modelPath = argv[1];
    CPUInferenceEngine* engine = CPUInferenceEngine::getInstance();

    std::cout << "Loading model: " << modelPath << std::endl;
    const auto startLoad = std::chrono::high_resolution_clock::now();
    const bool loaded = engine->LoadModel(modelPath);
    const auto endLoad = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> loadTime = endLoad - startLoad;

    if (!loaded) {
        std::cout << "Failed to load model" << std::endl;
        return 1;
    }

    std::cout << "Model loaded in " << loadTime.count() << " seconds" << std::endl;

    const std::string testText = "Hello world";
    std::cout << "Tokenizing: " << testText << std::endl;
    const auto tokens = engine->Tokenize(testText);
    std::cout << "Tokens: ";
    for (const auto token : tokens) {
        std::cout << token << ' ';
    }
    std::cout << std::endl;

    std::cout << "Generating text..." << std::endl;
    const auto startGen = std::chrono::high_resolution_clock::now();
    const auto generatedTokens = engine->Generate(tokens, 50);
    const auto endGen = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> genTime = endGen - startGen;

    const std::string generatedText = engine->Detokenize(generatedTokens);
    std::cout << "Generated: " << generatedText << std::endl;
    std::cout << "Generation time: " << genTime.count() << " seconds" << std::endl;
    std::cout << "Tokens per second: " << generatedTokens.size() / genTime.count() << std::endl;

    return 0;
}
