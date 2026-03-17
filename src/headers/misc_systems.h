#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>

// ============================================================================
// CATEGORY 5: Miscellaneous Systems (~15+ symbols)
// ============================================================================

// ============================================================================
// TelemetryCollector
// ============================================================================

struct JsonValue {
    // Simplified JSON value representation
};

class TelemetryCollector {
public:
    TelemetryCollector(const TelemetryCollector&) = delete;
    TelemetryCollector& operator=(const TelemetryCollector&) = delete;

    static TelemetryCollector* instance();
    
    void trackFeatureUsage(const std::string& feature_name, 
                           const std::unordered_map<std::string, JsonValue>& metadata);
    void trackPerformance(const std::string& metric_name, double value, 
                          const std::string& unit);
    
    std::unordered_map<std::string, JsonValue> getAllTelemetryData() const;

private:
    TelemetryCollector() = default;
    ~TelemetryCollector() = default;
};

// ============================================================================
// HotpatchSymbolProvider
// ============================================================================

namespace RawrXD {
namespace LSPBridge {

struct HotpatchSymbolEntry {
    std::string symbol_name;
    uint64_t address;
    std::string symbol_type;
};

}  // namespace LSPBridge
}  // namespace RawrXD

struct PatchResult {
    bool success;
    std::string message;
    int error_code;
};

class HotpatchSymbolProvider {
public:
    HotpatchSymbolProvider(const HotpatchSymbolProvider&) = delete;
    HotpatchSymbolProvider& operator=(const HotpatchSymbolProvider&) = delete;

    static HotpatchSymbolProvider& instance();
    
    std::vector<RawrXD::LSPBridge::HotpatchSymbolEntry> getAllSymbols() const;
    PatchResult rebuildIndex();

private:
    HotpatchSymbolProvider() = default;
    ~HotpatchSymbolProvider() = default;
};

// ============================================================================
// LSPHotpatchBridge
// ============================================================================

class LSPHotpatchBridge {
public:
    LSPHotpatchBridge(const LSPHotpatchBridge&) = delete;
    LSPHotpatchBridge& operator=(const LSPHotpatchBridge&) = delete;

    static LSPHotpatchBridge& instance();
    
    PatchResult rebuildSymbolIndex();
    PatchResult refreshDiagnostics();
    PatchResult detach();

private:
    LSPHotpatchBridge() = default;
    ~LSPHotpatchBridge() = default;
};

// ============================================================================
// ContextDeteriorationHotpatch
// ============================================================================

struct ContextPrepareResult {
    bool success;
    std::string prepared_context;
    uint32_t context_quality_score;
};

struct ContextDeteriorationHotpatchStats {
    uint32_t contexts_prepared;
    uint32_t degradation_events;
    float average_quality_score;
    uint64_t total_preparation_time_ms;
};

class ContextDeteriorationHotpatch {
public:
    ContextDeteriorationHotpatch(const ContextDeteriorationHotpatch&) = delete;
    ContextDeteriorationHotpatch& operator=(const ContextDeteriorationHotpatch&) = delete;

    static ContextDeteriorationHotpatch& instance();
    
    const ContextDeteriorationHotpatchStats& getStats() const;
    ContextPrepareResult prepareContextForInference(const std::string& context, 
                                                     uint32_t max_tokens,
                                                     const char* model_type);

private:
    ContextDeteriorationHotpatch() = default;
    ~ContextDeteriorationHotpatch() = default;
};

// ============================================================================
// RawrXD::LayerOffloadManager
// ============================================================================

namespace RawrXD {

class LayerOffloadManager {
public:
    LayerOffloadManager(const LayerOffloadManager&) = delete;
    LayerOffloadManager& operator=(const LayerOffloadManager&) = delete;

    static LayerOffloadManager& instance();
    
    bool shouldSkipLayer(uint32_t layer_id) const;
    PatchResult ensureLayerResident(uint32_t layer_id);
    PatchResult prefetchLayer(uint32_t layer_id);
    PatchResult releaseLayer(uint32_t layer_id);

private:
    LayerOffloadManager() = default;
    ~LayerOffloadManager() = default;
};

}  // namespace RawrXD

// ============================================================================
// RawrXD::Parity
// ============================================================================

namespace RawrXD {
namespace Parity {

/**
 * Verify feature-module wiring
 * @param context Implementation-specific context
 * @return Non-zero if wiring is correct
 */
int verifyFeaturesWiring(void* context);

}  // namespace Parity
}  // namespace RawrXD

// ============================================================================
// JSExtensionHost
// ============================================================================

