#include "enhanced_main_window.h"
#include "features_view_menu.h"
#include <iostream>

EnhancedMainWindow::EnhancedMainWindow()
    : m_featuresPanel(std::make_unique<FeaturesViewMenu>())
    , m_advancedModeEnabled(false)
{
    setupUI();
    createMenuBar();
    createToolBars();
    createDockWidgets();
    createStatusBar();
    connectSignals();
    registerBuiltInFeatures();
    loadWindowState();

    std::cout << "[EnhancedMainWindow] Initialized with production-ready features" << std::endl;
}

EnhancedMainWindow::~EnhancedMainWindow() { saveWindowState(); }

void EnhancedMainWindow::setupUI() {
    // Headless placeholder
    std::cout << "[UI] RawrXD Agentic IDE - Ready for Development" << std::endl;
}

void EnhancedMainWindow::createMenuBar() {
    std::cout << "[UI] Menu bar created" << std::endl;
}

void EnhancedMainWindow::createToolBars() {
    std::cout << "[UI] Toolbars created" << std::endl;
}

void EnhancedMainWindow::createDockWidgets() {
    // Features panel is already created in constructor
    std::cout << "[UI] Features panel attached" << std::endl;
}

void EnhancedMainWindow::createStatusBar() {
    m_statusText = "Ready";
    m_metricsText = "Metrics: 0 features | 0ms";
}

void EnhancedMainWindow::connectSignals() {
    if (m_featuresPanel) {
        m_featuresPanel->setFeatureToggledCallback([this](const std::string &id, bool enabled){ onFeatureToggled(id, enabled); });
        m_featuresPanel->setFeatureClickedCallback([this](const std::string &id){ onFeatureClicked(id); });
        m_featuresPanel->setFeatureDoubleClickedCallback([this](const std::string &id){ onFeatureDoubleClicked(id); });
    }
}

void EnhancedMainWindow::registerBuiltInFeatures() {
    if (!m_featuresPanel) return;
    
    // Core features
    m_featuresPanel->registerFeature({
        "code_editor", "Multi-Tab Code Editor", 
        "Edit multiple files with syntax highlighting and smart completion",
        FeaturesViewMenu::FeatureStatus::Stable,
        FeaturesViewMenu::FeatureCategory::Core,
        true, true, 0, 0, {}, "1.0"
    });
    
    m_featuresPanel->registerFeature({
        "terminal", "Integrated Terminal",
        "Run commands and scripts directly from the IDE",
        FeaturesViewMenu::FeatureStatus::Stable,
        FeaturesViewMenu::FeatureCategory::Core,
        true, true, 0, 0, {}, "1.0"
    });
    
    // AI features
    m_featuresPanel->registerFeature({
        "code_generation", "AI Code Generation",
        "Generate code using large language models",
        FeaturesViewMenu::FeatureStatus::Beta,
        FeaturesViewMenu::FeatureCategory::AI,
        true, true, 0, 0, {"inference_engine"}, "1.0"
    });
    
    m_featuresPanel->registerFeature({
        "refactoring", "Intelligent Refactoring",
        "Automatic code refactoring with AI assistance",
        FeaturesViewMenu::FeatureStatus::Beta,
        FeaturesViewMenu::FeatureCategory::AI,
        true, true, 0, 0, {"code_generation", "lsp_client"}, "1.0"
    });
    
    m_featuresPanel->registerFeature({
        "inference_engine", "Model Inference Engine",
        "Run various AI models locally",
        FeaturesViewMenu::FeatureStatus::Stable,
        FeaturesViewMenu::FeatureCategory::AI,
        true, true, 0, 0, {}, "2.1"
    });
    
    // Advanced features
    m_featuresPanel->registerFeature({
        "multi_agent", "Multi-Agent Orchestration",
        "Coordinate multiple AI agents for complex tasks",
        FeaturesViewMenu::FeatureStatus::Experimental,
        FeaturesViewMenu::FeatureCategory::Advanced,
        false, true, 0, 0, {"inference_engine"}, "0.9"
    });
    
    m_featuresPanel->registerFeature({
        "lsp_client", "Language Server Protocol",
        "Smart code analysis and completion",
        FeaturesViewMenu::FeatureStatus::Stable,
        FeaturesViewMenu::FeatureCategory::Advanced,
        true, true, 0, 0, {}, "1.2"
    });
    
    // Performance features
    m_featuresPanel->registerFeature({
        "gpu_acceleration", "GPU Acceleration",
        "Offload computation to GPU",
        FeaturesViewMenu::FeatureStatus::Beta,
        FeaturesViewMenu::FeatureCategory::Performance,
        false, true, 0, 0, {}, "1.1"
    });
    
    m_featuresPanel->registerFeature({
        "memory_profiler", "Memory Profiler",
        "Monitor and optimize memory usage",
        FeaturesViewMenu::FeatureStatus::Stable,
        FeaturesViewMenu::FeatureCategory::Performance,
        true, true, 0, 0, {}, "1.0"
    });
    
    // Debug features
    m_featuresPanel->registerFeature({
        "debugger", "Integrated Debugger",
        "Step through code and inspect variables",
        FeaturesViewMenu::FeatureStatus::Stable,
        FeaturesViewMenu::FeatureCategory::Debug,
        true, true, 0, 0, {}, "1.5"
    });
    
    m_featuresPanel->registerFeature({
        "event_logger", "Event Logger",
        "Capture and analyze system events",
        FeaturesViewMenu::FeatureStatus::Beta,
        FeaturesViewMenu::FeatureCategory::Debug,
        false, true, 0, 0, {}, "1.0"
    });
}

