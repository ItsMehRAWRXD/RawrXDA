/**
 * BigDaddyG IDE - Image Generation Engine
 * Like Microsoft Copilot's DALL-E integration
 * Generates images from text prompts
 */

// ============================================================================
// IMAGE GENERATION CONFIGURATION
// ============================================================================

const ImageGenConfig = {
    enabled: true,
    
    // Supported engines
    engines: {
        'stable-diffusion': {
            name: 'Stable Diffusion',
            type: 'local',
            endpoint: 'http://localhost:7860',
            models: ['sd-v1.5', 'sd-v2.1', 'sdxl'],
            maxSize: 1024,
            free: true
        },
        'dall-e': {
            name: 'DALL-E',
            type: 'api',
            endpoint: 'https://api.openai.com/v1/images/generations',
            models: ['dall-e-2', 'dall-e-3'],
            maxSize: 1024,
            requiresKey: true
        },
        'midjourney': {
            name: 'Midjourney',
            type: 'api',
            endpoint: 'https://api.midjourney.com/v1/imagine',
            models: ['v5', 'v6', 'niji'],
            maxSize: 2048,
            requiresKey: true
        },
        'local-sd': {
            name: 'Local Stable Diffusion',
            type: 'embedded',
            models: ['sd-1.5-quantized'],
            maxSize: 512,
            free: true,
            offline: true
        }
    },
    
    currentEngine: 'stable-diffusion',
    
    // Generation settings
    defaultSettings: {
        width: 512,
        height: 512,
        steps: 20,
        guidance: 7.5,
        sampler: 'DPM++ 2M Karras',
        seed: -1, // Random
        negativePrompt: 'ugly, blurry, low quality'
    },
    
    // History
    maxHistory: 100,
    saveToHistory: true
};

// ============================================================================
// IMAGE GENERATION ENGINE
// ============================================================================

class ImageGenerationEngine {
    constructor() {
        this.history = [];
        this.currentGeneration = null;
        this.apiKeys = this.loadAPIKeys();
        
        console.log('[ImageGen] 🎨 Image generation engine initialized');
    }
    
    /**
     * Generate image from text prompt
     */
    async generateImage(prompt, settings = {}) {
        const engine = ImageGenConfig.engines[ImageGenConfig.currentEngine];
        
        if (!engine) {
            throw new Error(`Invalid engine: ${ImageGenConfig.currentEngine}`);
        }
        
        console.log(`[ImageGen] 🎨 Generating image with ${engine.name}...`);
        console.log(`[ImageGen] 💬 Prompt: "${prompt}"`);
        
        // Merge settings with defaults
        const finalSettings = {
            ...ImageGenConfig.defaultSettings,
            ...settings
        };
        
        this.currentGeneration = {
            prompt,
            engine: ImageGenConfig.currentEngine,
            settings: finalSettings,
            startTime: Date.now(),
            status: 'generating'
        };
        
        try {
            let result;
            
            switch (engine.type) {
                case 'local':
                    result = await this.generateWithStableDiffusion(prompt, finalSettings);
                    break;
                case 'api':
                    result = await this.generateWithAPI(engine, prompt, finalSettings);
                    break;
                case 'embedded':
                    result = await this.generateWithEmbedded(prompt, finalSettings);
                    break;
                default:
                    throw new Error(`Unknown engine type: ${engine.type}`);
            }
            
            this.currentGeneration.status = 'complete';
            this.currentGeneration.endTime = Date.now();
            this.currentGeneration.duration = this.currentGeneration.endTime - this.currentGeneration.startTime;
            this.currentGeneration.imageUrl = result.imageUrl;
            this.currentGeneration.seed = result.seed;
            
            // Add to history
            if (ImageGenConfig.saveToHistory) {
                this.addToHistory(this.currentGeneration);
            }
            
            console.log(`[ImageGen] ✅ Image generated in ${this.currentGeneration.duration}ms`);
            
            return this.currentGeneration;
            
        } catch (error) {
            this.currentGeneration.status = 'error';
            this.currentGeneration.error = error.message;
            
            console.error('[ImageGen] ❌ Generation failed:', error);
            throw error;
        }
    }
    
