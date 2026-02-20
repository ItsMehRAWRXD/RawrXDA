// ============================================================================
// resource_generator.cpp — Pluginable Resource/Cloud Provisioning Implementation
// ============================================================================
// Built-in templates + DLL plugin loader + Mustache-like template engine.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "ide/resource_generator.h"
#include <algorithm>
#include <sstream>
#include <regex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace Resource {

// ============================================================================
// Singleton
// ============================================================================
ResourceGeneratorEngine& ResourceGeneratorEngine::Instance() {
    static ResourceGeneratorEngine instance;
    return instance;
}

ResourceGeneratorEngine::~ResourceGeneratorEngine() {
    Shutdown();
}

// ============================================================================
// Lifecycle
// ============================================================================
void ResourceGeneratorEngine::Initialize() {
    if (m_initialized.load()) return;
    registerBuiltins();
    m_initialized.store(true);
}

void ResourceGeneratorEngine::Shutdown() {
    if (!m_initialized.load()) return;
    UnloadAllPlugins();
    m_initialized.store(false);
}

// ============================================================================
// Registration
// ============================================================================
void ResourceGeneratorEngine::RegisterTemplate(const ResourceTemplate& tmpl) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_templates[tmpl.id] = tmpl;
}

void ResourceGeneratorEngine::UnregisterTemplate(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_templates.erase(id);
}

// ============================================================================
// Plugin Management
// ============================================================================
bool ResourceGeneratorEngine::LoadPlugin(const std::string& dllPath) {
#ifdef _WIN32
    HMODULE hMod = LoadLibraryA(dllPath.c_str());
    if (!hMod) return false;
    
    LoadedPlugin plugin;
    plugin.path = dllPath;
    plugin.hModule = hMod;
    
    plugin.fnGetInfo = reinterpret_cast<ResourcePlugin_GetInfo_fn>(
        GetProcAddress(hMod, "ResourcePlugin_GetInfo"));
    plugin.fnGetTemplates = reinterpret_cast<ResourcePlugin_GetTemplates_fn>(
        GetProcAddress(hMod, "ResourcePlugin_GetTemplates"));
    plugin.fnGenerate = reinterpret_cast<ResourcePlugin_Generate_fn>(
        GetProcAddress(hMod, "ResourcePlugin_Generate"));
    plugin.fnShutdown = reinterpret_cast<ResourcePlugin_Shutdown_fn>(
        GetProcAddress(hMod, "ResourcePlugin_Shutdown"));
    
    if (!plugin.fnGetInfo || !plugin.fnGetTemplates || !plugin.fnGenerate) {
        FreeLibrary(hMod);
        return false;
    }
    
    auto* info = plugin.fnGetInfo();
    if (info) plugin.name = info->name;
    
    // Load templates from plugin
    CResourceTemplate cTemplates[64];
    int count = plugin.fnGetTemplates(cTemplates, 64);
    
    for (int i = 0; i < count; ++i) {
        ResourceTemplate tmpl;
        tmpl.id = cTemplates[i].id;
        tmpl.name = cTemplates[i].name;
        tmpl.description = cTemplates[i].description ? cTemplates[i].description : "";
        tmpl.categoryName = cTemplates[i].category ? cTemplates[i].category : "Custom";
        tmpl.category = ResourceCategory::Custom;
        tmpl.outputFilename = cTemplates[i].outputFilename ? cTemplates[i].outputFilename : "";
        
        // Parse tags
        if (cTemplates[i].tags) {
            std::istringstream ss(cTemplates[i].tags);
            std::string tag;
            while (std::getline(ss, tag, ',')) {
                while (!tag.empty() && tag[0] == ' ') tag.erase(0, 1);
                while (!tag.empty() && tag.back() == ' ') tag.pop_back();
                if (!tag.empty()) tmpl.tags.push_back(tag);
            }
        }
        
        // Wire generation to plugin
        auto* genFn = plugin.fnGenerate;
        std::string tmplId = tmpl.id;
        tmpl.generate = [genFn, tmplId](const std::map<std::string, std::string>& params)
            -> std::string {
            // Serialize params to JSON
            std::ostringstream pss;
            pss << "{";
            bool first = true;
            for (const auto& [k, v] : params) {
                if (!first) pss << ",";
                pss << "\"" << k << "\":\"" << v << "\"";
                first = false;
            }
            pss << "}";
            
            CGenerationResult cr = genFn(tmplId.c_str(), pss.str().c_str());
            return cr.success ? (cr.content ? cr.content : "") : "";
        };
        
        RegisterTemplate(tmpl);
    }
    
    m_plugins.push_back(std::move(plugin));
    return true;
#else
    (void)dllPath;
    return false;
#endif
}

