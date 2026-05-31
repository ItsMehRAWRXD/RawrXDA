// ============================================================================
// EXTENSION_INFRASTRUCTURE_USAGE_EXAMPLE.cpp
// ============================================================================
// Example demonstrating integration of all 9 extension infrastructure systems
// This is reference code showing how all systems work together
// ============================================================================

#include "quickjs_extension_host.h"
#include "extension_activation_events.h"
#include "extension_manifest_parser.h"
#include "extension_permissions.h"
#include "marketplace_discovery_backend.h"
#include "extension_dependency_resolver.h"
#include "extension_auto_updater.h"
#include "extension_configuration_ui.h"
#include "workspace_trust_integration.h"

using namespace RawrXD::Extensions;

// ============================================================================
// Example 1: Load Extension from Marketplace and Parse Manifest
// ============================================================================
void ExampleLoadExtension() {
    auto& manifest_parser = ExtensionManifestParser();
    auto& marketplace = GetMarketplaceDiscoveryBackend();
    auto& activation_events = GetExtensionActivationEventManager();
    auto& permissions = GetExtensionPermissionsManager();

    // Step 1: Search marketplace for extension
    DiscoveryFilter filter;
    filter.searchQuery = "python";
    filter.sorting = "installs";
    auto results = marketplace.Search(filter);

    // Step 2: Parse extension manifest from downloaded VSIX
    if (!results.extensions.empty()) {
        const auto& ext = results.extensions[0];
        auto parseResult = manifest_parser.ParseString(ext.metadata);

        if (parseResult.success) {
            const auto& manifest = parseResult.manifest;
            
            // Step 3: Register permissions for extension
            auto permScopes = PermissionScope::FileSystemRead | 
                             PermissionScope::FileSystemWrite |
                             PermissionScope::NetworkRequestHTTPS;
            permissions.RegisterExtension(manifest.name, permScopes);

            // Step 4: Register activation events
            ActivationEvent activation_event;
            activation_event.extensionId = manifest.name;
            activation_event.eventTypes = {
                ActivationEventType::OnLanguage,
                ActivationEventType::OnCommand
            };
            activation_events.RegisterExtension(manifest.name, activation_event);
        }
    }
}

// ============================================================================
// Example 2: Resolve Extension Dependencies
// ============================================================================
void ExampleResolveDependencies() {
    auto& resolver = GetExtensionDependencyResolver();
    auto& marketplace = GetMarketplaceDiscoveryBackend();

    // Scenario: Python extension depends on "ms-python.vscode-pylance" ^1.1.0
    std::vector<Dependency> deps = {
        {"ms-python.vscode-pylance", VersionConstraint{ConstraintOperator::Caret, SemanticVersion{1, 1, 0}, {}}, false}
    };

    auto resolution = resolver.ResolveDependencies(deps);

    if (resolution.success) {
        std::cout << "Resolved " << resolution.resolved.size() << " dependencies\n";
        for (const auto& resolved : resolution.resolved) {
            std::cout << "  - " << resolved.extensionId 
                      << " @ " << resolved.version.ToString() << "\n";
        }

        // Check for conflicts
        auto conflicts = resolver.DetectConflicts(resolution.resolved);
        if (conflicts.empty()) {
            std::cout << "✅ No conflicts detected\n";
        } else {
            std::cout << "⚠️ Conflicts: " << conflicts.size() << "\n";
        }
    }
}

// ============================================================================
// Example 3: Check Permissions Before Executing Extension Activity
// ============================================================================
void ExamplePermissionGating() {
    auto& permissions = GetExtensionPermissionsManager();
    auto& workspace_trust = GetWorkspaceTrustManager();

    std::string extensionId = "ms-python.python";
    std::string workspacePath = "D:\\projects\\myapp";

    // Check if extension can write files in this workspace
    {
        WorkspaceTrustGuard guard(workspacePath, GuardedCapability::FileWrite, extensionId);
        
        if (guard.IsAllowed()) {
            std::cout << "Extension can write files\n";
            // Perform file operation
        } else {
            std::cout << "Permission denied: " << guard.GetReason() << "\n";
        }
    }

    // Check if extension can execute commands
    auto result = workspace_trust.VerifyCapability(
        workspacePath,
        GuardedCapability::ExecuteCommand,
        extensionId
    );

    if (result.allowed) {
        std::cout << "Extension can execute commands\n";
    } else {
        std::cout << "Capability denied: " << result.reason << "\n";
    }
}

// ============================================================================
// Example 4: Configure Extension Settings
// ============================================================================
void ExampleConfiguration() {
    auto& config_ui = GetExtensionConfigurationUI();

    // Register configuration schema for Python extension
    ConfigurationSchemaEntry pythonPath;
    pythonPath.key = "python.defaultInterpreterPath";
    pythonPath.type = ConfigValueType::String;
    pythonPath.title = "Python Interpreter Path";
    pythonPath.description = "Path to Python interpreter";
    pythonPath.defaultValue = "python3";

    config_ui.RegisterConfigurationSchema("ms-python.python", {pythonPath});

    // Set workspace-specific configuration
    ConfigurationValue userConfig;
    userConfig.key = "python.defaultInterpreterPath";
    userConfig.value = "C:\\Python310\\python.exe";
    userConfig.scope = ConfigScope::Workspace;

    if (config_ui.SetConfiguration(userConfig)) {
        std::cout << "Configuration saved\n";
    }

    // Retrieve configuration
    auto value = config_ui.GetConfiguration("python.defaultInterpreterPath");
    if (value.has_value()) {
        std::cout << "Configured path: " << value.value() << "\n";
    }
}

