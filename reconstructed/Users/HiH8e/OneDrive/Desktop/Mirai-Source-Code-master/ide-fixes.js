// IDE JavaScript Fixes
// This file contains implementations for missing functions that are causing errors

// File system navigation functions
function navigateToCustomPath() {
  try {
    const path = prompt("Enter path to navigate to:");
    if (path) {
      // Implementation depends on your specific file system interface
      console.log("Navigating to:", path);
      // Add your custom navigation logic here
    }
  } catch (error) {
    console.error("Error navigating to custom path:", error);
  }
}

function browseDrives() {
  try {
    // Implementation for browsing available drives
    console.log("Browsing available drives...");
    // Add your drive browsing logic here
    // This might involve calling a backend API or file system interface
  } catch (error) {
    console.error("Error browsing drives:", error);
  }
}

// AI Panel functions
function toggleFloatAIPanel() {
  try {
    const aiPanel = document.querySelector('.ai-panel, #ai-panel, [class*="ai-panel"]');
    if (aiPanel) {
      aiPanel.style.display = aiPanel.style.display === 'none' ? 'block' : 'none';
    } else {
      console.warn("AI panel element not found");
    }
  } catch (error) {
    console.error("Error toggling AI panel:", error);
  }
}

function clearAIChat() {
  try {
    const chatContainer = document.querySelector('.ai-chat, #ai-chat, [class*="chat"]');
    if (chatContainer) {
      chatContainer.innerHTML = '';
    } else {
      console.warn("AI chat container not found");
    }
  } catch (error) {
    console.error("Error clearing AI chat:", error);
  }
}

// Tuning panel function
function toggleTuningPanel() {
  try {
    const tuningPanel = document.querySelector('.tuning-panel, #tuning-panel, [class*="tuning"]');
    if (tuningPanel) {
      tuningPanel.style.display = tuningPanel.style.display === 'none' ? 'block' : 'none';
    } else {
      console.warn("Tuning panel element not found");
    }
  } catch (error) {
    console.error("Error toggling tuning panel:", error);
  }
}

// Code execution function
function runCode() {
  try {
    const codeEditor = document.querySelector('textarea, .code-editor, #code-editor');
    if (codeEditor) {
      const code = codeEditor.value || codeEditor.textContent;
      console.log("Running code:", code);
      // Add your code execution logic here
      // This might involve sending code to a backend service
    } else {
      console.warn("Code editor not found");
    }
  } catch (error) {
    console.error("Error running code:", error);
  }
}

// AI agent function
function askAgent() {
  try {
    const question = prompt("What would you like to ask the AI agent?");
    if (question) {
      console.log("Asking agent:", question);
      // Add your AI agent communication logic here
    }
  } catch (error) {
    console.error("Error asking agent:", error);
  }
}

// Context menu function
function contextMenuAction(action) {
  try {
    console.log("Context menu action:", action);
    // Add your context menu handling logic here
    switch (action) {
      case 'copy':
        document.execCommand('copy');
        break;
      case 'paste':
        document.execCommand('paste');
        break;
      case 'cut':
        document.execCommand('cut');
        break;
      default:
        console.log("Unknown context menu action:", action);
    }
  } catch (error) {
    console.error("Error executing context menu action:", error);
  }
}

// Queue input handler
function handleQueueInput(event) {
  try {
    if (event.key === 'Enter') {
      const input = event.target;
      const value = input.value.trim();
      if (value) {
        console.log("Queue input:", value);
        // Add your queue processing logic here
        input.value = ''; // Clear input after processing
      }
    }
  } catch (error) {
    console.error("Error handling queue input:", error);
  }
}

// AMD Loader fix - prevent redeclaration
if (typeof _amdLoaderGlobal === 'undefined') {
  window._amdLoaderGlobal = window._amdLoaderGlobal || {};
}

// Ollama GGUF Model Management - Beast System Optimized
class OllamaModelManager {
  constructor() {
    // Beast system detected: AMD 7800X3D + 64GB RAM - removing size limits!
    this.maxModelSize = Infinity; // No limits on beast hardware
    this.ollamaUrl = 'http://localhost:11434'; // Default Ollama URL
    this.loadedModels = new Map();
    this.beastMode = true; // Enable maximum performance mode
  }

  async checkOllamaStatus() {
    try {
      const response = await fetch(`${this.ollamaUrl}/api/version`);
      return response.ok;
    } catch (error) {
      console.warn("Ollama not running or not accessible:", error);
      return false;
    }
  }

  async listAvailableModels() {
    try {
      const response = await fetch(`${this.ollamaUrl}/api/tags`);
      if (!response.ok) throw new Error('Failed to fetch models');

      const data = await response.json();
      return data.models || [];
    } catch (error) {
      console.error("Error listing Ollama models:", error);
      return [];
    }
  }

