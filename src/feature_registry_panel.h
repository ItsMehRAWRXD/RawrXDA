#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

namespace Json {
using Value = nlohmann::json;
}

enum class FeatureCategory {
    Core, Editing, Navigation, Analysis, Debugging, Performance, 
    Integration, Autonomous, Visualization, Extension
};

enum class FeatureStatus {
    Available, Beta, Experimental, Deprecated, Hidden
};

struct KeyboardShortcut {
    std::string keys;
    std::string platform;
    std::string description;
};

struct FeatureExample {
    std::string title;
    std::string description;
    std::string code_snippet;
    std::string expected_output;
};

struct QuickStartGuide {
    std::string step;
    std::string description;
    std::vector<std::string> tips;
    std::string documentation_link;
};

struct Feature {
    std::string id;
    std::string name;
    std::string description;
    std::string longDescription;
    FeatureCategory category;
    FeatureStatus status;
    std::string version;
    std::vector<std::string> tags;
    std::vector<KeyboardShortcut> shortcuts;
    std::vector<QuickStartGuide> quickStart;
    std::vector<FeatureExample> examples;
    std::vector<std::string> relatedFeatures;
    std::vector<std::string> dependencies;
    int usageCount = 0;
    double lastUsedTime = 0.0;
    bool isEnabled = true;
    Json::Value metadata;
};

struct FeatureUsageStatistic {
    std::string featureId;
    std::string featureName;
    int totalUsageCount = 0;
    int sessionsUsed = 0;
    double averageUsagePerSession = 0.0;
    double firstUsedTime = 0.0;
    double lastUsedTime = 0.0;
    std::vector<double> usageTimeline;
    std::vector<std::string> usageContext;
};

struct RecommendedFeature {
    Feature feature;
    float relevanceScore = 0.0f;
    std::string reason;
    std::string category;
};

class FeatureRegistry {
public:
    FeatureRegistry();
    ~FeatureRegistry();

    // Feature management
    void registerFeature(const Feature& feature);
    void unregisterFeature(const std::string& featureId);
    Feature* getFeature(const std::string& featureId);
    std::vector<Feature> getAllFeatures();
    std::vector<Feature> getEnabledFeatures();

    // Search and filtering
    std::vector<Feature> searchFeatures(const std::string& query);
    std::vector<Feature> filterByCategory(FeatureCategory category);
    std::vector<Feature> filterByStatus(FeatureStatus status);
    std::vector<Feature> filterByTag(const std::string& tag);
    std::vector<Feature> filterByDependency(const std::string& dependency);

    // Discovery and recommendations
    std::vector<RecommendedFeature> getRecommendedFeatures(const std::string& context);
    std::vector<RecommendedFeature> getTrendingFeatures();
    std::vector<RecommendedFeature> getNewFeatures();
    std::vector<Feature> getRelatedFeatures(const std::string& featureId);

    // Usage tracking
    void recordFeatureUsage(const std::string& featureId, const std::string& context = "");
    FeatureUsageStatistic getUsageStats(const std::string& featureId);
    std::map<std::string, FeatureUsageStatistic> getAllUsageStats();
    std::vector<std::string> getMostUsedFeatures(int count = 10);
    std::vector<std::string> getUnusedFeatures();

    // Help and shortcuts
    std::vector<KeyboardShortcut> getShortcuts(const std::string& featureId);
    KeyboardShortcut* findShortcutByKeys(const std::string& keys);
    std::vector<QuickStartGuide> getQuickStartGuide(const std::string& featureId);

    // Data export
    Json::Value exportAsJson();
    std::string exportAsMarkdown();
    std::string generateCheatSheet();

    // Feature toggling
    void enableFeature(const std::string& featureId);
    void disableFeature(const std::string& featureId);
    bool isFeatureEnabled(const std::string& featureId);

    // Analytics
    float getFeatureAdoptionRate(const std::string& featureId);
    std::map<std::string, float> getFeatureCategoryStats();
    int getTotalFeatureUsage();
    double getAverageFeatureUsagePerSession();

    // Plugin discovery
    std::vector<Feature> discoverPlugins();
    bool installPluginFeature(const std::string& pluginId);
    bool uninstallPluginFeature(const std::string& pluginId);

    // Context-aware help
    std::vector<RecommendedFeature> getHelpForContext(const std::string& currentMode,
                                                      const std::string& currentFile);

private:
    std::map<std::string, Feature> m_features;
    std::map<std::string, FeatureUsageStatistic> m_usageStats;

    float computeRelevanceScore(const Feature& feature, const std::string& context);
    std::vector<std::string> tokenizeQuery(const std::string& query);
    bool featureMatchesQuery(const Feature& feature, const std::vector<std::string>& tokens);
    std::vector<RecommendedFeature> sortByRelevance(std::vector<RecommendedFeature>& recommendations);
    void initializeBuiltInFeatures();
};