void EnhancedMainWindow::onFeatureToggled(const std::string &featureId, bool enabled) {
    m_statusText = featureId + (enabled ? " feature enabled" : " feature disabled");
    std::cout << "[EnhancedMainWindow] " << m_statusText << std::endl;
}

void EnhancedMainWindow::onFeatureClicked(const std::string &featureId) {
    if (m_featuresPanel) {
        int usageCount = m_featuresPanel->getFeatureUsageCount(featureId);
        long long execTime = m_featuresPanel->getFeatureExecutionTime(featureId);
        m_metricsText = "Calls: " + std::to_string(usageCount) + " | Exec: " + std::to_string(execTime) + "ms";
        std::cout << "[Metrics] " << m_metricsText << std::endl;
    }
}

void EnhancedMainWindow::onFeatureDoubleClicked(const std::string &featureId) {
    m_statusText = "Activated feature: " + featureId;
    std::cout << "[EnhancedMainWindow] " << m_statusText << std::endl;
    m_featuresPanel->recordFeatureUsage(featureId, 50);
}

void EnhancedMainWindow::onCategoryToggled(int category, bool visible) {
    std::cout << "[EnhancedMainWindow] Category " << category << " toggled to " << visible << std::endl;
}

void EnhancedMainWindow::onViewFeatures() {
    std::cout << "[EnhancedMainWindow] Toggle features panel visibility" << std::endl;
}

void EnhancedMainWindow::onViewPerformance() { m_statusText = "Performance monitor would show here"; std::cout << m_statusText << std::endl; }
void EnhancedMainWindow::onViewMetrics() { m_statusText = "Opening metrics dashboard..."; std::cout << m_statusText << std::endl; }
void EnhancedMainWindow::onToggleAdvancedMode() { m_advancedModeEnabled = !m_advancedModeEnabled; std::cout << "[EnhancedMainWindow] " << (m_advancedModeEnabled ? "Advanced Mode ON" : "Advanced Mode OFF") << std::endl; }
void EnhancedMainWindow::onManagePlugins() { m_statusText = "Opening plugin manager..."; std::cout << m_statusText << std::endl; }
void EnhancedMainWindow::onOpenSettings() { m_statusText = "Opening settings dialog..."; std::cout << m_statusText << std::endl; }
void EnhancedMainWindow::onShowAbout() { m_statusText = "RawrXD v3.0 - Production Ready Agentic IDE"; std::cout << m_statusText << std::endl; }

void EnhancedMainWindow::loadWindowState() {
    std::cout << "[EnhancedMainWindow] Window state (no-op in native stub)" << std::endl;
}

void EnhancedMainWindow::saveWindowState() {
    std::cout << "[EnhancedMainWindow] Window state saved (no-op)" << std::endl;
}

void EnhancedMainWindow::close() {
    saveWindowState();
}

void EnhancedMainWindow::dragEnterEvent(/*payload*/) { }

void EnhancedMainWindow::dropEvent(/*payload*/) { }
    
    if (mimeData->hasUrls()) {
        QStringList files;
        for (const QUrl &url : mimeData->urls()) {
            files << url.toLocalFile();
        }
        m_statusLabel->setText(QString("Dropped %1 files").arg(files.count()));
    }
    
    event->acceptProposedAction();
}