// ============================================================================
// Example 5: Handle Extension Updates
// ============================================================================
void ExampleAutoUpdates() {
    auto& updater = GetExtensionAutoUpdater();
    auto& marketplace = GetMarketplaceDiscoveryBackend();

    // Set update policy for all extensions
    updater.SetUpdatePolicy(UpdatePolicy::AutoInstall);

    // Register for update available notifications
    updater.OnUpdateAvailable([](const ExtensionUpdateInfo& info) {
        std::cout << "Update available: " << info.newVersion << "\n";
        std::cout << "Release notes: " << info.releaseNotes << "\n";
    });

    updater.OnUpdateInstalled([](const std::string& extensionId, const std::string& newVersion) {
        std::cout << "Updated " << extensionId << " to " << newVersion << "\n";
    });

    // Start background auto-update scheduler
    updater.StartAutoUpdateScheduler();

    // Manually check for updates
    auto updates = updater.CheckForUpdates();
    std::cout << "Found " << updates.updates.size() << " available updates\n";

    // Install specific update
    if (!updates.updates.empty()) {
        updater.InstallUpdate(updates.updates[0].extensionId);
    }
}

// ============================================================================
// Example 6: Activate Extension on Language Open
// ============================================================================
void ExampleActivationEvents() {
    auto& activation_events = GetExtensionActivationEventManager();

    // When user opens a Python file
    std::string pythonContent = "print('hello')";
    
    // Notify language opened event
    activation_events.NotifyLanguageOpened("python");

    // Get all activated extensions
    auto activated = activation_events.GetActivatedExtensions("python");
    std::cout << "Activated " << activated.size() << " extensions for Python\n";

    // Query available commands
    auto commands = activation_events.GetAvailableCommands("ms-python.python");
    std::cout << "Python extension provides " << commands.size() << " commands\n";
}

// ============================================================================
// Example 7: Sandbox Extension Code with QuickJS
// ============================================================================
void ExampleQuickJSSandbox() {
    auto& host = GetQuickJSExtensionHost();

    // Create sandboxed runtime for extension
    auto runtime = host.CreateExtensionRuntime(
        "ms-python.python",
        QuickJSExtensionConfig{
            .allowDataDirectory = true,
            .allowNetworkRequests = true,
            .maxMemoryMB = 256
        }
    );

    if (!runtime) {
        std::cerr << "Failed to create runtime\n";
        return;
    }

    // Create context for extension
    auto context = host.CreateJSContext(runtime);
    if (!context) {
        std::cerr << "Failed to create context\n";
        return;
    }

    // Compile extension source
    std::string extensionCode = R"(
        exports.activate = function(context) {
            console.log("Extension activated");
        };
    )";

    if (host.CompileExtensionSource(context, extensionCode)) {
        // Bind VSCode API
        host.BindVSCodeAPI(context);

        // Execute event loop
        host.PumpEventLoop(context, 100);

        std::cout << "Extension executed in sandbox\n";
    }
}

// ============================================================================
// Example 8: Complete Extension Lifecycle
// ============================================================================
void ExampleCompleteLifecycle() {
    std::cout << "\n========================================\n";
    std::cout << "Extension Infrastructure - Complete Flow\n";
    std::cout << "========================================\n\n";

    // 1. Initialize all systems
    std::cout << "1. Initializing extension infrastructure...\n";
    auto& activation = GetExtensionActivationEventManager();
    auto& perms = GetExtensionPermissionsManager();
    auto& marketplace = GetMarketplaceDiscoveryBackend();
    auto& updater = GetExtensionAutoUpdater();
    auto& config_ui = GetExtensionConfigurationUI();
    auto& trust = GetWorkspaceTrustManager();

    // 2. Trust workspace
    std::cout << "2. Trusting workspace...\n";
    trust.TrustWorkspace("D:\\projects\\workspace");

    // 3. Search marketplace
    std::cout << "3. Searching marketplace...\n";
    DiscoveryFilter filter{.searchQuery = "python", .sorting = "rating"};
    auto results = marketplace.Search(filter);

    // 4. Parse manifest and check permissions
    std::cout << "4. Parsing manifest and registering permissions...\n";
    if (!results.extensions.empty()) {
        const auto& ext = results.extensions[0];
        auto manifest = ExtensionManifestParser::Parse(ext.metadata);
        
        if (manifest.success) {
            perms.RegisterExtension(ext.name, 
                PermissionScope::FileSystemRead |
                PermissionScope::FileSystemWrite);
            
            std::cout << "   ✅ " << ext.name << " registered\n";
        }
    }

    // 5. Start auto-updates
    std::cout << "5. Starting auto-update scheduler...\n";
    updater.SetUpdatePolicy(UpdatePolicy::AutoInstall);
    updater.StartAutoUpdateScheduler();

    // 6. Load configuration
    std::cout << "6. Loading configuration...\n";
    config_ui.LoadConfiguration("config.json");

    std::cout << "\n✅ All systems initialized and ready\n\n";
}

// ============================================================================
// Main - Run Examples
// ============================================================================
int main() {
    try {
        std::cout << "Extension Infrastructure System Examples\n";
        std::cout << "=========================================\n\n";

        ExampleLoadExtension();
        ExampleResolveDependencies();
        ExamplePermissionGating();
        ExampleConfiguration();
        ExampleAutoUpdates();
        ExampleActivationEvents();
        ExampleQuickJSSandbox();
        ExampleCompleteLifecycle();

        std::cout << "\n✅ All examples completed successfully\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

// ============================================================================
// End of EXTENSION_INFRASTRUCTURE_USAGE_EXAMPLE.cpp
// ============================================================================
