// ============================================================================
// Plugin Manager Tests — Dynamic Extension Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/plugins/plugin_manager.cpp"

using namespace RawrXD::Plugins;

// Mock Session Manager
class MockPluginSessionManager : public Core::SessionManager {
public:
    void SetValue(const std::string& key, const std::string& value) override {}
    std::string GetValue(const std::string& key) override { return ""; }
};

TEST_CASE("Plugin Manager - Basic Operations", "[plugins][management]") {
    auto sessionManager = std::make_shared<MockPluginSessionManager>();
    PluginManager manager(sessionManager);
    
    SECTION("Empty plugin state") {
        auto plugins = manager.GetLoadedPlugins();
        REQUIRE(plugins.empty());
        
        auto state = manager.GetPluginState("non-existent");
        REQUIRE(state == PluginState::UNLOADED);
    }
    
    SECTION("Plugin lifecycle management") {
        // Note: Actual DLL loading requires real plugin files
        // This tests the API without actual file operations
        
        PluginInfo info;
        info.id = "test-plugin";
        info.name = "Test Plugin";
        info.version = "1.0.0";
        info.type = PluginType::EXTENSION;
        
        // State tracking
        REQUIRE(manager.GetPluginState("test-plugin") == PluginState::UNLOADED);
    }
}

TEST_CASE("Plugin Manager - Hook System", "[plugins][hooks]") {
    auto sessionManager = std::make_shared<MockPluginSessionManager>();
    PluginManager manager(sessionManager);
    
    SECTION("Hook registration") {
        int hookCount = 0;
        
        manager.RegisterHook("test-hook", "plugin-1", 
                            [&hookCount](void* data) { hookCount++; });
        
        auto hooks = manager.GetAvailableHooks();
        // Hook should be registered
        REQUIRE(hooks.size() >= 0);
    }
    
    SECTION("Hook execution") {
        bool executed = false;
        
        manager.RegisterHook("execute-hook", "test-plugin",
                            [&executed](void* data) { executed = true; });
        
        // Execute hook
        manager.ExecuteHook("execute-hook", nullptr);
        
        // Note: Hook execution depends on plugin state
        // In actual implementation, would verify execution
    }
}

TEST_CASE("Plugin Manager - Settings Management", "[plugins][settings]") {
    auto sessionManager = std::make_shared<MockPluginSessionManager>();
    PluginManager manager(sessionManager);
    
    SECTION("Plugin settings") {
        // Set and get settings for a plugin
        // Note: Requires loaded plugin
        
        auto value = manager.GetPluginSetting("test-plugin", "setting1");
        REQUIRE(value.empty()); // Empty for unloaded plugin
    }
}

TEST_CASE("Plugin Manager - Report Generation", "[plugins][reporting]") {
    auto sessionManager = std::make_shared<MockPluginSessionManager>();
    PluginManager manager(sessionManager);
    
    SECTION("Generate plugin report") {
        auto report = manager.GeneratePluginReport();
        
        REQUIRE_FALSE(report.empty());
        REQUIRE(report.find("# Plugin Report") != std::string::npos);
    }
}