void ResourceGeneratorEngine::UnloadPlugin(const std::string& name) {
    auto it = std::find_if(m_plugins.begin(), m_plugins.end(),
                            [&](const LoadedPlugin& p) { return p.name == name; });
    if (it == m_plugins.end()) return;
    
    if (it->fnShutdown) it->fnShutdown();
#ifdef _WIN32
    if (it->hModule) FreeLibrary(static_cast<HMODULE>(it->hModule));
#endif
    m_plugins.erase(it);
}

void ResourceGeneratorEngine::UnloadAllPlugins() {
    for (auto& p : m_plugins) {
        if (p.fnShutdown) p.fnShutdown();
#ifdef _WIN32
        if (p.hModule) FreeLibrary(static_cast<HMODULE>(p.hModule));
#endif
    }
    m_plugins.clear();
}

std::vector<std::string> ResourceGeneratorEngine::GetLoadedPlugins() const {
    std::vector<std::string> names;
    for (const auto& p : m_plugins) names.push_back(p.name);
    return names;
}

// ============================================================================
// Discovery
// ============================================================================
std::vector<ResourceTemplate> ResourceGeneratorEngine::GetAllTemplates() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ResourceTemplate> out;
    for (const auto& [id, tmpl] : m_templates) out.push_back(tmpl);
    return out;
}

std::vector<ResourceTemplate> ResourceGeneratorEngine::GetByCategory(
    ResourceCategory cat) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ResourceTemplate> out;
    for (const auto& [id, tmpl] : m_templates) {
        if (tmpl.category == cat) out.push_back(tmpl);
    }
    return out;
}

