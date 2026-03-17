# GitHub Model Integration Guide

## Overview

RawrXD now includes **real (non-simulated) GitHub integration** for model management. This allows you to:

- **Authenticate** with GitHub using Personal Access Tokens
- **Discover** model repositories from organizations
- **Clone** model repositories to local filesystem
- **Download** GGUF models from GitHub repositories
- **Publish** custom models to GitHub repositories  
- **Sync** custom models with GitHub registry

## Architecture

### Core Components

1. **GitHubAPIClient** - Real HTTP client using WinHTTP for GitHub REST API v3
2. **GitHubModelManager** - High-level model management singleton
3. **AutoLoaderGitHubBridge** - Integration layer for AutoModelLoader
4. **AutoModelLoader Extensions** - Built-in GitHub support in model loader

### Authentication

Uses GitHub Personal Access Tokens (classic or fine-grained) for authentication:

```cpp
auto& manager = GitHubModelIntegration::GitHubModelManager::getInstance();
manager.setToken("ghp_your_token_here");
manager.setOrganization("your-org");
```

### Required Permissions

Your GitHub token needs:
- `repo` scope - Full repository access (read/write)
- `user:email` scope - Read user email (for commits)

## CLI Usage

### 1. Authenticate with GitHub

```bash
RawrXD-CLI github-auth --token ghp_YOUR_TOKEN_HERE
```

### 2. List Organization Models

```bash
RawrXD-CLI github-list --org your-org-name
```

### 3. Download Model from GitHub

```bash
RawrXD-CLI github-download --repo https://github.com/org/model-repo
```

### 4. Publish Custom Model

```bash
RawrXD-CLI github-publish --model my-custom-model --repo my-model-repo
```

### 5. Sync with GitHub Registry

```bash
RawrXD-CLI github-sync --org your-org-name
```

## API Reference

### GitHubAPIClient

Real HTTP client for GitHub API v3:

```cpp
#include "github_model_integration.h"

using namespace GitHubModelIntegration;

GitHubAPIClient client;

// Authenticate
bool success = client.authenticate("ghp_token");

// List repositories
auto repos = client.listOrgRepos("my-org");
auto repos = client.searchRepos("gguf", "gguf-model");

// Get repository info
auto repo = client.getRepo("owner", "repo-name");

// Create repository
client.createRepo("new-model-repo", "My custom model", false);

// Get file content
std::string content = client.getFileContent("owner", "repo", "model.json");

// Fetch model metadata
auto metadata = client.fetchModelMetadata("https://github.com/org/repo");
```

### GitHubModelManager

High-level model management singleton:

```cpp
auto& manager = GitHubModelManager::getInstance();

// Configure
manager.setToken("ghp_token");
manager.setOrganization("my-org");
manager.setCacheDirectory("./github_models");

// Discover models
auto models = manager.discoverModels("gguf-model");  // By topic
auto models = manager.listOrganizationModels();      // From org

// Download model
bool success = manager.downloadModel(
    "https://github.com/org/model-repo",
    "./models/model.gguf"
);

// Publish model
GitHubModelMetadata metadata;
metadata.modelName = "my-model";
metadata.version = "1.0.0";
metadata.description = "Custom model built with RawrXD";
metadata.format = "gguf";
metadata.architecture = "transformer";
metadata.tags = {"custom", "gguf", "rawrxd"};

bool success = manager.publishModel(
    "./custom_models/my-model.gguf",
    "my-model-repo",
    metadata
);

// Clone repository
manager.cloneRepo("https://github.com/org/repo", "./repos/local");

// List cached models
auto cached = manager.listCachedModels();
```

### AutoLoaderGitHubBridge

Integration layer for AutoModelLoader:

```cpp
// Initialize
AutoLoaderGitHubBridge::initialize("ghp_token", "my-org");

// Register GitHub model
AutoLoaderGitHubBridge::registerGitHubModel(
    "https://github.com/org/model-repo"
);

// List available models
auto models = AutoLoaderGitHubBridge::listAvailableGitHubModels();

// Sync with GitHub
AutoLoaderGitHubBridge::syncWithGitHub();

// Publish custom model
AutoLoaderGitHubBridge::publishCustomModel(
    "my-custom-model",
    "my-model-repo"
);

// Get model info
auto info = AutoLoaderGitHubBridge::getModelInfo("model-name");
```

