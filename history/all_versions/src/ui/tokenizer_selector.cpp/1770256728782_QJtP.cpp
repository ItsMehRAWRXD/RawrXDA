#include "tokenizer_selector.h"
#include <iostream>
#include <string>

// Console-based implementation (No Qt)

TokenizerSelector::TokenizerSelector(void* parent) {
    (void)parent;
} // No base class init

TokenizerSelector::~TokenizerSelector() {}

TokenizerConfig TokenizerSelector::getSelectedConfig() const {
    // Return a default config or the one selected via CLI
    TokenizerConfig config;
    config.model_path = "models/default.gguf"; // Example default
    return config;
}

bool TokenizerSelector::validateConfig() const {
    return true; 
}

void TokenizerSelector::setupUI() {
    std::cout << "[TokenizerSelector] Setup CLI (No GUI)\n";
}

void TokenizerSelector::updateTokenizerOptions() {
    std::cout << "[TokenizerSelector] Updating options...\n";
}

void TokenizerSelector::updatePreview() {
    // No preview
}

void TokenizerSelector::initializeTokenizerMap() {
    // Init map
}
