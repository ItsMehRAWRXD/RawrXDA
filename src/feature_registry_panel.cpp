// ============================================================================
// feature_registry_panel.cpp — IDE Feature Discovery and Registry Panel
// ============================================================================
// Production-ready feature registry and discovery system for RawrXD IDE.
// Provides feature catalog, search/filtering, usage statistics, quick-start guides,
// and feature recommendations based on user behavior and project structure.
//
// Features:
// - Real-time feature catalog with metadata
// - Intelligent search and filtering
// - Feature usage analytics and statistics
// - Quick-start guides and keyboard shortcuts
// - Feature recommendations based on context
// - Plugin/extension discovery
// ============================================================================

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <set>
#include <nlohmann/json.hpp>
#include <chrono>

namespace Json {
using Value = nlohmann::json;
}

// Feature metadata structures
enum class FeatureCategory {
    Core,
    Editing,
    Navigation,
    Analysis,
    Debugging,
    Performance,
    Integration,
    Autonomous,
    Visualization,
    Extension
};

enum class FeatureStatus {
    Available,
    Beta,
    Experimental,
    Deprecated,
    Hidden
};

struct KeyboardShortcut {
    std::string keys;
    std::string platform;  // "windows", "macos", "linux", "universal"
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
    double lastUsedTime = 0.0;  // Unix timestamp
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
    std::vector<double> usageTimeline;  // Timestamps of each usage
    std::vector<std::string> usageContext;  // What was user doing
};

struct RecommendedFeature {
    Feature feature;
    float relevanceScore;  // 0-1
    std::string reason;
    std::string category;  // "new", "related", "helpful", "trending"
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

    // Feature discovery and recommendations
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

    // Keyboard shortcuts and help
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

    // Helper methods
    float computeRelevanceScore(const Feature& feature, const std::string& context);
    std::vector<std::string> tokenizeQuery(const std::string& query);
    bool featureMatchesQuery(const Feature& feature, const std::vector<std::string>& tokens);
    std::vector<RecommendedFeature> sortByRelevance(std::vector<RecommendedFeature>& recommendations);

    // Built-in feature initialization
    void initializeBuiltInFeatures();
};

// Implementation
FeatureRegistry::FeatureRegistry() {
    initializeBuiltInFeatures();
}

FeatureRegistry::~FeatureRegistry() {}

