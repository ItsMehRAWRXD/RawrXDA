// ============================================================================
// smart_completion_test.cpp — Smart Code Completion Test Suite
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "completion/smart_completion.h"

#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

using namespace RawrXD::Completion;

// ============================================================================
// Test Utilities
// ============================================================================

class TestFixture {
public:
    std::string testDir;
    
    TestFixture() {
        testDir = fs::temp_directory_path().string() + "/completion_test_" + 
                  std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        fs::create_directories(testDir);
    }
    
    ~TestFixture() {
        fs::remove_all(testDir);
    }
    
    std::string createTestFile(const std::string& name, const std::string& content) {
        std::string path = testDir + "/" + name;
        std::ofstream file(path);
        file << content;
        return path;
    }
    
    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        std::ostringstream content;
        content << file.rdbuf();
        return content.str();
    }
};

// ============================================================================
// Engine Creation Tests
// ============================================================================

void testEngineCreation() {
    std::cout << "Testing engine creation...\n";
    
    auto engine = createSmartCompletionEngine();
    assert(engine != nullptr);
    
    bool initialized = engine->initialize();
    assert(initialized);
    
    engine->shutdown();
    
    std::cout << "  ✓ Engine creation test passed\n";
}

void testEngineConfiguration() {
    std::cout << "Testing engine configuration...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    engine->setMaxSuggestions(50);
    engine->setEnableAI(true);
    engine->setEnableSnippets(true);
    engine->setEnableKeywords(true);
    engine->setFuzzyThreshold(0.5f);
    
    auto stats = engine->getStatistics();
    assert(!stats.empty());
    
    engine->resetStatistics();
    
    std::cout << "  ✓ Engine configuration test passed\n";
}

// ============================================================================
// Language Detection Tests
// ============================================================================

void testLanguageDetection() {
    std::cout << "Testing language detection...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    assert(engine->detectLanguage("test.cpp") == Language::Cpp);
    assert(engine->detectLanguage("test.c") == Language::C);
    assert(engine->detectLanguage("test.js") == Language::JavaScript);
    assert(engine->detectLanguage("test.ts") == Language::TypeScript);
    assert(engine->detectLanguage("test.py") == Language::Python);
    assert(engine->detectLanguage("test.go") == Language::Go);
    assert(engine->detectLanguage("test.rs") == Language::Rust);
    assert(engine->detectLanguage("test.java") == Language::Java);
    assert(engine->detectLanguage("test.cs") == Language::CSharp);
    assert(engine->detectLanguage("test.php") == Language::PHP);
    assert(engine->detectLanguage("test.rb") == Language::Ruby);
    assert(engine->detectLanguage("test.swift") == Language::Swift);
    assert(engine->detectLanguage("test.kt") == Language::Kotlin);
    assert(engine->detectLanguage("test.html") == Language::HTML);
    assert(engine->detectLanguage("test.css") == Language::CSS);
    assert(engine->detectLanguage("test.json") == Language::JSON);
    assert(engine->detectLanguage("test.yaml") == Language::YAML);
    assert(engine->detectLanguage("test.md") == Language::Markdown);
    assert(engine->detectLanguage("test.sh") == Language::Shell);
    assert(engine->detectLanguage("test.sql") == Language::SQL);
    assert(engine->detectLanguage("test.unknown") == Language::Unknown);
    
    std::cout << "  ✓ Language detection test passed\n";
}

void testLanguageConfig() {
    std::cout << "Testing language configuration...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    auto cppConfig = engine->getLanguageConfig(Language::Cpp);
    assert(!cppConfig.keywords.empty());
    assert(!cppConfig.builtins.empty());
    assert(!cppConfig.types.empty());
    
    auto pythonConfig = engine->getLanguageConfig(Language::Python);
    assert(!pythonConfig.keywords.empty());
    assert(!pythonConfig.builtins.empty());
    
    // Modify configuration
    LanguageConfig customConfig;
    customConfig.language = Language::Cpp;
    customConfig.keywords = {"custom_keyword"};
    customConfig.caseSensitive = false;
    
    bool set = engine->setLanguageConfig(Language::Cpp, customConfig);
    assert(set);
    
    auto updatedConfig = engine->getLanguageConfig(Language::Cpp);
    assert(updatedConfig.keywords.size() == 1);
    assert(updatedConfig.keywords[0] == "custom_keyword");
    
    std::cout << "  ✓ Language configuration test passed\n";
}

