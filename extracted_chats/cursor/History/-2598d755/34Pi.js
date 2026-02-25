// BigDaddyGModelLoader.js
// Loads and manages your actual BigDaddyG:latest model (4.7 GB)
// Integrates with Ollama model storage

const { exec } = require('child_process');
const fs = require('fs');
const path = require('path');
const http = require('http');

class BigDaddyGModelLoader {
    constructor(config = {}) {
        this.config = {
            ollama_endpoint: config.ollama_endpoint || 'http://localhost:11434',
            model_path: config.model_path || 'C:\\Users\\HiH8e\\.ollama\\models',
            default_model: config.default_model || 'bigdaddyg:latest',
            fallback_models: config.fallback_models || [
                'your-custom-model:latest',
                'qwen2.5-coder:7b',
                'deepseek-coder:6.7b'
            ]
        };
        
        this.availableModels = [];
        this.loadedModel = null;
        this.modelInfo = null;
        
        this.initialize();
    }
    
    async initialize() {
        console.log('[BigDaddyG-Loader] 🚀 Initializing model loader...');
        console.log('[BigDaddyG-Loader] 📁 Model path:', this.config.model_path);
        console.log('[BigDaddyG-Loader] 🎯 Default model:', this.config.default_model);
        
        // Discover available models
        await this.discoverModels();
        
        // Check if BigDaddyG is available
        const hasBigDaddyG = this.availableModels.some(m => m.name === 'bigdaddyg:latest');
        
        if (hasBigDaddyG) {
            console.log('[BigDaddyG-Loader] ✅ BigDaddyG:latest found (4.7 GB)');
            await this.loadModel('bigdaddyg:latest');
        } else {
            console.log('[BigDaddyG-Loader] ⚠️ BigDaddyG:latest not found, checking fallbacks...');
            await this.loadFallbackModel();
        }
    }
    
    async discoverModels() {
        return new Promise((resolve) => {
            exec('ollama list', (error, stdout, stderr) => {
                if (error) {
                    console.log('[BigDaddyG-Loader] ⚠️ Ollama not running or not installed');
                    resolve();
                    return;
                }
                
                // Parse ollama list output
                const lines = stdout.split('\n').slice(1); // Skip header
                this.availableModels = lines
                    .filter(line => line.trim())
                    .map(line => {
                        const parts = line.trim().split(/\s+/);
                        return {
                            name: parts[0],
                            id: parts[1],
                            size: parts[2],
                            modified: parts.slice(3).join(' ')
                        };
                    });
                
                console.log(`[BigDaddyG-Loader] 📦 Found ${this.availableModels.length} models`);
                resolve();
            });
        });
    }
    
    async loadModel(modelName) {
        console.log(`[BigDaddyG-Loader] 📥 Loading model: ${modelName}...`);
        
        // Check if model exists
        const modelExists = this.availableModels.some(m => m.name === modelName);
        
        if (!modelExists) {
            console.log(`[BigDaddyG-Loader] ❌ Model not found: ${modelName}`);
            return false;
        }
        
        // Get model info from manifest
        const manifestPath = path.join(
            this.config.model_path,
            'manifests',
            'registry.ollama.ai',
            'library',
            modelName.split(':')[0],
            modelName.split(':')[1] || 'latest'
        );
        
        try {
            if (fs.existsSync(manifestPath)) {
                const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
                this.modelInfo = manifest;
                
                // Extract model size
                const modelLayer = manifest.layers.find(l => l.mediaType === 'application/vnd.ollama.image.model');
                const sizeGB = modelLayer ? (modelLayer.size / 1024 / 1024 / 1024).toFixed(2) : 'Unknown';
                
                console.log(`[BigDaddyG-Loader] ✅ Model loaded: ${modelName} (${sizeGB} GB)`);
                this.loadedModel = modelName;
                return true;
            }
        } catch (error) {
            console.log(`[BigDaddyG-Loader] ⚠️ Could not read manifest: ${error.message}`);
        }
        
        // Fallback: model exists in ollama list, so it's available
        this.loadedModel = modelName;
        console.log(`[BigDaddyG-Loader] ✅ Model available: ${modelName}`);
        return true;
    }
    
