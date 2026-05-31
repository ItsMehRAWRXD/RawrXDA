// rust_ast_nodes.hpp
// Rust-specific AST node extensions for the graph engine.
// No external dependencies; integrates with existing ast_graph_engine.hpp NodeType enum.

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rawrxd::ast::rust {

// Rust-specific node types (mapped to NodeType::Unknown with rust subtype)
enum class RustNodeKind : uint8_t {
    // Declarations
    ModuleDecl,          // mod foo { ... }
    CrateDecl,           // crate root
    UseDecl,             // use std::vec::Vec;
    ExternCrateDecl,     // extern crate foo;
    StaticDecl,          // static FOO: i32 = 0;
    ConstDecl,           // const FOO: i32 = 0;
    TraitDecl,           // trait Foo { ... }
    ImplDecl,            // impl Foo for Bar { ... }
    StructDecl,          // struct Foo { ... }
    EnumDecl,            // enum Foo { ... }
    UnionDecl,           // union Foo { ... }
    TypeAliasDecl,       // type Foo = Bar;
    FunctionDecl,        // fn foo() { ... }
    MacroDecl,           // macro_rules! foo { ... }

    // Expressions / Statements
    LetStmt,             // let x = ...;
    MatchExpr,           // match x { ... }
    IfExpr,              // if x { ... }
    LoopExpr,            // loop { ... }
    WhileExpr,           // while x { ... }
    ForExpr,             // for x in y { ... }
    ClosureExpr,         // |x| x + 1
    BlockExpr,           // { ... }
    UnsafeBlock,         // unsafe { ... }
    AsyncBlock,          // async { ... }
    AwaitExpr,           // x.await
    TryExpr,             // x?
    ReturnExpr,          // return x;
    BreakExpr,           // break;
    ContinueExpr,        // continue;

    // Types
    ReferenceType,       // &T, &mut T
    SliceType,           // [T]
    ArrayType,           // [T; N]
    TupleType,           // (T, U)
    FunctionType,        // fn(T) -> U
    TraitObjectType,     // dyn Trait
    ImplTraitType,       // impl Trait
    NeverType,           // !

    // Patterns
    IdentifierPat,       // x
    ReferencePat,        // &x
    StructPat,           // Foo { x }
    TuplePat,            // (x, y)
    SlicePat,            // [x, y]

    // Lifetime / Ownership
    Lifetime,            // 'a
    LifetimeParam,       // <'a>
    SelfParam,           // self, &self, &mut self
    PatParam,            // pattern parameter

    // Visibility
    VisibilityPub,       // pub
    VisibilityPubCrate,  // pub(crate)
    VisibilityPubSuper,  // pub(super)
    VisibilityPubSelf,   // pub(self)
    VisibilityPubIn,     // pub(in path)

    // Attributes
    Attribute,           // #[...]
    InnerAttribute,      // #![...]

    // Macros
    MacroInvocation,     // foo!()
    MacroInvocationStmt, // foo!();

    Unknown = 255
};

// Rust-specific symbol metadata attached to ASTNode via user_data
struct RustSymbolMeta {
    RustNodeKind kind{RustNodeKind::Unknown};
    std::string lifetime;           // 'a, 'static
    bool isMutable{false};          // mut binding
    bool isReference{false};        // &T
    bool isMutableReference{false}; // &mut T
    bool isOwned{false};            // owned value (not ref)
    bool isCopy{false};             // implements Copy
    bool isClone{false};            // implements Clone
    bool isSend{false};             // implements Send
    bool isSync{false};             // implements Sync
    bool isUnsafe{false};           // unsafe fn / unsafe block
    bool isAsync{false};            // async fn / async block
    bool isConst{false};            // const fn
    bool isExtern{false};           // extern fn
    bool isPub{false};              // pub visibility
    std::string visibilityPath;     // pub(in crate::foo)
    std::string traitBounds;        // T: Clone + Send
    std::vector<std::string> derives; // #[derive(Clone, Debug)]
    std::string docComment;         // /// doc comment text
    std::vector<std::pair<std::string, std::string>> attributes; // #[name(value)]
};

// Rust scope context (extends ScopeContext in completion bridge)
struct RustScopeContext {
    bool inUnsafeBlock{false};
    bool inAsyncBlock{false};
    bool inConstContext{false};
    bool inTraitDecl{false};
    bool inImplDecl{false};
    bool inMacroDef{false};
    bool inMatchArm{false};
    bool inClosure{false};
    std::string activeLifetime;     // Current lifetime scope
    std::vector<std::string> importedTraits; // use traits in scope
    std::vector<std::string> visibleCrates; // extern crate names
};

} // namespace rawrxd::ast::rust
