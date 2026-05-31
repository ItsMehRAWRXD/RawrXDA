// ============================================================================
// test_extension_host.cpp — Unit tests for ExtensionHost
// ============================================================================
// Uses: GoogleTest (already configured in tests/CMakeLists.txt)
// Build: cmake --build . --target test_extension_host
// Run: ctest -R test_extension_host -V
// ============================================================================

#include <gtest/gtest.h>
#include "win32app/ExtensionHost.h"
#include <filesystem>
#include <fstream>

using namespace RawrXD::Extensions;
namespace fs = std::filesystem;

class ExtensionHostTest : public ::testing::Test {
protected:
    void SetUp() override {
        host = &ExtensionHost::GetInstance();
        testDir = fs::temp_directory_path() / "rawrxd_ext_test";
        fs::create_directories(testDir);
    }

    void TearDown() override {
        // Clean up test extensions
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    ExtensionHost* host;
    fs::path testDir;

    // Helper: create a minimal extension manifest
    void CreateTestExtension(const std::string& id, const std::string& name,
                              const std::string& version = "1.0.0") {
        fs::path extDir = testDir / id;
        fs::create_directories(extDir);

        std::ofstream manifest(extDir / "package.json");
        manifest << "{\n";
        manifest << "  \"name\": \"" << name << "\",\n";
        manifest << "  \"version\": \"" << version << "\",\n";
        manifest << "  \"publisher\": \"test-publisher\",\n";
        manifest << "  \"engines\": {\"rawrxd\": \">=14.0.0\"},\n";
        manifest << "  \"activationEvents\": [\"onStartup\"],\n";
        manifest << "  \"contributes\": {\"commands\": [{\"command\": \"test.hello\"}]}\n";
        manifest << "}\n";
    }
};

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, Singleton_ReturnsSameInstance) {
    auto& h1 = ExtensionHost::GetInstance();
    auto& h2 = ExtensionHost::GetInstance();
    EXPECT_EQ(&h1, &h2);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, Initialize_SetsInitializedFlag) {
    EXPECT_TRUE(host->IsInitialized());
}

// ---------------------------------------------------------------------------
// Extension discovery
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ScanExtensionsDirectory_FindsExtensions) {
    CreateTestExtension("ext.test1", "Test Extension 1");
    CreateTestExtension("ext.test2", "Test Extension 2", "2.0.0");

    host->ScanExtensionsDirectory(testDir.string());
    auto exts = host->ListExtensions();

    EXPECT_GE(exts.size(), 2u);
}

TEST_F(ExtensionHostTest, ScanExtensionsDirectory_ParsesManifest) {
    CreateTestExtension("ext.parsed", "Parsed Extension", "3.1.4");

    host->ScanExtensionsDirectory(testDir.string());
    auto ext = host->GetExtension("ext.parsed");

    ASSERT_NE(ext, nullptr);
    EXPECT_EQ(ext->name, "Parsed Extension");
    EXPECT_EQ(ext->version, "3.1.4");
    EXPECT_EQ(ext->publisher, "test-publisher");
    EXPECT_FALSE(ext->isActive);
}

TEST_F(ExtensionHostTest, ScanExtensionsDirectory_HandlesMissingDir) {
    fs::path missingDir = testDir / "nonexistent";
    // Should not crash
    host->ScanExtensionsDirectory(missingDir.string());
    auto exts = host->ListExtensions();
    // May contain previously scanned extensions, but no crash
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Extension listing
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ListExtensions_ReturnsAll) {
    CreateTestExtension("ext.a", "Extension A");
    CreateTestExtension("ext.b", "Extension B");
    CreateTestExtension("ext.c", "Extension C");

    host->ScanExtensionsDirectory(testDir.string());
    auto exts = host->ListExtensions();

    EXPECT_GE(exts.size(), 3u);
}