std::vector<ResourceTemplate> ResourceGeneratorEngine::SearchTemplates(
    const std::string& query) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string lq = query;
    std::transform(lq.begin(), lq.end(), lq.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    
    std::vector<ResourceTemplate> out;
    for (const auto& [id, tmpl] : m_templates) {
        std::string lname = tmpl.name;
        std::transform(lname.begin(), lname.end(), lname.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        std::string ldesc = tmpl.description;
        std::transform(ldesc.begin(), ldesc.end(), ldesc.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        
        if (lname.find(lq) != std::string::npos || ldesc.find(lq) != std::string::npos) {
            out.push_back(tmpl);
            continue;
        }
        
        for (const auto& tag : tmpl.tags) {
            std::string ltag = tag;
            std::transform(ltag.begin(), ltag.end(), ltag.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (ltag.find(lq) != std::string::npos) {
                out.push_back(tmpl);
                break;
            }
        }
    }
    return out;
}

const ResourceTemplate* ResourceGeneratorEngine::FindById(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_templates.find(id);
    return (it != m_templates.end()) ? &it->second : nullptr;
}

// ============================================================================
// Mustache-like Template Engine
// ============================================================================
std::string ResourceGeneratorEngine::RenderTemplate(
    const std::string& tmpl,
    const std::map<std::string, std::string>& vars) const {
    std::string result = tmpl;
    
    // Simple {{variable}} replacement
    std::regex varRe(R"(\{\{(\w+)\}\})");
    std::smatch match;
    
    std::string output;
    std::string remaining = result;
    
    while (std::regex_search(remaining, match, varRe)) {
        output += match.prefix().str();
        std::string key = match[1].str();
        auto it = vars.find(key);
        output += (it != vars.end()) ? it->second : ("{{" + key + "}}");
        remaining = match.suffix().str();
    }
    output += remaining;
    result = output;
    
    // {{#if variable}} ... {{/if}} conditional blocks
    std::regex ifRe(R"(\{\{#if\s+(\w+)\}\}([\s\S]*?)\{\{/if\}\})");
    while (std::regex_search(result, match, ifRe)) {
        std::string key = match[1].str();
        auto it = vars.find(key);
        bool truthy = (it != vars.end()) && !it->second.empty() &&
                       it->second != "false" && it->second != "0";
        
        std::string replacement = truthy ? match[2].str() : "";
        result = match.prefix().str() + replacement + match.suffix().str();
    }
    
    // {{#unless variable}} ... {{/unless}} inverted conditional
    std::regex unlessRe(R"(\{\{#unless\s+(\w+)\}\}([\s\S]*?)\{\{/unless\}\})");
    while (std::regex_search(result, match, unlessRe)) {
        std::string key = match[1].str();
        auto it = vars.find(key);
        bool falsy = (it == vars.end()) || it->second.empty() ||
                      it->second == "false" || it->second == "0";
        
        std::string replacement = falsy ? match[2].str() : "";
        result = match.prefix().str() + replacement + match.suffix().str();
    }
    
    return result;
}

// ============================================================================
// Generation
// ============================================================================
GenerationResult ResourceGeneratorEngine::Generate(
    const std::string& templateId,
    const std::map<std::string, std::string>& params) {
    
    const ResourceTemplate* tmpl = FindById(templateId);
    if (!tmpl) return GenerationResult::fail("Unknown template: " + templateId);
    
    // Validate required params
    for (const auto& p : tmpl->params) {
        if (p.required && params.find(p.name) == params.end()) {
            return GenerationResult::fail("Missing required parameter: " + p.name);
        }
    }
    
    // Merge defaults
    std::map<std::string, std::string> mergedParams = params;
    for (const auto& p : tmpl->params) {
        if (mergedParams.find(p.name) == mergedParams.end() && !p.defaultValue.empty()) {
            mergedParams[p.name] = p.defaultValue;
        }
    }
    
    std::string content;
    
    if (tmpl->generate) {
        content = tmpl->generate(mergedParams);
    } else if (!tmpl->templateContent.empty()) {
        content = RenderTemplate(tmpl->templateContent, mergedParams);
    } else {
        return GenerationResult::fail("Template has no generator or content");
    }
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalGenerations++;
        m_stats.generationsByTemplate[templateId]++;
        m_stats.generationsByCategory[tmpl->categoryName]++;
    }
    
    return GenerationResult::ok(content, tmpl->outputFilename, tmpl->description);
}

GenerationResult ResourceGeneratorEngine::GenerateProject(
    const std::vector<std::string>& templateIds,
    const std::map<std::string, std::string>& params) {
    
    std::vector<GenerationResult::FileOutput> files;
    
    for (const auto& id : templateIds) {
        auto result = Generate(id, params);
        if (!result.success) {
            return GenerationResult::fail("Failed on template " + id + ": " + result.error);
        }
        
        if (!result.files.empty()) {
            for (const auto& f : result.files) files.push_back(f);
        } else {
            files.push_back({result.suggestedFilename, result.content});
        }
    }
    
    return GenerationResult::multiFile(files, "Project scaffold generated");
}

// ============================================================================
// Statistics
// ============================================================================
ResourceGeneratorEngine::Stats ResourceGeneratorEngine::GetStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

// ============================================================================
// Built-in Template Definitions
// ============================================================================
void ResourceGeneratorEngine::registerBuiltins() {
    
    // ── Dockerfile: Simple ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "docker.simple";
        tmpl.name = "Dockerfile (Simple)";
        tmpl.description = "Simple single-stage Dockerfile";
        tmpl.category = ResourceCategory::Container;
        tmpl.categoryName = "Container";
        tmpl.outputFilename = "Dockerfile";
        tmpl.tags = {"docker", "container", "image"};
        tmpl.params = {
            {"baseImage", "Base image", "ubuntu:22.04", "string", {}, true},
            {"appName", "Application name", "myapp", "string", {}, true},
            {"port", "Exposed port", "8080", "string", {}, false},
            {"workdir", "Working directory", "/app", "string", {}, false},
        };
        tmpl.templateContent =
R"(FROM {{baseImage}}

LABEL maintainer="RawrXD IDE"

WORKDIR {{workdir}}

COPY . .

{{#if port}}EXPOSE {{port}}{{/if}}

CMD ["./{{appName}}"]
)";
        RegisterTemplate(tmpl);
    }
    
    // ── Dockerfile: Multi-stage C++ ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "docker.multistage.cpp";
        tmpl.name = "Dockerfile (Multi-stage C++)";
        tmpl.description = "Multi-stage build for C++ applications";
        tmpl.category = ResourceCategory::Container;
        tmpl.categoryName = "Container";
        tmpl.outputFilename = "Dockerfile";
        tmpl.tags = {"docker", "cpp", "cmake", "multistage"};
        tmpl.params = {
            {"appName", "Application name", "myapp", "string", {}, true},
            {"cmakeArgs", "Additional CMake arguments", "", "string", {}, false},
            {"port", "Exposed port", "8080", "string", {}, false},
        };
        tmpl.templateContent =
R"(# Build stage
FROM gcc:13 AS builder

RUN apt-get update && apt-get install -y cmake ninja-build && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN cmake -G Ninja -DCMAKE_BUILD_TYPE=Release {{cmakeArgs}} . && \
    cmake --build . --config Release --target {{appName}}

# Runtime stage
FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y libstdc++6 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=builder /build/{{appName}} .

{{#if port}}EXPOSE {{port}}{{/if}}

ENTRYPOINT ["./{{appName}}"]
)";
        RegisterTemplate(tmpl);
    }
    
    // ── Docker Compose ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "docker.compose";
        tmpl.name = "Docker Compose";
        tmpl.description = "Docker Compose service definition";
        tmpl.category = ResourceCategory::Orchestration;
        tmpl.categoryName = "Orchestration";
        tmpl.outputFilename = "docker-compose.yml";
        tmpl.tags = {"docker", "compose", "services"};
        tmpl.params = {
            {"serviceName", "Service name", "app", "string", {}, true},
            {"image", "Docker image", ".", "string", {}, false},
            {"port", "Host:Container port", "8080:8080", "string", {}, false},
            {"database", "Include database", "false", "bool", {}, false},
            {"dbType", "Database type", "postgres", "enum", {"postgres","mysql","redis","mongodb"}, false},
        };
        tmpl.templateContent =
R"(version: '3.8'

services:
  {{serviceName}}:
    build: {{image}}
    ports:
      - "{{port}}"
    environment:
      - NODE_ENV=production
{{#if database}}
    depends_on:
      - db

  db:
    image: {{dbType}}:latest
    environment:
      - POSTGRES_DB={{serviceName}}
      - POSTGRES_USER=admin
      - POSTGRES_PASSWORD=changeme
    volumes:
      - db_data:/var/lib/postgresql/data
    ports:
      - "5432:5432"

volumes:
  db_data:
{{/if}}
)";
        RegisterTemplate(tmpl);
    }
    
    // ── Kubernetes Deployment ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "k8s.deployment";
        tmpl.name = "Kubernetes Deployment";
        tmpl.description = "Kubernetes Deployment manifest";
        tmpl.category = ResourceCategory::Orchestration;
        tmpl.categoryName = "Orchestration";
        tmpl.outputFilename = "deployment.yaml";
        tmpl.tags = {"kubernetes", "k8s", "deployment", "pod"};
        tmpl.params = {
            {"appName", "Application name", "myapp", "string", {}, true},
            {"image", "Container image", "myapp:latest", "string", {}, true},
            {"replicas", "Number of replicas", "3", "string", {}, false},
            {"port", "Container port", "8080", "string", {}, false},
            {"namespace", "Kubernetes namespace", "default", "string", {}, false},
            {"cpuLimit", "CPU limit", "500m", "string", {}, false},
            {"memoryLimit", "Memory limit", "256Mi", "string", {}, false},
        };
        tmpl.templateContent =
R"(apiVersion: apps/v1
kind: Deployment
metadata:
  name: {{appName}}
  namespace: {{namespace}}
  labels:
    app: {{appName}}
spec:
  replicas: {{replicas}}
  selector:
    matchLabels:
      app: {{appName}}
  template:
    metadata:
      labels:
        app: {{appName}}
    spec:
      containers:
      - name: {{appName}}
        image: {{image}}
        ports:
        - containerPort: {{port}}
        resources:
          limits:
            cpu: {{cpuLimit}}
            memory: {{memoryLimit}}
          requests:
            cpu: "100m"
            memory: "64Mi"
        livenessProbe:
          httpGet:
            path: /health
            port: {{port}}
          initialDelaySeconds: 10
          periodSeconds: 30
        readinessProbe:
          httpGet:
            path: /ready
            port: {{port}}
          initialDelaySeconds: 5
          periodSeconds: 10
)";
        RegisterTemplate(tmpl);
    }
    
    // ── Kubernetes Service ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "k8s.service";
        tmpl.name = "Kubernetes Service";
        tmpl.description = "Kubernetes Service manifest";
        tmpl.category = ResourceCategory::Orchestration;
        tmpl.categoryName = "Orchestration";
        tmpl.outputFilename = "service.yaml";
        tmpl.tags = {"kubernetes", "k8s", "service", "networking"};
        tmpl.params = {
            {"appName", "Application name", "myapp", "string", {}, true},
            {"port", "Service port", "80", "string", {}, false},
            {"targetPort", "Target container port", "8080", "string", {}, false},
            {"serviceType", "Service type", "ClusterIP", "enum",
                {"ClusterIP","NodePort","LoadBalancer"}, false},
            {"namespace", "Kubernetes namespace", "default", "string", {}, false},
        };
        tmpl.templateContent =
R"(apiVersion: v1
kind: Service
metadata:
  name: {{appName}}-svc
  namespace: {{namespace}}
spec:
  type: {{serviceType}}
  selector:
    app: {{appName}}
  ports:
  - protocol: TCP
    port: {{port}}
    targetPort: {{targetPort}}
)";
        RegisterTemplate(tmpl);
    }
    
    // ── Terraform: AWS EC2 ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "terraform.aws.ec2";
        tmpl.name = "Terraform AWS EC2";
        tmpl.description = "Terraform configuration for AWS EC2 instance";
        tmpl.category = ResourceCategory::IaC;
        tmpl.categoryName = "Infrastructure as Code";
        tmpl.outputFilename = "main.tf";
        tmpl.tags = {"terraform", "aws", "ec2", "iac"};
        tmpl.params = {
            {"region", "AWS Region", "us-east-1", "string", {}, true},
            {"instanceType", "EC2 instance type", "t3.micro", "string", {}, false},
            {"ami", "AMI ID", "ami-0c55b159cbfafe1f0", "string", {}, true},
            {"projectName", "Project name", "myproject", "string", {}, true},
        };
        tmpl.templateContent =
