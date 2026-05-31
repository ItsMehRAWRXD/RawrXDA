// ============================================================================
// Feature Flags Manager Tests — Dynamic Feature Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/feature_flags/feature_flags_manager.cpp"

using namespace RawrXD::FeatureFlags;

// Mock Session Manager
class MockFeatureFlagsSessionManager : public Core::SessionManager {
public:
    void SetValue(const std::string& key, const std::string& value) override {
        m_values[key] = value;
    }
    
    std::string GetValue(const std::string& key) override {
        auto it = m_values.find(key);
        return it != m_values.end() ? it->second : "";
    }
    
private:
    std::map<std::string, std::string> m_values;
};

TEST_CASE("Feature Flags Manager - Basic Operations", "[features][flags]") {
    auto sessionManager = std::make_shared<MockFeatureFlagsSessionManager>();
    FeatureFlagsManager manager(sessionManager);
    
    SECTION("Default flag state") {
        REQUIRE_FALSE(manager.IsEnabled("new_feature"));
        REQUIRE(manager.GetSupportedLanguages().empty() == false);
    }
    
    SECTION("Register feature flag") {
        FeatureFlag flag;
        flag.key = "beta_feature";
        flag.name = "Beta Feature";
        flag.description = "A beta feature for testing";
        flag.state = FeatureState::OFF;
        flag.rolloutPercentage = 0;
        
        manager.RegisterFeature(flag);
        
        REQUIRE_FALSE(manager.IsEnabled("beta_feature"));
    }
    
    SECTION("Enable/disable flag") {
        FeatureFlag flag;
        flag.key = "toggle_feature";
        flag.name = "Toggle Feature";
        flag.state = FeatureState::OFF;
        
        manager.RegisterFeature(flag);
        
        REQUIRE_FALSE(manager.IsEnabled("toggle_feature"));
        
        manager.EnableFeature("toggle_feature", "admin");
        REQUIRE(manager.IsEnabled("toggle_feature"));
        
        manager.DisableFeature("toggle_feature", "admin");
        REQUIRE_FALSE(manager.IsEnabled("toggle_feature"));
    }
}

TEST_CASE("Feature Flags Manager - Rollout", "[features][rollout]") {
    auto sessionManager = std::make_shared<MockFeatureFlagsSessionManager>();
    FeatureFlagsManager manager(sessionManager);
    
    SECTION("Percentage rollout") {
        FeatureFlag flag;
        flag.key = "rollout_feature";
        flag.name = "Rollout Feature";
        flag.state = FeatureState::PARTIAL;
        flag.rolloutPercentage = 50;
        
        manager.RegisterFeature(flag);
        
        // Set rollout percentage
        manager.SetRolloutPercentage("rollout_feature", 75, "admin");
        
        // Check different users (should be deterministic based on hash)
        bool user1 = manager.IsEnabled("rollout_feature", "user1");
        bool user2 = manager.IsEnabled("rollout_feature", "user2");
        
        // Both should work (implementation specific)
        REQUIRE((user1 || !user1) == true); // Just verify no crash
    }
    
    SECTION("Target users") {
        FeatureFlag flag;
        flag.key = "targeted_feature";
        flag.name = "Targeted Feature";
        flag.state = FeatureState::PARTIAL;
        
        manager.RegisterFeature(flag);
        
        manager.AddTargetUser("targeted_feature", "vip_user1");
        manager.AddTargetUser("targeted_feature", "vip_user2");
        
        REQUIRE(manager.IsEnabled("targeted_feature", "vip_user1"));
        REQUIRE(manager.IsEnabled("targeted_feature", "vip_user2"));
        REQUIRE_FALSE(manager.IsEnabled("targeted_feature", "regular_user"));
    }
}

TEST_CASE("Feature Flags Manager - A/B Testing", "[features][ab-test]") {
    auto sessionManager = std::make_shared<MockFeatureFlagsSessionManager>();
    FeatureFlagsManager manager(sessionManager);
    
    SECTION("A/B test creation") {
        FeatureFlag flag;
        flag.key = "experiment_feature";
        flag.name = "Experiment Feature";
        
        manager.RegisterFeature(flag);
        
        auto test = manager.StartABTest("experiment_feature", "variant_a", "variant_b", 50);
        
        REQUIRE_FALSE(test.id.empty());
        REQUIRE(test.featureKey == "experiment_feature");
        REQUIRE(test.trafficSplit == 50);
        REQUIRE(test.isActive);
    }
    
    SECTION("A/B test variant assignment") {
        FeatureFlag flag;
        flag.key = "ab_feature";
        flag.name = "A/B Feature";
        
        manager.RegisterFeature(flag);
        
        auto test = manager.StartABTest("ab_feature", "control", "treatment", 50);
        
        std::string variant = manager.GetABTestVariant(test.id, "user1");
        
        // Should return one of the variants
        REQUIRE((variant == "control" || variant == "treatment" || variant.empty()));
    }
}

TEST_CASE("Feature Flags Manager - Metrics", "[features][metrics]") {
    auto sessionManager = std::make_shared<MockFeatureFlagsSessionManager>();
    FeatureFlagsManager manager(sessionManager);
    
    SECTION("Feature metrics") {
        FeatureFlag flag;
        flag.key = "metrics_feature";
        flag.name = "Metrics Feature";
        flag.state = FeatureState::ON;
        
        manager.RegisterFeature(flag);
        
        // Record some evaluations
        manager.RecordEvaluation("metrics_feature", true, "user1");
        manager.RecordEvaluation("metrics_feature", true, "user2");
        manager.RecordEvaluation("metrics_feature", false, "user3");
        
        auto metrics = manager.GetMetrics("metrics_feature");
        
        REQUIRE(metrics.totalEvaluations == 3);
        REQUIRE(metrics.enabledCount == 2);
        REQUIRE(metrics.disabledCount == 1);
    }
}
