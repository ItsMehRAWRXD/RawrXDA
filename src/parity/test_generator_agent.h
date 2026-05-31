// test_generator_agent.h - /tests-style unit-test skeleton generator
// Feature 12/15 (Copilot parity).
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace rawrxd::parity {

enum class TestFramework {
    Auto,
    GTest,         // C++ googletest
    Catch2,        // C++ catch2
    PyTest,        // Python
    Jest,          // JS/TS
    Mocha,         // JS/TS
    RustBuiltin,   // #[test]
    XCTest,        // Swift
    NUnit,         // C#
};

struct FunctionSignature {
    std::string name;
    std::string return_type;
    std::vector<std::pair<std::string, std::string>> params; // (type, name)
};

struct TestGenRequest {
    std::string language;           // e.g. "cpp"
    TestFramework framework{TestFramework::Auto};
    std::string module_or_class;    // optional
    std::vector<FunctionSignature> targets;
    std::string header_include;     // for cpp: #include "foo.h"
};

struct TestGenOutput {
    std::string framework_name;     // human readable
    std::string suggested_path;
    std::string source;             // generated test source
};

class TestGeneratorAgent {
public:
    TestGenOutput generate(const TestGenRequest& req) const;

    // Parse C-family function signatures out of a source blob (cheap/heuristic).
    static std::vector<FunctionSignature> extract_cpp_functions(std::string_view src);

    static TestFramework autodetect(std::string_view language);
};

} // namespace rawrxd::parity
