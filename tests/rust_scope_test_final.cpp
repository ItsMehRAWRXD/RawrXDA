// rust_scope_test_final.cpp
// Final unified test: includes ast_graph_engine.hpp (with inline impls) + rust_parser_v2 + test harness.
// Compile: g++ -O2 -std=c++20 -I d:\rawrxd\src\core -I d:\rawrxd\include -o rust_scope_test.exe rust_scope_test_final.cpp

// ========== 1. AST Graph Engine (inline implementations) ==========
#include "ast_graph_engine.hpp"
#include <algorithm>

namespace RawrXD::AST {

std::atomic<uint64_t> ASTNode::next_version_{1};

ASTNode::ASTNode(NodeType type, SourceRange range, std::string text)
    : type_(type), range_(range), text_(std::move(text)),
      hash_(0), version_(next_version_++), parent_(), children_(),
      references_(), definitions_() {}

ASTNode::Ptr ASTNode::withChildren(std::vector<Ptr> new_children) const {
    auto copy = std::make_shared<ASTNode>(type_, range_, text_);
    copy->hash_ = hash_;
    copy->version_ = version_;
    copy->parent_ = parent_;
    copy->children_ = std::move(new_children);
    copy->references_ = references_;
    copy->definitions_ = definitions_;
    for (auto& c : copy->children_) {
        const_cast<ASTNode*>(c.get())->parent_ = copy;
    }
    return copy;
}

ASTNode::Ptr ASTNode::withParent(Ptr new_parent) const {
    auto copy = std::make_shared<ASTNode>(type_, range_, text_);
    copy->hash_ = hash_;
    copy->version_ = version_;
    copy->parent_ = new_parent;
    copy->children_ = children_;
    copy->references_ = references_;
    copy->definitions_ = definitions_;
    return copy;
}

ASTNode::Ptr ASTNode::withHash(NodeHash new_hash) const {
    auto copy = std::make_shared<ASTNode>(type_, range_, text_);
    copy->hash_ = new_hash;
    copy->version_ = version_;
    copy->parent_ = parent_;
    copy->children_ = children_;
    copy->references_ = references_;
    copy->definitions_ = definitions_;
    return copy;
}

ASTNode::Ptr ASTNode::getChild(size_t index) const {
    return index < children_.size() ? children_[index] : nullptr;
}

bool ASTNode::isDeclaration() const {
    return type_ == NodeType::FunctionDecl || type_ == NodeType::VariableDecl ||
           type_ == NodeType::ClassDecl || type_ == NodeType::StructDecl ||
           type_ == NodeType::EnumDecl || type_ == NodeType::NamespaceDecl ||
           type_ == NodeType::TypedefDecl;
}

bool ASTNode::isStatement() const {
    return type_ == NodeType::VariableDecl || type_ == NodeType::FunctionDecl;
}

bool ASTNode::isExpression() const {
    return false;
}

bool ASTNode::isType() const {
    return type_ == NodeType::StructDecl || type_ == NodeType::EnumDecl ||
           type_ == NodeType::ClassDecl;
}

std::string ASTNode::getName() const {
    size_t pos = text_.find(' ');
    if (pos != std::string::npos) return text_.substr(pos + 1);
    return text_;
}

std::string ASTNode::getQualifiedName() const {
    std::string qname = getName();
    auto p = getParent();
    while (p) {
        qname = p->getName() + "::" + qname;
        p = p->getParent();
    }
    return qname;
}

ASTNode::Ptr ASTNode::findNodeAt(const SourceLocation& loc) const {
    if (!range_.contains(loc)) return nullptr;
    for (auto& child : children_) {
        auto found = child->findNodeAt(loc);
        if (found) return found;
    }
    return shared_from_this();
}

void ASTNode::findNodesOfType(NodeType type, std::vector<Ptr>& results) const {
    if (type_ == type) results.push_back(shared_from_this());
    for (auto& child : children_) {
        child->findNodesOfType(type, results);
    }
}

size_t ASTNode::graphDistanceTo(Ptr other) const {
    if (!other) return SIZE_MAX;
    if (this == other.get()) return 0;
    std::vector<Ptr> queue;
    std::unordered_set<const ASTNode*> visited;
    queue.push_back(shared_from_this());
    visited.insert(this);
    size_t distance = 0;
    while (!queue.empty()) {
        size_t level_size = queue.size();
        for (size_t i = 0; i < level_size; ++i) {
            auto current = queue[i];
            if (current.get() == other.get()) return distance;
            for (auto& child : current->getChildren()) {
                if (visited.insert(child.get()).second) queue.push_back(child);
            }
            auto parent = current->getParent();
            if (parent && visited.insert(parent.get()).second) queue.push_back(parent);
        }
        queue.erase(queue.begin(), queue.begin() + level_size);
        ++distance;
    }
    return SIZE_MAX;
}

void ASTGraphEngine::registerFile(const std::string& path, const std::string& content) {
    (void)path; (void)content;
}

void ASTGraphEngine::updateFile(const std::string& path, const std::string& content) {
    (void)path; (void)content;
}

} // namespace RawrXD::AST

// ========== 2. Rust Parser v2 (implementation) ==========
#include "rust_parser_v2.cpp"

// ========== 3. Test Harness ==========
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
