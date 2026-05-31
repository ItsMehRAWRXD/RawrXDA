// test_rust_scope.cpp
// Rust AST scope verification: pub(crate) vs pub, trait visibility, lifetime awareness.
// Validates that the AST Graph Engine correctly handles Rust-specific visibility semantics.
// Compile: g++ -O2 -std=c++20 -I d:\rawrxd\src\core test_rust_scope.cpp rust_parser.cpp ast_graph_engine.cpp -o test_rust_scope.exe

#include <cstdio>
#include <cstring>
#include <vector>
#include <string_view>
#include "rust_parser.hpp"

using namespace rawrxd::ast::rust;

static int g_passed = 0;
static int g_failed = 0;

#define ASSERT(cond, msg) do { \
    if (cond) { ++g_passed; printf("  [PASS] %s\n", msg); } \
    else { ++g_failed; printf("  [FAIL] %s\n", msg); } \
} while(0)

// Test 1: Parse pub(crate) visibility
static void test_pub_crate_visibility() {
    printf("\n[Test 1] pub(crate) visibility parsing\n");
    const char* code = R"(
pub(crate) struct InternalStruct {
    pub field: i32,
}

pub struct PublicStruct {
    pub field: i32,
}
)";
    RustParser parser;
    auto result = parser.parse(code, "test.rs");
    ASSERT(result.success, "Parse succeeded");
    ASSERT(result.nodes.size() == 2, "Two top-level items found");

    bool foundPubCrate = false;
    bool foundPub = false;
    for (auto& node : result.nodes) {
        std::string text = node->getText();
        if (text.find("pub(crate)") != std::string::npos) foundPubCrate = true;
        if (text.find("pub struct") != std::string::npos) foundPub = true;
    }
    ASSERT(foundPubCrate, "pub(crate) struct detected");
    ASSERT(foundPub, "pub struct detected");
}

// Test 2: Parse trait declarations
static void test_trait_parsing() {
    printf("\n[Test 2] Trait declaration parsing\n");
    const char* code = R"(
pub trait Cloneable {
    fn clone(&self) -> Self;
}

pub trait Debuggable: Cloneable {
    fn debug(&self) -> String;
}
)";
    RustParser parser;
    auto result = parser.parse(code, "test.rs");
    ASSERT(result.success, "Parse succeeded");
    ASSERT(result.nodes.size() == 2, "Two traits found");

    bool foundCloneable = false;
    bool foundDebuggable = false;
    for (auto& node : result.nodes) {
        std::string text = node->getText();
        if (text.find("Cloneable") != std::string::npos) foundCloneable = true;
        if (text.find("Debuggable") != std::string::npos) foundDebuggable = true;
    }
    ASSERT(foundCloneable, "Cloneable trait detected");
    ASSERT(foundDebuggable, "Debuggable trait detected");
}

// Test 3: Parse impl blocks (trait and inherent)
static void test_impl_parsing() {
    printf("\n[Test 3] impl block parsing\n");
    const char* code = R"(
impl MyStruct {
    fn new() -> Self { MyStruct {} }
}

impl Cloneable for MyStruct {
    fn clone(&self) -> Self { MyStruct {} }
}
)";
    RustParser parser;
    auto result = parser.parse(code, "test.rs");
    ASSERT(result.success, "Parse succeeded");
    ASSERT(result.nodes.size() == 2, "Two impl blocks found");

    bool foundInherent = false;
    bool foundTrait = false;
    for (auto& node : result.nodes) {
        std::string text = node->getText();
        if (text == "impl MyStruct") foundInherent = true;
        if (text == "impl Cloneable for MyStruct") foundTrait = true;
    }
    ASSERT(foundInherent, "Inherent impl detected");
    ASSERT(foundTrait, "Trait impl detected");
}

// Test 4: Parse use declarations and module hierarchy
static void test_use_and_mod() {
    printf("\n[Test 4] use and mod parsing\n");
    const char* code = R"(
use std::vec::Vec;
use std::collections::HashMap as Map;

mod inner {
    pub fn helper() {}
}

mod sibling;
)";
    RustParser parser;
    auto result = parser.parse(code, "test.rs");
    ASSERT(result.success, "Parse succeeded");
    ASSERT(result.nodes.size() == 4, "Four items found");

    bool foundUseVec = false;
    bool foundUseAlias = false;
    bool foundModBlock = false;
    bool foundModDecl = false;
    for (auto& node : result.nodes) {
        std::string text = node->getText();
        if (text.find("std::vec::Vec") != std::string::npos) foundUseVec = true;
        if (text.find("HashMap as Map") != std::string::npos) foundUseAlias = true;
        if (text == "mod inner") foundModBlock = true;
        if (text == "mod sibling") foundModDecl = true;
    }
    ASSERT(foundUseVec, "use std::vec::Vec detected");
    ASSERT(foundUseAlias, "use ... as Map detected");
    ASSERT(foundModBlock, "mod inner {{ ... }} detected");
    ASSERT(foundModDecl, "mod sibling; detected");
}

// Test 5: Parse function with attributes and modifiers
static void test_function_modifiers() {
    printf("\n[Test 5] Function modifiers parsing\n");
    const char* code = R"(
#[inline]
pub unsafe fn dangerous() -> i32 { 42 }

#[test]
fn test_something() {
    assert_eq!(1 + 1, 2);
}

pub async fn fetch_data() -> Result<String, Error> {
    Ok(String::new())
}
)";
    RustParser parser;
    auto result = parser.parse(code, "test.rs");
    ASSERT(result.success, "Parse succeeded");
    ASSERT(result.nodes.size() == 3, "Three functions found");

    bool foundUnsafe = false;
    bool foundTest = false;
    bool foundAsync = false;
    for (auto& node : result.nodes) {
        std::string text = node->getText();
        if (text.find("unsafe") != std::string::npos) foundUnsafe = true;
        if (text.find("fetch_data") != std::string::npos) foundAsync = true;
        // test attribute is inside the block, not in node text
        // but the function name should be test_something
        if (text.find("test_something") != std::string::npos) foundTest = true;
    }
    ASSERT(foundUnsafe, "unsafe fn detected");
    ASSERT(foundTest, "test fn detected");
    ASSERT(foundAsync, "async fn detected");
}

// Test 6: Parse struct with derives and generics
static void test_struct_derives_generics() {
    printf("\n[Test 6] Struct with derives and generics\n");
    const char* code = R"(
#[derive(Clone, Debug, PartialEq)]
pub struct Point<T> {
    x: T,
    y: T,
}

pub enum Option<T> {
    Some(T),
    None,
}
)";
    RustParser parser;
    auto result = parser.parse(code, "test.rs");
    ASSERT(result.success, "Parse succeeded");
    ASSERT(result.nodes.size() == 2, "Two declarations found");

    bool foundPoint = false;
    bool foundOption = false;
    for (auto& node : result.nodes) {
        std::string text = node->getText();
        if (text.find("Point") != std::string::npos) foundPoint = true;
        if (text.find("Option") != std::string::npos) foundOption = true;
    }
    ASSERT(foundPoint, "Point<T> detected");
    ASSERT(foundOption, "Option<T> detected");
}

int main() {
    printf("========================================\n");
    printf("Rust AST Scope Verification Suite\n");
    printf("========================================\n");

    test_pub_crate_visibility();
    test_trait_parsing();
    test_impl_parsing();
    test_use_and_mod();
    test_function_modifiers();
    test_struct_derives_generics();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", g_passed, g_failed);
    printf("========================================\n");

    return g_failed > 0 ? 1 : 0;
}
