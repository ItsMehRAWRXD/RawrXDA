// ============================================================================
// Win32IDE_ComponentManagers.h — Forward-complete type declarations for
// component manager classes used as unique_ptr members in Win32IDE.
//
// Include this in Win32IDE.cpp AFTER all other includes so that the
// unique_ptr destructors in ~Win32IDE() see the complete types.
//
// RULE: Each manager class defines its own destructor here as inline so
//       the compiler only needs this header (not the full .cpp) to generate
//       the correct destructor for std::unique_ptr<T>.
//
// NOTE: The actual class implementation lives in Win32IDE_<name>.cpp which
//       includes Win32IDE.h.  This header must NOT include Win32IDE.h itself
//       to avoid a circular dependency — it relies only on a forward-declare
//       of Win32IDE via the pointer type already available at include time.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

// ── EnterpriseStressTester ──────────────────────────────────────────────────
// Defined fully in Win32IDE_EnterpriseStressTests.cpp.
// Providing a destructor declaration here satisfies unique_ptr<T> in Win32IDE.cpp.
class Win32IDE;

class EnterpriseStressTester {
public:
    explicit EnterpriseStressTester(Win32IDE* ide);
    ~EnterpriseStressTester();
    bool initialize();
    bool executeStressTestSuite(int durationSeconds = 300, int threadCount = 8);
    void stopStressTest();
    bool isRunning() const;
};

// ── SQLite3DatabaseManager ───────────────────────────────────────────────────
class SQLite3DatabaseManager {
public:
    explicit SQLite3DatabaseManager(Win32IDE* ide);
    ~SQLite3DatabaseManager();
    bool initialize();
    void shutdown();
    bool saveSetting(const std::string& key, const std::string& value);
    std::string loadSetting(const std::string& key, const std::string& defaultValue = "");
    bool storeTelemetryEvent(const std::string& eventType, const std::string& eventData);
    bool saveAgentState(const std::string& agentId, const std::string& stateData);
    std::string loadAgentState(const std::string& agentId);
    std::vector<std::vector<std::string>> query(const std::string& sql);
};

// ── TelemetryExportManager ───────────────────────────────────────────────────
class TelemetryExportManager {
public:
    explicit TelemetryExportManager(Win32IDE* ide);
    ~TelemetryExportManager();
    bool initialize();
    bool exportTelemetryData(const std::string& format,
                              const std::string& timeRange,
                              const std::string& filename = "");
    std::vector<std::string> getSupportedFormats() const;
    std::string getExportDirectory() const;
};

// ── RefactoringPluginManager ─────────────────────────────────────────────────
class RefactoringPluginManager {
public:
    explicit RefactoringPluginManager(Win32IDE* ide);
    ~RefactoringPluginManager();
    bool initialize();
    bool handleCommand(int commandId);
    std::vector<std::string> getAvailableRefactorings() const;
    bool executeRefactoring(const std::string& type, int start, int end);
    bool applyRefactoring(const std::string& type, const std::string& code,
                          int startPos, int endPos, std::string& outCode);
};

// ── LanguagePluginManager ────────────────────────────────────────────────────
namespace IDEPlugin {
    struct CompletionItem;
    struct Diagnostic;
}

class LanguagePluginManager {
public:
    explicit LanguagePluginManager(Win32IDE* ide);
    ~LanguagePluginManager();
    bool initialize();
    bool handleCommand(int commandId);
    std::string detectLanguage(const std::string& filename, const std::string& content);
    std::string getCurrentLanguage() const;
    std::vector<std::string> getSupportedLanguages() const;
    bool supportsLSP(const std::string& filename) const;
    std::string getLSPServerCommand(const std::string& filename) const;
    std::vector<IDEPlugin::CompletionItem> getCompletions(const std::string& file,
                                                           const std::string& code,
                                                           int position);
    std::vector<IDEPlugin::Diagnostic> getDiagnostics(const std::string& file,
                                                        const std::string& code);
};

// ── ResourceGeneratorManager ─────────────────────────────────────────────────
class ResourceGeneratorManager {
public:
    explicit ResourceGeneratorManager(Win32IDE* ide);
    ~ResourceGeneratorManager();
    void initializeBuiltInGenerators();
    bool handleCommand(int commandId);
    std::vector<std::string> getAvailableGenerators() const;
    bool generateResource(const std::string& type, const std::string& name,
                          const std::string& outputPath,
                          const std::unordered_map<std::string, std::string>& params);
};