struct JSExtensionState {
    std::string extension_id;
    std::string extension_name;
    bool is_active;
    uint32_t version;
};

class JSExtensionHost {
public:
    JSExtensionHost(const JSExtensionHost&) = delete;
    JSExtensionHost& operator=(const JSExtensionHost&) = delete;

    static JSExtensionHost& instance();
    
    PatchResult initialize();
    bool isInitialized() const;
    
    PatchResult activateExtension(const char* extension_id);
    PatchResult deactivateExtension(const char* extension_id);
    
    void getLoadedExtensions(JSExtensionState* extensions, uint64_t max_count, uint64_t* actual_count) const;

private:
    JSExtensionHost() = default;
    ~JSExtensionHost() = default;
};

// ============================================================================
// RawrXD::Plugin::PluginSignatureVerifier
// ============================================================================

namespace RawrXD {
namespace Plugin {

class PluginSignatureVerifier {
public:
    PluginSignatureVerifier(const PluginSignatureVerifier&) = delete;
    PluginSignatureVerifier& operator=(const PluginSignatureVerifier&) = delete;

    static PluginSignatureVerifier& instance();
    
    bool initialize();
    void shutdown();

private:
    PluginSignatureVerifier() = default;
    ~PluginSignatureVerifier() = default;
};

}  // namespace Plugin
}  // namespace RawrXD

// ============================================================================
// RawrXD::Sandbox::PluginSandbox
// ============================================================================

namespace RawrXD {
namespace Sandbox {

struct SandboxResult {
    bool success;
    std::string message;
    int error_code;
};

class PluginSandbox {
public:
    PluginSandbox(const PluginSandbox&) = delete;
    PluginSandbox& operator=(const PluginSandbox&) = delete;

    static PluginSandbox& instance();
    
    SandboxResult initialize();
    void shutdown();

private:
    PluginSandbox() = default;
    ~PluginSandbox() = default;
};

}  // namespace Sandbox
}  // namespace RawrXD

// ============================================================================
// RawrXD::Swarm::SwarmReconciler
// ============================================================================

namespace RawrXD {
namespace Swarm {

class SwarmReconciler {
public:
    SwarmReconciler(const SwarmReconciler&) = delete;
    SwarmReconciler& operator=(const SwarmReconciler&) = delete;

    static SwarmReconciler& instance();
    
    void shutdown();

private:
    SwarmReconciler() = default;
    ~SwarmReconciler() = default;
};

}  // namespace Swarm
}  // namespace RawrXD

// ============================================================================
// RawrXD::Update::AutoUpdateSystem
// ============================================================================

namespace RawrXD {
namespace Update {

struct UpdateCheckResult {
    bool update_available;
    std::string new_version;
    std::string release_notes;
    std::string download_url;
};

typedef void (*UpdateCheckCallback)(const UpdateCheckResult* result, void* context);

class AutoUpdateSystem {
public:
    AutoUpdateSystem(const AutoUpdateSystem&) = delete;
    AutoUpdateSystem& operator=(const AutoUpdateSystem&) = delete;

    static AutoUpdateSystem& instance();
    
    void setCurrentVersion(uint32_t major, uint32_t minor, uint32_t patch, uint32_t build);
    void setRepository(const char* repo_url, const char* auth_token);
    void checkForUpdatesAsync(UpdateCheckCallback callback, void* context);

private:
    AutoUpdateSystem() = default;
    ~AutoUpdateSystem() = default;
};

}  // namespace Update
}  // namespace RawrXD

// ============================================================================
// RawrXD::Recovery::DiskRecoveryAsmAgent
// ============================================================================

namespace RawrXD {
namespace Recovery {

struct AsmRecoveryResult {
    bool success;
    std::string status;
    uint64_t data_recovered_bytes;
};

struct AsmRecoveryStats {
    uint32_t total_sectors_scanned;
    uint32_t sectors_recovered;
    uint64_t recovery_time_ms;
    float success_rate;
};

class DiskRecoveryAsmAgent {
public:
    DiskRecoveryAsmAgent() = default;
    ~DiskRecoveryAsmAgent();
    
    DiskRecoveryAsmAgent(const DiskRecoveryAsmAgent&) = delete;
    DiskRecoveryAsmAgent& operator=(const DiskRecoveryAsmAgent&) = delete;
    DiskRecoveryAsmAgent(DiskRecoveryAsmAgent&&) = default;
    DiskRecoveryAsmAgent& operator=(DiskRecoveryAsmAgent&&) = default;

