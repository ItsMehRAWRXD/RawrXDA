// AI-Inference-Engine.js
// Real neural network inference using llama.cpp
// 🤖 Runs GGUF models locally for TRUE AI responses

const path = require('path');
const fs = require('fs');

// Note: node-llama-cpp will be installed via npm
let LlamaModel, LlamaContext, LlamaChatSession;
let llamaAvailable = false;

try {
    const llama = require('node-llama-cpp');
    LlamaModel = llama.LlamaModel;
    LlamaContext = llama.LlamaContext;
    LlamaChatSession = llama.LlamaChatSession;
    llamaAvailable = true;
    console.log('✅ node-llama-cpp loaded - Real AI available!');
} catch (e) {
    console.log('ℹ️ node-llama-cpp not installed - using pattern matching');
    console.log('   Run: npm install node-llama-cpp');
}

class AIInferenceEngine {
    constructor() {
        this.model = null;
        this.context = null;
        this.session = null;
        this.modelPath = null;
        this.isLoaded = false;
        this.modelName = 'None';
    }
    
    /**
     * Scan for GGUF and Ollama blob model files
     */
    async scanForModels() {
        const searchPaths = [
            path.join(__dirname, '../models/blobs'),
            path.join(__dirname, '../models'),
            path.join(process.env.USERPROFILE || process.env.HOME, '.ollama/models/blobs'),
            'D:\\AI-Models',
            'E:\\models'
        ];
        
        const foundModels = [];
        
        for (const searchPath of searchPaths) {
            if (!fs.existsSync(searchPath)) continue;
            
            try {
                const files = fs.readdirSync(searchPath);
                for (const file of files) {
                    const fullPath = path.join(searchPath, file);
                    
                    // Check for .gguf files OR Ollama blobs (sha256-*)
                    const isGguf = file.endsWith('.gguf');
                    const isOllamaBlob = file.startsWith('sha256-') && file.length === 71; // sha256- + 64 hex chars
                    
                    if (isGguf || isOllamaBlob) {
                        const stats = fs.statSync(fullPath);
                        
                        // Only include files > 100MB (actual models, not metadata)
                        if (stats.size > 100 * 1024 * 1024) {
                            foundModels.push({
                                name: isGguf ? file.replace('.gguf', '') : file,
                                path: fullPath,
                                size: stats.size,
                                sizeGB: (stats.size / 1024 / 1024 / 1024).toFixed(2),
                                format: isGguf ? 'gguf' : 'ollama_blob'
                            });
                        }
                    }
                }
            } catch (e) {
                // Skip inaccessible directories
            }
        }
        
        // Sort by size (prefer smaller models for faster loading)
        foundModels.sort((a, b) => a.size - b.size);
        
        return foundModels;
    }
    
    /**
     * Load a GGUF model into memory
     */
    async loadModel(modelPath) {
        if (!llamaAvailable) {
            console.log('❌ Cannot load model - node-llama-cpp not installed');
            return false;
        }
        
        try {
            console.log(`🧠 Loading AI model: ${path.basename(modelPath)}`);
            console.log(`   Size: ${(fs.statSync(modelPath).size / 1024 / 1024 / 1024).toFixed(2)} GB`);
            console.log(`   This may take 10-60 seconds...`);
            
            const startTime = Date.now();
            
            // Load model with optimized settings
            this.model = new LlamaModel({
                modelPath: modelPath,
                gpuLayers: 35, // Use GPU if available
                contextSize: 8192, // Large context window
                batchSize: 512
            });
            
            // Create context
            this.context = new LlamaContext({
                model: this.model,
                contextSize: 8192
            });
            
            // Create chat session
            this.session = new LlamaChatSession({
                context: this.context,
                systemPrompt: `You are BigDaddyG AI, an expert coding assistant trained on 200K lines of Assembly, Security, and Encryption code. You provide:
- Clear, working code examples
- Detailed explanations
- Security best practices
- Performance optimization tips

Respond concisely but thoroughly. Always provide working code when requested.`
            });
            
            const loadTime = ((Date.now() - startTime) / 1000).toFixed(1);
            
            this.modelPath = modelPath;
            this.modelName = path.basename(modelPath);
            this.isLoaded = true;
            
            console.log(`✅ Model loaded in ${loadTime}s`);
            console.log(`🤖 Real AI inference ACTIVE!`);
            
            return true;
        } catch (error) {
            console.error(`❌ Failed to load model: ${error.message}`);
            this.isLoaded = false;
            return false;
        }
    }
    
