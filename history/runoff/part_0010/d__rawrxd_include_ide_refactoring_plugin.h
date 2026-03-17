// ============================================================================
// refactoring_plugin.h — Pluginable Automated Refactoring System
// ============================================================================
// Extends SmartRewriteEngine with a plugin architecture for 200+ refactorings:
//
//   Built-in refactoring categories:
//     - Extract (method, variable, constant, interface, class)
//     - Rename (symbol, file, namespace)
//     - Move (method, field, class to file)
//     - Inline (method, variable, constant)
//     - Change signature (add/remove/reorder params)
//     - Convert (for→forEach, if→ternary, class→interface, sync→async)
//     - Organize (imports, includes, using directives)
//     - Safety (add null checks, bounds checks, error handling)
//     - Performance (loop optimization, caching, lazy initialization)
//     - Modern C++ (auto, range-for, smart pointers, structured bindings)
//
//   Plugin interface (C-ABI DLL):
//     - Custom refactoring registration
//     - Language-specific refactoring providers
//     - AST-aware transformations via hooks
//
// Integrates with:
//   - SmartRewriteEngine (base transformation engine)
//   - ContextMentionParser (@codebase for scope)
//   - Win32IDE (UI for refactoring selection)
//   - TelemetryExporter (refactoring usage metrics)
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <memory>

namespace RawrXD {
namespace IDE {

// ============================================================================
// Refactoring categories
// ============================================================================
enum class RefactoringCategory {
    Extract,        // Extract method/variable/constant/interface
    Rename,         // Rename symbol/file/namespace
    Move,           // Move method/field/class
    Inline,         // Inline method/variable/constant
    Signature,      // Change function signature
    Convert,        // Convert between patterns
    Organize,       // Organize imports/includes
    Safety,         // Add safety checks
    Performance,    // Performance optimizations
    ModernCpp,      // Modern C++ idioms
    General,        // General-purpose
    Custom          // Plugin-provided
};

// ============================================================================
// Refactoring descriptor
// ============================================================================
struct RefactoringDescriptor {
    std::string         id;             // Unique identifier: "extract.method"
    std::string         name;           // Display name: "Extract Method"
    std::string         description;    // Detailed description
    RefactoringCategory category;
    std::string         categoryName;   // Human-readable category
    
    // Scope
    std::vector<std::string> supportedLanguages; // ["cpp", "python", "javascript"]
    bool                requiresSelection;  // Needs selected code?
    bool                requiresSymbol;     // Needs symbol under cursor?
    bool                isMultiFile;        // Can affect multiple files?
    
    // UI
    std::string         iconId;         // Icon for menu
    std::string         shortcut;       // Keyboard shortcut
    int                 menuOrder;      // Order in refactoring menu
    
    // Availability check
    std::function<bool(const std::string& code, const std::string& language,
                       int cursorLine, int selectionStart, int selectionEnd)>
        isAvailable;
    
    // The actual transformation
    std::function<TransformationResult(const std::string& code,
                                        const std::string& language,
                                        const std::map<std::string, std::string>& params)>
        execute;
};

// ============================================================================
// Refactoring execution context
// ============================================================================
struct RefactoringContext {
    std::string     filePath;
    std::string     code;
    std::string     language;
    int             cursorLine;
    int             cursorColumn;
    int             selectionStartLine;
    int             selectionEndLine;
    std::string     selectedText;
    std::string     symbolUnderCursor;
    
    // Additional parameters (from UI dialog)
    std::map<std::string, std::string> params;
};

// ============================================================================
// Refactoring result with multi-file support
// ============================================================================
struct RefactoringResult {
    bool            success;
    std::string     error;
    
    struct FileEdit {
        std::string filePath;
        std::string originalContent;
        std::string newContent;
        std::string diffPreview;
        int         addedLines;
        int         removedLines;
    };
    
    std::vector<FileEdit> edits;
    std::string          explanation;
    float               confidence;
    bool                requiresApproval;
    
    static RefactoringResult ok(const std::vector<FileEdit>& edits,
                                 const std::string& explanation = "",
                                 float confidence = 1.0f) {
        RefactoringResult r;
        r.success = true;
        r.edits = edits;
        r.explanation = explanation;
        r.confidence = confidence;
        r.requiresApproval = (confidence < 0.9f);
        return r;
    }
    static RefactoringResult fail(const std::string& err) {
        RefactoringResult r;
        r.success = false;
        r.error = err;
        return r;
    }
};

// ============================================================================
// Refactoring Provider Plugin interface (C-ABI for DLL)
// ============================================================================
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RefactoringPluginInfo {
    char name[64];
    char version[32];
    char description[256];
    char author[64];
    int  refactoringCount;              // How many refactorings this plugin provides
} RefactoringPluginInfo;

// C-ABI refactoring descriptor
typedef struct CRefactoringDescriptor {
    char id[128];
    char name[128];
    char description[512];
    char category[32];
    char languages[256];                // Comma-separated
    int  requiresSelection;
    int  requiresSymbol;
    int  isMultiFile;
} CRefactoringDescriptor;

// C-ABI refactoring result
typedef struct CRefactoringResult {
    int         success;
    char        error[512];
    const char* transformedCode;        // Plugin must keep this alive
    int         addedLines;
    int         removedLines;
    float       confidence;
} CRefactoringResult;

typedef RefactoringPluginInfo* (*RefactoringPlugin_GetInfo_fn)(void);
typedef int  (*RefactoringPlugin_Init_fn)(const char* configJson);
typedef int  (*RefactoringPlugin_GetDescriptors_fn)(CRefactoringDescriptor* out, int maxCount);
typedef CRefactoringResult (*RefactoringPlugin_Execute_fn)(const char* refactoringId,
                                                            const char* code,
                                                            const char* language,
                                                            const char* paramsJson);
typedef void (*RefactoringPlugin_Shutdown_fn)(void);

#ifdef __cplusplus
}
#endif

// ============================================================================
// RefactoringEngine — Main pluggable refactoring engine
// ============================================================================
class RefactoringEngine {
public:
    static RefactoringEngine& Instance();