### AutoModelLoader Integration

Built-in GitHub support in AutoModelLoader:

```cpp
#include "auto_model_loader.h"

using namespace AutoModelLoader;

AutoModelLoader loader(config);

// Authenticate
loader.authenticateGitHub("ghp_token");

// Check authentication
if (loader.isGitHubAuthenticated()) {
    // Fetch repositories
    auto repos = loader.fetchGitHubModelRepos("my-org");
    
    // Clone model
    loader.cloneModelFromGitHub(
        "https://github.com/org/repo",
        "./models/local"
    );
    
    // Download model
    loader.downloadGitHubModel("https://github.com/org/repo");
    
    // Push custom model
    loader.pushCustomModelToGitHub(
        "./custom_models/model.gguf",
        "my-model-repo"
    );
    
    // Sync registry
    loader.syncWithGitHubRegistry();
    
    // Get metadata
    std::string json = loader.getGitHubModelMetadata(
        "https://github.com/org/repo"
    );
    
    // List GitHub models
    auto models = loader.listGitHubModels();
}
```

## Configuration

### LoaderConfig Extensions

```cpp
LoaderConfig config;

// Enable custom models
config.enableCustomModels = true;
config.customModelsDirectory = "./custom_models";

// Enable GitHub integration
config.enableGitHubIntegration = true;
config.githubToken = "ghp_your_token";
config.githubOrg = "your-organization";
```

### model_loader_config.json

```json
{
  "enableCustomModels": true,
  "customModelsDirectory": "./custom_models",
  "enableGitHubIntegration": true,
  "githubToken": "",
  "githubOrg": "your-org",
  "autoSync": true,
  "cacheDirectory": "./github_models"
}
```

## Repository Structure

Model repositories should follow this structure:

```
model-repo/
├── README.md           # Model description
├── model.json          # Model metadata (required)
├── model.gguf          # GGUF model file (required)
├── LICENSE             # Model license
└── examples/           # Usage examples (optional)
    └── prompt.txt
```

### model.json Format

```json
{
  "name": "my-custom-model",
  "version": "1.0.0",
  "description": "Custom model for code completion",
  "format": "gguf",
  "architecture": "transformer",
  "parameters": {
    "contextLength": 4096,
    "embeddingDim": 768,
    "numLayers": 12,
    "numHeads": 12,
    "vocabSize": 32000
  },
  "quantization": "Q4_K_M",
  "tags": ["code", "completion", "custom"],
  "author": "Your Name",
  "license": "MIT",
  "trainingData": "Custom dataset description"
}
```

## Workflow Examples

### Publishing a Custom Model

```cpp
// 1. Build custom model
ModelBuilder builder;
builder.digestSources({
    "./src",
    "./docs",
    "./conversations"
});
builder.buildModel(
    "my-model",
    "Custom code completion model",
    {"cpp", "python", "docs"}
);

// 2. Initialize GitHub integration
auto& loader = AutoModelLoader::getInstance();
loader.authenticateGitHub("ghp_token");

// 3. Publish to GitHub
bool success = loader.pushCustomModelToGitHub(
    "./custom_models/my-model.gguf",
    "my-model-repo"
);

if (success) {
    std::cout << "Model published successfully!" << std::endl;
}
```

### Discovering and Downloading Models

```cpp
auto& manager = GitHubModelManager::getInstance();
manager.setToken("ghp_token");
manager.setOrganization("awesome-models");

// Discover all GGUF models with topic "gguf-model"
auto models = manager.discoverModels("gguf-model");

std::cout << "Found " << models.size() << " models:" << std::endl;

for (const auto& model : models) {
    std::cout << "  - " << model.modelName << std::endl;
    std::cout << "    " << model.description << std::endl;
    std::cout << "    Size: " << (model.sizeBytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "    Repo: " << model.repoUrl << std::endl;
    std::cout << std::endl;
    
    // Download interesting models
    if (model.tags.find("code") != model.tags.end()) {
        std::string localPath = "./models/" + model.modelName + ".gguf";
        manager.downloadModel(model.repoUrl, localPath);
    }
}
```