    /**
     * Auto-load best available model
     */
    async autoLoadModel() {
        if (!llamaAvailable) {
            console.log('ℹ️ Auto-load skipped - node-llama-cpp not installed');
            return false;
        }
        
        console.log('🔍 Scanning for AI models...');
        const models = await this.scanForModels();
        
        if (models.length === 0) {
            console.log('ℹ️ No GGUF models found');
            console.log('   Download models from: https://huggingface.co/models');
            console.log('   Or install Ollama: https://ollama.ai');
            return false;
        }
        
        console.log(`📦 Found ${models.length} model(s):`);
        models.forEach(m => {
            console.log(`   - ${m.name} (${m.sizeGB} GB)`);
        });
        
        // Prefer smaller models for faster loading (phi, mistral, etc.)
        const preferredOrder = ['phi', 'mistral', 'codellama', 'llama'];
        let selectedModel = models[0];
        
        for (const pref of preferredOrder) {
            const found = models.find(m => m.name.toLowerCase().includes(pref));
            if (found) {
                selectedModel = found;
                break;
            }
        }
        
        console.log(`🎯 Auto-loading: ${selectedModel.name}`);
        return await this.loadModel(selectedModel.path);
    }
    
    /**
     * Generate AI response (REAL neural network inference!)
     */
    async generate(prompt, options = {}) {
        if (!this.isLoaded || !this.session) {
            return {
                success: false,
                response: null,
                error: 'No AI model loaded. Install node-llama-cpp and place GGUF models in models/ folder.',
                mode: 'pattern_matching'
            };
        }
        
        try {
            console.log(`🤖 [AI Inference] Generating response (${prompt.length} chars)...`);
            const startTime = Date.now();
            
            // Generate response using real AI
            const response = await this.session.prompt(prompt, {
                maxTokens: options.maxTokens || 2048,
                temperature: options.temperature || 0.7,
                topP: options.topP || 0.9,
                topK: options.topK || 40,
                repeatPenalty: options.repeatPenalty || 1.1
            });
            
            const duration = ((Date.now() - startTime) / 1000).toFixed(1);
            const tokensPerSec = (response.length / duration).toFixed(1);
            
            console.log(`✅ [AI Inference] Generated ${response.length} chars in ${duration}s (${tokensPerSec} tok/s)`);
            
            return {
                success: true,
                response: response,
                model: this.modelName,
                duration: duration,
                tokensPerSecond: tokensPerSec,
                mode: 'neural_network'
            };
        } catch (error) {
            console.error(`❌ [AI Inference] Error: ${error.message}`);
            return {
                success: false,
                response: null,
                error: error.message,
                mode: 'neural_network'
            };
        }
    }
    
    /**
     * Check if real AI is available
     */
    isAvailable() {
        return llamaAvailable && this.isLoaded;
    }
    
    /**
     * Get model info
     */
    getInfo() {
        return {
            available: llamaAvailable,
            loaded: this.isLoaded,
            model: this.modelName,
            path: this.modelPath,
            mode: this.isLoaded ? 'neural_network' : 'pattern_matching'
        };
    }
    
    /**
     * Unload model to free memory
     */
    async unload() {
        if (this.session) {
            this.session = null;
        }
        if (this.context) {
            this.context = null;
        }
        if (this.model) {
            this.model = null;
        }
        this.isLoaded = false;
        console.log('🔓 AI model unloaded');
    }
}

// Export singleton instance
const aiEngine = new AIInferenceEngine();

module.exports = {
    aiEngine,
    AIInferenceEngine
};

