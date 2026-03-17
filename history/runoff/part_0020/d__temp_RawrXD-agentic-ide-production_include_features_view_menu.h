
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>

class FeaturesViewMenu {
public:
    enum class FeatureStatus { Stable, Beta, Experimental, Deprecated, Disabled };
    enum class FeatureCategory { Core, AI, Advanced, Experimental, Utilities, Performance, Debug };

    struct Feature {
        std::string id;
        std::string name;
        std::string description;
        FeatureStatus status = FeatureStatus::Stable;
        FeatureCategory category = FeatureCategory::Core;
        bool enabled = true;
        bool visible = true;
        int usageCount = 0;
        long long totalTimeMs = 0;
        std::vector<std::string> dependencies;
        std::string version;
    };

    struct FeatureMetrics {
        long long totalTimeMs = 0;
        long long peakTimeMs = 0;
        int callCount = 0;
        long long lastCallTime = 0;
    };

    explicit FeaturesViewMenu();
    ~FeaturesViewMenu();

    void registerFeature(const Feature &feature);
    void registerFeature(const std::string &id, const std::string &name, const std::string &description, FeatureCategory category, bool enabled);
    void enableFeature(const std::string &featureId, bool enable);
    bool isFeatureEnabled(const std::string &featureId) const;
    void toggleFeatureVisibility(const std::string &categoryId, bool visible);
    void recordFeatureUsage(const std::string &featureId, long long timeMs = 0);
    long long getFeatureExecutionTime(const std::string &featureId) const;
    int getFeatureUsageCount(const std::string &featureId) const;
    void expandCategory(FeatureCategory category, bool expand);
    void expandAllCategories();
    void collapseAllCategories();
    void filterFeatures(const std::string &searchText);
    void clearFilter();

    // Callbacks
    void setFeatureToggledCallback(std::function<void(const std::string&, bool)> cb) { m_featureToggled = std::move(cb); }
    void setFeatureClickedCallback(std::function<void(const std::string&)> cb) { m_featureClicked = std::move(cb); }
    void setFeatureDoubleClickedCallback(std::function<void(const std::string&)> cb) { m_featureDoubleClicked = std::move(cb); }
    void setCategoryToggledCallback(std::function<void(int, bool)> cb) { m_categoryToggled = std::move(cb); }

private:
    void setupUI();
    void createContextMenu();
    void addCategoryNodes();
    std::string statusToString(FeatureStatus status) const;
    std::string categoryToString(FeatureCategory category) const;
    void updateFeatureItem(const std::string &featureId, const Feature &feature);
    void logFeatureMetrics(const std::string &featureId);
    void populateCategory(FeatureCategory category);
    void saveState();
    void loadState();

    // UI placeholders
    std::string m_currentFilter;

    std::unordered_map<FeatureCategory, std::string> m_categories; // category meta
    std::unordered_map<std::string, Feature> m_features;
    std::unordered_map<std::string, FeatureMetrics> m_metrics;
    std::unordered_map<std::string, std::string> m_itemMap;
    std::unordered_set<std::string> m_enabledFeatures;
    std::unordered_map<FeatureCategory, bool> m_categoryExpanded;

    bool m_suppressSignals = false;

    // Callbacks
    std::function<void(const std::string&, bool)> m_featureToggled;
    std::function<void(const std::string&)> m_featureClicked;
    std::function<void(const std::string&)> m_featureDoubleClicked;
    std::function<void(int, bool)> m_categoryToggled;
};
