#include "src/agent/agentic_puppeteer.hpp"
#include "src/ultra_fast_inference.h"
#include <iostream>

int main() {
    AgenticPuppeteer p;
    auto r = p.correctResponse("As of my knowledge cutoff, I think this probably works.", "");
    if (!r.success || r.correctedOutput.find("knowledge cutoff") != std::string::npos) {
        std::cout << "puppeteer_smoke: FAIL\n";
        return 1;
    }

    rawrxd::inference::AutonomousInferenceEngine eng;
    bool loaded = eng.loadModelAutomatic("d:/phi3mini.gguf");
    std::cout << "puppeteer_smoke: PASS\n";
    std::cout << "ultra_fast_load_smoke: " << (loaded ? "PASS" : "FAIL") << "\n";
    return loaded ? 0 : 1;
}
