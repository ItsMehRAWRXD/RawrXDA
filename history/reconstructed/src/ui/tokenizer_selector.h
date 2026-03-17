#pragma once
#include <string>

struct TokenizerConfig {
    std::string model_path;
};

class TokenizerSelector {
public:
    TokenizerSelector(void* parent = nullptr);
    ~TokenizerSelector();

    TokenizerConfig getSelectedConfig() const;
    bool validateConfig() const;
    void setupUI();
    void updateTokenizerOptions();
    void updatePreview();
    void initializeTokenizerMap();
};
