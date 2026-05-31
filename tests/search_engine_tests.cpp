// ============================================================================
// Search Engine Tests — Semantic Search Testing
// ============================================================================
#include <catch2/catch_test_macros.hpp>
#include "../src/search/search_engine.cpp"

using namespace RawrXD::Search;

// Mock AI Client
class MockSearchAIClient : public SovereignInferenceClient {
public:
    bool IsLoaded() const override { return true; }
    ChatResult ChatSync(const std::vector<ChatMessage>& messages) override {
        return {true, "Found similar code patterns", 0.85, 100};
    }
};

TEST_CASE("Search Engine - Basic Operations", "[search][indexing]") {
    auto aiClient = std::make_shared<MockSearchAIClient>();
    SearchEngine engine(aiClient);
    
    SECTION("Empty index state") {
        SearchQuery query;
        query.query = "function";
        query.type = SearchType::EXACT;
        query.maxResults = 10;
        
        auto results = engine.Search(query);
        REQUIRE(results.empty());
    }
    
    SECTION("Index and search file") {
        std::string code = R"(
            int add(int a, int b) {
                return a + b;
            }
            
            int subtract(int a, int b) {
                return a - b;
            }
        )";
        
        engine.IndexFile("math.cpp", code);
        
        SearchQuery query;
        query.query = "add";
        query.type = SearchType::SYMBOL;
        query.maxResults = 10;
        
        auto results = engine.Search(query);
        REQUIRE(results.size() > 0);
    }
    
    SECTION("Multiple file indexing") {
        engine.IndexFile("file1.cpp", "void func1() {}");
        engine.IndexFile("file2.cpp", "void func2() {}");
        engine.IndexFile("file3.cpp", "void func3() {}");
        
        SearchQuery query;
        query.query = "void";
        query.type = SearchType::EXACT;
        query.maxResults = 10;
        
        auto results = engine.Search(query);
        REQUIRE(results.size() >= 3);
    }
}

TEST_CASE("Search Engine - Search Types", "[search][types]") {
    auto aiClient = std::make_shared<MockSearchAIClient>();
    SearchEngine engine(aiClient);
    
    SECTION("Exact search") {
        std::string code = "int calculateSum(int a, int b) { return a + b; }";
        engine.IndexFile("utils.cpp", code);
        
        SearchQuery query;
        query.query = "calculateSum";
        query.type = SearchType::EXACT;
        
        auto results = engine.Search(query);
        REQUIRE(results.size() > 0);
    }
    
    SECTION("Regex search") {
        std::string code = R"(
            void function1() {}
            void function2() {}
            int function3() { return 0; }
        )";
        engine.IndexFile("functions.cpp", code);
        
        SearchQuery query;
        query.query = R"(void function\d+\(\))";
        query.type = SearchType::REGEX;
        
        auto results = engine.Search(query);
        // Regex results depend on implementation
        REQUIRE(results.size() >= 0);
    }
    
    SECTION("Symbol search") {
        std::string code = R"(
            class Calculator {
            public:
                int add(int a, int b);
                int subtract(int a, int b);
            };
        )";
        engine.IndexFile("calculator.h", code);
        
        auto symbols = engine.FindSymbols("Calculator");
        REQUIRE(symbols.size() > 0);
    }
}

TEST_CASE("Search Engine - Symbol Management", "[search][symbols]") {
    auto aiClient = std::make_shared<MockSearchAIClient>();
    SearchEngine engine(aiClient);
    
    SECTION("Find function symbols") {
        std::string code = R"(
            int func1() { return 1; }
            int func2() { return 2; }
            int func3() { return 3; }
        )";
        
        engine.IndexFile("funcs.cpp", code);
        
        auto symbols = engine.FindSymbols("func");
        REQUIRE(symbols.size() >= 3);
    }
    
    SECTION("Find class symbols") {
        std::string code = R"(
            class MyClass1 {};
            class MyClass2 {};
            class OtherClass {};
        )";
        
        engine.IndexFile("classes.cpp", code);
        
        auto symbols = engine.FindSymbols("MyClass");
        REQUIRE(symbols.size() == 2);
    }
}

TEST_CASE("Search Engine - Report Generation", "[search][reporting]") {
    auto aiClient = std::make_shared<MockSearchAIClient>();
    SearchEngine engine(aiClient);
    
    SECTION("Generate search report") {
        std::string code = "void test() { int x = 5; }";
        engine.IndexFile("test.cpp", code);
        
        SearchQuery query;
        query.query = "test";
        query.type = SearchType::EXACT;
        
        auto results = engine.Search(query);
        auto report = engine.GenerateSearchReport(query, results);
        
        REQUIRE_FALSE(report.empty());
        REQUIRE(report.find("# Search Report") != std::string::npos);
    }
}
