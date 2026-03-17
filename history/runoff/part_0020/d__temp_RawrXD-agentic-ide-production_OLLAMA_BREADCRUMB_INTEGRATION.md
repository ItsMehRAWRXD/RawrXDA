# Ollama Model Integration in Agent Chat Breadcrumb

## Overview
The AgentChatBreadcrumb widget now automatically fetches and displays available Ollama models in the model dropdown when the breadcrumb is initialized.

## Features

### 1. **Dynamic Ollama Model Discovery**
- Automatically queries the Ollama API (`http://localhost:11434/api/tags`) on initialization
- Discovers all running Ollama models by name
- Displays models with Ollama icon (🦙) and model name format: `🦙 model-name (Ollama)`

### 2. **Automatic Model Population**
The breadcrumb dropdown displays models organized by type:
```
⚡ Auto (Smart Selection) → Suggested: [model-name]
━━━━━━━━━━━━━━━━━━━━━━━━━ (separator)
📁 local-model (Local)
━━━━━━━━━━━━━━━━━━━━━━━━━ (separator)
🦙 llama2 (Ollama)
🦙 mistral (Ollama)
🦙 neural-chat (Ollama)
━━━━━━━━━━━━━━━━━━━━━━━━━ (separator)
☁️ claude-3-5-sonnet (Claude)
☁️ gpt-4-turbo (OpenAI)
... (other cloud models)
```

### 3. **Initialization Usage**

In your main window or chat panel, initialize the breadcrumb like this:

```cpp
// Create the breadcrumb
AgentChatBreadcrumb* breadcrumb = new AgentChatBreadcrumb(this);

// Initialize - this will fetch Ollama models automatically
breadcrumb->initialize();

// Or fetch from a custom Ollama endpoint
breadcrumb->fetchOllamaModels("http://custom-ollama-host:11434");
```

### 4. **API Endpoint**
- **Default Endpoint**: `http://localhost:11434`
- **Custom Endpoint**: Use `fetchOllamaModels(endpoint)` to query a different Ollama instance

### 5. **Model Extraction**
When fetching models, the implementation:
- Queries `/api/tags` endpoint which returns model list
- Extracts base model name (removes tags like `:latest`)
- Registers each model with Ollama type
- Removes duplicates and old models before adding new ones

### 6. **Error Handling**
- 5-second timeout for Ollama API queries
- Gracefully falls back to pre-configured models if Ollama is unavailable
- Logs debug information about fetch success/failure

### 7. **Smart Model Selection**
Once models are loaded, the breadcrumb provides smart selection:
- **Ask Mode**: Prefers smaller, context-aware models
- **Plan Mode**: Selects large models (Claude, GPT) for reasoning
- **Edit Mode**: Picks code-specialized models (Copilot, GPT)
- **Configure Mode**: Uses first available model

### 8. **Displayed Format**
Each Ollama model is displayed as:
```
🦙 model-name (Ollama)
```

Examples:
- `🦙 llama2 (Ollama)`
- `🦙 mistral (Ollama)`
- `🦙 neural-chat (Ollama)`
- `🦙 dolphin-mixtral (Ollama)`

## Example: Complete Integration

```cpp
// In MainWindow or AIChatPanel
void MainWindow::initializeAgentChat()
{
    AgentChatBreadcrumb* breadcrumb = new AgentChatBreadcrumb(this);
    
    // Initialize - automatically fetches Ollama models
    breadcrumb->initialize();
    
    // Connect signals to handle mode/model changes
    connect(breadcrumb, &AgentChatBreadcrumb::agentModeChanged,
            this, [](AgentChatBreadcrumb::AgentMode mode) {
        qDebug() << "Agent mode changed to:" << mode;
    });
    
    connect(breadcrumb, &AgentChatBreadcrumb::modelSelected,
            this, [](const QString& modelName) {
        qDebug() << "Model selected:" << modelName;
    });
    
    // Add to layout
    m_layout->insertWidget(0, breadcrumb);
}
```

## Network Requirements

- Ollama must be running at the configured endpoint
- Network connectivity to Ollama API
- Port 11434 (default) or custom port specified

## Timeout Behavior

- Default timeout: **5 seconds** for Ollama API query
- If timeout occurs, falls back to pre-configured models
- Non-blocking operation with event loop integration

## Implementation Details

### Key Methods

| Method | Purpose |
|--------|---------|
| `initialize()` | Load config + fetch Ollama models + populate dropdown |
| `fetchOllamaModels(endpoint)` | Query Ollama API and register models |
| `registerOllamaModel(name)` | Add single Ollama model to registry |
| `populateModelDropdown()` | Refresh UI dropdown with current models |
| `onOllamaModelsRetrieved()` | Slot called after models fetched |

### Headers Required

The implementation uses:
- `<QNetworkAccessManager>` - for HTTP requests
- `<QNetworkRequest>` - for building API requests
- `<QEventLoop>` - for synchronous API queries
- `<QJsonDocument>` - for parsing Ollama JSON response
- `<QTimer>` - for timeout handling

## Debugging

Enable Qt debug output to see fetch logs:

```cpp
// In your application startup
QLoggingCategory::setFilterRules("*.debug=true");
```

Watch the debug output for messages like:
```
Ollama models fetched: ("llama2", "mistral", "neural-chat")
Failed to fetch Ollama models: "Connection refused"
```

## Future Enhancements

- Asynchronous model fetching without blocking UI
- Periodic refresh of Ollama models
- Support for multiple Ollama instances
- Model capability indicators (context window, speed, accuracy)
- Model download/pull integration