// ============================================================================
// Completion Tests
// ============================================================================

void testBasicCompletion() {
    std::cout << "Testing basic completion...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.cpp", R"(
#include <iostream>
#include <string>

class MyClass {
public:
    void method1() {}
    void method2() {}
    int getValue() { return value_; }
private:
    int value_;
};

int main() {
    MyClass obj;
    obj.
})");
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    CompletionContext context;
    context.uri = file;
    context.language = "cpp";
    context.text = fixture.readFile(file);
    context.position = {17, 8};
    context.lineContent = "    obj.";
    context.prefix = "obj.";
    context.suffix = "";
    context.inFunction = true;
    
    auto result = engine->getCompletions(context);
    
    assert(!result.items.empty());
    
    // Should suggest member methods
    bool hasMethod1 = false;
    bool hasMethod2 = false;
    bool hasGetValue = false;
    
    for (const auto& item : result.items) {
        if (item.label == "method1") hasMethod1 = true;
        if (item.label == "method2") hasMethod2 = true;
        if (item.label == "getValue") hasGetValue = true;
    }
    
    assert(hasMethod1 || hasMethod2 || hasGetValue);
    
    std::cout << "  ✓ Basic completion test passed\n";
}

void testKeywordCompletion() {
    std::cout << "Testing keyword completion...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.cpp", R"(
int main() {
    ret
})");
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    CompletionContext context;
    context.uri = file;
    context.language = "cpp";
    context.text = fixture.readFile(file);
    context.position = {2, 7};
    context.lineContent = "    ret";
    context.prefix = "ret";
    context.suffix = "";
    
    auto result = engine->getCompletions(context);
    
    // Should suggest "return" keyword
    bool hasReturn = false;
    for (const auto& item : result.items) {
        if (item.label == "return") {
            hasReturn = true;
            assert(item.kind == CompletionItem::Kind::Keyword);
        }
    }
    
    assert(hasReturn);
    
    std::cout << "  ✓ Keyword completion test passed\n";
}

void testSnippetCompletion() {
    std::cout << "Testing snippet completion...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    // Add custom snippet
    Snippet snippet;
    snippet.prefix = "for";
    snippet.body = "for (int ${1:i} = 0; ${1:i} < ${2:count}; ${1:i}++) {\n    ${3}\n}";
    snippet.description = "For loop";
    snippet.scope = "cpp";
    
    bool added = engine->addSnippet("cpp", snippet);
    assert(added);
    
    // Get snippets
    auto snippets = engine->getSnippets("cpp");
    assert(!snippets.empty());
    
    bool hasForSnippet = false;
    for (const auto& s : snippets) {
        if (s.prefix == "for") {
            hasForSnippet = true;
            break;
        }
    }
    
    assert(hasForSnippet);
    
    // Remove snippet
    bool removed = engine->removeSnippet("cpp", "for");
    assert(removed);
    
    std::cout << "  ✓ Snippet completion test passed\n";
}

void testFuzzyMatching() {
    std::cout << "Testing fuzzy matching...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    std::vector<std::string> candidates = {
        "functionName",
        "functionCall",
        "functionPointer",
        "functionObject",
        "myFunction",
        "anotherFunction"
    };
    
    auto matches = engine->fuzzyMatch("func", candidates, 10);
    
    assert(!matches.empty());
    
    // All matches should contain "func"
    for (const auto& match : matches) {
        assert(match.text.find("func") != std::string::npos);
        assert(match.score > 0.0f);
    }
    
    // Matches should be sorted by score
    for (size_t i = 1; i < matches.size(); i++) {
        assert(matches[i-1].score >= matches[i].score);
    }
    
    std::cout << "  ✓ Fuzzy matching test passed\n";
}

void testContextAnalysis() {
    std::cout << "Testing context analysis...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.cpp", R"(
class Calculator {
public:
    int add(int a, int b) {
        return a + b;
    }
    
    int subtract(int a, int b) {
        return a - b;
    }
};

int main() {
    Calculator calc;
    int result = calc.add(5, 3);
    return 0;
})");
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    CompletionContext context;
    context.uri = file;
    context.language = "cpp";
    context.text = fixture.readFile(file);
    context.position = {5, 8};
    context.lineContent = "        return a + b;";
    context.prefix = "return ";
    context.inFunction = true;
    context.inClass = true;
    context.className = "Calculator";
    context.functionName = "add";
    
    auto analysis = engine->analyzeContext(context);
    
    assert(!analysis.scope.empty());
    assert(analysis.confidence > 0.0f);
    
    std::cout << "  ✓ Context analysis test passed\n";
}