R"(terraform {
  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = "{{region}}"
}

resource "aws_instance" "{{projectName}}" {
  ami           = "{{ami}}"
  instance_type = "{{instanceType}}"

  tags = {
    Name    = "{{projectName}}"
    Project = "{{projectName}}"
    ManagedBy = "Terraform"
  }
}

output "instance_id" {
  value = aws_instance.{{projectName}}.id
}

output "public_ip" {
  value = aws_instance.{{projectName}}.public_ip
}
)";
        RegisterTemplate(tmpl);
    }
    
    // ── Terraform: Azure Resource Group ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "terraform.azure.rg";
        tmpl.name = "Terraform Azure Resource Group";
        tmpl.description = "Terraform configuration for Azure Resource Group + VM";
        tmpl.category = ResourceCategory::IaC;
        tmpl.categoryName = "Infrastructure as Code";
        tmpl.outputFilename = "main.tf";
        tmpl.tags = {"terraform", "azure", "iac", "vm"};
        tmpl.params = {
            {"location", "Azure location", "eastus", "string", {}, true},
            {"projectName", "Project name", "myproject", "string", {}, true},
            {"vmSize", "VM size", "Standard_B1s", "string", {}, false},
        };
        tmpl.templateContent =
R"(terraform {
  required_providers {
    azurerm = {
      source  = "hashicorp/azurerm"
      version = "~> 3.0"
    }
  }
}

