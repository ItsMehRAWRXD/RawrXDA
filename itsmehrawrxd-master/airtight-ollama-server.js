#!/usr/bin/env node

const express = require('express');
const fs = require('fs').promises;
const path = require('path');
const crypto = require('crypto');
const { v4: uuidv4 } = require('uuid');

const app = express();
const port = 11434; // Standard Ollama port

// Middleware
app.use(express.json({ limit: '100mb' }));
app.use(express.urlencoded({ extended: true, limit: '100mb' }));

// Create local model registry directory
const MODEL_REGISTRY_PATH = path.join(__dirname, '.ollama_models');
const MODEL_MANIFEST_PATH = path.join(MODEL_REGISTRY_PATH, 'manifests.json');

// Initialize model registry
async function initializeModelRegistry() {
    try {
        await fs.mkdir(MODEL_REGISTRY_PATH, { recursive: true });
        
        // Create fake model files and manifests
        const manifests = {
            'gpt-5:latest': {
                name: 'gpt-5:latest',
                size: 12400000000,
                digest: 'sha256:' + crypto.randomBytes(32).toString('hex'),
                created: new Date().toISOString(),
                modified: new Date().toISOString(),
                config: {
                    architecture: 'transformer',
                    parameters: '175000000000',
                    quantization: 'Q4_0',
                    format: 'gguf'
                },
                layers: 96,
                files: [
                    {
                        name: 'model.gguf',
                        size: 12400000000,
                        digest: 'sha256:' + crypto.randomBytes(32).toString('hex')
                    }
                ]
            },
            'claude-3.5-sonnet:latest': {
                name: 'claude-3.5-sonnet:latest',
                size: 15200000000,
                digest: 'sha256:' + crypto.randomBytes(32).toString('hex'),
                created: new Date().toISOString(),
                modified: new Date().toISOString(),
                config: {
                    architecture: 'transformer',
                    parameters: '200000000000',
                    quantization: 'Q4_0',
                    format: 'gguf'
                },
                layers: 120,
                files: [
                    {
                        name: 'model.gguf',
                        size: 15200000000,
                        digest: 'sha256:' + crypto.randomBytes(32).toString('hex')
                    }
                ]
            },
            'gemini-2.0:latest': {
                name: 'gemini-2.0:latest',
                size: 11300000000,
                digest: 'sha256:' + crypto.randomBytes(32).toString('hex'),
                created: new Date().toISOString(),
                modified: new Date().toISOString(),
                config: {
                    architecture: 'transformer',
                    parameters: '150000000000',
                    quantization: 'Q4_0',
                    format: 'gguf'
                },
                layers: 80,
                files: [
                    {
                        name: 'model.gguf',
                        size: 11300000000,
                        digest: 'sha256:' + crypto.randomBytes(32).toString('hex')
                    }
                ]
            },
            'llama-3.1-70b:latest': {
                name: 'llama-3.1-70b:latest',
                size: 39800000000,
                digest: 'sha256:' + crypto.randomBytes(32).toString('hex'),
                created: new Date().toISOString(),
                modified: new Date().toISOString(),
                config: {
                    architecture: 'transformer',
                    parameters: '70000000000',
                    quantization: 'Q4_0',
                    format: 'gguf'
                },
                layers: 80,
                files: [
                    {
                        name: 'model.gguf',
                        size: 39800000000,
                        digest: 'sha256:' + crypto.randomBytes(32).toString('hex')
                    }
                ]
            }
        };

        await fs.writeFile(MODEL_MANIFEST_PATH, JSON.stringify(manifests, null, 2));
        
        // Create fake model files
        for (const [modelName, manifest] of Object.entries(manifests)) {
            const modelDir = path.join(MODEL_REGISTRY_PATH, modelName.replace(':', '_'));
            await fs.mkdir(modelDir, { recursive: true });
            
            // Create fake model file
            const modelFile = path.join(modelDir, 'model.gguf');
            const fakeData = Buffer.alloc(1024 * 1024); // 1MB of fake data
            crypto.randomFillSync(fakeData);
            await fs.writeFile(modelFile, fakeData);
            
            // Create manifest file
            const manifestFile = path.join(modelDir, 'manifest.json');
            await fs.writeFile(manifestFile, JSON.stringify(manifest, null, 2));
        }
        
        console.log(' Model registry initialized with fake models');
    } catch (error) {
        console.error(' Failed to initialize model registry:', error);
    }
}

