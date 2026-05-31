/**
 * @file ast_completion_bridge.cpp
 * @brief AST Completion Bridge Implementation
 */

#include "ide/ast_completion_bridge.h"
#include "LanguageServerIntegration.h"
#include "completion/smart_completion.h"
#include "core/rust_parser.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>

namespace {

std::string uriToPath(const std::string& uri) {
    if (uri.size() >= 8 && uri.substr(0, 8) == "file:///") {
        std::string path = uri.substr(8);
        std::replace(path.begin(), path.end(), '/', '\\');
        return path;
    }
    return uri;
}

std::string readFileContent(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

RawrXD::IDE::SymbolInfo nodeToSymbolInfo(const RawrXD::AST::ASTNode::Ptr& node) {
    RawrXD::IDE::SymbolInfo info;
    info.name = node->getName();
    info.line = static_cast<int32_t>(node->getRange().start.line);
    info.column = static_cast<int32_t>(node->getRange().start.column);
    info.scope = "global";
    info.isPublic = false;

    const std::string& text = node->getText();

    switch (node->getType()) {
        case RawrXD::AST::NodeType::FunctionDecl:
            info.type = "function";
            info.kind = "function";
            info.isPublic = (text.find("pub ") == 0) || (text.find("pub(") == 0);
            break;
        case RawrXD::AST::NodeType::StructDecl:
            info.type = "struct";
            info.kind = "struct";
            info.isPublic = (text.find("pub ") == 0) || (text.find("pub(") == 0);
            break;
        case RawrXD::AST::NodeType::EnumDecl:
            info.type = "enum";
            info.kind = "enum";
            info.isPublic = (text.find("pub ") == 0) || (text.find("pub(") == 0);
            break;
        case RawrXD::AST::NodeType::ClassDecl:
            if (text.find("trait ") == 0 || text.find("pub trait ") == 0) {
                info.type = "trait";
                info.kind = "trait";
            } else if (text.find("impl ") == 0 || text.find("pub impl ") == 0) {
                info.type = "impl";
                info.kind = "impl";
            } else {
                info.type = "class";
                info.kind = "class";
            }
            info.isPublic = (text.find("pub ") == 0) || (text.find("pub(") == 0);
            break;
        case RawrXD::AST::NodeType::NamespaceDecl:
            info.type = "module";
            info.kind = "namespace";
            info.isPublic = (text.find("pub ") == 0) || (text.find("pub(") == 0);
            break;
        case RawrXD::AST::NodeType::TypedefDecl:
            info.type = "type";
            info.kind = "typedef";
            info.isPublic = (text.find("pub ") == 0) || (text.find("pub(") == 0);
            break;
        case RawrXD::AST::NodeType::UsingDecl:
            info.type = "import";
            info.kind = "using";
            info.isPublic = (text.find("pub ") == 0) || (text.find("pub(") == 0);
            break;
        case RawrXD::AST::NodeType::VariableDecl:
            if (text.find("const ") == 0 || text.find("pub const ") == 0) {
                info.type = "const";
                info.kind = "variable";
                info.isConst = true;
            } else if (text.find("static ") == 0 || text.find("pub static ") == 0) {
                info.type = "static";
                info.kind = "variable";
                info.isStatic = true;
            } else {
                info.type = "variable";
                info.kind = "variable";
            }
            info.isPublic = (text.find("pub ") == 0) || (text.find("pub(") == 0);
            break;
        default:
            info.type = "unknown";
            info.kind = "text";
            break;
    }

    return info;
}

} // anonymous namespace

namespace RawrXD {
namespace IDE {

// ============================================================================
// Constructor / Destructor
// ============================================================================

ASTCompletionBridge::ASTCompletionBridge() = default;
ASTCompletionBridge::~ASTCompletionBridge() = default;

// ============================================================================
// Initialization
// ============================================================================

void ASTCompletionBridge::initialize(std::shared_ptr<LanguageServerIntegration> lsp) {
    m_lsp = lsp;
}

// ============================================================================
// AST Context Capture
// ============================================================================

ASTContext ASTCompletionBridge::captureASTContext(const std::string& uri,
                                                const std::string& language,
                                                int line, int column) {
    ASTContext ast;
    ast.uri = uri;
    ast.language = language;

    if (!m_lsp) {
        ast.isValid = false;
        return ast;
    }

    // Build scope context from position
    ast.scope.currentScope = "global";
    ast.scope.scopeStack.push_back("global");

    // ------------------------------------------------------------------------
    // Rust: use native parser (no LSP dependency)
    // ------------------------------------------------------------------------
    if (language == "rust" || language == "rs" ||
        (uri.size() >= 3 && uri.substr(uri.size() - 3) == ".rs")) {
        std::string filePath = uriToPath(uri);
        std::string content = readFileContent(filePath);
        if (!content.empty()) {
            rawrxd::ast::rust::RustParser parser;
            auto result = parser.parse(content, filePath);
            if (result.success) {
                for (const auto& node : result.nodes) {
                    if (!node) continue;
                    auto info = nodeToSymbolInfo(node);
                    ast.visibleSymbols.push_back(info);

                    // Determine if cursor is inside this node for scope detection
                    const auto& range = node->getRange();
                    if (range.start.line <= static_cast<uint32_t>(line) &&
                        range.end.line >= static_cast<uint32_t>(line)) {
                        ast.scope.inFunction = (node->getType() == RawrXD::AST::NodeType::FunctionDecl);
                        ast.scope.inClass = (node->getType() == RawrXD::AST::NodeType::StructDecl ||
                                             node->getType() == RawrXD::AST::NodeType::ClassDecl);
                        if (ast.scope.inClass) {
                            ast.currentClass = info;
                        }
                        if (ast.scope.inFunction) {
                            ast.currentFunction = info;
                        }
                    }
                }
            }
        }
        ast.isValid = true;
        return ast;
    }

    // ------------------------------------------------------------------------
    // Other languages: fall back to LSP-based context (stubbed for now)
    // ------------------------------------------------------------------------
    ast.isValid = true;
    return ast;
}

// ============================================================================
// Completion Context Enrichment
// ============================================================================

void ASTCompletionBridge::enrichCompletionContext(Completion::CompletionContext& ctx,
                                               const ASTContext& ast) {
    if (!ast.isValid) return;
    
    // Add scope information to context
    ctx.definedSymbols["__current_scope__"] = ast.scope.currentScope;
    ctx.definedSymbols["__in_class__"] = ast.scope.inClass ? "true" : "false";
    ctx.definedSymbols["__in_function__"] = ast.scope.inFunction ? "true" : "false";
    ctx.definedSymbols["__access__"] = ast.scope.inPrivateBlock ? "private" :
                                        ast.scope.inProtectedBlock ? "protected" : "public";
    
    // Add visible symbols
    for (const auto& symbol : ast.visibleSymbols) {
        if (isSymbolAccessible(symbol, ast.scope)) {
            ctx.definedSymbols[symbol.name] = symbol.type;
        }
    }
    
    // Add member symbols if in class context
    if (ast.scope.inClass && ast.currentClass) {
        for (const auto& member : ast.memberSymbols) {
            if (isSymbolAccessible(member, ast.scope)) {
                ctx.definedSymbols[member.name] = member.type;
            }
        }
    }
    
    // Add local symbols if in function
    if (ast.scope.inFunction) {
        for (const auto& local : ast.localSymbols) {
            ctx.definedSymbols[local.name] = local.type;
        }
    }
}

// ============================================================================
// Scope-Based Filtering
// ============================================================================

std::vector<Completion::CompletionItem> ASTCompletionBridge::filterByScope(
    const std::vector<Completion::CompletionItem>& items,
    const ASTContext& ast) {
    
    if (!ast.isValid) return items;
    
    std::vector<Completion::CompletionItem> filtered;
    
    for (const auto& item : items) {
        // Check if item is accessible from current scope
        // This uses the detail field which may contain scope info
        bool accessible = true;
        
        // Filter private members if not in class scope
        if (ast.scope.inClass) {
            if (item.detail.find("private") != std::string::npos &&
                !ast.scope.inPrivateBlock && !ast.scope.inProtectedBlock) {
                accessible = false;
            }
        }
        
        if (accessible) {
            filtered.push_back(item);
        }
    }
    
    return filtered;
}

// ============================================================================
// Scope-Aware Completions
// ============================================================================

std::vector<Completion::CompletionItem> ASTCompletionBridge::getScopeCompletions(
    const ASTContext& ast,
    const std::string& prefix) {
    
    std::vector<Completion::CompletionItem> completions;
    
    if (!ast.isValid) return completions;
    
    // Add visible symbols
    for (const auto& symbol : ast.visibleSymbols) {
        if (symbol.name.find(prefix) == 0) {
            Completion::CompletionItem item;
            item.label = symbol.name;
            item.insertText = symbol.name;
            item.detail = symbol.type + " (" + symbol.kind + ")";
            item.documentation = "Scope: " + symbol.scope;
            
            // Set kind based on symbol type
            if (symbol.kind == "function") {
                item.kind = Completion::CompletionItem::Kind::Function;
            } else if (symbol.kind == "class" || symbol.kind == "struct") {
                item.kind = Completion::CompletionItem::Kind::Class;
            } else if (symbol.kind == "variable") {
                item.kind = Completion::CompletionItem::Kind::Variable;
            } else {
                item.kind = Completion::CompletionItem::Kind::Text;
            }
            
            completions.push_back(item);
        }
    }
    
    // Add member symbols if in class
    if (ast.scope.inClass) {
        for (const auto& member : ast.memberSymbols) {
            if (member.name.find(prefix) == 0 && isSymbolAccessible(member, ast.scope)) {
                Completion::CompletionItem item;
                item.label = member.name;
                item.insertText = member.name;
                item.detail = member.type + " (" + member.kind + ")";
                item.documentation = "Member of " + ast.currentClass->name;
                item.kind = Completion::CompletionItem::Kind::Field;
                completions.push_back(item);
            }
        }
    }
    
    return completions;
}

// ============================================================================
// Symbol Accessibility
// ============================================================================

bool ASTCompletionBridge::isSymbolAccessible(const SymbolInfo& symbol,
                                            const ScopeContext& scope) {
    // Public symbols are always accessible
    if (symbol.isPublic) return true;
    
    // Private symbols only accessible within same class
    if (!symbol.isPublic && scope.inClass) {
        // Check if we're in the same class or a friend
        // For now, simplified: accessible if in any class scope
        return true;
    }
    
    return false;
}

// ============================================================================
// Member Completions
// ============================================================================

std::vector<Completion::CompletionItem> ASTCompletionBridge::getMemberCompletions(
    const std::string& typeName,
    const ASTContext& ast) {
    
    std::vector<Completion::CompletionItem> completions;
    
    // Find the type in visible symbols
    auto it = std::find_if(ast.visibleSymbols.begin(), ast.visibleSymbols.end(),
        [&typeName](const SymbolInfo& s) { return s.name == typeName && s.kind == "class"; });
    
    if (it == ast.visibleSymbols.end()) return completions;
    
    // Get members of this type
    // In a full implementation, this would query the LSP for type members
    // For now, we return member symbols that match the type
    
    for (const auto& member : ast.memberSymbols) {
        if (member.scope == it->scope + "::" + it->name) {
            Completion::CompletionItem item;
            item.label = member.name;
            item.insertText = member.name;
            item.detail = member.type;
            item.kind = member.kind == "function" ? 
                Completion::CompletionItem::Kind::Method : 
                Completion::CompletionItem::Kind::Field;
            completions.push_back(item);
        }
    }
    
    return completions;
}

// ============================================================================
// C API Implementation
// ============================================================================

extern "C" {

void* ASTBridge_Create() {
    return new ASTCompletionBridge();
}

void ASTBridge_Destroy(void* bridge) {
    delete static_cast<ASTCompletionBridge*>(bridge);
}

void ASTBridge_Initialize(void* bridge, void* lsp) {
    if (!bridge || !lsp) return;
    auto* b = static_cast<ASTCompletionBridge*>(bridge);
    auto* lspPtr = static_cast<LanguageServerIntegration*>(lsp);
    // Note: This is a simplification - in practice we'd need shared_ptr management
}

const char* ASTBridge_CaptureContext(void* bridge,
                                    const char* uri,
                                    const char* language,
                                    int line, int column) {
    if (!bridge || !uri || !language) return nullptr;
    
    auto* b = static_cast<ASTCompletionBridge*>(bridge);
    auto ast = b->captureASTContext(uri, language, line, column);
    
    // Serialize to JSON
    nlohmann::json j;
    j["uri"] = ast.uri;
    j["language"] = ast.language;
    j["isValid"] = ast.isValid;
    j["scope"]["current"] = ast.scope.currentScope;
    j["scope"]["inClass"] = ast.scope.inClass;
    j["scope"]["inFunction"] = ast.scope.inFunction;
    
    std::string result = j.dump();
    char* cstr = new char[result.length() + 1];
    strcpy(cstr, result.c_str());
    return cstr;
}

void ASTBridge_FreeString(const char* str) {
    delete[] str;
}

} // extern "C"

} // namespace IDE
} // namespace RawrXD
