#include "tokenizer_selector.h"
#include <QString>
#include <QJsonObject>

TokenizerSelector::TokenizerSelector(QWidget* parent) : QDialog(parent) {}
TokenizerSelector::~TokenizerSelector() {}

TokenizerConfig TokenizerSelector::getSelectedConfig() const {
    return TokenizerConfig(); // Stub implementation
}

bool TokenizerSelector::validateConfig() const {
    return false; // Stub implementation
}

void TokenizerSelector::setupUI() {
    // Stub implementation
}

void TokenizerSelector::updateTokenizerOptions() {
    // Stub implementation
}

void TokenizerSelector::updatePreview() {
    // Stub implementation
}

void TokenizerSelector::initializeTokenizerMap() {
    // Stub implementation
}