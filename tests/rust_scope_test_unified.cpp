// rust_scope_test_unified.cpp
// Unified test: stub AST + rust_parser_v2 implementation + test harness in one TU.
// Compile: g++ -O2 -std=c++20 -I d:\rawrxd\tests -I d:\rawrxd\src\core -o rust_scope_test.exe rust_scope_test_unified.cpp

// 1. Stub AST (must come first — defines RawrXD::AST before real headers are included)
#include "ast_graph_engine.hpp"

// 2. Rust parser implementation (includes rust_parser.hpp which now sees the stub)
#include "rust_parser_v2.cpp"

// 3. Test harness
#include <cstdio>
#include <cstring>
#include <vector>

using namespace rawrxd::ast::rust;

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_passed; printf("  [PASS] %s\n", msg); } \
    else { ++g_failed; printf("  [FAIL] %s\n", msg); } \
} while(0)

static ASTNode::Ptr findNodeWithText(const std::vector<ASTNode::Ptr>& nodes, const char* substr) {
    for (auto& n : nodes) {
        if (n->getText().find(substr) != std::string::npos) return n;
    }
    return nullptr;
}

static void test_pub_crate_visibility() {
    printf("\n[Test 1] pub(crate) visibility boundary\n");
    const char* code = R"(
        pub(crate) fn internal_helper() -> i32 { 42 }
        pub fn public_api() -> i32 { internal_helper() }
        fn private_fn() {}
    )";
    RustParser parser;
    auto result = parser.parse(code, "test1.rs");
    CHECK(result.success, "parse succeeded");
    CHECK(result.nodes.size() == 3, "three top-level items");

    auto pub_crate = findNodeWithText(result.nodes, "pub(crate) fn internal_helper");
    auto pub_fn    = findNodeWithText(result.nodes, "pub fn public_api");
    auto priv_fn   = findNodeWithText(result.nodes, "fn private_fn");

    CHECK(pub_crate != nullptr, "pub(crate) fn found");
    CHECK(pub_fn != nullptr, "pub fn found");
    CHECK(priv_fn != nullptr, "private fn found");

    if (pub_crate && pub_fn && priv_fn) {
        CHECK(pub_crate->getText().find("pub(crate)") != std::string::npos,
              "pub(crate) text preserved");
        CHECK(pub_fn->getText().find("pub ") != std::string::npos,
              "pub text preserved");
        CHECK(priv_fn->getText().find("pub") == std::string::npos,
              "private has no pub prefix");
    }
}

static void test_struct_field_visibility() {
    printf("\n[Test 2] struct field visibility (pub vs private)\n");
    const char* code = R"(
        pub struct User {
            pub name: String,
            email: String,
            pub(crate) id: u64,
        }
    )";
    RustParser parser;
    auto result = parser.parse(code, "test2.rs");
    CHECK(result.success, "parse succeeded");
    CHECK(result.nodes.size() == 1, "one top-level item");

    auto user_struct = findNodeWithText(result.nodes, "pub struct User");
    CHECK(user_struct != nullptr, "pub struct User found");
}

static void test_trait_impl_visibility() {
    printf("\n[Test 3] trait + impl method visibility\n");
    const char* code = R"(
        pub trait Drawable {
            fn draw(&self);
        }
        impl Drawable for Circle {
            pub fn draw(&self) {}
        }
    )";
    RustParser parser;
    auto result = parser.parse(code, "test3.rs");
    CHECK(result.success, "parse succeeded");
    CHECK(result.nodes.size() == 2, "two top-level items (trait + impl)");

    auto trait_node = findNodeWithText(result.nodes, "pub trait Drawable");
    auto impl_node  = findNodeWithText(result.nodes, "impl Drawable for Circle");

    CHECK(trait_node != nullptr, "pub trait Drawable found");
    CHECK(impl_node != nullptr, "impl Drawable for Circle found");

    if (impl_node) {
        CHECK(impl_node->getChildCount() >= 1, "impl has child methods");
    }
}

static void test_modifiers_preserved() {
    printf("\n[Test 4] unsafe/async/const modifiers preserved\n");
    const char* code = R"(
        pub unsafe fn raw_ptr() {}
        pub async fn fetch() {}
        pub const fn max(a: i32, b: i32) -> i32 { if a > b { a } else { b } }
    )";
    RustParser parser;
    auto result = parser.parse(code, "test4.rs");
    CHECK(result.success, "parse succeeded");

    auto unsafe_fn = findNodeWithText(result.nodes, "unsafe fn raw_ptr");
    auto async_fn  = findNodeWithText(result.nodes, "async fn fetch");
    auto const_fn  = findNodeWithText(result.nodes, "const fn max");

    CHECK(unsafe_fn != nullptr, "unsafe fn found");
    CHECK(async_fn != nullptr, "async fn found");
    CHECK(const_fn != nullptr, "const fn found");
}

static void test_lifetime_tokenization() {
    printf("\n[Test 5] lifetime tokenization ('a, 'static, '_)\n");
    const char* code = R"(
        fn borrow<'a>(x: &'a i32) -> &'a i32 { x }
        fn static_ref() -> &'static str { "hello" }
        fn anon(x: &'_ i32) {}
    )";
    RustParser parser;
    auto result = parser.parse(code, "test5.rs");
    CHECK(result.success, "parse succeeded");
    CHECK(result.nodes.size() == 3, "three functions");

    auto borrow_fn = findNodeWithText(result.nodes, "fn borrow");
    auto static_fn = findNodeWithText(result.nodes, "fn static_ref");
    auto anon_fn   = findNodeWithText(result.nodes, "fn anon");

    CHECK(borrow_fn != nullptr, "borrow fn found");
    CHECK(static_fn != nullptr, "static_ref fn found");
    CHECK(anon_fn != nullptr, "anon fn found");
}

static void test_edge_case_tokenization() {
    printf("\n[Test 6] raw strings + doc comments + nested block comments\n");
    const char* code = R"test6(
        /// This is a doc comment
        /** Block doc comment */
        pub fn regex() -> &'static str {
            r##"(?P<year>\d{4})"##
        }
        /* outer /* nested */ still comment */
        pub fn dummy() {}
    )test6";
    RustParser parser;
    auto result = parser.parse(code, "test6.rs");
    CHECK(result.success, "parse succeeded");

    auto regex_fn = findNodeWithText(result.nodes, "pub fn regex");
    auto dummy_fn = findNodeWithText(result.nodes, "pub fn dummy");

    CHECK(regex_fn != nullptr, "regex fn found despite raw string");
    CHECK(dummy_fn != nullptr, "dummy fn found after nested comment");
}

int main() {
    printf("========================================\n");
    printf("Rust Scope Verification Test Suite\n");
    printf("Access Modifier Sovereignty for v1.1.0-dev\n");
    printf("========================================\n");

    test_pub_crate_visibility();
    test_struct_field_visibility();
    test_trait_impl_visibility();
    test_modifiers_preserved();
    test_lifetime_tokenization();
    test_edge_case_tokenization();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", g_passed, g_failed);
    printf("========================================\n");

    return g_failed > 0 ? 1 : 0;
}