  validateModelSize(modelSize) {
    if (this.beastMode) {
      // Beast system: No size limits with 64GB RAM
      console.log(`Beast Mode: Loading model of size ${(modelSize / 1024 / 1024 / 1024).toFixed(2)}GB`);
      return true;
    }

    if (modelSize > this.maxModelSize) {
      throw new Error(`Model size ${(modelSize / 1024 / 1024 / 1024).toFixed(2)}GB exceeds limit`);
    }
    return true;
  }

  // Beast mode: Get largest, most capable models first
  async getBeastModeModels() {
    try {
      const models = await this.listAvailableModels();

      // Prioritize by capability and size for beast hardware
      const prioritized = models.sort((a, b) => {
        // Beast models first
        if (a.name.includes('beast') && !b.name.includes('beast')) return -1;
        if (!a.name.includes('beast') && b.name.includes('beast')) return 1;

        // BigDaddyG models next
        if (a.name.includes('bigdaddyg') && !b.name.includes('bigdaddyg')) return -1;
        if (!a.name.includes('bigdaddyg') && b.name.includes('bigdaddyg')) return 1;

        // Larger models preferred on beast system
        return b.size - a.size;
      });

      return prioritized;
    } catch (error) {
      console.error("Error getting beast mode models:", error);
      return [];
    }
  }

  async loadModel(modelName) {
    try {
      console.log(`Loading Ollama model: ${modelName}`);

      // Check if Ollama is running
      if (!(await this.checkOllamaStatus())) {
        throw new Error("Ollama is not running. Please start Ollama first.");
      }

      // Get model info to check size
      const models = await this.listAvailableModels();
      const targetModel = models.find(m => m.name === modelName);

      if (!targetModel) {
        throw new Error(`Model ${modelName} not found. Available models: ${models.map(m => m.name).join(', ')}`);
      }

      // Validate size
      this.validateModelSize(targetModel.size || 0);

      // Load the model
      const response = await fetch(`${this.ollamaUrl}/api/generate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          model: modelName,
          prompt: '', // Empty prompt to just load the model
          stream: false
        })
      });

      if (!response.ok) {
        throw new Error(`Failed to load model: ${response.statusText}`);
      }

      this.loadedModels.set(modelName, {
        name: modelName,
        size: targetModel.size,
        loadedAt: new Date()
      });

      console.log(`Model ${modelName} loaded successfully`);
      return { success: true, model: modelName };

    } catch (error) {
      console.error("Error loading Ollama model:", error);
      return { success: false, error: error.message };
    }
  }

  async generateResponse(modelName, prompt, options = {}) {
    try {
      if (!this.loadedModels.has(modelName)) {
        const loadResult = await this.loadModel(modelName);
        if (!loadResult.success) {
          throw new Error(`Failed to load model: ${loadResult.error}`);
        }
      }

      const response = await fetch(`${this.ollamaUrl}/api/generate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          model: modelName,
          prompt: prompt,
          stream: options.stream || false,
          options: {
            temperature: options.temperature || 0.7,
            max_tokens: options.maxTokens || 2048,
            ...options
          }
        })
      });

      if (!response.ok) {
        throw new Error(`Generation failed: ${response.statusText}`);
      }

      const data = await response.json();
      return {
        success: true,
        response: data.response,
        model: modelName,
        done: data.done
      };

    } catch (error) {
      console.error("Error generating response:", error);
      return { success: false, error: error.message };
    }
  }

  async unloadModel(modelName) {
    try {
      // Ollama doesn't have explicit unload, but we can remove from our tracking
      this.loadedModels.delete(modelName);
      console.log(`Model ${modelName} unloaded from tracking`);
      return { success: true };
    } catch (error) {
      console.error("Error unloading model:", error);
      return { success: false, error: error.message };
    }
  }

  getLoadedModels() {
    return Array.from(this.loadedModels.values());
  }
}

// Global Ollama instance for HyperIDE
const ollamaManager = new OllamaModelManager();

// GGUF Model loading error handler (legacy compatibility)
function handleGGUFModelError() {
  console.warn("GGUF model loading failed. Attempting Ollama fallback...");
  console.warn("1. Check if Ollama is running");
  console.warn("2. Verify model exists and is under 4.7GB");
  console.warn("3. Check network connectivity to Ollama");

  return {
    error: true,
    message: "Model loading failed, try using Ollama models instead",
    suggestion: "Use ollamaManager.loadModel('model-name') for better compatibility"
  };
}

// ========================================
// HYPERIDE INTEGRATION FUNCTIONS
// ========================================

// AI Agent functions for HyperIDE
function askAgent() {
  try {
    if (window.sendMessage && typeof window.sendMessage === 'function') {
      // Use existing HyperIDE chat system
      const input = document.querySelector('#ai-input, .ai-input');
      if (input) {
        input.focus();
        const message = prompt("Ask the AI agent:");
        if (message) {
          input.value = message;
          window.sendMessage();
        }
      }
    } else {
      // Fallback to Ollama
      const question = prompt("What would you like to ask the AI agent?");
      if (question) {
        askOllamaAgent(question);
      }
    }
  } catch (error) {
    console.error("Error asking agent:", error);
  }
}

