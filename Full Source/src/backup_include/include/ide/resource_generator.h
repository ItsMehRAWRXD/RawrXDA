// ============================================================================
// resource_generator.h — Pluginable Resource/Cloud Provisioning System
// ============================================================================
// Template-based resource generation for infrastructure-as-code:
//   - Docker, Kubernetes, Terraform, CloudFormation
//   - Azure ARM, GCP Deployment Manager
//   - CI/CD pipelines (GitHub Actions, GitLab CI)
//   - C-ABI DLL plugin contract for custom generators
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace RawrXD {
namespace Resource {

// ============================================================================
// Resource Category
// ============================================================================
enum class ResourceCategory : uint16_t {
    Container       = 0,    // Docker, Podman
    Orchestration   = 1,    // Kubernetes, Docker Compose
    IaC             = 2,    // Terraform, Pulumi, CDK
    CloudFormation  = 3,    // AWS CloudFormation
    AzureARM        = 4,    // Azure Resource Manager
    GCPDeploy       = 5,    // GCP Deployment Manager
    CICD            = 6,    // GitHub Actions, GitLab CI, Jenkins
    Database        = 7,    // Migration scripts, schema files
    Config          = 8,    // nginx.conf, .env, settings
    Security        = 9,    // SSL certs, IAM policies
    Monitoring      = 10,   // Prometheus rules, Grafana dashboards
    Custom          = 99,
};

// ============================================================================
// Template Parameter
// ============================================================================
struct TemplateParam {
    std::string     name;
    std::string     description;
    std::string     defaultValue;
    std::string     type;           // "string", "int", "bool", "enum", "list"
    std::vector<std::string> enumValues; // For type="enum"
    bool            required = false;
};

// ============================================================================
// Resource Template — describes what can be generated
// ============================================================================
struct ResourceTemplate {
    std::string                     id;             // e.g. "docker.multistage"
    std::string                     name;           // Display name
    std::string                     description;
    ResourceCategory                category = ResourceCategory::Custom;
    std::string                     categoryName;
    std::string                     outputFilename; // Default filename
    std::vector<TemplateParam>      params;
    std::vector<std::string>        tags;           // Searchable tags
    
    std::string                     templateContent;// Mustache-like template body
    
    // Generate function (if not using template string)
    std::function<std::string(const std::map<std::string, std::string>& params)> generate;
};

// ============================================================================
// Generation Result
// ============================================================================
struct GenerationResult {
    bool            success = false;
    std::string     content;
    std::string     suggestedFilename;
    std::string     error;
    std::string     explanation;
    
    // Multi-file output
    struct FileOutput {
        std::string path;
        std::string content;
    };
    std::vector<FileOutput> files;
    
    static GenerationResult ok(const std::string& content,
                                const std::string& filename,
                                const std::string& explanation = "") {
        GenerationResult r;
        r.success = true;
        r.content = content;
        r.suggestedFilename = filename;
        r.explanation = explanation;
        return r;
    }
    
    static GenerationResult fail(const std::string& error) {
        GenerationResult r;
        r.success = false;
        r.error = error;
        return r;
    }
    
    static GenerationResult multiFile(const std::vector<FileOutput>& files,
                                       const std::string& explanation = "") {
        GenerationResult r;
        r.success = true;
        r.files = files;
        r.explanation = explanation;
        if (!files.empty()) {
            r.suggestedFilename = files[0].path;
            r.content = files[0].content;
        }
        return r;
    }
};

// ============================================================================
// C-ABI Plugin Contract
// ============================================================================
struct CResourceTemplate {
    const char*     id;
    const char*     name;
    const char*     description;
    const char*     category;
    const char*     outputFilename;
    const char*     params;         // JSON-encoded parameter array
    const char*     tags;           // Comma-separated
};

struct CGenerationResult {
    int32_t         success;
    const char*     content;
    const char*     suggestedFilename;
    const char*     error;
    int32_t         fileCount;
};

struct CResourcePluginInfo {
    const char*     name;
    const char*     version;
    const char*     author;
    int32_t         templateCount;
};

extern "C" {
    typedef CResourcePluginInfo* (*ResourcePlugin_GetInfo_fn)();
    typedef int     (*ResourcePlugin_Init_fn)(const char* configJson);
    typedef int     (*ResourcePlugin_GetTemplates_fn)(CResourceTemplate* outTemplates,
                                                       int maxCount);
    typedef CGenerationResult (*ResourcePlugin_Generate_fn)(const char* templateId,
                                                              const char* paramsJson);
    typedef void    (*ResourcePlugin_Shutdown_fn)();
}

// ============================================================================
// Resource Generator Engine — Singleton
// ============================================================================
class ResourceGeneratorEngine {
public:
    static ResourceGeneratorEngine& Instance();
    ~ResourceGeneratorEngine();
    
    // ── Lifecycle ──
    void Initialize();
    void Shutdown();
    
    // ── Template Registration ──
    void RegisterTemplate(const ResourceTemplate& tmpl);
    void UnregisterTemplate(const std::string& id);
    
    // ── Plugin Management ──
    bool LoadPlugin(const std::string& dllPath);
    void UnloadPlugin(const std::string& name);
    void UnloadAllPlugins();
    std::vector<std::string> GetLoadedPlugins() const;
    
    // ── Discovery ──
    std::vector<ResourceTemplate> GetAllTemplates() const;
    std::vector<ResourceTemplate> GetByCategory(ResourceCategory cat) const;
    std::vector<ResourceTemplate> SearchTemplates(const std::string& query) const;
    const ResourceTemplate* FindById(const std::string& id) const;
    
    // ── Generation ──
    GenerationResult Generate(const std::string& templateId,
                               const std::map<std::string, std::string>& params);
    
    // Multi-template project scaffold
    GenerationResult GenerateProject(const std::vector<std::string>& templateIds,
                                      const std::map<std::string, std::string>& params);
    
    // ── Mustache-like Template Engine ──
    std::string RenderTemplate(const std::string& tmpl,
                                const std::map<std::string, std::string>& vars) const;
    
    // ── Statistics ──
    struct Stats {
        size_t totalTemplates = 0;
        size_t totalGenerations = 0;
        std::map<std::string, size_t> generationsByTemplate;
        std::map<std::string, size_t> generationsByCategory;
    };
    Stats GetStats() const;
    
private:
    ResourceGeneratorEngine() = default;
    void registerBuiltins();
    
    struct LoadedPlugin {
        std::string                         path;
        std::string                         name;
        void*                               hModule = nullptr;
        ResourcePlugin_GetInfo_fn           fnGetInfo = nullptr;
        ResourcePlugin_GetTemplates_fn      fnGetTemplates = nullptr;
        ResourcePlugin_Generate_fn          fnGenerate = nullptr;
        ResourcePlugin_Shutdown_fn          fnShutdown = nullptr;
    };
    
    mutable std::mutex                                  m_mutex;
    std::map<std::string, ResourceTemplate>             m_templates;
    std::vector<LoadedPlugin>                           m_plugins;
    
    mutable std::mutex                                  m_statsMutex;
    Stats                                               m_stats;
    
    std::atomic<bool>                                   m_initialized{false};
};

} // namespace Resource
} // namespace RawrXD
