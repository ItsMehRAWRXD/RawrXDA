#include "interpretability_panel.h"


InterpretabilityPanel::InterpretabilityPanel(void* parent) : void(parent) {}
InterpretabilityPanel::~InterpretabilityPanel() {}

void InterpretabilityPanel::updateVisualization(VisualizationType type, const void*& data) {
    // Stub implementation
}

void InterpretabilityPanel::setLayerRange(int minLayer, int maxLayer) {
    // Stub implementation
}

void InterpretabilityPanel::setAttentionHeads(const std::vector<std::string>& heads) {
    // Stub implementation
}

void* InterpretabilityPanel::getCurrentVisualization() const {
    return void*(); // Stub implementation
}

void InterpretabilityPanel::clearVisualization() {
    // Stub implementation
}

void InterpretabilityPanel::setupUI() {
    // Stub implementation
}

void InterpretabilityPanel::updateChart() {
    // Stub implementation
}

void InterpretabilityPanel::updateStats() {
    // Stub implementation
}

