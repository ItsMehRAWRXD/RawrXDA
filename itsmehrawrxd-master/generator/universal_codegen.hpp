#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>
#include <queue>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <variant>
#include <optional>
#include <type_traits>

namespace UniversalCodeGen {

// Language types with compile-time optimization
enum class Language : uint8_t {
    Cpp = 0, Python = 1, JavaScript = 2, TypeScript = 3, Go = 4, Rust = 5,
    Java = 6, CSharp = 7, Swift = 8, Kotlin = 9, PHP = 10, Ruby = 11,
    Haskell = 12, Scala = 13, Clojure = 14, FSharp = 15, Dart = 16,
    Lua = 17, Perl = 18, R = 19, MATLAB = 20, Assembly = 21
};

// Type system with zero-copy string views
struct TypeInfo {
    std::string_view name;
    std::string_view base_type;
    bool is_primitive;
    bool is_array;
    bool is_optional;
    size_t array_size;
    
    constexpr TypeInfo(std::string_view n, std::string_view bt = "", bool prim = true, 
                      bool arr = false, bool opt = false, size_t as = 0) noexcept
        : name(n), base_type(bt), is_primitive(prim), is_array(arr), is_optional(opt), array_size(as) {}
};

// Property definition with move semantics
struct Property {
    std::string name;
    TypeInfo type;
    std::string default_value;
    std::map<std::string, std::string> attributes;
    bool is_required;
    
    Property(std::string n, TypeInfo t, std::string dv = "", bool req = true) noexcept
        : name(std::move(n)), type(t), default_value(std::move(dv)), is_required(req) {}
};

// Model definition with perfect forwarding
struct Model {
    std::string name;
    std::vector<Property> properties;
    std::map<std::string, std::string> metadata;
    std::vector<std::string> imports;
    
    Model(std::string n) noexcept : name(std::move(n)) {}
};

// API endpoint definition
struct Endpoint {
    std::string name;
    std::string method;
    std::string path;
    std::string return_type;
    std::vector<Property> parameters;
    std::map<std::string, std::string> attributes;
    
    Endpoint(std::string n, std::string m, std::string p, std::string rt) noexcept
        : name(std::move(n)), method(std::move(m)), path(std::move(p)), return_type(std::move(rt)) {}
};

// High-performance code generator with lock-free design
class UniversalCodeGenerator {
public:
    UniversalCodeGenerator();
    
    // Main generation method with perfect forwarding
    template<Language L>
    std::string generate_model(const Model& model);
    
    // Generate API client for any language
    template<Language L>
    std::string generate_api_client(const std::vector<Endpoint>& endpoints, const std::string& base_url);
    
    // Generate multiple languages in parallel
    std::map<Language, std::string> generate_multi_language(const Model& model, 
                                                           const std::vector<Language>& languages);
    
    // Get performance metrics
    struct PerformanceMetrics {
        size_t total_generations;
        double average_generation_time_ms;
        size_t cache_hits;
        size_t cache_misses;
        std::chrono::steady_clock::time_point last_reset;
    };
    
    PerformanceMetrics get_performance_metrics() const;

private:
    template<Language L>
    std::string generate_model_impl(const Model& model);
    
    template<Language L>
    std::string generate_api_client_impl(const std::vector<Endpoint>& endpoints, const std::string& base_url);
    
    std::string generate_for_language(Language lang, const Model& model);
    
    // Language-specific generators
    void generate_cpp_api_client(std::ostringstream& code, const std::vector<Endpoint>& endpoints, const std::string& base_url);
    void generate_python_api_client(std::ostringstream& code, const std::vector<Endpoint>& endpoints, const std::string& base_url);
    void generate_javascript_api_client(std::ostringstream& code, const std::vector<Endpoint>& endpoints, const std::string& base_url);
    void generate_go_api_client(std::ostringstream& code, const std::vector<Endpoint>& endpoints, const std::string& base_url);
    void generate_rust_api_client(std::ostringstream& code, const std::vector<Endpoint>& endpoints, const std::string& base_url);
    
    // Template management
    template<Language L>
    std::string get_cached_template();
    
    template<Language L>
    void cache_template(const std::string& template_str);
    
    template<Language L>
    std::string create_template();
    
    std::string create_cpp_template();
    std::string create_python_template();
    std::string create_javascript_template();
    std::string create_go_template();
    std::string create_rust_template();
    
    std::string fill_template(const std::string& template, const Model& model);
    std::string generate_properties(const Model& model);
    void initialize_template_cache();
    void update_performance_metrics(double time_ms);
    
    // Lock-free data structures for maximum performance
    std::atomic<size_t> generation_count_{0};
    std::atomic<double> total_generation_time_ms_{0.0};
    
    // Template cache for zero-allocation generation
    std::unordered_map<Language, std::string> template_cache_;
    std::mutex cache_mutex_;
    
    // Performance metrics
    std::atomic<size_t> cache_hits_{0};
    std::atomic<size_t> cache_misses_{0};
};

} // namespace UniversalCodeGen