provider "azurerm" {
  features {}
}

resource "azurerm_resource_group" "{{projectName}}" {
  name     = "rg-{{projectName}}"
  location = "{{location}}"

  tags = {
    project   = "{{projectName}}"
    managedBy = "Terraform"
  }
}

output "resource_group_id" {
  value = azurerm_resource_group.{{projectName}}.id
}
)";
        RegisterTemplate(tmpl);
    }
    
    // ── GitHub Actions: CI ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "cicd.github.ci";
        tmpl.name = "GitHub Actions CI";
        tmpl.description = "GitHub Actions CI workflow";
        tmpl.category = ResourceCategory::CICD;
        tmpl.categoryName = "CI/CD";
        tmpl.outputFilename = ".github/workflows/ci.yml";
        tmpl.tags = {"github", "actions", "ci", "pipeline"};
        tmpl.params = {
            {"language", "Project language", "cpp", "enum",
                {"cpp","python","node","rust","go"}, true},
            {"projectName", "Project name", "myproject", "string", {}, true},
            {"os", "Runner OS", "ubuntu-latest", "enum",
                {"ubuntu-latest","windows-latest","macos-latest"}, false},
        };
        tmpl.generate = [](const std::map<std::string, std::string>& params) -> std::string {
            std::string lang = "cpp";
            auto it = params.find("language");
            if (it != params.end()) lang = it->second;
            
            std::string os = "ubuntu-latest";
            it = params.find("os");
            if (it != params.end()) os = it->second;
            
            std::string projName = "myproject";
            it = params.find("projectName");
            if (it != params.end()) projName = it->second;
            
            std::ostringstream oss;
            oss << "name: CI\n\n";
            oss << "on:\n  push:\n    branches: [main]\n  pull_request:\n    branches: [main]\n\n";
            oss << "jobs:\n  build:\n    runs-on: " << os << "\n    steps:\n";
            oss << "    - uses: actions/checkout@v4\n\n";
            
            if (lang == "cpp") {
                oss << "    - name: Configure CMake\n";
                oss << "      run: cmake -B build -DCMAKE_BUILD_TYPE=Release\n\n";
                oss << "    - name: Build\n";
                oss << "      run: cmake --build build --config Release\n\n";
                oss << "    - name: Test\n";
                oss << "      run: ctest --test-dir build --output-on-failure\n";
            } else if (lang == "python") {
                oss << "    - uses: actions/setup-python@v4\n";
                oss << "      with:\n        python-version: '3.11'\n\n";
                oss << "    - name: Install\n";
                oss << "      run: pip install -r requirements.txt\n\n";
                oss << "    - name: Test\n";
                oss << "      run: pytest\n";
            } else if (lang == "node") {
                oss << "    - uses: actions/setup-node@v4\n";
                oss << "      with:\n        node-version: 20\n\n";
                oss << "    - name: Install\n";
                oss << "      run: npm ci\n\n";
                oss << "    - name: Test\n";
                oss << "      run: npm test\n";
            } else if (lang == "rust") {
                oss << "    - uses: dtolnay/rust-toolchain@stable\n\n";
                oss << "    - name: Build\n";
                oss << "      run: cargo build --release\n\n";
                oss << "    - name: Test\n";
                oss << "      run: cargo test\n";
            } else if (lang == "go") {
                oss << "    - uses: actions/setup-go@v4\n";
                oss << "      with:\n        go-version: '1.21'\n\n";
                oss << "    - name: Build\n";
                oss << "      run: go build ./...\n\n";
                oss << "    - name: Test\n";
                oss << "      run: go test ./...\n";
            }
            
            return oss.str();
        };
        RegisterTemplate(tmpl);
    }
    
    // ── GitLab CI ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "cicd.gitlab";
        tmpl.name = "GitLab CI Pipeline";
        tmpl.description = "GitLab CI/CD configuration";
        tmpl.category = ResourceCategory::CICD;
        tmpl.categoryName = "CI/CD";
        tmpl.outputFilename = ".gitlab-ci.yml";
        tmpl.tags = {"gitlab", "ci", "pipeline"};
        tmpl.params = {
            {"image", "CI image", "gcc:13", "string", {}, true},
            {"projectName", "Project name", "myproject", "string", {}, true},
        };
        tmpl.templateContent =