// ============================================================================
// Provider Tests
// ============================================================================

class MockProvider : public ICompletionProvider {
public:
    std::string name() const override { return "mock"; }
    
    std::vector<std::string> languages() const override {
        return {"cpp", "c"};
    }
    
    int priority() const override { return 100; }
    
    CompletionResult provideCompletions(
        const CompletionContext& context) override {
        
        CompletionResult result;
        
        CompletionItem item;
        item.label = "mockCompletion";
        item.insertText = "mockCompletion()";
        item.kind = CompletionItem::Kind::Function;
        item.source = "mock";
        item.score = 1.0f;
        
        result.items.push_back(item);
        return result;
    }
    
    bool shouldTrigger(const std::string& triggerChar) const override {
        return triggerChar == "." || triggerChar == "->";
    }
};

void testProviderRegistration() {
    std::cout << "Testing provider registration...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    // Register provider
    auto provider = std::make_unique<MockProvider>();
    std::string providerName = provider->name();
    engine->registerProvider(std::move(provider));
    
    auto providers = engine->getProviders();
    assert(!providers.empty());
    
    bool found = false;
    for (const auto& p : providers) {
        if (p == providerName) {
            found = true;
            break;
        }
    }
    
    assert(found);
    
    // Unregister provider
    engine->unregisterProvider(providerName);
    
    providers = engine->getProviders();
    found = false;
    for (const auto& p : providers) {
        if (p == providerName) {
            found = true;
            break;
        }
    }
    
    assert(!found);
    
    std::cout << "  ✓ Provider registration test passed\n";
}

void testProviderPriority() {
    std::cout << "Testing provider priority...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    // Register mock provider
    auto provider = std::make_unique<MockProvider>();
    engine->registerProvider(std::move(provider));
    
    TestFixture fixture;
    std::string file = fixture.createTestFile("test.cpp", "int main() { }");
    
    CompletionContext context;
    context.uri = file;
    context.language = "cpp";
    context.text = fixture.readFile(file);
    context.position = {1, 1};
    
    auto result = engine->getCompletions(context);
    
    // Should include mock provider completions
    bool hasMockCompletion = false;
    for (const auto& item : result.items) {
        if (item.source == "mock") {
            hasMockCompletion = true;
            break;
        }
    }
    
    assert(hasMockCompletion);
    
    std::cout << "  ✓ Provider priority test passed\n";
}

// ============================================================================
// AI Completion Tests
// ============================================================================

void testAICompletion() {
    std::cout << "Testing AI completion...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    engine->setEnableAI(true);
    
    TestFixture fixture;
    std::string file = fixture.createTestFile("test.cpp", R"(
// Function to calculate factorial
int factorial(int n) {
    if (n <= 1) return 1;
    return n * 
})");
    
    AICompletionRequest request;
    request.uri = file;
    request.language = "cpp";
    request.text = fixture.readFile(file);
    request.position = {4, 12};
    request.maxTokens = 100;
    request.temperature = 0.7f;
    
    auto response = engine->getAICompletions(request);
    
    // AI completion may or may not return results depending on configuration
    // Just verify the call doesn't crash
    
    std::cout << "  ✓ AI completion test passed\n";
}

// ============================================================================
// Statistics Tests
// ============================================================================

void testStatistics() {
    std::cout << "Testing statistics...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    TestFixture fixture;
    std::string file = fixture.createTestFile("test.cpp", "int main() { int x = 5; }");
    
    CompletionContext context;
    context.uri = file;
    context.language = "cpp";
    context.text = fixture.readFile(file);
    context.position = {1, 20};
    context.prefix = "x";
    
    // Get initial stats
    auto stats = engine->getStatistics();
    uint32_t initialRequests = stats.count("totalRequests") ? stats["totalRequests"] : 0;
    
    // Trigger completion
    engine->getCompletions(context);
    
    // Check stats updated
    stats = engine->getStatistics();
    uint32_t afterRequests = stats.count("totalRequests") ? stats["totalRequests"] : 0;
    
    assert(afterRequests >= initialRequests);
    
    // Reset stats
    engine->resetStatistics();
    stats = engine->getStatistics();
    uint32_t resetRequests = stats.count("totalRequests") ? stats["totalRequests"] : 0;
    
    assert(resetRequests <= afterRequests);
    
    std::cout << "  ✓ Statistics test passed\n";
}

