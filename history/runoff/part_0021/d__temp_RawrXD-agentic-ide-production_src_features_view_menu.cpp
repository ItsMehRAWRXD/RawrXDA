#include "features_view_menu.h"
#include <iostream>
#include <chrono>
#include <fstream>

FeaturesViewMenu::FeaturesViewMenu() {
    setupUI();
    addCategoryNodes();
    loadState();
    std::cout << "[FeaturesViewMenu] Initialized" << std::endl;
}

FeaturesViewMenu::~FeaturesViewMenu() { saveState(); }

void FeaturesViewMenu::setupUI() {
    // Minimal headless UI: nothing to allocate, state maintained in-memory
}

void FeaturesViewMenu::createContextMenu() {}

void FeaturesViewMenu::addCategoryNodes() {
    m_categories[FeatureCategory::Core] = "Core Features";
    m_categories[FeatureCategory::AI] = "AI & Machine Learning";
    m_categories[FeatureCategory::Advanced] = "Advanced Features";
    m_categories[FeatureCategory::Experimental] = "Experimental";
    m_categories[FeatureCategory::Utilities] = "Utilities";
    m_categories[FeatureCategory::Performance] = "Performance";
    m_categories[FeatureCategory::Debug] = "Debug Tools";
}

void FeaturesViewMenu::registerFeature(const Feature &feature) {
    m_features[feature.id] = feature;
    m_enabledFeatures.insert(feature.id);
    m_itemMap[feature.id] = feature.name;
    std::cout << "[Features] Registered: " << feature.name << " (" << feature.id << ")" << std::endl;
}

void FeaturesViewMenu::enableFeature(const std::string &featureId, bool enable) {
    auto it = m_features.find(featureId);
    if (it == m_features.end()) return;
    it->second.enabled = enable;
    if (enable) m_enabledFeatures.insert(featureId); else m_enabledFeatures.erase(featureId);
    if (m_featureToggled) m_featureToggled(featureId, enable);
    logFeatureMetrics(featureId);
}

bool FeaturesViewMenu::isFeatureEnabled(const std::string &featureId) const {
    return m_enabledFeatures.find(featureId) != m_enabledFeatures.end();
}

void FeaturesViewMenu::toggleFeatureVisibility(const std::string &categoryId, bool visible) {
    for (auto &it : m_categories) {
        if (it.second == categoryId) {
            if (m_categoryToggled) m_categoryToggled(static_cast<int>(it.first), visible);
            break;
        }
    }
}

void FeaturesViewMenu::recordFeatureUsage(const std::string &featureId, long long timeMs) {
    auto it = m_features.find(featureId);
    if (it == m_features.end()) return;
    auto &metrics = m_metrics[featureId];
    metrics.totalTimeMs += timeMs;
    metrics.callCount++;
    metrics.lastCallTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    metrics.peakTimeMs = std::max(metrics.peakTimeMs, timeMs);
    it->second.usageCount = metrics.callCount;
    it->second.totalTimeMs = metrics.totalTimeMs;
}

long long FeaturesViewMenu::getFeatureExecutionTime(const std::string &featureId) const {
    auto it = m_metrics.find(featureId);
    return it == m_metrics.end() ? 0 : it->second.totalTimeMs;
}

int FeaturesViewMenu::getFeatureUsageCount(const std::string &featureId) const {
    auto it = m_metrics.find(featureId);
    return it == m_metrics.end() ? 0 : it->second.callCount;
}

void FeaturesViewMenu::expandCategory(FeatureCategory category, bool /*expand*/) {}

void FeaturesViewMenu::expandAllCategories() {}

void FeaturesViewMenu::collapseAllCategories() {
    for (auto &cat : m_categories) {
        m_categoryExpanded[cat.first] = false;
    }
}