R"(image: {{image}}

stages:
  - build
  - test
  - deploy

build:
  stage: build
  script:
    - cmake -B build -DCMAKE_BUILD_TYPE=Release
    - cmake --build build --config Release
  artifacts:
    paths:
      - build/

test:
  stage: test
  script:
    - cd build && ctest --output-on-failure

deploy:
  stage: deploy
  script:
    - echo "Deploy {{projectName}}"
  only:
    - main
)";
        RegisterTemplate(tmpl);
    }
    
    // ── .env file ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "config.env";
        tmpl.name = "Environment Variables (.env)";
        tmpl.description = "Environment variable configuration file";
        tmpl.category = ResourceCategory::Config;
        tmpl.categoryName = "Configuration";
        tmpl.outputFilename = ".env";
        tmpl.tags = {"env", "config", "environment"};
        tmpl.params = {
            {"appName", "Application name", "myapp", "string", {}, true},
            {"port", "Port", "8080", "string", {}, false},
            {"dbHost", "Database host", "localhost", "string", {}, false},
            {"dbPort", "Database port", "5432", "string", {}, false},
            {"dbName", "Database name", "mydb", "string", {}, false},
        };
        tmpl.templateContent =
R"(# {{appName}} Environment Configuration
# Generated by RawrXD IDE