### Syncing Custom Models

```cpp
// Build multiple custom models
std::vector<std::string> models = {
    "cpp-expert",
    "python-guru",
    "docs-writer"
};

// Publish all to GitHub
for (const auto& modelName : models) {
    std::string modelPath = "./custom_models/" + modelName + ".gguf";
    loader.pushCustomModelToGitHub(modelPath, modelName + "-model");
}

// Later: sync from another machine
loader.authenticateGitHub("ghp_token");
loader.syncWithGitHubRegistry();  // Downloads all new models
```

## Security Considerations

### Token Storage

**NEVER** commit GitHub tokens to source control. Use:

1. **Environment Variables**: `GITHUB_TOKEN` env var
2. **Secure Credential Storage**: Windows Credential Manager
3. **Config File (gitignored)**: `~/.rawrxd/github.conf`

Example:

```cpp
// Read from environment
const char* token = std::getenv("GITHUB_TOKEN");
if (token) {
    loader.authenticateGitHub(token);
}
```

### Token Permissions

Use **fine-grained tokens** with minimal permissions:

- Repository access: Only to model repositories
- Scopes: `repo` (read/write), `user:email` (read)
- Expiration: Set reasonable expiration (90 days max)

### Private Models

For private models, ensure:

```cpp
// Create private repository
client.createRepo("private-model", "Confidential model", true);  // true = private

// Only accessible with authenticated token
auto repos = manager.listOrganizationModels();  // Includes private repos
```

## Troubleshooting

### Authentication Failures

**Problem**: `Authentication failed: 401 Unauthorized`

**Solutions**:
1. Check token is valid: `curl -H "Authorization: token YOUR_TOKEN" https://api.github.com/user`
2. Verify token has `repo` scope
3. Ensure token hasn't expired

### Clone Failures

**Problem**: `Failed to clone repository`

**Solutions**:
1. Check git is installed: `git --version`
2. Verify repository URL is correct
3. Check network connectivity
4. For private repos, ensure token has access

### Rate Limiting

**Problem**: `API rate limit exceeded`

**Solutions**:
1. Authenticated requests: 5000 requests/hour
2. Unauthenticated: 60 requests/hour
3. Wait for rate limit reset: Check `X-RateLimit-Reset` header
4. Use conditional requests (ETags) to save quota

### Large Model Downloads

**Problem**: Download times out or fails

**Solutions**:
1. Use GitHub Releases for large files (>100MB)
2. Enable Git LFS for model files
3. Use resume-capable HTTP client (future enhancement)
4. Split large models into chunks

## Future Enhancements

### Planned Features

1. **GitHub Actions Integration**: Automatic model building/publishing
2. **Model Versioning**: Semantic versioning support
3. **Release Management**: Create releases with multiple quantizations
4. **Git LFS Support**: Handle large model files efficiently
5. **Model Registry**: Central registry of available models
6. **Pull Request Workflow**: Submit model updates via PR
7. **CI/CD Pipeline**: Automated testing and validation
8. **Model Marketplace**: Browse and discover models in GUI
9. **Collaborative Training**: Distributed model training workflows
10. **Model Analytics**: Usage tracking and performance metrics

## References

- [GitHub REST API v3](https://docs.github.com/en/rest)
- [GGUF Format Specification](https://github.com/ggerganov/ggml/blob/master/docs/gguf.md)
- [Custom Model Builder Guide](CUSTOM_MODEL_BUILDER_GUIDE.md)
- [AutoModelLoader Documentation](AUTO_MODEL_LOADER_GUIDE.md)

## Support

For issues or questions:
- GitHub Issues: https://github.com/your-org/rawrxd/issues
- Documentation: https://github.com/your-org/rawrxd/wiki
- Discord: https://discord.gg/rawrxd