void FeaturesViewMenu::filterFeatures(const std::string &searchText) {
    // Case-insensitive substring filter
    auto lower = [](const std::string &s){ std::string r = s; for (auto &c : r) c = std::tolower(c); return r; };
    m_currentFilter = lower(searchText);

    for (auto &kv : m_features) {
        const auto &featureId = kv.first;
        const auto &feature = kv.second;
        std::string name = lower(feature.name);
        std::string desc = lower(feature.description);
        bool matches = name.find(m_currentFilter) != std::string::npos || desc.find(m_currentFilter) != std::string::npos || featureId.find(m_currentFilter) != std::string::npos;
        // Update the item map visibility state (headless)
        if (m_itemMap.find(featureId) != m_itemMap.end()) {
            m_itemMap[featureId] = matches ? feature.name : std::string();
        }
    }
}

void FeaturesViewMenu::clearFilter() {
    m_currentFilter.clear();
}

void FeaturesViewMenu::saveState() {
    // Simple plain-text persistence (no Qt). Write enabled features and category states.
    std::ofstream out("features_state.txt");
    if (!out) return;
    out << "#enabled_features\n";
    for (const auto &f : m_enabledFeatures) out << f << "\n";
    out << "#categories\n";
    for (const auto &c : m_categoryExpanded) out << static_cast<int>(c.first) << ":" << (c.second ? 1 : 0) << "\n";
}

void FeaturesViewMenu::loadState() {
    std::ifstream in("features_state.txt");
    if (!in) return;
    std::string line;
    enum Section { None, Enabled, Categories } sec = None;
    while (std::getline(in, line)) {
        if (line == "#enabled_features") { sec = Enabled; continue; }
        if (line == "#categories") { sec = Categories; continue; }
        if (sec == Enabled && !line.empty()) m_enabledFeatures.insert(line);
        if (sec == Categories && !line.empty()) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                int cat = std::stoi(line.substr(0,pos));
                int val = std::stoi(line.substr(pos+1));
                m_categoryExpanded[static_cast<FeatureCategory>(cat)] = (val != 0);
            }
        }
    }
}



std::string FeaturesViewMenu::statusToString(FeatureStatus status) const {
    switch (status) {
        case FeatureStatus::Stable: return "Stable";
        case FeatureStatus::Beta: return "Beta";
        case FeatureStatus::Experimental: return "Experimental";
        case FeatureStatus::Deprecated: return "Deprecated";
        case FeatureStatus::Disabled: return "Disabled";
    }
    return "Unknown";
}

std::string FeaturesViewMenu::categoryToString(FeatureCategory category) const {
    switch (category) {
        case FeatureCategory::Core: return "Core";
        case FeatureCategory::AI: return "AI & ML";
        case FeatureCategory::Advanced: return "Advanced";
        case FeatureCategory::Experimental: return "Experimental";
        case FeatureCategory::Utilities: return "Utilities";
        case FeatureCategory::Performance: return "Performance";
        case FeatureCategory::Debug: return "Debug";
    }
    return "Unknown";
}

void FeaturesViewMenu::updateFeatureItem(const std::string &featureId, const Feature &feature) {
    // Headless: update item map
    m_itemMap[featureId] = feature.name;
}

void FeaturesViewMenu::logFeatureMetrics(const std::string &featureId) {
    auto it = m_metrics.find(featureId);
    if (it == m_metrics.end()) return;
    const auto &metrics = it->second;
    std::cout << "[FeaturesViewMenu] Feature: " << featureId
              << " Calls:" << metrics.callCount
              << " Total:" << metrics.totalTimeMs << "ms"
              << " Peak:" << metrics.peakTimeMs << "ms" << std::endl;
}

void FeaturesViewMenu::populateCategory(FeatureCategory category) {
    // Dynamically populate category based on registered features
}

void FeaturesViewMenu::registerFeature(const std::string &id, const std::string &name, const std::string &description, FeatureCategory category, bool enabled) {
    Feature feature;
    feature.id = id;
    feature.name = name;
    feature.description = description;
    feature.category = category;
    feature.enabled = enabled;
    feature.status = FeatureStatus::Stable;
    feature.version = "1.0.0";
    
    registerFeature(feature);
}
