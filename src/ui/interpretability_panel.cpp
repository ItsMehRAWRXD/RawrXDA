#include "interpretability_panel.h"
#include <QWidget>
#include <QString>
#include <QJsonObject>

InterpretabilityPanel::InterpretabilityPanel(QWidget* parent) : QWidget(parent) {}
InterpretabilityPanel::~InterpretabilityPanel() {}

void InterpretabilityPanel::updateVisualization(VisualizationType type, const QJsonObject& data) {
    // Stub implementation
}

void InterpretabilityPanel::setLayerRange(int minLayer, int maxLayer) {
    // Stub implementation
}

void InterpretabilityPanel::setAttentionHeads(const QStringList& heads) {
    // Stub implementation
}

QJsonObject InterpretabilityPanel::getCurrentVisualization() const {
    return QJsonObject(); // Stub implementation
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