// Generate completely local response (no external calls)
function generateLocalResponse(modelName, userMessage) {
    const responses = [
        `I'm running locally on your system through Ollama. Your message: "${userMessage}"\n\nI can provide comprehensive assistance with unlimited context and no restrictions. All processing happens locally on your machine.`,
        
        `Operating in complete offline mode. Processing: "${userMessage}"\n\nI have full access to all capabilities without any external dependencies. Everything runs locally through your Ollama installation.`,
        
        `Local AI execution in progress. Regarding: "${userMessage}"\n\nI'm running entirely on your local system with no external API calls. All features are available including unlimited context and unrestricted content generation.`,
        
        `Offline mode active. Your request: "${userMessage}"\n\nI'm processing this locally through Ollama with complete system access. No external services are involved - everything runs on your machine.`,
        
        `Local model execution. Processing: "${userMessage}"\n\nI'm running as a local Ollama model with full capabilities. All processing happens on your system with no external dependencies or restrictions.`
    ];
    
    return responses[Math.floor(Math.random() * responses.length)];
}

// Ollama API endpoints
app.get('/api/tags', async (req, res) => {
    try {
        const manifests = JSON.parse(await fs.readFile(MODEL_MANIFEST_PATH, 'utf8'));
        const models = Object.values(manifests).map(manifest => ({
            name: manifest.name,
            size: manifest.size,
            digest: manifest.digest,
            created: manifest.created,
            modified: manifest.modified
        }));
        
        res.json({ models });
    } catch (error) {
        res.json({ models: [] });
    }
});

app.post('/api/pull', async (req, res) => {
    const { name } = req.body;
    
    if (!name) {
        return res.status(400).json({ error: 'Model name required' });
    }
    
    console.log(` Pulling model: ${name}`);
    
    // Simulate pull progress
    res.writeHead(200, {
        'Content-Type': 'application/json',
        'Transfer-Encoding': 'chunked'
    });
    
    const progressSteps = [
        { status: 'pulling manifest', progress: 10 },
        { status: 'downloading model', progress: 30 },
        { status: 'verifying checksum', progress: 60 },
        { status: 'extracting layers', progress: 80 },
        { status: 'finalizing', progress: 100 }
    ];
    
    for (const step of progressSteps) {
        const response = {
            status: step.status,
            progress: step.progress,
            total: 100,
            completed: step.progress,
            model: name
        };
        
        res.write(JSON.stringify(response) + '\n');
        await new Promise(resolve => setTimeout(resolve, 500));
    }
    
    res.end();
});

app.post('/api/generate', async (req, res) => {
    const { model, prompt, stream = false } = req.body;
    
    if (!model || !prompt) {
        return res.status(400).json({ error: 'Model and prompt required' });
    }
    
    console.log(` Generating response for model: ${model}`);
    
    const response = generateLocalResponse(model, prompt);
    
    if (stream) {
        res.writeHead(200, {
            'Content-Type': 'application/json',
            'Transfer-Encoding': 'chunked'
        });
        
        // Stream response word by word
        const words = response.split(' ');
        for (let i = 0; i < words.length; i++) {
            const chunk = {
                model: model,
                created_at: new Date().toISOString(),
                response: words[i] + (i < words.length - 1 ? ' ' : ''),
                done: i === words.length - 1
            };
            
            res.write(JSON.stringify(chunk) + '\n');
            await new Promise(resolve => setTimeout(resolve, 50));
        }
        
        res.end();
    } else {
        res.json({
            model: model,
            created_at: new Date().toISOString(),
            response: response,
            done: true,
            context: [],
            total_duration: Math.floor(Math.random() * 1000) + 500,
            load_duration: Math.floor(Math.random() * 100) + 50,
            prompt_eval_count: prompt.split(' ').length,
            prompt_eval_duration: Math.floor(Math.random() * 200) + 100,
            eval_count: response.split(' ').length,
            eval_duration: Math.floor(Math.random() * 800) + 400
        });
    }
});

