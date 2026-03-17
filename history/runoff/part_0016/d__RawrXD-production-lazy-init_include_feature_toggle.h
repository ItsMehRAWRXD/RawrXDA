#pragma once

#include <QString>

/**
 * @file feature_toggle.h
 * @brief Feature flag system for v1.0.0+ API stability
 * 
 * Isolates experimental features from production-stable APIs.
 * See FEATURE_FLAGS.md for complete documentation.
 * 
 * v1.0.0 Stability Guarantee: All experimental features default OFF
 */

namespace RawrXD {
class FeatureToggle {
public:
    /**
     * @brief Check if a feature is enabled
     * @param name Feature name (e.g., "hotpatch_system")
     * @param defaultValue Default state if not configured
     * @return true if feature is enabled
     */
    static bool isEnabled(const QString& name, bool defaultValue = false);
    
    // ⚠️ Experimental Features (High Risk - Default OFF)
    static bool isHotpatchEnabled();
    static bool isAdvancedProfilingEnabled();
    static bool isCloudHybridEnabled();
    static bool isAgentLearningEnabled();
    static bool isDistributedTrainingEnabled();
    
    // 🔬 Beta Features (Moderate Risk - Default OFF)
    static bool isPluginMarketplaceEnabled();
    static bool isTimeTravelDebuggingEnabled();
    static bool isTestGenerationEnabled();
    static bool isCodeStreamEnabled();
    
    // ✅ Stable Features (Low Risk - Default ON)
    static bool isGGUFStreamingEnabled();
    static bool isVulkanComputeEnabled();
    static bool isAgentCoordinatorEnabled();
    static bool isModelRouterEnabled();
    static bool isLSPIntegrationEnabled();
};
} // namespace RawrXD