    /**
     * Generate with Stable Diffusion (local)
     */
    async generateWithStableDiffusion(prompt, settings) {
        const engine = ImageGenConfig.engines['stable-diffusion'];
        
        const response = await fetch(`${engine.endpoint}/sdapi/v1/txt2img`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                prompt: prompt,
                negative_prompt: settings.negativePrompt,
                width: settings.width,
                height: settings.height,
                steps: settings.steps,
                cfg_scale: settings.guidance,
                sampler_name: settings.sampler,
                seed: settings.seed
            })
        });
        
        if (!response.ok) {
            throw new Error(`Stable Diffusion API error: ${response.statusText}`);
        }
        
        const data = await response.json();
        
        // Convert base64 to data URL
        const imageUrl = `data:image/png;base64,${data.images[0]}`;
        
        return {
            imageUrl,
            seed: data.info.seed || settings.seed
        };
    }
    
    /**
     * Generate with API (DALL-E, Midjourney, etc.)
     */
    async generateWithAPI(engine, prompt, settings) {
        const apiKey = this.apiKeys[ImageGenConfig.currentEngine];
        
        if (!apiKey && engine.requiresKey) {
            throw new Error(`API key required for ${engine.name}`);
        }
        
        if (ImageGenConfig.currentEngine === 'dall-e') {
            return await this.generateWithDALLE(prompt, settings, apiKey);
        } else if (ImageGenConfig.currentEngine === 'midjourney') {
            return await this.generateWithMidjourney(prompt, settings, apiKey);
        }
        
        throw new Error(`API generation not implemented for ${engine.name}`);
    }
    
    /**
     * Generate with DALL-E
     */
    async generateWithDALLE(prompt, settings, apiKey) {
        const response = await fetch('https://api.openai.com/v1/images/generations', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${apiKey}`
            },
            body: JSON.stringify({
                model: 'dall-e-3',
                prompt: prompt,
                n: 1,
                size: `${settings.width}x${settings.height}`,
                quality: 'hd'
            })
        });
        
        if (!response.ok) {
            throw new Error(`DALL-E API error: ${response.statusText}`);
        }
        
        const data = await response.json();
        
        return {
            imageUrl: data.data[0].url,
            seed: Math.random() * 1000000 // DALL-E doesn't return seeds
        };
    }
    
    /**
     * Generate with embedded model (offline)
     */
    async generateWithEmbedded(prompt, settings) {
        // This would use a quantized Stable Diffusion model
        // Running entirely in-browser using WASM + WebGPU
        
        console.log('[ImageGen] 🔧 Using embedded model (offline)');
        
        // Placeholder for now - would integrate with actual model
        return new Promise((resolve) => {
            setTimeout(() => {
                resolve({
                    imageUrl: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==',
                    seed: settings.seed
                });
            }, 3000);
        });
    }
    
    /**
     * Add to history
     */
    addToHistory(generation) {
        this.history.unshift(generation);
        
        // Keep only max history
        if (this.history.length > ImageGenConfig.maxHistory) {
            this.history = this.history.slice(0, ImageGenConfig.maxHistory);
        }
    }
    
    /**
     * Load API keys from storage
     */
    loadAPIKeys() {
        try {
            const stored = localStorage.getItem('imageGenAPIKeys');
            return stored ? JSON.parse(stored) : {};
        } catch (error) {
            return {};
        }
    }
    
    /**
     * Save API key
     */
    saveAPIKey(engine, key) {
        this.apiKeys[engine] = key;
        localStorage.setItem('imageGenAPIKeys', JSON.stringify(this.apiKeys));
        console.log(`[ImageGen] 🔑 API key saved for ${engine}`);
    }
    
    /**
     * Get generation history
     */
    getHistory() {
        return this.history;
    }
    
    /**
     * Clear history
     */
    clearHistory() {
        this.history = [];
        console.log('[ImageGen] 🗑️ History cleared');
    }
}

// ============================================================================
// UI FUNCTIONS
// ============================================================================

function showImageGenerationDialog() {
    const dialog = `
        <div id="image-gen-dialog" class="image-gen-dialog">
            <div class="dialog-content">
                <div class="dialog-header">
                    <h2>🎨 Generate Image</h2>
                    <button onclick="closeImageGenerationDialog()">✕</button>
                </div>
                
                <div class="dialog-body">
                    <div class="form-group">
                        <label>Prompt:</label>
                        <textarea id="image-prompt" placeholder="Describe the image you want to generate..." rows="4"></textarea>
                    </div>
                    
                    <div class="form-group">
                        <label>Engine:</label>
                        <select id="image-engine" onchange="updateImageEngineSettings()">
                            ${Object.entries(ImageGenConfig.engines).map(([key, engine]) => `
                                <option value="${key}" ${key === ImageGenConfig.currentEngine ? 'selected' : ''}>
                                    ${engine.name} ${engine.offline ? '(Offline)' : ''} ${engine.free ? '(Free)' : ''}
                                </option>
                            `).join('')}
                        </select>
                    </div>
                    
                    <div class="form-row">
                        <div class="form-group">
                            <label>Width:</label>
                            <input type="number" id="image-width" value="512" min="256" max="2048" step="64">
                        </div>
                        <div class="form-group">
                            <label>Height:</label>
                            <input type="number" id="image-height" value="512" min="256" max="2048" step="64">
                        </div>
                    </div>
                    
                    <div class="form-group">
                        <label>Negative Prompt:</label>
                        <input type="text" id="image-negative" value="ugly, blurry, low quality">
                    </div>
                    
                    <div class="form-row">
                        <div class="form-group">
                            <label>Steps:</label>
                            <input type="range" id="image-steps" min="10" max="50" value="20">
                            <span id="steps-value">20</span>
                        </div>
                        <div class="form-group">
                            <label>Guidance:</label>
                            <input type="range" id="image-guidance" min="1" max="20" step="0.5" value="7.5">
                            <span id="guidance-value">7.5</span>
                        </div>
                    </div>
                    
                    <div id="generation-progress" class="generation-progress" style="display: none;">
                        <div class="progress-bar">
                            <div class="progress-fill"></div>
                        </div>
                        <p>Generating image...</p>
                    </div>
                    
                    <div id="generated-image" class="generated-image" style="display: none;">
                        <img id="result-image" src="" alt="Generated image">
                        <div class="image-actions">
                            <button onclick="downloadGeneratedImage()">💾 Download</button>
                            <button onclick="insertImageToEditor()">➕ Insert to Editor</button>
                            <button onclick="copyImageToClipboard()">📋 Copy</button>
                        </div>
                    </div>
                </div>
                
                <div class="dialog-footer">
                    <button onclick="generateImageFromDialog()" class="primary-btn">🎨 Generate</button>
                    <button onclick="closeImageGenerationDialog()">Cancel</button>
                </div>
            </div>
        </div>
    `;
    
    document.body.insertAdjacentHTML('beforeend', dialog);
}

function closeImageGenerationDialog() {
    const dialog = document.getElementById('image-gen-dialog');
    if (dialog) dialog.remove();
}

async function generateImageFromDialog() {
    const prompt = document.getElementById('image-prompt').value;
    if (!prompt.trim()) {
        alert('Please enter a prompt');
        return;
    }
    
    const settings = {
        width: parseInt(document.getElementById('image-width').value),
        height: parseInt(document.getElementById('image-height').value),
        steps: parseInt(document.getElementById('image-steps').value),
        guidance: parseFloat(document.getElementById('image-guidance').value),
        negativePrompt: document.getElementById('image-negative').value
    };
    
    // Show progress
    document.getElementById('generation-progress').style.display = 'block';
    document.getElementById('generated-image').style.display = 'none';
    
    try {
        const result = await window.imageGenEngine.generateImage(prompt, settings);
        
        // Hide progress
        document.getElementById('generation-progress').style.display = 'none';
        
        // Show result
        const resultImg = document.getElementById('result-image');
        resultImg.src = result.imageUrl;
        document.getElementById('generated-image').style.display = 'block';
        
        // Store for later use
        window.lastGeneratedImage = result;
        
    } catch (error) {
        document.getElementById('generation-progress').style.display = 'none';
        alert(`Image generation failed: ${error.message}`);
    }
}

function downloadGeneratedImage() {
    if (!window.lastGeneratedImage) return;
    
    const a = document.createElement('a');
    a.href = window.lastGeneratedImage.imageUrl;
    a.download = `generated-${Date.now()}.png`;
    a.click();
}

function insertImageToEditor() {
    if (!window.lastGeneratedImage || !window.editor) return;
    
    const markdown = `![Generated Image](${window.lastGeneratedImage.imageUrl})`;
    const position = window.editor.getPosition();
    window.editor.executeEdits('image-insert', [{
        range: new monaco.Range(
            position.lineNumber,
            position.column,
            position.lineNumber,
            position.column
        ),
        text: markdown
    }]);
    
    closeImageGenerationDialog();
}

function copyImageToClipboard() {
    if (!window.lastGeneratedImage) return;
    
    // Copy image URL to clipboard
    navigator.clipboard.writeText(window.lastGeneratedImage.imageUrl);
    alert('Image URL copied to clipboard!');
}

// ============================================================================
// INITIALIZATION
// ============================================================================

// Initialize on load
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        ImageGenerationEngine,
        ImageGenConfig,
        showImageGenerationDialog
    };
}

console.log('[ImageGen] 🎨 Image generation module loaded');