    AsmRecoveryResult Initialize(int drive_index);
    int FindDrive(void);
    AsmRecoveryResult ExtractEncryptionKey(void);
    AsmRecoveryResult RunRecovery(void);
    AsmRecoveryStats GetStats(void) const;
    void Abort(void);
};

}  // namespace Recovery
}  // namespace RawrXD

// ============================================================================
// EngineRegistry
// ============================================================================

class Engine {
public:
    virtual ~Engine() = default;
};

class EngineRegistry {
public:
    static void register_engine(Engine* engine);
};

// ============================================================================
// VSCodeMarketplace
// ============================================================================

struct VSCodeMarketplace {
    struct MarketplaceEntry {
        std::string id;
        std::string name;
        std::string description;
        std::string version;
        uint64_t download_count;
    };

    static bool Query(const std::string& search_term, int skip, int take, 
                     std::vector<MarketplaceEntry>& results);
    
    static bool DownloadVsix(const std::string& extension_id, 
                            const std::string& version,
                            const std::string& output_path,
                            const std::string& api_key);
};

// ============================================================================
// IDE Support Classes
// ============================================================================

struct CommandContext {
    std::string command_name;
    nlohmann::json parameters;
};

struct CommandResult {
    bool success;
    std::string output;
    int error_code;
};

// Local Parity C binding
extern "C" {
void LocalParity_SetModelPath(const char* model_path);
}

// ============================================================================
// Supporting MeshBrain C bindings
// ============================================================================

extern "C" {
uint64_t asm_mesh_crdt_lookup(uint64_t value);
uint32_t asm_mesh_topology_count(void);
const uint64_t* asm_mesh_topology_list(void);
void asm_mesh_topology_remove(const uint64_t* peer_id);
}

struct MeshNodeInfo {
    uint64_t node_id;
    std::string node_name;
    uint64_t last_heartbeat_ms;
};

// ============================================================================
// RawrXD::ProjectContext
// ============================================================================

namespace RawrXD {

class ProjectContext {
public:
    ProjectContext();
    ~ProjectContext() = default;
    
    ProjectContext(const ProjectContext&) = delete;
    ProjectContext& operator=(const ProjectContext&) = delete;
};

}  // namespace RawrXD

// ============================================================================
// UI/IDE Components (minimal declarations for linking)
// ============================================================================

class BenchmarkMenu {
public:
    explicit BenchmarkMenu(struct HWND__* hwnd);
    ~BenchmarkMenu();
    
    void initialize();
    void openBenchmarkDialog();
};

class CheckpointManager {
public:
    explicit CheckpointManager(void* context);
    ~CheckpointManager();
    
    bool initialize(const std::string& config_path, int version);
};

class FeatureRegistryPanel {
public:
    ~FeatureRegistryPanel();
};

class IocpFileWatcher {
public:
    IocpFileWatcher();
    ~IocpFileWatcher();
    
    bool Start(const std::wstring& directory);
    void Stop();
    void SetCallback(std::function<void(const std::string&)> callback);
};

class ModelRegistry {
public:
    explicit ModelRegistry(void* context);
    ~ModelRegistry();
    
    void initialize();
    std::vector<std::string> getAllModels() const;
    bool setActiveModel(int model_index);
};

class MultiFileSearchWidget {
public:
    ~MultiFileSearchWidget();
};

class UniversalModelRouter {
public:
    UniversalModelRouter();
    ~UniversalModelRouter();
    
    std::vector<std::string> getAvailableBackends() const;
    void initializeLocalEngine(const std::string& backend_name);
    void routeRequest(const std::string& prompt, const std::string& model_name,
                     const RawrXD::ProjectContext& context,
                     std::function<void(const std::string&, bool)> callback);
};

class InterpretabilityPanel {
public:
    void initialize();
    void setParent(struct HWND__* hwnd);
    void show();
};

namespace rawrxd {
namespace thermal {

class ThermalDashboard {
public:
    explicit ThermalDashboard(struct HWND__* hwnd);
    ~ThermalDashboard();
    
    void show();
};

}  // namespace thermal
}  // namespace rawrxd

namespace RawrXD {
namespace UI {

class MonacoSettingsDialog {
public:
    explicit MonacoSettingsDialog(struct HWND__* hwnd);
    ~MonacoSettingsDialog();
    
    int64_t showModal();
};

}  // namespace UI
}  // namespace RawrXD

// Forward declarations for Win32 IDE internals
struct IDEConfig {
    void setDefaults();
    int getInt(const std::string& key, int default_value) const;
};

class Win32IDE {
public:
    void logMessage(const std::string& category, const std::string& message);
};