// Enhanced GGUF/Ollama integration for HyperIDE with BigDaddyG optimization
async function askOllamaAgent(question) {
  try {
    console.log("Asking Ollama agent:", question);

    // Beast system: Get best available models (no size restrictions) 
    const models = ollamaManager.beastMode
      ? await ollamaManager.getBeastModeModels()
      : await ollamaManager.listAvailableModels();

    let modelName = null;

    // Prefer beast models and BigDaddyG variants
    const preferredModels = ['bigdaddyg-beast', 'bigdaddyg-turbo', 'bigdaddyg:latest'];

    // Try preferred models first
    for (const preferred of preferredModels) {
      if (models.some(m => m.name === preferred)) {
        modelName = preferred;
        break;
      }
    }

    // Fallback to best available model (beast mode ignores size limits)
    if (!modelName) {
      if (ollamaManager.beastMode) {
        // Use the largest/best model available
        modelName = models[0]?.name;
      } else {
        const suitableModels = models.filter(model =>
          model.size <= ollamaManager.maxModelSize
        );

        if (suitableModels.length === 0) {
          throw new Error("No suitable models found");
        }
        modelName = suitableModels[0].name;
      }
    }

    if (!modelName) {
      throw new Error("No models available");
    }

    console.log(`Beast Mode: Using ${modelName}`);

    // Generate response with beast mode optimizations
    const result = await ollamaManager.generateResponse(modelName, question, {
      temperature: 0.7,
      maxTokens: 4096,  // Higher token limit for beast system  
      num_ctx: 8192,    // Large context window
      num_thread: 16    // Use all 16 threads
    });

    if (result.success) {
      // Display in HyperIDE if available, otherwise console
      if (window.addMessage && typeof window.addMessage === 'function') {
        window.addMessage('assistant', result.response, modelName);
      } else {
        console.log(`AI Response (${modelName}):`, result.response);
        alert(`AI Response:\n\n${result.response}`);
      }
    } else {
      throw new Error(result.error);
    }

  } catch (error) {
    console.error("Ollama agent error:", error);
    alert(`AI Agent Error: ${error.message}`);
  }
}

// Enhanced model management for HyperIDE
async function manageGGUFModels() {
  try {
    const models = await ollamaManager.listAvailableModels();
    const loadedModels = ollamaManager.getLoadedModels();

    let modelInfo = "🤖 Available Ollama Models (≤4.7GB):\n\n";

    models.forEach(model => {
      const sizeMB = (model.size / 1024 / 1024).toFixed(1);
      const sizeGB = (model.size / 1024 / 1024 / 1024).toFixed(2);
      const isLoaded = loadedModels.some(loaded => loaded.name === model.name);
      const status = isLoaded ? "✅ LOADED" : "⭕ Available";

      if (model.size <= ollamaManager.maxModelSize) {
        modelInfo += `${status} ${model.name}\n`;
        modelInfo += `   Size: ${sizeMB}MB (${sizeGB}GB)\n\n`;
      }
    });

    console.log(modelInfo);

    // Show in UI if possible
    if (typeof window.showToast === 'function') {
      window.showToast('Model info logged to console', 'info');
    }

    return models.filter(model => model.size <= ollamaManager.maxModelSize);

  } catch (error) {
    console.error("Error managing models:", error);
    return [];
  }
}

// Integration with HyperIDE's model system
function integrateWithHyperIDE() {
  try {
    // Add Ollama models to HyperIDE dropdown if it exists
    if (window.populateOllamaModels && typeof window.populateOllamaModels === 'function') {
      window.populateOllamaModels();
    }

    // Integrate with existing model management
    if (window.Models && typeof window.Models === 'object') {
      window.Models.ollama = ollamaManager;
      console.log("✅ Ollama manager integrated with HyperIDE Models");
    }

    // Add to global scope for easy access
    window.ollamaManager = ollamaManager;
    window.manageGGUFModels = manageGGUFModels;
    window.askOllamaAgent = askOllamaAgent;

    console.log("✅ HyperIDE integration complete");

  } catch (error) {
    console.error("HyperIDE integration error:", error);
  }
}

// Auto-integrate when DOM is ready
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', integrateWithHyperIDE);
} else {
  integrateWithHyperIDE();
}

// Initialize error handlers
window.addEventListener('error', function (event) {
  console.error('JavaScript Error:', event.error);
});

window.addEventListener('unhandledrejection', function (event) {
  console.error('Unhandled Promise Rejection:', event.reason);
});

console.log("IDE fixes with Ollama integration loaded successfully");