# Application
APP_NAME={{appName}}
APP_PORT={{port}}
APP_ENV=development
LOG_LEVEL=info

# Database
DB_HOST={{dbHost}}
DB_PORT={{dbPort}}
DB_NAME={{dbName}}
DB_USER=admin
DB_PASSWORD=changeme

# Security
SECRET_KEY=change-this-in-production
CORS_ORIGINS=http://localhost:3000
)";
        RegisterTemplate(tmpl);
    }
    
    // ── Nginx Config ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "config.nginx";
        tmpl.name = "Nginx Configuration";
        tmpl.description = "Nginx reverse proxy configuration";
        tmpl.category = ResourceCategory::Config;
        tmpl.categoryName = "Configuration";
        tmpl.outputFilename = "nginx.conf";
        tmpl.tags = {"nginx", "proxy", "webserver", "config"};
        tmpl.params = {
            {"serverName", "Server name", "example.com", "string", {}, true},
            {"upstreamPort", "Backend port", "8080", "string", {}, true},
            {"sslEnabled", "Enable SSL", "false", "bool", {}, false},
        };
        tmpl.templateContent =
R"(upstream backend {
    server 127.0.0.1:{{upstreamPort}};
}

server {
    listen 80;
    server_name {{serverName}};

{{#if sslEnabled}}
    listen 443 ssl;
    ssl_certificate     /etc/ssl/certs/{{serverName}}.crt;
    ssl_certificate_key /etc/ssl/private/{{serverName}}.key;
{{/if}}

    location / {
        proxy_pass http://backend;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    location /health {
        return 200 'OK';
        add_header Content-Type text/plain;
    }
}
)";
        RegisterTemplate(tmpl);
    }
    
    // ── Prometheus Alert Rules ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "monitoring.prometheus.rules";
        tmpl.name = "Prometheus Alert Rules";
        tmpl.description = "Prometheus alerting rules";
        tmpl.category = ResourceCategory::Monitoring;
        tmpl.categoryName = "Monitoring";
        tmpl.outputFilename = "alert-rules.yml";
        tmpl.tags = {"prometheus", "alerting", "monitoring"};
        tmpl.params = {
            {"appName", "Application name", "myapp", "string", {}, true},
            {"cpuThreshold", "CPU threshold %", "80", "string", {}, false},
            {"memThreshold", "Memory threshold %", "85", "string", {}, false},
        };
        tmpl.templateContent =