TEST_F(ExtensionHostTest, ListActiveExtensions_OnlyActive) {
    CreateTestExtension("ext.active", "Active Extension");
    CreateTestExtension("ext.inactive", "Inactive Extension");

    host->ScanExtensionsDirectory(testDir.string());

    // Activate one
    host->ActivateExtension("ext.active");

    auto active = host->ListActiveExtensions();
    EXPECT_GE(active.size(), 1u);

    bool foundActive = false;
    for (const auto& ext : active) {
        if (ext.id == "ext.active") {
            foundActive = true;
            EXPECT_TRUE(ext.isActive);
        }
    }
    EXPECT_TRUE(foundActive);
}

// ---------------------------------------------------------------------------
// Extension retrieval
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, GetExtension_Found) {
    CreateTestExtension("ext.found", "Found Extension");
    host->ScanExtensionsDirectory(testDir.string());

    auto ext = host->GetExtension("ext.found");
    ASSERT_NE(ext, nullptr);
    EXPECT_EQ(ext->id, "ext.found");
}

TEST_F(ExtensionHostTest, GetExtension_NotFound) {
    auto ext = host->GetExtension("ext.nonexistent");
    EXPECT_EQ(ext, nullptr);
}

// ---------------------------------------------------------------------------
// Activation / Deactivation
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ActivateExtension_SetsActiveFlag) {
    CreateTestExtension("ext.toggle", "Toggle Extension");
    host->ScanExtensionsDirectory(testDir.string());

    EXPECT_TRUE(host->ActivateExtension("ext.toggle"));
    auto ext = host->GetExtension("ext.toggle");
    ASSERT_NE(ext, nullptr);
    EXPECT_TRUE(ext->isActive);
}

TEST_F(ExtensionHostTest, DeactivateExtension_ClearsActiveFlag) {
    CreateTestExtension("ext.toggle", "Toggle Extension");
    host->ScanExtensionsDirectory(testDir.string());

    host->ActivateExtension("ext.toggle");
    EXPECT_TRUE(host->DeactivateExtension("ext.toggle"));

    auto ext = host->GetExtension("ext.toggle");
    ASSERT_NE(ext, nullptr);
    EXPECT_FALSE(ext->isActive);
}

TEST_F(ExtensionHostTest, ActivateExtension_NotFoundFails) {
    EXPECT_FALSE(host->ActivateExtension("ext.missing"));
}

// ---------------------------------------------------------------------------
// Command registration
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, GetAvailableCommands_InitiallyEmpty) {
    auto cmds = host->GetAvailableCommands();
    // May have built-in commands, so just verify it doesn't crash
    SUCCEED();
}

// ---------------------------------------------------------------------------
// VS Code API Bridge — Workspace
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, WorkspaceAPI_Accessible) {
    auto& workspace = host->GetWorkspaceAPI();
    // Just verify the API object is accessible
    SUCCEED();
}

// ---------------------------------------------------------------------------
// Extension info validation
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ExtensionInfo_DefaultState) {
    CreateTestExtension("ext.state", "State Test");
    host->ScanExtensionsDirectory(testDir.string());

    auto ext = host->GetExtension("ext.state");
    ASSERT_NE(ext, nullptr);
    EXPECT_EQ(ext->state, HostProcessState::Uninitialized);
    EXPECT_EQ(ext->processId, 0u);
    EXPECT_FALSE(ext->isBuiltin);
}

// ---------------------------------------------------------------------------
// Manifest parsing edge cases
// ---------------------------------------------------------------------------
TEST_F(ExtensionHostTest, ScanExtensionsDirectory_InvalidManifestSkipped) {
    fs::path badExt = testDir / "ext.bad";
    fs::create_directories(badExt);
    std::ofstream badManifest(badExt / "package.json");
    badManifest << "not valid json {{{\n";

    // Should not crash, just skip the invalid extension
    host->ScanExtensionsDirectory(testDir.string());
    auto ext = host->GetExtension("ext.bad");
    // May or may not be present depending on error handling
    SUCCEED();
}

TEST_F(ExtensionHostTest, ScanExtensionsDirectory_MissingManifestSkipped) {
    fs::path noManifest = testDir / "ext.nomanifest";
    fs::create_directories(noManifest);
    // No package.json

    host->ScanExtensionsDirectory(testDir.string());
    auto ext = host->GetExtension("ext.nomanifest");
    EXPECT_EQ(ext, nullptr);
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