    // ---- Lifecycle ----
    void Initialize();
    void Shutdown();
    bool IsInitialized() const { return m_initialized.load(); }

    // ---- Built-in Refactoring Registration ----
    void RegisterRefactoring(const RefactoringDescriptor& desc);
    void UnregisterRefactoring(const std::string& id);

    // ---- Plugin Management ----
    bool LoadPlugin(const std::string& dllPath);
    void UnloadPlugin(const std::string& name);
    void UnloadAllPlugins();
    std::vector<std::string> GetLoadedPlugins() const;

    // ---- Discovery ----
    std::vector<RefactoringDescriptor> GetAllRefactorings() const;
    std::vector<RefactoringDescriptor> GetByCategory(RefactoringCategory cat) const;
    std::vector<RefactoringDescriptor> GetByLanguage(const std::string& lang) const;
    std::vector<RefactoringDescriptor> GetAvailable(const RefactoringContext& ctx) const;
    const RefactoringDescriptor* FindById(const std::string& id) const;

    // ---- Execution ----
    RefactoringResult Execute(const std::string& refactoringId,
                               const RefactoringContext& ctx);
    
    // ---- Batch Execution ----
    std::vector<RefactoringResult> ExecuteBatch(
        const std::vector<std::string>& refactoringIds,
        const RefactoringContext& ctx);

    // ---- Preview ----
    std::string PreviewRefactoring(const std::string& refactoringId,
                                    const RefactoringContext& ctx);

    // ---- Statistics ----
    struct Stats {
        uint64_t totalExecutions;
        uint64_t successfulExecutions;
        uint64_t failedExecutions;
        std::map<std::string, uint64_t> executionsByRefactoring;
        std::map<std::string, uint64_t> executionsByLanguage;
    };
    Stats GetStats() const;

private:
    RefactoringEngine() = default;
    ~RefactoringEngine();

    void registerBuiltins();

    // Built-in refactoring implementations
    RefactoringResult doExtractMethod(const RefactoringContext& ctx);
    RefactoringResult doExtractVariable(const RefactoringContext& ctx);
    RefactoringResult doExtractConstant(const RefactoringContext& ctx);
    RefactoringResult doInlineVariable(const RefactoringContext& ctx);
    RefactoringResult doRenameSymbol(const RefactoringContext& ctx);
    RefactoringResult doConvertForToRangeFor(const RefactoringContext& ctx);
    RefactoringResult doConvertRawToSmartPtr(const RefactoringContext& ctx);
    RefactoringResult doOrganizeIncludes(const RefactoringContext& ctx);
    RefactoringResult doAddNullChecks(const RefactoringContext& ctx);
    RefactoringResult doConvertSyncToAsync(const RefactoringContext& ctx);
    RefactoringResult doAddErrorHandling(const RefactoringContext& ctx);
    RefactoringResult doConvertToAuto(const RefactoringContext& ctx);
    RefactoringResult doConvertToStructuredBindings(const RefactoringContext& ctx);
    RefactoringResult doSplitDeclaration(const RefactoringContext& ctx);
    RefactoringResult doConvertIfToTernary(const RefactoringContext& ctx);
    RefactoringResult doRemoveDeadCode(const RefactoringContext& ctx);
    RefactoringResult doAddBracesToControlFlow(const RefactoringContext& ctx);
    RefactoringResult doFlipConditional(const RefactoringContext& ctx);
    RefactoringResult doMergeNestedIf(const RefactoringContext& ctx);
    RefactoringResult doConvertStringToStringView(const RefactoringContext& ctx);

    std::atomic<bool>                           m_initialized{false};
    mutable std::mutex                          m_mutex;
    std::map<std::string, RefactoringDescriptor> m_refactorings;

    // Plugin storage
    struct LoadedPlugin {
        std::string                         name;
        std::string                         path;
        RefactoringPlugin_GetInfo_fn        fnGetInfo;
        RefactoringPlugin_GetDescriptors_fn fnGetDescriptors;
        RefactoringPlugin_Execute_fn        fnExecute;
        RefactoringPlugin_Shutdown_fn       fnShutdown;
#ifdef _WIN32
        HMODULE                             hModule;
#else
        void*                               hModule;
#endif
    };
    std::vector<LoadedPlugin>       m_plugins;

    mutable std::mutex              m_statsMutex;
    Stats                           m_stats{};
};

} // namespace IDE
} // namespace RawrXD