    async loadFallbackModel() {
        for (const fallback of this.config.fallback_models) {
            console.log(`[BigDaddyG-Loader] 🔄 Trying fallback: ${fallback}...`);
            const loaded = await this.loadModel(fallback);
            if (loaded) {
                console.log(`[BigDaddyG-Loader] ✅ Using fallback model: ${fallback}`);
                return true;
            }
        }
        
        console.log('[BigDaddyG-Loader] ❌ No models available');
        return false;
    }
    
    async query(prompt, options = {}) {
        const modelName = options.model || this.loadedModel || this.config.default_model;
        const emotionalState = options.emotional_state || 'CALM';
        
        console.log(`[BigDaddyG-Loader] 🤖 Querying ${modelName} with emotional context: ${emotionalState}`);
        
        // Build the enhanced prompt with emotional context
        const systemPrompt = `[Emotional State: ${emotionalState}] You are BigDaddyG, a specialized AI assistant for development.`;
        const enhancedPrompt = `${systemPrompt}\n\n${prompt}`;
        
        return new Promise((resolve, reject) => {
            const postData = JSON.stringify({
                model: modelName,
                prompt: enhancedPrompt,
                stream: false
            });
            
            const options = {
                hostname: 'localhost',
                port: 11434,
                path: '/api/generate',
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Content-Length': Buffer.byteLength(postData)
                }
            };
            
            const req = http.request(options, (res) => {
                let body = '';
                res.on('data', chunk => body += chunk);
                res.on('end', () => {
                    try {
                        const data = JSON.parse(body);
                        resolve({
                            model: modelName,
                            content: data.response,
                            emotional_state: emotionalState,
                            done: data.done,
                            context: data.context
                        });
                    } catch (error) {
                        reject(error);
                    }
                });
            });
            
            req.on('error', reject);
            req.write(postData);
            req.end();
        });
    }
    
    listModels() {
        return this.availableModels;
    }
    
    getLoadedModel() {
        return this.loadedModel;
    }
    
    getModelInfo() {
        return this.modelInfo;
    }
    
    async downloadModel(modelName) {
        console.log(`[BigDaddyG-Loader] 📥 Downloading model: ${modelName}...`);
        
        return new Promise((resolve, reject) => {
            const process = exec(`ollama pull ${modelName}`, (error, stdout, stderr) => {
                if (error) {
                    console.log(`[BigDaddyG-Loader] ❌ Download failed: ${error.message}`);
                    reject(error);
                    return;
                }
                
                console.log(`[BigDaddyG-Loader] ✅ Model downloaded: ${modelName}`);
                resolve(stdout);
            });
            
            // Stream output
            process.stdout.on('data', (data) => {
                console.log('[BigDaddyG-Loader]', data.toString().trim());
            });
        });
    }
    
    async deleteModel(modelName) {
        console.log(`[BigDaddyG-Loader] 🗑️ Deleting model: ${modelName}...`);
        
        return new Promise((resolve, reject) => {
            exec(`ollama rm ${modelName}`, (error, stdout, stderr) => {
                if (error) {
                    console.log(`[BigDaddyG-Loader] ❌ Delete failed: ${error.message}`);
                    reject(error);
                    return;
                }
                
                console.log(`[BigDaddyG-Loader] ✅ Model deleted: ${modelName}`);
                this.availableModels = this.availableModels.filter(m => m.name !== modelName);
                resolve(stdout);
            });
        });
    }
    
    printStatus() {
        console.log('\n[BigDaddyG-Loader] 📊 Model Loader Status');
        console.log('='.repeat(50));
        console.log(`Loaded Model: ${this.loadedModel || 'None'}`);
        console.log(`Available Models: ${this.availableModels.length}`);
        console.log('\nYour Models:');
        this.availableModels.filter(m => 
            m.name.includes('bigdaddyg') || 
            m.name.includes('your-custom')
        ).forEach(m => {
            console.log(`  ✅ ${m.name} (${m.size})`);
        });
    }
}

module.exports = BigDaddyGModelLoader;