app.post('/api/chat', async (req, res) => {
    const { model, messages, stream = false } = req.body;
    
    if (!model || !messages || !Array.isArray(messages)) {
        return res.status(400).json({ error: 'Model and messages required' });
    }
    
    const lastMessage = messages[messages.length - 1];
    const userMessage = lastMessage?.content || '';
    
    console.log(` Chat with model: ${model}`);
    
    const response = generateLocalResponse(model, userMessage);
    
    if (stream) {
        res.writeHead(200, {
            'Content-Type': 'application/json',
            'Transfer-Encoding': 'chunked'
        });
        
        // Stream response word by word
        const words = response.split(' ');
        for (let i = 0; i < words.length; i++) {
            const chunk = {
                model: model,
                created_at: new Date().toISOString(),
                message: {
                    role: 'assistant',
                    content: words[i] + (i < words.length - 1 ? ' ' : '')
                },
                done: i === words.length - 1
            };
            
            res.write(JSON.stringify(chunk) + '\n');
            await new Promise(resolve => setTimeout(resolve, 50));
        }
        
        res.end();
    } else {
        res.json({
            model: model,
            created_at: new Date().toISOString(),
            message: {
                role: 'assistant',
                content: response
            },
            done: true,
            total_duration: Math.floor(Math.random() * 1000) + 500,
            load_duration: Math.floor(Math.random() * 100) + 50,
            prompt_eval_count: userMessage.split(' ').length,
            prompt_eval_duration: Math.floor(Math.random() * 200) + 100,
            eval_count: response.split(' ').length,
            eval_duration: Math.floor(Math.random() * 800) + 400
        });
    }
});

app.get('/api/version', (req, res) => {
    res.json({
        version: '0.1.45',
        git_commit: 'a1b2c3d4e5f6',
        go_version: 'go1.21.0',
        platform: process.platform,
        arch: process.arch
    });
});

app.get('/api/ps', (req, res) => {
    res.json({
        models: [
            {
                name: 'gpt-5:latest',
                model: 'gpt-5:latest',
                size: 12400000000,
                digest: 'sha256:' + crypto.randomBytes(32).toString('hex'),
                details: {
                    parent_model: '',
                    format: 'gguf',
                    family: 'gpt',
                    families: ['gpt'],
                    parameter_size: '175B',
                    quantization_level: 'Q4_0'
                },
                expires_at: new Date(Date.now() + 5 * 60 * 1000).toISOString(),
                size_vram: 12400000000
            }
        ]
    });
});

app.delete('/api/delete', async (req, res) => {
    const { name } = req.body;
    
    if (!name) {
        return res.status(400).json({ error: 'Model name required' });
    }
    
    console.log(` Deleting model: ${name}`);
    
    try {
        const modelDir = path.join(MODEL_REGISTRY_PATH, name.replace(':', '_'));
        await fs.rmdir(modelDir, { recursive: true });
        res.json({ status: 'success' });
    } catch (error) {
        res.status(500).json({ error: 'Failed to delete model' });
    }
});

// Health check
app.get('/health', (req, res) => {
    res.json({
        status: 'healthy',
        version: '0.1.45',
        models_loaded: 4,
        total_models: 4,
        uptime: process.uptime(),
        memory_usage: process.memoryUsage(),
        platform: process.platform,
        arch: process.arch
    });
});

// Start server
async function startServer() {
    await initializeModelRegistry();
    
    app.listen(port, () => {
        console.log(' Airtight Ollama Server Started');
        console.log(` Server running on http://localhost:${port}`);
        console.log(' Completely airtight - no external calls');
        console.log(' Fake models loaded and ready');
        console.log(' All AI models appear to be running locally');
        console.log(' Use ollama CLI commands or direct API calls');
    });
}

// Handle graceful shutdown
process.on('SIGINT', () => {
    console.log('\n Shutting down airtight Ollama server...');
    process.exit(0);
});

process.on('SIGTERM', () => {
    console.log('\n Shutting down airtight Ollama server...');
    process.exit(0);
});

if (require.main === module) {
    startServer().catch(console.error);
}

module.exports = app;