R"(groups:
  - name: {{appName}}-alerts
    rules:
      - alert: HighCPUUsage
        expr: 100 - (avg by(instance) (rate(node_cpu_seconds_total{mode="idle"}[5m])) * 100) > {{cpuThreshold}}
        for: 5m
        labels:
          severity: warning
          app: {{appName}}
        annotations:
          summary: "High CPU usage on {{ $labels.instance }}"
          description: "CPU usage is above {{cpuThreshold}}% for 5 minutes"

      - alert: HighMemoryUsage
        expr: (1 - (node_memory_MemAvailable_bytes / node_memory_MemTotal_bytes)) * 100 > {{memThreshold}}
        for: 5m
        labels:
          severity: warning
          app: {{appName}}
        annotations:
          summary: "High memory usage on {{ $labels.instance }}"
          description: "Memory usage is above {{memThreshold}}%"

      - alert: ServiceDown
        expr: up{job="{{appName}}"} == 0
        for: 1m
        labels:
          severity: critical
          app: {{appName}}
        annotations:
          summary: "{{appName}} is down"
          description: "{{appName}} has been down for more than 1 minute"
)";
        RegisterTemplate(tmpl);
    }
    
    // ── CMakeLists.txt ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "config.cmake";
        tmpl.name = "CMakeLists.txt";
        tmpl.description = "CMake project configuration";
        tmpl.category = ResourceCategory::Config;
        tmpl.categoryName = "Configuration";
        tmpl.outputFilename = "CMakeLists.txt";
        tmpl.tags = {"cmake", "build", "cpp"};
        tmpl.params = {
            {"projectName", "Project name", "MyProject", "string", {}, true},
            {"cppStandard", "C++ Standard", "20", "enum", {"14","17","20","23"}, false},
            {"projectType", "Project type", "executable", "enum",
                {"executable","library","header-only"}, false},
        };
        tmpl.templateContent =
R"(cmake_minimum_required(VERSION 3.20)
project({{projectName}} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD {{cppStandard}})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES src/*.cpp)
file(GLOB_RECURSE HEADERS include/*.h include/*.hpp)

add_executable({{projectName}} ${SOURCES} ${HEADERS})

target_include_directories({{projectName}} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Optional: Add tests
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
)";
        RegisterTemplate(tmpl);
    }
    
    // ── .gitignore ──
    {
        ResourceTemplate tmpl;
        tmpl.id = "config.gitignore";
        tmpl.name = ".gitignore";
        tmpl.description = "Git ignore file for common project types";
        tmpl.category = ResourceCategory::Config;
        tmpl.categoryName = "Configuration";
        tmpl.outputFilename = ".gitignore";
        tmpl.tags = {"git", "ignore", "config"};
        tmpl.params = {
            {"language", "Project language", "cpp", "enum",
                {"cpp","python","node","rust","go","all"}, true},
        };
        tmpl.generate = [](const std::map<std::string, std::string>& params) -> std::string {
            std::string lang = "cpp";
            auto it = params.find("language");
            if (it != params.end()) lang = it->second;
            
            std::ostringstream oss;
            oss << "# OS\n.DS_Store\nThumbs.db\n*.swp\n*.swo\n*~\n\n";
            oss << "# IDE\n.vscode/\n.idea/\n*.suo\n*.user\n*.sln.docstates\n\n";
            
            if (lang == "cpp" || lang == "all") {
                oss << "# C++\nbuild/\ncmake-build*/\n*.o\n*.obj\n*.exe\n*.dll\n*.so\n*.dylib\n";
                oss << "*.a\n*.lib\n*.pdb\n*.ilk\n*.exp\nDebug/\nRelease/\nx64/\n\n";
            }
            if (lang == "python" || lang == "all") {
                oss << "# Python\n__pycache__/\n*.py[cod]\n*.egg-info/\ndist/\nvenv/\n.env\n\n";
            }
            if (lang == "node" || lang == "all") {
                oss << "# Node.js\nnode_modules/\nnpm-debug.log\nyarn-error.log\n.env\ndist/\n\n";
            }
            if (lang == "rust" || lang == "all") {
                oss << "# Rust\ntarget/\nCargo.lock\n\n";
            }
            if (lang == "go" || lang == "all") {
                oss << "# Go\nbin/\nvendor/\n*.test\n\n";
            }
            
            return oss.str();
        };
        RegisterTemplate(tmpl);
    }
    
    // Update stats
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalTemplates = m_templates.size();
    }
}

} // namespace Resource
} // namespace RawrXD
