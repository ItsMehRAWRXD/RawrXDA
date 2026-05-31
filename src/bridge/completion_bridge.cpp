#include "bridge/completion_bridge.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

namespace RawrXD {
namespace Bridge {

// Implementation
class CompletionBridge::Impl {
public:
    std::string filePath_;
    std::string content_;
    uint64_t lastFingerprint_ = 0;
    
    // Simple scope tracking
    std::vector<std::string> scopeStack_;
    std::unordered_map<std::string, std::vector<EnrichedCompletionItem>> scopeSymbols_;
    
    bool ParseScopes(const std::string& content);
    std::string GetCurrentScope(int line);
};

CompletionBridge::CompletionBridge() : pImpl(std::make_unique<Impl>()) {}
CompletionBridge::~CompletionBridge() = default;

bool CompletionBridge::Initialize(const std::string& filePath, const std::string& content) {
    pImpl->filePath_ = filePath;
    pImpl->content_ = content;
    return pImpl->ParseScopes(content);
}

ASTEnrichedContext CompletionBridge::GetCompletions(const std::string& prefix, int line, int column) {
    ASTEnrichedContext context;
    context.cursorLine = line;
    context.cursorColumn = column;
    context.filePath = pImpl->filePath_;
    context.currentScope = pImpl->GetCurrentScope(line);
    
    // Generate fingerprint
    context.fingerprint = ContextFingerprint::Generate(context.currentScope, prefix, line);
    
    // Get symbols from current scope
    auto it = pImpl->scopeSymbols_.find(context.currentScope);
    if (it != pImpl->scopeSymbols_.end()) {
        context.items = ScopeAwareFilter::FilterByScope(
            it->second, 
            context.currentScope,
            AccessModifier::Public,
            !context.currentScope.empty()
        );
    }
    
    // Filter by prefix
    std::vector<EnrichedCompletionItem> filtered;
    for (const auto& item : context.items) {
        if (item.label.find(prefix) == 0 || prefix.empty()) {
            filtered.push_back(item);
        }
    }
    context.items = filtered;
    
    return context;
}

void CompletionBridge::UpdateContent(const std::string& newContent) {
    pImpl->content_ = newContent;
    pImpl->ParseScopes(newContent);
}

bool CompletionBridge::IsContextStale(uint64_t fingerprint) const {
    return fingerprint != pImpl->lastFingerprint_;
}

// Simple scope parsing
bool CompletionBridge::Impl::ParseScopes(const std::string& content) {
    scopeSymbols_.clear();
    scopeStack_.clear();
    
    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    std::string currentScope;
    
    while (std::getline(stream, line)) {
        lineNum++;
        
        // Very simple scope detection
        size_t classPos = line.find("class ");
        if (classPos != std::string::npos) {
            size_t nameStart = classPos + 6;
            size_t nameEnd = line.find_first_of(" {:;", nameStart);
            if (nameEnd != std::string::npos) {
                std::string className = line.substr(nameStart, nameEnd - nameStart);
                scopeStack_.push_back(className);
                currentScope = className;
            }
        }
        
        size_t structPos = line.find("struct ");
        if (structPos != std::string::npos) {
            size_t nameStart = structPos + 7;
            size_t nameEnd = line.find_first_of(" {:;", nameStart);
            if (nameEnd != std::string::npos) {
                std::string structName = line.substr(nameStart, nameEnd - nameStart);
                scopeStack_.push_back(structName);
                currentScope = structName;
            }
        }
        
        // Detect member declarations
        size_t privatePos = line.find("private:");
        size_t protectedPos = line.find("protected:");
        size_t publicPos = line.find("public:");
        
        // Simple symbol extraction (very basic)
        if (line.find("int ") != std::string::npos || 
            line.find("void ") != std::string::npos ||
            line.find("bool ") != std::string::npos ||
            line.find("std::") != std::string::npos) {
            
            EnrichedCompletionItem item;
            item.scopePath = currentScope;
            item.scopeDepth = scopeStack_.size();
            
            // Extract identifier
            size_t idStart = line.find_last_of(" ") + 1;
            size_t idEnd = line.find_first_of("(;=", idStart);
            if (idEnd == std::string::npos) idEnd = line.length();
            
            if (idStart < line.length()) {
                item.label = line.substr(idStart, idEnd - idStart);
                item.detail = line.substr(0, idEnd);
                
                // Determine accessibility
                if (privatePos != std::string::npos) {
                    item.isAccessible = false;
                } else if (protectedPos != std::string::npos) {
                    item.isAccessible = false;
                }
                
                scopeSymbols_[currentScope].push_back(item);
            }
        }
        
        // Pop scope on closing brace
        if (line.find("};") != std::string::npos || 
            (line.find("}") != std::string::npos && line.find("{") == std::string::npos)) {
            if (!scopeStack_.empty()) {
                scopeStack_.pop_back();
                currentScope = scopeStack_.empty() ? "" : scopeStack_.back();
            }
        }
    }
    
    return true;
}

std::string CompletionBridge::Impl::GetCurrentScope(int line) {
    return scopeStack_.empty() ? "" : scopeStack_.back();
}

// Scope-aware filter implementation
std::vector<EnrichedCompletionItem> ScopeAwareFilter::FilterByScope(
    const std::vector<EnrichedCompletionItem>& items,
    const std::string& currentScope,
    AccessModifier currentAccess,
    bool inClassScope
) {
    std::vector<EnrichedCompletionItem> result;
    
    for (const auto& item : items) {
        if (IsAccessible(item, currentScope, currentAccess)) {
            result.push_back(item);
        }
    }
    
    return result;
}

bool ScopeAwareFilter::IsAccessible(
    const EnrichedCompletionItem& item,
    const std::string& currentScope,
    AccessModifier currentAccess
) {
    // Same scope - always accessible
    if (item.scopePath == currentScope) {
        return true;
    }
    
    // Public items are accessible from anywhere
    if (item.isAccessible) {
        return true;
    }
    
    // Private items only accessible within same scope
    return false;
}

// Context fingerprint implementation
uint64_t ContextFingerprint::Generate(const std::string& scope, const std::string& prefix, int line) {
    // Simple FNV-1a hash
    uint64_t hash = 14695981039346656037ULL;
    
    auto hash_bytes = [&hash](const char* data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            hash ^= static_cast<uint64_t>(data[i]);
            hash *= 1099511628211ULL;
        }
    };
    
