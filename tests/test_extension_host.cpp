// ============================================================================
// test_extension_host.cpp — Unit tests for ExtensionHost
// ============================================================================
// Uses RawrXD's existing GoogleTest v1.14.0 infrastructure
// Note: Tests run without actual extension DLLs — tests the coordinator logic
// ============================================================================

#include <gtest/gtest.h>
#include "win32app/ExtensionHost.h"
#include <filesystem>
#include <fstream>

using namespace RawrXD::Extensions;

class ExtensionHostTest : public ::testing::Test {
protected:
    void SetUp() override {
        host = &ExtensionHost::GetInstance();
        // Ensure clean state
        if (host->IsInitialized()) {
            host->Shutdown();
        }
    }

    void TearDown() override {
        if (host->IsInitialized()) {
            host->Shutdown();
        }
    }

    ExtensionHost* host;
};

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, InitializeShutdown) {
    EXPECT_FALSE(host->IsInitialized());
    EXPECT_TRUE(host->Initialize());
    EXPECT_TRUE(host->IsInitialized());
    host->Shutdown();
    EXPECT_FALSE(host->IsInitialized());
}

TEST_F(ExtensionHostTest, DoubleInitialize) {
    EXPECT_TRUE(host->Initialize());
    EXPECT_TRUE(host->IsInitialized());
    // Second initialize should succeed (idempotent)
    EXPECT_TRUE(host->Initialize());
}

TEST_F(ExtensionHostTest, ShutdownWithoutInit) {
    // Should not crash
    host->Shutdown();
    EXPECT_FALSE(host->IsInitialized());
}

// ---------------------------------------------------------------------------
// Extension listing (empty state)
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ListExtensionsEmpty) {
    EXPECT_TRUE(host->Initialize());
    auto extensions = host->ListExtensions();
    EXPECT_EQ(extensions.size(), 0u);
}

TEST_F(ExtensionHostTest, ListActiveExtensionsEmpty) {
    EXPECT_TRUE(host->Initialize());
    auto active = host->ListActiveExtensions();
    EXPECT_EQ(active.size(), 0u);
}

TEST_F(ExtensionHostTest, GetExtensionNotFound) {
    EXPECT_TRUE(host->Initialize());
    auto ext = host->GetExtension("nonexistent");
    EXPECT_EQ(ext, nullptr);
}

// ---------------------------------------------------------------------------
// Command execution (no extensions loaded)
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ExecuteCommandNoExtensions) {
    EXPECT_TRUE(host->Initialize());
    // Should return false when no extensions are loaded
    EXPECT_FALSE(host->ExecuteCommand("test.command"));
}

TEST_F(ExtensionHostTest, GetAvailableCommandsEmpty) {
    EXPECT_TRUE(host->Initialize());
    auto commands = host->GetAvailableCommands();
    EXPECT_EQ(commands.size(), 0u);
}

// ---------------------------------------------------------------------------
// VS Code API Bridge (basic)
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, WorkspaceAPIAccessible) {
    EXPECT_TRUE(host->Initialize());
    // Should be able to get workspace API reference
    auto& workspaceAPI = host->GetWorkspaceAPI();
    // Just verify it doesn't crash
    (void)workspaceAPI;
    EXPECT_TRUE(true);
}

// ---------------------------------------------------------------------------
// Provider registration (no extensions)
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, RegisterCompletionProviderNoExtension) {
    EXPECT_TRUE(host->Initialize());
    // Should fail when extension doesn't exist
    EXPECT_FALSE(host->RegisterCompletionProvider(
        "nonexistent", "cpp", nullptr));
}

TEST_F(ExtensionHostTest, RegisterHoverProviderNoExtension) {
    EXPECT_TRUE(host->Initialize());
    EXPECT_FALSE(host->RegisterHoverProvider(
        "nonexistent", "cpp", nullptr));
}

// ---------------------------------------------------------------------------
// Messaging (no extensions)
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, SendMessageNoExtensions) {
    EXPECT_TRUE(host->Initialize());
    EXPECT_FALSE(host->SendMessageToExtension(
        "from", "to", "hello"));
}

