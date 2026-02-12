// ============================================================================
// refactoring_plugin.h — Pluginable Refactoring Engine Header
// ============================================================================
// Defines the RefactoringEngine singleton, descriptor/result types,
// C-API structures for DLL plugin interop, and function pointer typedefs.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#ifndef RAWRXD_IDE_REFACTORING_PLUGIN_H
#define RAWRXD_IDE_REFACTORING_PLUGIN_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <atomic>
#include <mutex>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

// TransformationResult is defined in SmartRewriteEngine.h
#include "SmartRewriteEngine.h"

namespace RawrXD {
namespace IDE {

// ============================================================================
// Enums
// ============================================================================
enum class RefactoringCategory {
    Extract,
    Inline,
    Rename,
    Convert,
    ModernCpp,
    Organize,
    Safety,
    General,
    Custom
};

// ============================================================================
// RefactoringContext — input context for refactoring execution
// ============================================================================
struct RefactoringContext {
    std::string code;
    std::string language;
    std::string filePath;
    std::string selectedText;
    std::string symbolUnderCursor;
    int cursorLine = 0;
    int cursorColumn = 0;
    int selectionStartLine = 0;
    int selectionEndLine = 0;
    std::map<std::string, std::string> params;
};

// ============================================================================
// RefactoringDescriptor — describes a single refactoring operation
// ============================================================================
struct RefactoringDescriptor {
    std::string id;
    std::string name;
    std::string description;
    RefactoringCategory category = RefactoringCategory::General;
    std::string categoryName;
    bool requiresSelection = false;
    bool requiresSymbol = false;
    bool isMultiFile = false;
    int menuOrder = 100;
    std::vector<std::string> supportedLanguages; // empty = all languages

    // Execute callback: (code, language, params) -> TransformationResult
    std::function<TransformationResult(
        const std::string& code,
        const std::string& language,
        const std::map<std::string, std::string>& params)> execute;

    // Availability check (optional)
    std::function<bool(
        const std::string& code,
        const std::string& language,
        int cursorLine,
        int selectionStart,
        int selectionEnd)> isAvailable;
};

// ============================================================================
// RefactoringResult — result of executing a refactoring
// ============================================================================
struct RefactoringResult {
    bool success = false;
    std::string error;
    std::string explanation;
    float confidence = 0.0f;
    bool requiresApproval = false;

    struct FileEdit {
        std::string filePath;
        std::string originalContent;
        std::string newContent;
        int startLine = 0;
        int endLine = 0;
    };

    std::vector<FileEdit> edits;

    static RefactoringResult ok(std::vector<FileEdit> edits,
                                 const std::string& explanation = "",
                                 float confidence = 1.0f) {
        RefactoringResult r;
        r.success = true;
        r.edits = std::move(edits);
        r.explanation = explanation;
        r.confidence = confidence;
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
// C-API Interop Structures (for DLL plugin boundary)
// ============================================================================
struct CRefactoringDescriptor {
    const char* id;
    const char* name;
    const char* description;
    const char* category;
    const char* languages;       // comma-separated
    int requiresSelection;
    int requiresSymbol;
    int isMultiFile;
};

struct CRefactoringResult {
    int success;
    const char* transformedCode;
    const char* error;
    float confidence;
};

struct CPluginInfo {
    const char* name;
    const char* version;
    const char* author;
    const char* description;
};

// ============================================================================
// Plugin Function Pointer Typedefs
// ============================================================================
typedef CPluginInfo* (*RefactoringPlugin_GetInfo_fn)();
typedef int          (*RefactoringPlugin_GetDescriptors_fn)(CRefactoringDescriptor* out, int maxCount);
typedef CRefactoringResult (*RefactoringPlugin_Execute_fn)(const char* refactoringId,
                                                            const char* code,
                                                            const char* language,
                                                            const char* paramsJson);
typedef void         (*RefactoringPlugin_Shutdown_fn)();
typedef void         (*RefactoringPlugin_Init_fn)(const char* configJson);

// ============================================================================
// LoadedPlugin — represents a loaded DLL plugin
// ============================================================================
struct LoadedPlugin {
    std::string path;
    std::string name;
#ifdef _WIN32
    HMODULE hModule = nullptr;
#else
    void* hModule = nullptr;
#endif
    RefactoringPlugin_GetInfo_fn       fnGetInfo = nullptr;
    RefactoringPlugin_GetDescriptors_fn fnGetDescriptors = nullptr;
    RefactoringPlugin_Execute_fn       fnExecute = nullptr;
    RefactoringPlugin_Shutdown_fn      fnShutdown = nullptr;
};

// ============================================================================
// RefactoringEngine — Singleton managing all refactoring operations
// ============================================================================
class RefactoringEngine {
public:
    static RefactoringEngine& Instance();
    ~RefactoringEngine();

    // Lifecycle
    void Initialize();
    void Shutdown();

    // Registration
    void RegisterRefactoring(const RefactoringDescriptor& desc);
    void UnregisterRefactoring(const std::string& id);

    // Plugin Management
    bool LoadPlugin(const std::string& dllPath);
    void UnloadPlugin(const std::string& name);
    void UnloadAllPlugins();
    std::vector<std::string> GetLoadedPlugins() const;

    // Discovery
    std::vector<RefactoringDescriptor> GetAllRefactorings() const;
    std::vector<RefactoringDescriptor> GetByCategory(RefactoringCategory cat) const;
    std::vector<RefactoringDescriptor> GetByLanguage(const std::string& lang) const;
    std::vector<RefactoringDescriptor> GetAvailable(const RefactoringContext& ctx) const;
    const RefactoringDescriptor* FindById(const std::string& id) const;

    // Execution
    RefactoringResult Execute(const std::string& refactoringId,
                               const RefactoringContext& ctx);
    std::vector<RefactoringResult> ExecuteBatch(
        const std::vector<std::string>& refactoringIds,
        const RefactoringContext& ctx);
    std::string PreviewRefactoring(const std::string& refactoringId,
                                     const RefactoringContext& ctx);

    // Statistics
    struct Stats {
        uint64_t totalExecutions = 0;
        uint64_t successfulExecutions = 0;
        uint64_t failedExecutions = 0;
        std::map<std::string, uint64_t> executionsByRefactoring;
        std::map<std::string, uint64_t> executionsByLanguage;
    };
    Stats GetStats() const;

private:
    RefactoringEngine() = default;
    RefactoringEngine(const RefactoringEngine&) = delete;
    RefactoringEngine& operator=(const RefactoringEngine&) = delete;

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

    // Data
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;
    mutable std::mutex m_statsMutex;
    std::map<std::string, RefactoringDescriptor> m_refactorings;
    std::vector<LoadedPlugin> m_plugins;
    Stats m_stats;
};

} // namespace IDE
} // namespace RawrXD

#endif // RAWRXD_IDE_REFACTORING_PLUGIN_H