void FeatureRegistry::initializeBuiltInFeatures() {
    // Real-time Code Completion
    {
        Feature completion;
        completion.id = "feat_realtime_completion";
        completion.name = "Real-Time Code Completion";
        completion.description = "AI-powered completions as you type";
        completion.longDescription = "Get intelligent code suggestions in real-time using a state-of-the-art neural language model. "
                                     "Supports C++, Python, JavaScript, and more.";
        completion.category = FeatureCategory::Editing;
        completion.status = FeatureStatus::Available;
        completion.version = "2.0";
        completion.tags = {"ai", "coding", "editing", "productivity"};

        KeyboardShortcut shortcut;
        shortcut.keys = "Ctrl+Space";
        shortcut.platform = "windows";
        shortcut.description = "Trigger completion";
        completion.shortcuts.push_back(shortcut);

        QuickStartGuide guide;
        guide.step = "1. Enable Feature";
        guide.description = "Enable Real-Time Completion in Settings > AI Features";
        guide.documentation_link = "https://docs.rawrxd.io/completion";
        completion.quickStart.push_back(guide);

        registerFeature(completion);
    }

    // Go to Definition
    {
        Feature gotoDef;
        gotoDef.id = "feat_goto_definition";
        gotoDef.name = "Go to Definition (LSP)";
        gotoDef.description = "Navigate to symbol definitions using Language Server Protocol";
        gotoDef.category = FeatureCategory::Navigation;
        gotoDef.status = FeatureStatus::Available;
        gotoDef.tags = {"navigation", "lsp", "symbols"};

        KeyboardShortcut shortcut;
        shortcut.keys = "Ctrl+Click";
        shortcut.platform = "windows";
        shortcut.description = "Jump to definition";
        gotoDef.shortcuts.push_back(shortcut);

        registerFeature(gotoDef);
    }

    // Find References
    {
        Feature findRefs;
        findRefs.id = "feat_find_references";
        findRefs.name = "Find All References";
        findRefs.description = "Find all usages of a symbol across the codebase";
        findRefs.category = FeatureCategory::Navigation;
        findRefs.status = FeatureStatus::Available;
        findRefs.tags = {"search", "navigation", "refactoring"};

        KeyboardShortcut shortcut;
        shortcut.keys = "Ctrl+Shift+F";
        shortcut.platform = "windows";
        shortcut.description = "Find references";
        findRefs.shortcuts.push_back(shortcut);

        registerFeature(findRefs);
    }

    // Autonomous Code Analysis
    {
        Feature autonomousAnalysis;
        autonomousAnalysis.id = "feat_autonomous_analysis";
        autonomousAnalysis.name = "Autonomous Code Analysis";
        autonomousAnalysis.description = "Automatic detection of bugs, security issues, and optimization opportunities";
        autonomousAnalysis.longDescription = "Run continuous analysis on your codebase to detect potential issues including: "
                                            "bugs, security vulnerabilities, performance bottlenecks, and missing documentation. "
                                            "Get real-time suggestions for improvements.";
        autonomousAnalysis.category = FeatureCategory::Autonomous;
        autonomousAnalysis.status = FeatureStatus::Available;
        autonomousAnalysis.tags = {"analysis", "security", "performance", "autonomous"};

        QuickStartGuide guide1;
        guide1.step = "1. Start Analysis";
        guide1.description = "Click 'Start Autonomous Mode' in the toolbar";
        autonomousAnalysis.quickStart.push_back(guide1);

        QuickStartGuide guide2;
        guide2.step = "2. Review Suggestions";
        guide2.description = "Check the Issues panel for detected problems and suggestions";
        autonomousAnalysis.quickStart.push_back(guide2);

        registerFeature(autonomousAnalysis);
    }

    // Interpretability Analysis
    {
        Feature interpretability;
        interpretability.id = "feat_model_interpretability";
        interpretability.name = "Model Interpretability Panel";
        interpretability.description = "Visualize and understand model inference with attention heatmaps and token attribution";
        interpretability.longDescription = "Analyze how the AI model makes predictions. View attention weight heatmaps across layers, "
                                          "explore token importance scores, see gradient flows, and understand model decisions.";
        interpretability.category = FeatureCategory::Visualization;
        interpretability.status = FeatureStatus::Available;
        interpretability.version = "1.5";
        interpretability.tags = {"ai", "visualization", "debugging", "analysis"};

        QuickStartGuide guide;
        guide.step = "1. Generate Completion";
        guide.description = "Trigger any AI code completion";
        interpretability.quickStart.push_back(guide);

        guide.step = "2. Open Interpretability Panel";
        guide.description = "View > Interpretability Panel (or press Ctrl+I)";
        interpretability.quickStart.push_back(guide);

        registerFeature(interpretability);
    }

    // Security Analysis
    {
        Feature security;
        security.id = "feat_security_scanner";
        security.name = "Security Vulnerability Scanner";
        security.description = "Detect security issues and suggest fixes";
        security.category = FeatureCategory::Analysis;
        security.status = FeatureStatus::Available;
        security.tags = {"security", "analysis", "vulnerability"};

        registerFeature(security);
    }

    // Performance Optimization
    {
        Feature perf;
        perf.id = "feat_perf_optimizer";
        perf.name = "Performance Optimization Suggestions";
        perf.description = "AI-powered suggestions for code optimization";
        perf.category = FeatureCategory::Performance;
        perf.status = FeatureStatus::Available;
        perf.tags = {"performance", "optimization", "analysis"};

        registerFeature(perf);
    }

    // Test Generation
    {
        Feature testGen;
        testGen.id = "feat_test_generation";
        testGen.name = "Automatic Unit Test Generation";
        testGen.description = "Generate tests for functions using AI";
        testGen.category = FeatureCategory::Editing;
        testGen.status = FeatureStatus::Beta;
        testGen.tags = {"testing", "ai", "productivity"};

        registerFeature(testGen);
    }

    // Documentation Generation
    {
        Feature docGen;
        docGen.id = "feat_doc_generation";
        docGen.name = "Documentation Generation";
        docGen.description = "Automatically generate code documentation and comments";
        docGen.category = FeatureCategory::Editing;
        docGen.status = FeatureStatus::Available;
        docGen.tags = {"documentation", "ai", "productivity"};

        KeyboardShortcut shortcut;
        shortcut.keys = "Ctrl+Alt+D";
        shortcut.platform = "windows";
        shortcut.description = "Generate documentation";
        docGen.shortcuts.push_back(shortcut);

        registerFeature(docGen);
    }

    // Live Model Switching
    {
        Feature modelSwitch;
        modelSwitch.id = "feat_model_switching";
        modelSwitch.name = "Live Model Switching";
        modelSwitch.description = "Switch between different AI models without restarting";
        modelSwitch.category = FeatureCategory::Integration;
        modelSwitch.status = FeatureStatus::Available;
        modelSwitch.tags = {"models", "switching", "configuration"};

        registerFeature(modelSwitch);
    }
}

