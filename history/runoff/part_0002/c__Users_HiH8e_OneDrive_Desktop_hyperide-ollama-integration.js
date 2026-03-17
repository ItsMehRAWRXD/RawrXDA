// ============================================================================
// HYPERIDE OLLAMA INTEGRATION - 4.7GB MODEL SUPPORT
// Include this in your IDEre2.html file for Ollama support
// ============================================================================

(function () {
  'use strict';

  // Configuration for HyperIDE
  const HYPERIDE_OLLAMA_CONFIG = {
    maxModelSize: 4.7 * 1024 * 1024 * 1024, // 4.7GB in bytes
    ollamaUrl: 'http://localhost:11434',
    orchestraUrl: 'http://localhost:11442', // HyperIDE's Orchestra server
    preferredModels: [
      'llama3.2:3b',
      'phi3:3.8b',
      'mistral:7b',
      'codellama:7b',
      'gemma:2b',
      'qwen2:0.5b'
    ]
  };

  // Enhanced Ollama Manager for HyperIDE
  class HyperIDEOllamaManager extends OllamaModelManager {
    constructor() {
      super();
      this.maxModelSize = HYPERIDE_OLLAMA_CONFIG.maxModelSize;
      this.ollamaUrl = HYPERIDE_OLLAMA_CONFIG.ollamaUrl;
      this.orchestraUrl = HYPERIDE_OLLAMA_CONFIG.orchestraUrl;
      this.hyperIDEIntegrated = false;
    }

    // Override to use Orchestra proxy if available
    async checkOllamaStatus() {
      try {
        // Try Orchestra first (HyperIDE's proxy)
        const orchestraResponse = await fetch(`${this.orchestraUrl}/health`);
        if (orchestraResponse.ok) {
          console.log("✅ Orchestra proxy available");
          this.useOrchestra = true;
          return true;
        }
      } catch (error) {
        console.log("Orchestra not available, trying direct Ollama...");
      }

      // Fallback to direct Ollama
      try {
        const response = await fetch(`${this.ollamaUrl}/api/version`);
        this.useOrchestra = false;
        return response.ok;
      } catch (error) {
        console.warn("Ollama not running:", error);
        return false;
      }
    }

    // Get models with size filtering for HyperIDE
    async getRecommendedModels() {
      try {
        const models = await this.listAvailableModels();
        const filtered = models.filter(model => {
          const sizeOK = model.size <= this.maxModelSize;
          const isPreferred = HYPERIDE_OLLAMA_CONFIG.preferredModels.some(
            preferred => model.name.includes(preferred.split(':')[0])
          );
          return sizeOK && isPreferred;
        });

        // Sort by size (smaller first for better performance)
        return filtered.sort((a, b) => a.size - b.size);
      } catch (error) {
        console.error("Error getting recommended models:", error);
        return [];
      }
    }

    // Enhanced generation with HyperIDE context
    async generateWithContext(modelName, prompt, context = {}) {
      try {
        const systemPrompt = this.buildHyperIDESystemPrompt(context);
        const fullPrompt = `${systemPrompt}\n\nUser: ${prompt}`;

        return await this.generateResponse(modelName, fullPrompt, {
          temperature: context.temperature || 0.7,
          maxTokens: context.maxTokens || 2048,
          stream: context.stream || false
        });
      } catch (error) {
        console.error("Generation with context failed:", error);
        return { success: false, error: error.message };
      }
    }

    buildHyperIDESystemPrompt(context) {
      const currentFile = context.currentFile || 'Unknown';
      const language = this.detectLanguage(currentFile);
      const timestamp = new Date().toISOString();

      return `You are an AI assistant integrated into HyperIDE, a browser-based development environment.

CONTEXT:
- Current file: ${currentFile}
- Language: ${language}
- Timestamp: ${timestamp}
- IDE Features: File explorer, code editor, terminal, AI chat
- Environment: Browser-based (no Node.js APIs)

CAPABILITIES:
- Code generation and analysis
- Debugging assistance
- File operations guidance
- Best practices recommendations
- Browser-compatible solutions only

CONSTRAINTS:
- Keep responses concise and actionable
- Generate browser-compatible code only
- Consider HyperIDE's existing functionality
- Provide step-by-step instructions when needed

Your role is to assist with development tasks within the HyperIDE environment.`;
    }

    detectLanguage(filename) {
      if (!filename) return 'text';

      const ext = filename.split('.').pop()?.toLowerCase();
      const langMap = {
        'js': 'javascript',
        'ts': 'typescript',
        'py': 'python',
        'html': 'html',
        'css': 'css',
        'json': 'json',
        'md': 'markdown',
        'txt': 'text'
      };

      return langMap[ext] || 'text';
    }

    // Integration with HyperIDE's UI
    async integrateWithHyperIDE() {
      try {
        // Check if HyperIDE functions are available
        const requiredFunctions = [
          'addMessage', 'showToast', 'updateUI'
        ];

        const available = requiredFunctions.filter(fn =>
          window[fn] && typeof window[fn] === 'function'
        );

        console.log(`HyperIDE integration: ${available.length}/${requiredFunctions.length} functions available`);

        // Integrate with model dropdown
        if (window.populateOllamaModels) {
          await this.populateModelDropdown();
        }

        // Add to global Models object
        if (window.Models) {
          window.Models.hyperOllama = this;
        }

        // Override askAgent function
        if (window.askAgent) {
          window.askAgent = this.createEnhancedAskAgent();
        }

        this.hyperIDEIntegrated = true;
        console.log("✅ HyperIDE-Ollama integration complete");

      } catch (error) {
        console.error("HyperIDE integration failed:", error);
      }
    }

    async populateModelDropdown() {
      try {
        const models = await this.getRecommendedModels();
        const dropdown = document.querySelector('#ai-model-select, .model-select');

        if (!dropdown) {
          console.log("Model dropdown not found, skipping population");
          return;
        }

        // Add Ollama models to dropdown
        models.forEach(model => {
          const option = document.createElement('option');
          option.value = `ollama:${model.name}`;
          option.textContent = `Ollama: ${model.name} (${(model.size / 1024 / 1024 / 1024).toFixed(1)}GB)`;
          dropdown.appendChild(option);
        });

        console.log(`Added ${models.length} Ollama models to dropdown`);
      } catch (error) {
        console.error("Error populating model dropdown:", error);
      }
    }

    createEnhancedAskAgent() {
      const self = this;
      return async function () {
        try {
          const input = document.querySelector('#ai-input, .ai-input');
          if (!input) {
            throw new Error("AI input not found");
          }

          const question = input.value.trim();
          if (!question) {
            const promptQuestion = prompt("Ask the AI agent:");
            if (!promptQuestion) return;
            input.value = promptQuestion;
          }

          // Get current context
          const context = {
            currentFile: self.getCurrentFileName(),
            selectedCode: self.getSelectedCode(),
            temperature: 0.7
          };

          // Get recommended model
          const models = await self.getRecommendedModels();
          if (models.length === 0) {
            throw new Error("No suitable Ollama models available");
          }

          const modelName = models[0].name;
          console.log(`Using model: ${modelName}`);

          // Generate response
          const result = await self.generateWithContext(
            modelName,
            input.value,
            context
          );

          if (result.success) {
            // Add to chat if function exists
            if (window.addMessage) {
              window.addMessage('assistant', result.response, `Ollama: ${modelName}`);
            } else {
              console.log("AI Response:", result.response);
            }
            input.value = '';
          } else {
            throw new Error(result.error);
          }

        } catch (error) {
          console.error("Enhanced ask agent error:", error);
          if (window.showToast) {
            window.showToast(`AI Error: ${error.message}`, 'error');
          } else {
            alert(`AI Error: ${error.message}`);
          }
        }
      };
    }

    getCurrentFileName() {
      try {
        // Try to get from various possible elements
        const titleEl = document.querySelector('.editor-tab.active, .file-title, .current-file');
        if (titleEl) {
          return titleEl.textContent || titleEl.getAttribute('data-file') || 'untitled';
        }
        return 'untitled';
      } catch (error) {
        return 'untitled';
      }
    }

    getSelectedCode() {
      try {
        // Try to get selected text from editor
        const selection = window.getSelection();
        if (selection && selection.toString().trim()) {
          return selection.toString();
        }

        // Try Monaco editor if available
        if (window.editor && window.editor.getSelection) {
          const model = window.editor.getModel();
          const selection = window.editor.getSelection();
          if (model && selection) {
            return model.getValueInRange(selection);
          }
        }

        return '';
      } catch (error) {
        return '';
      }
    }
  }

  // Initialize the enhanced manager
  const hyperIDEOllama = new HyperIDEOllamaManager();

  // Global exposure
  window.hyperIDEOllama = hyperIDEOllama;
  window.ollamaManager = hyperIDEOllama; // Backward compatibility

  // Auto-integrate with HyperIDE
  function initializeIntegration() {
    console.log("🚀 Initializing HyperIDE-Ollama integration...");

    // Wait a bit for HyperIDE to load
    setTimeout(async () => {
      await hyperIDEOllama.integrateWithHyperIDE();

      // Show status
      if (window.showToast) {
        const status = await hyperIDEOllama.checkOllamaStatus();
        if (status) {
          window.showToast("✅ Ollama integration ready (4.7GB limit)", 'success');
        } else {
          window.showToast("⚠️ Ollama not detected. Install Ollama for AI features.", 'warning');
        }
      }
    }, 2000);
  }

  // Initialize when ready
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initializeIntegration);
  } else {
    initializeIntegration();
  }

  console.log("🤖 HyperIDE Ollama Integration v1.0 loaded");

})();

// ============================================================================
// USAGE INSTRUCTIONS
// ============================================================================
/*

To integrate this with your IDEre2.html:

1. Add this script to IDEre2.html before the closing </body> tag:
   <script src="hyperide-ollama-integration.js"></script>

2. Or include the content directly in a <script> block

3. Make sure Ollama is running: ollama serve

4. Install a small model: ollama pull llama3.2:3b

5. The integration will:
   - Automatically detect and filter models ≤4.7GB
   - Integrate with HyperIDE's chat system
   - Enhance the askAgent() function
   - Add models to dropdowns
   - Provide browser-compatible responses

6. Access via:
   - window.hyperIDEOllama.generateWithContext(model, prompt, context)
   - Enhanced askAgent() function
   - Model management through hyperIDEOllama

7. Models under 4.7GB that work well:
   - llama3.2:3b (3.2GB) - Great for general tasks
   - phi3:3.8b (3.8GB) - Good for coding
   - gemma:2b (1.4GB) - Fast responses
   - qwen2:0.5b (0.5GB) - Very fast, basic tasks

*/