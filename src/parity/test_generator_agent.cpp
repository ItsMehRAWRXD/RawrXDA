// test_generator_agent.cpp - Implementation
#include "test_generator_agent.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace rawrxd::parity {

namespace {
std::string lower(std::string_view v) {
    std::string s(v);
    for (auto& c : s) c = static_cast<char>(std::tolower((unsigned char)c));
    return s;
}
std::string camel_to_snake(std::string_view v) {
    std::string o;
    for (std::size_t i = 0; i < v.size(); ++i) {
        char c = v[i];
        if (i > 0 && std::isupper((unsigned char)c)) o.push_back('_');
        o.push_back(static_cast<char>(std::tolower((unsigned char)c)));
    }
    return o;
}
} // namespace

TestFramework TestGeneratorAgent::autodetect(std::string_view language) {
    std::string l = lower(language);
    if (l == "cpp" || l == "c++" || l == "cxx" || l == "cc" || l == "c")   return TestFramework::GTest;
    if (l == "py"  || l == "python")                                        return TestFramework::PyTest;
    if (l == "js"  || l == "ts" || l == "tsx" || l == "jsx")                return TestFramework::Jest;
    if (l == "rs"  || l == "rust")                                          return TestFramework::RustBuiltin;
    if (l == "swift")                                                       return TestFramework::XCTest;
    if (l == "cs"  || l == "csharp" || l == "c#")                           return TestFramework::NUnit;
    return TestFramework::GTest;
}

std::vector<FunctionSignature> TestGeneratorAgent::extract_cpp_functions(std::string_view src) {
    // Heuristic: match `ret name(params) { ... }` at reasonable line starts.
    // Not a real parser; good enough for stub generation.
    static const std::regex rx(R"(^([A-Za-z_][A-Za-z_0-9:<>\*&\s]*?)\s+([A-Za-z_][A-Za-z_0-9]*)\s*\(([^;{)]*)\)\s*(?:const\s*)?\{)",
                               std::regex::ECMAScript);
    std::vector<FunctionSignature> out;
    std::string s(src);
    auto begin = std::sregex_iterator(s.begin(), s.end(), rx);
    auto end   = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        FunctionSignature sig;
        sig.return_type = (*it)[1].str();
        sig.name        = (*it)[2].str();
        std::string params = (*it)[3].str();
        std::stringstream ss(params);
        std::string piece;
        while (std::getline(ss, piece, ',')) {
            auto pos = piece.find_last_of(" \t*&");
            if (pos == std::string::npos) continue;
            std::string type = piece.substr(0, pos + 1);
            std::string name = piece.substr(pos + 1);
            sig.params.emplace_back(type, name);
        }
        // Skip accidental matches on keywords.
        if (sig.name == "if" || sig.name == "for" || sig.name == "while" ||
            sig.name == "switch" || sig.name == "return") continue;
        out.push_back(std::move(sig));
    }
    return out;
}

TestGenOutput TestGeneratorAgent::generate(const TestGenRequest& req) const {
    TestGenOutput o;
    TestFramework fw = (req.framework == TestFramework::Auto)
                           ? autodetect(req.language) : req.framework;

    std::ostringstream os;
    switch (fw) {
        case TestFramework::GTest: {
            o.framework_name = "googletest";
            o.suggested_path = "tests/test_" +
                               (req.module_or_class.empty() ? std::string("module") :
                                camel_to_snake(req.module_or_class)) + ".cpp";
            os << "#include <gtest/gtest.h>\n";
            if (!req.header_include.empty()) os << req.header_include << "\n";
            os << "\n";
            for (const auto& fn : req.targets) {
                os << "TEST(" << (req.module_or_class.empty() ? "Suite" : req.module_or_class)
                   << ", " << fn.name << "_HappyPath) {\n"
                   << "    // TODO: construct inputs\n"
                   << "    // " << fn.return_type << " result = " << fn.name << "(...);\n"
                   << "    SUCCEED();\n"
                   << "}\n\n";
            }
            if (req.targets.empty()) os << "TEST(Suite, Placeholder) { SUCCEED(); }\n";
            break;
        }
        case TestFramework::Catch2: {
            o.framework_name = "catch2";
            o.suggested_path = "tests/test_" + camel_to_snake(req.module_or_class) + ".cpp";
            os << "#define CATCH_CONFIG_MAIN\n#include <catch2/catch_all.hpp>\n";
            if (!req.header_include.empty()) os << req.header_include << "\n";
            os << "\n";
            for (const auto& fn : req.targets)
                os << "TEST_CASE(\"" << fn.name << " happy path\") { SUCCEED(); }\n";
            break;
        }
        case TestFramework::PyTest: {
            o.framework_name = "pytest";
            o.suggested_path = "tests/test_" + (req.module_or_class.empty() ? "module"
                                                                            : camel_to_snake(req.module_or_class)) + ".py";
            os << "import pytest\n\n";
            if (req.targets.empty()) os << "def test_placeholder():\n    assert True\n";
            for (const auto& fn : req.targets)
                os << "def test_" << camel_to_snake(fn.name) << "_happy_path():\n"
                   << "    # TODO: exercise " << fn.name << "\n"
                   << "    assert True\n\n";
            break;
        }
        case TestFramework::Jest: {
            o.framework_name = "jest";
            o.suggested_path = "tests/" + (req.module_or_class.empty() ? "module" : req.module_or_class) + ".test.ts";
            for (const auto& fn : req.targets)
                os << "test('" << fn.name << " happy path', () => {\n  expect(true).toBe(true);\n});\n";
            if (req.targets.empty()) os << "test('placeholder', () => { expect(true).toBe(true); });\n";
            break;
        }
        case TestFramework::Mocha: {
            o.framework_name = "mocha";
            o.suggested_path = "tests/" + req.module_or_class + ".spec.ts";
            os << "import { expect } from 'chai';\n";
            for (const auto& fn : req.targets)
                os << "describe('" << fn.name << "', () => { it('works', () => { expect(true).to.equal(true); }); });\n";
            break;
        }
        case TestFramework::RustBuiltin: {
            o.framework_name = "rust #[test]";
            o.suggested_path = "src/" + camel_to_snake(req.module_or_class) + "_tests.rs";
            os << "#[cfg(test)]\nmod tests {\n    use super::*;\n";
            for (const auto& fn : req.targets)
                os << "    #[test]\n    fn " << camel_to_snake(fn.name) << "_happy() { assert!(true); }\n";
            if (req.targets.empty()) os << "    #[test]\n    fn placeholder() { assert!(true); }\n";
            os << "}\n";
            break;
        }
        case TestFramework::XCTest: {
            o.framework_name = "xctest";
            o.suggested_path = "Tests/" + req.module_or_class + "Tests.swift";
            os << "import XCTest\n\nfinal class " << req.module_or_class << "Tests: XCTestCase {\n";
            for (const auto& fn : req.targets)
                os << "    func test" << fn.name << "_happy() { XCTAssertTrue(true) }\n";
            os << "}\n";
            break;
        }
        case TestFramework::NUnit: {
            o.framework_name = "nunit";
            o.suggested_path = "Tests/" + req.module_or_class + "Tests.cs";
            os << "using NUnit.Framework;\n\npublic class " << req.module_or_class << "Tests {\n";
            for (const auto& fn : req.targets)
                os << "    [Test] public void " << fn.name << "_Happy() { Assert.IsTrue(true); }\n";
            os << "}\n";
            break;
        }
        default: break;
    }
    o.source = os.str();
    return o;
}

} // namespace rawrxd::parity