void FeatureRegistry::registerFeature(const Feature& feature) {
    m_features[feature.id] = feature;
    m_usageStats[feature.id] = FeatureUsageStatistic{feature.id, feature.name, 0, 0, 0.0, 0.0, 0.0};
}

void FeatureRegistry::unregisterFeature(const std::string& featureId) {
    m_features.erase(featureId);
    m_usageStats.erase(featureId);
}

Feature* FeatureRegistry::getFeature(const std::string& featureId) {
    auto it = m_features.find(featureId);
    if (it != m_features.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<Feature> FeatureRegistry::getAllFeatures() {
    std::vector<Feature> result;
    for (const auto& [id, feature] : m_features) {
        result.push_back(feature);
    }
    return result;
}

std::vector<Feature> FeatureRegistry::getEnabledFeatures() {
    std::vector<Feature> result;
    for (const auto& [id, feature] : m_features) {
        if (feature.isEnabled) {
            result.push_back(feature);
        }
    }
    return result;
}

std::vector<std::string> FeatureRegistry::tokenizeQuery(const std::string& query) {
    std::vector<std::string> tokens;
    std::string currentToken;
    for (char c : query) {
        if (c == ' ' || c == '-' || c == '_') {
            if (!currentToken.empty()) {
                std::transform(currentToken.begin(), currentToken.end(), currentToken.begin(), ::tolower);
                tokens.push_back(currentToken);
                currentToken.clear();
            }
        } else {
            currentToken += c;
        }
    }
    if (!currentToken.empty()) {
        std::transform(currentToken.begin(), currentToken.end(), currentToken.begin(), ::tolower);
        tokens.push_back(currentToken);
    }
    return tokens;
}

bool FeatureRegistry::featureMatchesQuery(const Feature& feature,
                                          const std::vector<std::string>& tokens) {
    std::string featureName = feature.name;
    std::string featureDesc = feature.description;
    std::transform(featureName.begin(), featureName.end(), featureName.begin(), ::tolower);
    std::transform(featureDesc.begin(), featureDesc.end(), featureDesc.begin(), ::tolower);

    for (const auto& token : tokens) {
        if (featureName.find(token) != std::string::npos ||
            featureDesc.find(token) != std::string::npos) {
            return true;
        }
        // Check tags
        for (const auto& tag : feature.tags) {
            std::string lowerTag = tag;
            std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), ::tolower);
            if (lowerTag.find(token) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

std::vector<Feature> FeatureRegistry::searchFeatures(const std::string& query) {
    std::vector<Feature> results;
    std::vector<std::string> tokens = tokenizeQuery(query);

    for (const auto& [id, feature] : m_features) {
        if (featureMatchesQuery(feature, tokens)) {
            results.push_back(feature);
        }
    }

    // Sort by relevance (usage)
    std::sort(results.begin(), results.end(),
              [this](const Feature& a, const Feature& b) {
                  return m_usageStats[a.id].totalUsageCount >
                         m_usageStats[b.id].totalUsageCount;
              });

    return results;
}

std::vector<Feature> FeatureRegistry::filterByCategory(FeatureCategory category) {
    std::vector<Feature> results;
    for (const auto& [id, feature] : m_features) {
        if (feature.category == category && feature.isEnabled) {
            results.push_back(feature);
        }
    }
    return results;
}

std::vector<Feature> FeatureRegistry::filterByTag(const std::string& tag) {
    std::vector<Feature> results;
    std::string lowerTag = tag;
    std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), ::tolower);

    for (const auto& [id, feature] : m_features) {
        for (const auto& featureTag : feature.tags) {
            std::string lowerFeatureTag = featureTag;
            std::transform(lowerFeatureTag.begin(), lowerFeatureTag.end(), lowerFeatureTag.begin(), ::tolower);
            if (lowerFeatureTag == lowerTag) {
                results.push_back(feature);
                break;
            }
        }
    }
    return results;
}

std::vector<Feature> FeatureRegistry::filterByStatus(FeatureStatus status) {
    std::vector<Feature> results;
    for (const auto& [id, feature] : m_features) {
        if (feature.status == status) {
            results.push_back(feature);
        }
    }
    return results;
}

std::vector<Feature> FeatureRegistry::filterByDependency(const std::string& dependency) {
    std::vector<Feature> results;
    for (const auto& [id, feature] : m_features) {
        for (const auto& dep : feature.dependencies) {
            if (dep == dependency) {
                results.push_back(feature);
                break;
            }
        }
    }
    return results;
}

std::vector<RecommendedFeature> FeatureRegistry::getRecommendedFeatures(const std::string& context) {
    std::vector<RecommendedFeature> recommendations;

    for (const auto& [id, feature] : m_features) {
        if (!feature.isEnabled) continue;

        RecommendedFeature rec;
        rec.feature = feature;
        rec.relevanceScore = computeRelevanceScore(feature, context);
        rec.reason = "Based on your usage patterns";

        if (rec.relevanceScore > 0.3f) {
            recommendations.push_back(rec);
        }
    }

    return sortByRelevance(recommendations);
}

std::vector<RecommendedFeature> FeatureRegistry::getTrendingFeatures() {
    std::vector<RecommendedFeature> trending;

    // Implement trend detection based on recent usage spike
    // For now, return high-usage features
    std::vector<std::pair<std::string, int>> usageRanking;
    for (const auto& [id, stats] : m_usageStats) {
        usageRanking.push_back({id, stats.totalUsageCount});
    }
    std::sort(usageRanking.rbegin(), usageRanking.rend());

    for (int i = 0; i < std::min(5, static_cast<int>(usageRanking.size())); ++i) {
        auto* feature = getFeature(usageRanking[i].first);
        if (feature) {
            RecommendedFeature rec;
            rec.feature = *feature;
            rec.relevanceScore = 0.9f;
            rec.category = "trending";
            rec.reason = "Trending in your workspace";
            trending.push_back(rec);
        }
    }

    return trending;
}

std::vector<RecommendedFeature> FeatureRegistry::getNewFeatures() {
    std::vector<RecommendedFeature> newFeatures;

    for (const auto& [id, feature] : m_features) {
        if (feature.status == FeatureStatus::Beta || feature.status == FeatureStatus::Experimental) {
            if (m_usageStats[id].totalUsageCount == 0) {  // Unused new feature
                RecommendedFeature rec;
                rec.feature = feature;
                rec.relevanceScore = 0.7f;
                rec.category = "new";
                rec.reason = "Recently added feature";
                newFeatures.push_back(rec);
            }
        }
    }

    return newFeatures;
}

std::vector<Feature> FeatureRegistry::getRelatedFeatures(const std::string& featureId) {
    auto* feature = getFeature(featureId);
    if (!feature) return {};

    std::vector<Feature> related;
    for (const auto& relatedId : feature->relatedFeatures) {
        auto* relatedFeature = getFeature(relatedId);
        if (relatedFeature) {
            related.push_back(*relatedFeature);
        }
    }
    return related;
}

void FeatureRegistry::recordFeatureUsage(const std::string& featureId, const std::string& context) {
    auto& stats = m_usageStats[featureId];
    stats.totalUsageCount++;
    stats.lastUsedTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (stats.firstUsedTime == 0.0) {
        stats.firstUsedTime = stats.lastUsedTime;
    }
    stats.usageTimeline.push_back(stats.lastUsedTime);
    if (!context.empty()) {
        stats.usageContext.push_back(context);
    }
}

FeatureUsageStatistic FeatureRegistry::getUsageStats(const std::string& featureId) {
    auto it = m_usageStats.find(featureId);
    if (it != m_usageStats.end()) {
        return it->second;
    }
    return FeatureUsageStatistic{featureId, "", 0, 0, 0.0, 0.0, 0.0};
}

std::vector<std::string> FeatureRegistry::getMostUsedFeatures(int count) {
    std::vector<std::pair<std::string, int>> ranking;
    for (const auto& [id, stats] : m_usageStats) {
        ranking.push_back({id, stats.totalUsageCount});
    }
    std::sort(ranking.rbegin(), ranking.rend());

    std::vector<std::string> result;
    for (int i = 0; i < std::min(count, static_cast<int>(ranking.size())); ++i) {
        result.push_back(ranking[i].first);
    }
    return result;
}

std::vector<std::string> FeatureRegistry::getUnusedFeatures() {
    std::vector<std::string> unused;
    for (const auto& [id, stats] : m_usageStats) {
        if (stats.totalUsageCount == 0) {
            unused.push_back(id);
        }
    }
    return unused;
}

std::vector<KeyboardShortcut> FeatureRegistry::getShortcuts(const std::string& featureId) {
    auto* feature = getFeature(featureId);
    if (feature) {
        return feature->shortcuts;
    }
    return {};
}

KeyboardShortcut* FeatureRegistry::findShortcutByKeys(const std::string& keys) {
    for (auto& [id, feature] : m_features) {
        for (auto& shortcut : feature.shortcuts) {
            if (shortcut.keys == keys) {
                return &shortcut;
            }
        }
    }
    return nullptr;
}

std::vector<QuickStartGuide> FeatureRegistry::getQuickStartGuide(const std::string& featureId) {
    auto* feature = getFeature(featureId);
    if (feature) {
        return feature->quickStart;
    }
    return {};
}

void FeatureRegistry::enableFeature(const std::string& featureId) {
    auto* feature = getFeature(featureId);
    if (feature) {
        feature->isEnabled = true;
    }
}

void FeatureRegistry::disableFeature(const std::string& featureId) {
    auto* feature = getFeature(featureId);
    if (feature) {
        feature->isEnabled = false;
    }
}

bool FeatureRegistry::isFeatureEnabled(const std::string& featureId) {
    auto* feature = getFeature(featureId);
    if (feature) {
        return feature->isEnabled;
    }
    return false;
}

float FeatureRegistry::computeRelevanceScore(const Feature& feature, const std::string& context) {
    float score = 0.0f;

    // Usage-based scoring
    int usage = m_usageStats[feature.id].totalUsageCount;
    score += std::min(0.5f, usage * 0.05f);

    // Tag matching
    std::vector<std::string> contextTokens = tokenizeQuery(context);
    for (const auto& token : contextTokens) {
        for (const auto& tag : feature.tags) {
            std::string lowerTag = tag;
            std::transform(lowerTag.begin(), lowerTag.end(), lowerTag.begin(), ::tolower);
            if (lowerTag.find(token) != std::string::npos) {
                score += 0.3f;
            }
        }
    }

    // Status-based scoring (prefer available over beta)
    if (feature.status == FeatureStatus::Available) {
        score += 0.2f;
    } else if (feature.status == FeatureStatus::Beta) {
        score += 0.1f;
    }

    return std::min(1.0f, score);
}

std::vector<RecommendedFeature> FeatureRegistry::sortByRelevance(
    std::vector<RecommendedFeature>& recommendations) {

    std::sort(recommendations.begin(), recommendations.end(),
              [](const RecommendedFeature& a, const RecommendedFeature& b) {
                  return a.relevanceScore > b.relevanceScore;
              });

    return recommendations;
}

int FeatureRegistry::getTotalFeatureUsage() {
    int total = 0;
    for (const auto& [id, stats] : m_usageStats) {
        total += stats.totalUsageCount;
    }
    return total;
}

float FeatureRegistry::getFeatureAdoptionRate(const std::string& featureId) {
    auto* feature = getFeature(featureId);
    if (!feature) return 0.0f;

    int totalUsage = getTotalFeatureUsage();
    if (totalUsage == 0) return 0.0f;

    return static_cast<float>(m_usageStats[featureId].totalUsageCount) / totalUsage;
}

std::map<std::string, float> FeatureRegistry::getFeatureCategoryStats() {
    std::map<std::string, float> categoryStats;

    std::map<FeatureCategory, int> categoryUsage;
    for (const auto& [id, feature] : m_features) {
        categoryUsage[feature.category] += m_usageStats[id].totalUsageCount;
    }

    int totalUsage = getTotalFeatureUsage();
    if (totalUsage > 0) {
        for (const auto& [category, usage] : categoryUsage) {
            std::string categoryName;
            switch (category) {
                case FeatureCategory::Core: categoryName = "Core"; break;
                case FeatureCategory::Editing: categoryName = "Editing"; break;
                case FeatureCategory::Navigation: categoryName = "Navigation"; break;
                case FeatureCategory::Analysis: categoryName = "Analysis"; break;
                case FeatureCategory::Debugging: categoryName = "Debugging"; break;
                case FeatureCategory::Performance: categoryName = "Performance"; break;
                case FeatureCategory::Integration: categoryName = "Integration"; break;
                case FeatureCategory::Autonomous: categoryName = "Autonomous"; break;
                case FeatureCategory::Visualization: categoryName = "Visualization"; break;
                case FeatureCategory::Extension: categoryName = "Extension"; break;
            }
            categoryStats[categoryName] = static_cast<float>(usage) / totalUsage;
        }
    }

    return categoryStats;
}

double FeatureRegistry::getAverageFeatureUsagePerSession() {
    int sessions = 0;
    for (const auto& [id, stats] : m_usageStats) {
        sessions = std::max(sessions, stats.sessionsUsed);
    }
    if (sessions == 0) return 0.0;

    return static_cast<double>(getTotalFeatureUsage()) / sessions;
}

std::vector<Feature> FeatureRegistry::discoverPlugins() {
    std::vector<Feature> plugins = filterByCategory(FeatureCategory::Extension);
    return plugins;
}

bool FeatureRegistry::installPluginFeature(const std::string& pluginId) {
    auto* feature = getFeature(pluginId);
    if (feature) {
        enableFeature(pluginId);
        return true;
    }
    return false;
}

bool FeatureRegistry::uninstallPluginFeature(const std::string& pluginId) {
    auto* feature = getFeature(pluginId);
    if (feature) {
        disableFeature(pluginId);
        return true;
    }
    return false;
}

std::vector<RecommendedFeature> FeatureRegistry::getHelpForContext(
    const std::string& currentMode,
    const std::string& currentFile) {

    std::string context = currentMode + " " + currentFile;
    return getRecommendedFeatures(context);
}

Json::Value FeatureRegistry::exportAsJson() {
    Json::Value root;
    Json::Value features;

    for (const auto& feature : getAllFeatures()) {
        Json::Value featureJson;
        featureJson["id"] = feature.id;
        featureJson["name"] = feature.name;
        featureJson["description"] = feature.description;
        featureJson["version"] = feature.version;
        featureJson["enabled"] = feature.isEnabled;
        featureJson["usage_count"] = feature.usageCount;

        features.push_back(featureJson);
    }

    root["features"] = features;
    root["total_features"] = static_cast<int>(m_features.size());
    root["total_usage"] = getTotalFeatureUsage();

    return root;
}

std::string FeatureRegistry::exportAsMarkdown() {
    std::string markdown = "# RawrXD IDE Feature Registry\n\n";
    markdown += "## Feature Summary\n";
    markdown += "- **Total Features**: " + std::to_string(m_features.size()) + "\n";
    markdown += "- **Total Usage**: " + std::to_string(getTotalFeatureUsage()) + "\n\n";

    std::map<FeatureCategory, std::vector<Feature>> byCategory;
    for (const auto& feature : getAllFeatures()) {
        byCategory[feature.category].push_back(feature);
    }

    for (const auto& [category, features] : byCategory) {
        std::string categoryName;
        switch (category) {
            case FeatureCategory::Core: categoryName = "Core"; break;
            case FeatureCategory::Editing: categoryName = "Editing"; break;
            case FeatureCategory::Navigation: categoryName = "Navigation"; break;
            case FeatureCategory::Analysis: categoryName = "Analysis"; break;
            case FeatureCategory::Debugging: categoryName = "Debugging"; break;
            case FeatureCategory::Performance: categoryName = "Performance"; break;
            case FeatureCategory::Integration: categoryName = "Integration"; break;
            case FeatureCategory::Autonomous: categoryName = "Autonomous"; break;
            case FeatureCategory::Visualization: categoryName = "Visualization"; break;
            case FeatureCategory::Extension: categoryName = "Extension"; break;
        }

        markdown += "## " + categoryName + "\n\n";
        for (const auto& feature : features) {
            markdown += "### " + feature.name + "\n";
            markdown += feature.description + "\n\n";
        }
    }

    return markdown;
}

std::string FeatureRegistry::generateCheatSheet() {
    std::string cheatSheet = "# RawrXD IDE Cheat Sheet\n\n";

    for (const auto& [id, feature] : m_features) {
        if (!feature.shortcuts.empty()) {
            cheatSheet += "## " + feature.name + "\n";
            for (const auto& shortcut : feature.shortcuts) {
                cheatSheet += "- **" + shortcut.keys + "**: " + shortcut.description + "\n";
            }
            cheatSheet += "\n";
        }
    }

    return cheatSheet;
}

std::map<std::string, FeatureUsageStatistic> FeatureRegistry::getAllUsageStats() {
    return m_usageStats;
}