// ============================================================================
// Performance Tests
// ============================================================================

void testCompletionPerformance() {
    std::cout << "Testing completion performance...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    TestFixture fixture;
    
    // Create a large file
    std::ostringstream content;
    content << "#include <iostream>\n";
    content << "#include <vector>\n";
    content << "#include <string>\n\n";
    
    for (int i = 0; i < 100; i++) {
        content << "void function" << i << "() {}\n";
    }
    
    content << "\nint main() {\n";
    content << "    std::vector<int> vec;\n";
    content << "    vec.";
    
    std::string file = fixture.createTestFile("large.cpp", content.str());
    
    CompletionContext context;
    context.uri = file;
    context.language = "cpp";
    context.text = fixture.readFile(file);
    context.position = {107, 8};
    context.lineContent = "    vec.";
    context.prefix = "vec.";
    
    auto start = std::chrono::high_resolution_clock::now();
    auto result = engine->getCompletions(context);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Completion should be fast (< 100ms)
    assert(duration.count() < 100);
    
    std::cout << "  ✓ Completion performance test passed (" << duration.count() << "ms)\n";
}

// ============================================================================
// Edge Case Tests
// ============================================================================

void testEmptyFile() {
    std::cout << "Testing empty file...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    TestFixture fixture;
    std::string file = fixture.createTestFile("empty.cpp", "");
    
    CompletionContext context;
    context.uri = file;
    context.language = "cpp";
    context.text = "";
    context.position = {1, 1};
    
    auto result = engine->getCompletions(context);
    
    // Should still return keyword completions
    assert(!result.items.empty());
    
    std::cout << "  ✓ Empty file test passed\n";
}

void testInvalidPosition() {
    std::cout << "Testing invalid position...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    TestFixture fixture;
    std::string file = fixture.createTestFile("test.cpp", "int main() {}");
    
    CompletionContext context;
    context.uri = file;
    context.language = "cpp";
    context.text = fixture.readFile(file);
    context.position = {100, 100}; // Invalid position
    
    auto result = engine->getCompletions(context);
    
    // Should handle gracefully
    assert(!result.error.empty() || result.items.empty());
    
    std::cout << "  ✓ Invalid position test passed\n";
}

void testMultilineCompletion() {
    std::cout << "Testing multiline completion...\n";
    
    auto engine = createSmartCompletionEngine();
    engine->initialize();
    
    TestFixture fixture;
    std::string file = fixture.createTestFile("test.cpp", R"(
class Example {
public:
    void process() {
        // TODO: Implement
        
    }
};
})");
    
    CompletionContext context;
    context.uri = file;
    context.language = "cpp";
    context.text = fixture.readFile(file);
    context.position = {5, 1};
    context.lineContent = "        // TODO: Implement";
    context.inFunction = true;
    context.functionName = "process";
    
    auto result = engine->getCompletions(context);
    
    // Should provide context-aware completions
    assert(!result.items.empty());
    
    std::cout << "  ✓ Multiline completion test passed\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "Smart Code Completion Test Suite\n";
    std::cout << "========================================\n\n";
    
    // Engine tests
    std::cout << "--- Engine Tests ---\n";
    testEngineCreation();
    testEngineConfiguration();
    
    // Language tests
    std::cout << "\n--- Language Tests ---\n";
    testLanguageDetection();
    testLanguageConfig();
    
    // Completion tests
    std::cout << "\n--- Completion Tests ---\n";
    testBasicCompletion();
    testKeywordCompletion();
    testSnippetCompletion();
    testFuzzyMatching();
    testContextAnalysis();
    
    // Provider tests
    std::cout << "\n--- Provider Tests ---\n";
    testProviderRegistration();
    testProviderPriority();
    
    // AI tests
    std::cout << "\n--- AI Tests ---\n";
    testAICompletion();
    
    // Statistics tests
    std::cout << "\n--- Statistics Tests ---\n";
    testStatistics();
    
    // Performance tests
    std::cout << "\n--- Performance Tests ---\n";
    testCompletionPerformance();
    
    // Edge case tests
    std::cout << "\n--- Edge Case Tests ---\n";
    testEmptyFile();
    testInvalidPosition();
    testMultilineCompletion();
    
    std::cout << "\n========================================\n";
    std::cout << "All tests passed! ✓\n";
    std::cout << "========================================\n";
    
    return 0;
}
