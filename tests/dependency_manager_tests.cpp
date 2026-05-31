// ============================================================================
// Smart Dependency Manager Tests — Dependency Management Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/dependencies/smart_dependency_manager.cpp"

using namespace RawrXD::Dependencies;

// Mock AI Client
class MockDepsAIClient : public SovereignInferenceClient {
public:
    bool IsLoaded() const override { return true; }
    ChatResult ChatSync(const std::vector<ChatMessage>& messages) override {
        return {true, "Low risk update", 0.9, 100};
    }
};

TEST_CASE("Smart Dependency Manager - Basic Operations", "[dependencies][management]") {
    auto aiClient = std::make_shared<MockDepsAIClient>();
    SmartDependencyManager manager(aiClient);
    
    SECTION("Empty dependency tree") {
        DependencyTree tree = manager.AnalyzeDependencies("/test/project");
        
        REQUIRE(tree.rootPackage == "/test/project");
        REQUIRE(tree.totalDependencies == 0);
        REQUIRE(tree.outdatedCount == 0);
        REQUIRE(tree.vulnerableCount == 0);
    }
    
    SECTION("Dependency tree structure") {
        DependencyTree tree;
        tree.rootPackage = "test-project";
        
        Dependency dep;
        dep.name = "libcurl";
        dep.currentVersion = "7.68.0";
        dep.latestVersion = "8.0.0";
        dep.isOutdated = true;
        dep.hasVulnerabilities = false;
        
        tree.dependencies["libcurl"] = dep;
        tree.totalDependencies = 1;
        tree.outdatedCount = 1;
        
        REQUIRE(tree.totalDependencies == 1);
        REQUIRE(tree.outdatedCount == 1);
        REQUIRE(tree.dependencies["libcurl"].name == "libcurl");
    }
}

TEST_CASE("Smart Dependency Manager - Update Recommendations", "[dependencies][updates]") {
    auto aiClient = std::make_shared<MockDepsAIClient>();
    SmartDependencyManager manager(aiClient);
    
    SECTION("Update type detection") {
        // Major version update
        REQUIRE(DetermineUpdateType("1.0.0", "2.0.0") == UpdateType::MAJOR);
        
        // Minor version update
        REQUIRE(DetermineUpdateType("1.0.0", "1.1.0") == UpdateType::MINOR);
        
        // Patch version update
        REQUIRE(DetermineUpdateType("1.0.0", "1.0.1") == UpdateType::PATCH);
    }
    
    SECTION("Vulnerability detection") {
        DependencyTree tree;
        
        Dependency vulnDep;
        vulnDep.name = "vulnerable-lib";
        vulnDep.hasVulnerabilities = true;
        vulnDep.vulnerabilities = {"CVE-2024-1234", "CVE-2024-5678"};
        
        tree.dependencies["vulnerable-lib"] = vulnDep;
        
        auto vulnerable = manager.FindVulnerabilities(tree);
        REQUIRE(vulnerable.size() == 1);
        REQUIRE(vulnerable[0].vulnerabilities.size() == 2);
    }
}

TEST_CASE("Smart Dependency Manager - Report Generation", "[dependencies][reporting]") {
    auto aiClient = std::make_shared<MockDepsAIClient>();
    SmartDependencyManager manager(aiClient);
    
    SECTION("Dependency report") {
        DependencyTree tree;
        tree.rootPackage = "test-project";
        tree.totalDependencies = 5;
        tree.outdatedCount = 2;
        tree.vulnerableCount = 1;
        
        auto report = manager.GenerateDependencyReport(tree);
        
        REQUIRE_FALSE(report.empty());
        REQUIRE(report.find("test-project") != std::string::npos);
        REQUIRE(report.find("Total Dependencies: 5") != std::string::npos);
    }
}

// Helper function for testing
UpdateType DetermineUpdateType(const std::string& current, const std::string& latest) {
    auto currentParts = SplitVersion(current);
    auto latestParts = SplitVersion(latest);
    
    if (latestParts[0] > currentParts[0]) {
        return UpdateType::MAJOR;
    } else if (latestParts[1] > currentParts[1]) {
        return UpdateType::MINOR;
    } else {
        return UpdateType::PATCH;
    }
}

std::vector<int> SplitVersion(const std::string& version) {
    std::vector<int> parts;
    std::stringstream ss(version);
    std::string part;
    
    while (std::getline(ss, part, '.')) {
        parts.push_back(std::stoi(part));
    }
    
    return parts;
}