    hash_bytes(scope.data(), scope.size());
    hash_bytes(prefix.data(), prefix.size());
    hash_bytes(reinterpret_cast<const char*>(&line), sizeof(line));
    
    return hash;
}

uint64_t ContextFingerprint::Combine(uint64_t a, uint64_t b) {
    return a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
}

// C-API implementation
extern "C" {

struct RawrXD_ASTContext {
    CompletionBridge* bridge;
};

struct RawrXD_CompletionResult {
    std::vector<EnrichedCompletionItem> items;
};

RawrXD_ASTContext* RawrXD_ast_context_create(const char* filePath) {
    auto* ctx = new RawrXD_ASTContext();
    ctx->bridge = new CompletionBridge();
    return ctx;
}

void RawrXD_ast_context_destroy(RawrXD_ASTContext* ctx) {
    if (ctx) {
        delete ctx->bridge;
        delete ctx;
    }
}

RawrXD_CompletionResult* RawrXD_ast_completion_enrich(
    RawrXD_ASTContext* ctx,
    const char* prefix,
    int line,
    int column
) {
    if (!ctx || !ctx->bridge) return nullptr;
    
    auto* result = new RawrXD_CompletionResult();
    auto enriched = ctx->bridge->GetCompletions(prefix ? prefix : "", line, column);
    result->items = enriched.items;
    
    return result;
}

void RawrXD_completion_result_destroy(RawrXD_CompletionResult* result) {
    delete result;
}

int RawrXD_completion_result_count(RawrXD_CompletionResult* result) {
    return result ? static_cast<int>(result->items.size()) : 0;
}

const char* RawrXD_completion_result_item_label(RawrXD_CompletionResult* result, int index) {
    if (!result || index < 0 || index >= result->items.size()) return "";
    return result->items[index].label.c_str();
}

int RawrXD_completion_result_item_is_accessible(RawrXD_CompletionResult* result, int index) {
    if (!result || index < 0 || index >= result->items.size()) return 0;
    return result->items[index].isAccessible ? 1 : 0;
}

} // extern "C"

} // namespace Bridge
} // namespace RawrXD
