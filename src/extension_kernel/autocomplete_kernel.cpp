// autocomplete_kernel.cpp - Minimal in-host autocomplete kernel for RawrXD
// Collapses IPC + speculative pipeline + AST grounding + arbitration into one unit.
// No new dependencies.

#include "autocomplete_kernel.h"
#include "autocomplete_protocol.h"
#include "cross_repo_router.hpp"
#include "inference_controller.hpp"
#include "../ipc/shm_channel.hpp"
#include "../lsp/treesitter_parser.h"
#include "../agents/arbitration_engine.hpp"
#include "../RawrXD_Interfaces.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RawrXD::ExtensionKernel {

namespace {

inline std::string view_cstr(const char* p) {
    return p ? std::string(p) : std::string();
}

inline std::string trim_to(const std::string& s, size_t n) {
    if (s.size() <= n) return s;
    return s.substr(0, n);
}

inline uint64_t fnv1a64(const void* data, size_t size, uint64_t seed = 1469598103934665603ull) {
    const auto* p = static_cast<const uint8_t*>(data);
    uint64_t h = seed;
    for (size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

inline std::string file_ext_lower(const std::string& path) {
    const size_t pos = path.find_last_of('.');
    if (pos == std::string::npos) return {};
    std::string e = path.substr(pos);
    for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return e;
}

struct AstHint {
    std::string scope;
    bool memberAccess = false;
    std::vector<std::string> symbols;
    uint64_t symbolHash = 0;
};

class KernelArbiter {
public:
    KernelArbiter() {
        m_engine.register_agent("logic", "logic", [](const Agents::ArbitrationTask& t) {
            return t.payload;
        });
        m_engine.register_agent("style", "style", [](const Agents::ArbitrationTask& t) {
            return t.payload + "\n";
        });
        m_engine.register_agent("doc", "doc", [](const Agents::ArbitrationTask& t) {
            return std::string("/* ") + t.payload + " */";
        });
        m_engine.start();
    }

    ~KernelArbiter() {
        m_engine.shutdown();
    }

    std::string choosePrompt(const std::string& basePrompt,
                             const AstHint& hint,
                             const std::string& ext) {
        Agents::ArbitrationTask logicTask;
        logicTask.capability = "logic";
        logicTask.payload = basePrompt;
        logicTask.priority = (ext == ".cpp" || ext == ".h" || ext == ".hpp") ? 140 : 100;

        Agents::ArbitrationTask styleTask;
        styleTask.capability = "style";
        styleTask.payload = basePrompt;
        styleTask.priority = (ext == ".md" || ext == ".txt")
            ? 145
            : (hint.scope == "FunctionDef" ? 120 : 90);

        Agents::ArbitrationTask docTask;
        docTask.capability = "doc";
        docTask.payload = basePrompt;
        docTask.priority = hint.memberAccess ? 110 : 80;

        auto f1 = m_engine.submit(std::move(logicTask));
        auto f2 = m_engine.submit(std::move(styleTask));
        auto f3 = m_engine.submit(std::move(docTask));

        auto r1 = f1.get();
        auto r2 = f2.get();
        auto r3 = f3.get();

        std::string best = r1.ok ? r1.output : basePrompt;
        int bestScore = 100;

        if (r2.ok) {
            int s = (hint.scope == "FunctionDef") ? 130 : 95;
            if (s > bestScore) { bestScore = s; best = r2.output; }
        }
        if (r3.ok) {
            int s = hint.memberAccess ? 125 : 85;
            if (s > bestScore) { best = r3.output; }
        }

        return best;
    }

private:
    Agents::ArbitrationEngine m_engine;
};

} // namespace

class AutocompleteKernel {
public:
    AutocompleteKernel(InferenceEngine* draft, InferenceEngine* target)
        : m_draft(draft),
          m_target(target),
                    m_controller(draft, target) {}

    bool openServer(const std::string& baseName = "RawrXD_Autocomplete") {
        return m_channel.open_server(baseName, 256);
    }

    bool openClient(const std::string& baseName = "RawrXD_Autocomplete") {
        return m_channel.open_client(baseName, 256);
    }

    bool registerRepoIndex(const RepoSymbolIndex* index) {
        if (!index || !index->nodes || index->node_count == 0) {
            return false;
        }
        if (m_external_indexes.size() >= 8) {
            return false;
        }
        m_external_indexes.push_back(index);
        return true;
    }

    // Called by host tick / idle loop.
    bool onIdle() {
        std::vector<uint8_t> raw;
        if (!m_channel.rx().read_copy(raw)) {
            return false;
        }
        if (raw.size() < sizeof(CompletionRequest)) {
            return false;
        }

        CompletionRequest req{};
        std::memcpy(&req, raw.data(), sizeof(CompletionRequest));
        if (req.version != kAutocompleteWireVersion || !m_draft || !m_target) {
            return false;
        }

        CompletionResult out{};
        process(req, out);
        m_channel.tx().write(&out, static_cast<uint32_t>(sizeof(out)));
        return true;
    }

private:
    void process(const CompletionRequest& req, CompletionResult& out) {
        const std::string uri = view_cstr(req.filePath);
        const std::string content = view_cstr(req.content);
        const std::string prefix = view_cstr(req.prefix);
        const std::string suffix = view_cstr(req.suffix);
        const std::string ext = file_ext_lower(uri);

        const AstHint hint = astHintFrom(uri, content, req.line, req.col);

        std::string prompt = prefix;
        if (!suffix.empty()) {
            prompt += "\n";
            prompt += suffix;
        }
        prompt = m_arbiter.choosePrompt(prompt, hint, ext);

        std::vector<int32_t> promptTokens = m_target->Tokenize(prompt);
        if (promptTokens.empty()) {
            out.version = kAutocompleteWireVersion;
            out.count = 0;
            return;
        }

        ControllerRequestContext controllerReq;
        controllerReq.context_hash = makeContextHash(req, prompt);
        controllerReq.file_hash = fnv1a64(uri.data(), uri.size());
        controllerReq.symbol_hash = hint.symbolHash;
        controllerReq.prompt_tokens = std::move(promptTokens);
        controllerReq.ast_allowed_tokens = symbolsToAllowedTokens(hint.symbols);
        appendRoutedExternalTokens(controllerReq.file_hash, hint, controllerReq.ast_allowed_tokens);
        controllerReq.prefix_hint_tokens = m_target->Tokenize(prefix);
        controllerReq.max_tokens = 32;
        controllerReq.flags = req.flags;

        const auto fused = m_controller.run(controllerReq);

        const uint32_t n = static_cast<uint32_t>(std::min<size_t>(fused.tokens.size(), 32));
        out.version = kAutocompleteWireVersion;
        out.count = n;
        out.acceptanceRate = fused.acceptance_rate;
        out.speedupEstimate = fused.speedup_estimate;
        out.cacheHit = fused.metrics.cache_hit;
        out.kvStitchCount = fused.metrics.kv_stitch_count;
        out.tokensGenerated = fused.metrics.tokens_generated;
        out.tokensAccepted = fused.metrics.tokens_accepted;
        out.verifyRejects = fused.metrics.verify_rejects;
        out.specDepth = fused.metrics.spec_depth;
        out.specHeads = fused.metrics.spec_heads;
        out.specHeadsPruned = fused.metrics.spec_heads_pruned;

        for (uint32_t i = 0; i < n; ++i) {
            out.tokens[i] = fused.tokens[i];
        }

        const std::vector<int32_t> decodeTokens(fused.tokens.begin(), fused.tokens.begin() + n);
        const std::string text = trim_to(m_target->Detokenize(decodeTokens), sizeof(out.text) - 1);
        std::memcpy(out.text, text.data(), text.size());
        out.text[text.size()] = '\0';
    }

    AstHint astHintFrom(const std::string& uri,
                        const std::string& content,
                        uint32_t line,
                        uint32_t col) {
        AstHint hint;
        if (uri.empty() || content.empty()) {
            hint.scope = "Unknown";
            return hint;
        }

        const auto root = m_parser.parse(uri, content, LSP::TreeSitterParser::detectLanguage(uri, content));
        if (!root) {
            hint.scope = "Unknown";
            return hint;
        }

        const auto scope = m_parser.enclosingScope(root, line, col);
        hint.scope = scope ? nodeKindName(scope->kind) : "Unknown";

        std::unordered_set<std::string> seen;
        for (const auto& c : root->children) {
            if (!c || !c->isDeclaration() || c->name.empty()) continue;
            if (seen.insert(c->name).second) {
                hint.symbols.push_back(c->name);
            }
            if (hint.symbols.size() >= 64) break;
        }
        uint64_t sh = 1469598103934665603ull;
        for (const auto& s : hint.symbols) {
            sh = fnv1a64(s.data(), s.size(), sh);
        }
        hint.symbolHash = sh;

        // Lightweight member-access hint from source text near cursor.
        hint.memberAccess = hasMemberAccess(content, line, col);
        return hint;
    }

    uint64_t makeContextHash(const CompletionRequest& req, const std::string& prompt) const {
        uint64_t h = 1469598103934665603ull;
        h = fnv1a64(&req.line, sizeof(req.line), h);
        h = fnv1a64(&req.col, sizeof(req.col), h);
        h = fnv1a64(&req.flags, sizeof(req.flags), h);
        if (!prompt.empty()) {
            h = fnv1a64(prompt.data(), prompt.size(), h);
        }
        return h;
    }

    std::vector<int32_t> symbolsToAllowedTokens(const std::vector<std::string>& symbols) {
        std::vector<int32_t> out;
        out.reserve(symbols.size() + 8);
        std::unordered_set<int32_t> seen;
        for (const auto& s : symbols) {
            if (s.empty()) continue;
            const auto t = m_target->Tokenize(s);
            if (!t.empty() && seen.insert(t.front()).second) {
                out.push_back(t.front());
            }
            if (out.size() >= 128) break;
        }
        for (const char* kw : {"if", "for", "while", "return", "class", "struct"}) {
            const auto t = m_target->Tokenize(kw);
            if (!t.empty() && seen.insert(t.front()).second) {
                out.push_back(t.front());
            }
        }
        return out;
    }

    void appendRoutedExternalTokens(uint64_t activeFileHash,
                                    const AstHint& hint,
                                    std::vector<int32_t>& allowedTokens) {
        if (m_external_indexes.empty() || !m_target) return;
        RouteContext routeCtx;
        routeCtx.active_file = activeFileHash;
        routeCtx.active_symbol = hint.symbolHash;
        routeCtx.signature_hint = hint.symbolHash;
        routeCtx.edit_tick = ++m_route_tick;

        RouterLimits limits;
        limits.max_repos = 4;
        limits.max_symbols = 32;
        limits.max_symbols_per_repo = 12;
        limits.min_score = 0.65f;

        auto routed = routeCrossRepoSymbols(
            std::span<const RepoSymbolIndex* const>(m_external_indexes.data(), m_external_indexes.size()),
            routeCtx,
            limits);
        if (routed.empty()) return;

        std::unordered_set<int32_t> seen(allowedTokens.begin(), allowedTokens.end());
        for (const auto& sym : routed) {
            char label[64]{};
            std::snprintf(label, sizeof(label), "rx_%llx_%llx",
                          static_cast<unsigned long long>(sym.repo_id),
                          static_cast<unsigned long long>(sym.signature_hash ? sym.signature_hash : sym.symbol_id));
            const auto t = m_target->Tokenize(label);
            if (!t.empty() && seen.insert(t.front()).second) {
                allowedTokens.push_back(t.front());
            }
            if (allowedTokens.size() >= 160) break;
        }
    }

    static bool hasMemberAccess(const std::string& content, uint32_t line, uint32_t col) {
        uint32_t curLine = 0;
        size_t start = 0;
        for (size_t i = 0; i < content.size(); ++i) {
            if (curLine == line) {
                size_t end = content.find('\n', i);
                if (end == std::string::npos) end = content.size();
                const size_t cut = std::min<size_t>(col, end - i);
                const std::string before = content.substr(i, cut);
                return before.rfind("->") != std::string::npos
                    || before.rfind("::") != std::string::npos
                    || before.rfind('.') != std::string::npos;
            }
            if (content[i] == '\n') {
                ++curLine;
                start = i + 1;
            }
        }
        (void)start;
        return false;
    }

    static std::string nodeKindName(LSP::ASTNodeKind k) {
        using K = LSP::ASTNodeKind;
        switch (k) {
            case K::FunctionDecl: return "FunctionDecl";
            case K::FunctionDef: return "FunctionDef";
            case K::ClassDecl: return "ClassDecl";
            case K::StructDecl: return "StructDecl";
            case K::EnumDecl: return "EnumDecl";
            case K::VariableDecl: return "VariableDecl";
            case K::Namespace: return "Namespace";
            case K::Block: return "Block";
            default: return "Unknown";
        }
    }

private:
    InferenceEngine* m_draft = nullptr;
    InferenceEngine* m_target = nullptr;

    IPC::ShmBiChannel m_channel;
    InferenceController m_controller;
    LSP::TreeSitterParser m_parser;
    KernelArbiter m_arbiter;
    std::vector<const RepoSymbolIndex*> m_external_indexes;
    uint32_t m_route_tick = 1;
};

// -----------------------------------------------------------------------------
// Narrow C-style entry point for host wiring
// -----------------------------------------------------------------------------
extern "C" __declspec(dllexport)
bool RawrXD_AutocompleteKernel_OnIdle(AutocompleteKernel* kernel) {
    return kernel ? kernel->onIdle() : false;
}

extern "C" __declspec(dllexport)
bool RawrXD_AutocompleteKernel_RegisterRepoIndex(AutocompleteKernel* kernel,
                                                 const RepoSymbolIndex* index) {
    return kernel ? kernel->registerRepoIndex(index) : false;
}

} // namespace RawrXD::ExtensionKernel
