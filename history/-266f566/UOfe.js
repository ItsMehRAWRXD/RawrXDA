/**
 * Model Selector UI - Dropdown for Ollama, Amazon Q, Copilot
 */

class ModelSelector {
  constructor(containerId) {
    this.container = document.getElementById(containerId);
    this.aiManager = null;
    this.selectedModel = null;
  }

  async init(aiManager) {
    this.aiManager = aiManager;
    await this.render();
  }

  async render() {
    const models = this.aiManager.getAvailableModels();
    
    this.container.innerHTML = `
      <select id="model-select" style="padding:8px;border-radius:4px;background:#2d2d2d;color:#fff;border:1px solid #444">
        <optgroup label="Ollama Models">
          ${models.ollama.combined.map(m => `<option value="ollama:${m}">${m}</option>`).join('')}
        </optgroup>
        <optgroup label="Built-in">
          ${models.builtin.bigdaddyg ? `<option value="builtin:${models.builtin.bigdaddyg}">${models.builtin.bigdaddyg}</option>` : ''}
        </optgroup>
        <optgroup label="Extensions">
          <option value="ext:amazonq">Amazon Q</option>
          <option value="ext:copilot">GitHub Copilot</option>
        </optgroup>
      </select>
    `;

    document.getElementById('model-select').addEventListener('change', (e) => {
      this.selectedModel = e.target.value;
      console.log('[ModelSelector] Selected:', this.selectedModel);
    });
  }

  getSelected() {
    const [type, model] = (this.selectedModel || 'ollama:llama3.2').split(':');
    return { type, model };
  }
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = ModelSelector;
} else {
  window.ModelSelector = ModelSelector;
}