// ---------------------------------------------------------------------------
// Activation events
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, FireActivationEventNoCrash) {
    EXPECT_TRUE(host->Initialize());
    // Should not crash even with no extensions
    host->FireActivationEvent("onCommand", "test.command");
    EXPECT_TRUE(true);
}

// ---------------------------------------------------------------------------
// Telemetry
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, LogTelemetryNoCrash) {
    EXPECT_TRUE(host->Initialize());
    std::map<std::string, std::string> props;
    props["key"] = "value";
    // Should not crash even with no extensions
    host->LogExtensionTelemetry("test.ext", "test_event", props);
    EXPECT_TRUE(true);
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, StatsEmpty) {
    EXPECT_TRUE(host->Initialize());
    auto stats = host->GetStats();
    EXPECT_EQ(stats.totalExtensions, 0u);
    EXPECT_EQ(stats.activeExtensions, 0u);
    EXPECT_EQ(stats.crashedExtensions, 0u);
    EXPECT_EQ(stats.totalMessagesExchanged, 0u);
}

// ---------------------------------------------------------------------------
// Manifest validation
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ValidateInvalidManifest) {
    EXPECT_TRUE(host->Initialize());

    // Create a temporary invalid manifest file
    std::string tempPath = std::filesystem::temp_directory_path().string() + "/invalid_manifest.json";
    {
        std::ofstream f(tempPath);
        f << "{ invalid json }";
    }

    std::vector<std::string> errors, warnings;
    bool valid = host->ValidateExtensionManifest(tempPath, errors, warnings);

    std::filesystem::remove(tempPath);

    EXPECT_FALSE(valid);
    EXPECT_GT(errors.size(), 0u);
}

TEST_F(ExtensionHostTest, ValidateValidManifest) {
    EXPECT_TRUE(host->Initialize());

    std::string tempPath = std::filesystem::temp_directory_path().string() + "/valid_manifest.json";
    {
        std::ofstream f(tempPath);
        f << R"({
            "name": "test-extension",
            "version": "1.0.0",
            "publisher": "test",
            "engines": { "vscode": "^1.0.0" },
            "activationEvents": ["onCommand:test.hello"]
        })";
    }

    std::vector<std::string> errors, warnings;
    bool valid = host->ValidateExtensionManifest(tempPath, errors, warnings);

    std::filesystem::remove(tempPath);

    EXPECT_TRUE(valid);
    EXPECT_EQ(errors.size(), 0u);
}

// ---------------------------------------------------------------------------
// Dependency resolution
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ResolveDependenciesNotFound) {
    EXPECT_TRUE(host->Initialize());
    auto deps = host->ResolveDependencies("nonexistent");
    EXPECT_EQ(deps.size(), 0u);
}

// ---------------------------------------------------------------------------
// Event broadcasting
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, BroadcastEventNoHandlers) {
    EXPECT_TRUE(host->Initialize());
    // Should not crash with no handlers registered
    host->BroadcastEvent("test_event", "data");
    EXPECT_TRUE(true);
}

TEST_F(ExtensionHostTest, RegisterAndFireEventHandler) {
    EXPECT_TRUE(host->Initialize());

    bool fired = false;
    std::string receivedEvent, receivedData;

    host->RegisterEventHandler("test_event", [&](const std::string& event, const std::string& data) {
        fired = true;
        receivedEvent = event;
        receivedData = data;
    });

    host->BroadcastEvent("test_event", "hello");

    EXPECT_TRUE(fired);
    EXPECT_EQ(receivedEvent, "test_event");
    EXPECT_EQ(receivedData, "hello");
}

// ---------------------------------------------------------------------------
// Environment API
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, GetMachineId) {
    EXPECT_TRUE(host->Initialize());
    auto id = host->GetMachineId();
    EXPECT_FALSE(id.empty());
}

TEST_F(ExtensionHostTest, GetSessionId) {
    EXPECT_TRUE(host->Initialize());
    auto id = host->GetSessionId();
    EXPECT_FALSE(id.empty());
}

// ---------------------------------------------------------------------------
// Window API (no crash)
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ShowMessagesNoCrash) {
    EXPECT_TRUE(host->Initialize());
    host->ShowInformationMessage("test", "Hello");
    host->ShowWarningMessage("test", "Warning");
    host->ShowErrorMessage("test", "Error");
    EXPECT_TRUE(true);
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
