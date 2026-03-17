#!/usr/bin/env node
/**
 * BigDaddyG Model Server for Cursor IDE
 * Serves BigDaddyG assembly and custom models
 */

const express = require('express');
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const cors = require('cors');

const app = express();
const PORT = 11441;

// Middleware
app.use(cors());
app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ extended: true, limit: '50mb' }));

// Model configurations
const modelConfigs = {
    'bigdaddyg-assembly': {
        name: 'BigDaddyG Assembly Model',
        type: 'assembly',
        configPath: path.join(__dirname, '..', 'models', 'BigDaddyG-Assembly-Model.json'),
        executable: path.join(__dirname, '..', 'models', 'your-custom-model', 'bigdaddyg-model-final.exe'),
        description: 'Pure assembly model for reverse engineering and low-level programming'
    },
    'bigdaddyg-ensemble': {
        name: 'BigDaddyG Ensemble Model',
        type: 'ensemble',
        configPath: path.join(__dirname, '..', 'models', 'BigDaddyG-Ensemble-Model.json'),
        executable: path.join(__dirname, '..', 'models', 'your-custom-model', 'bigdaddyg-model.exe'),
        description: 'Ensemble model combining multiple BigDaddyG capabilities'
    },
    'bigdaddyg-pe': {
        name: 'BigDaddyG PE Model',
        type: 'pe',
        configPath: path.join(__dirname, '..', 'models', 'BigDaddyG-PE-Model.json'),
        executable: path.join(__dirname, '..', 'models', 'your-custom-model', 'bigdaddyg-pure-model.exe'),
        description: 'PE (Portable Executable) analysis and generation model'
    },
    'bigdaddyg-reverse': {
        name: 'BigDaddyG Reverse Model',
        type: 'reverse',
        configPath: path.join(__dirname, '..', 'models', 'BigDaddyG-Reverse-Model.json'),
        executable: path.join(__dirname, '..', 'models', 'your-custom-model', 'bigdaddyg-turbo.exe'),
        description: 'Reverse engineering and analysis model'
    },
    'custom-agentic-coder': {
        name: 'Custom Agentic Coder (ASM)',
        type: 'assembly',
        configPath: path.join(__dirname, '..', 'models', 'ollama', 'Modelfile.custom-agentic-coder'),
        executable: path.join(__dirname, '..', 'models', 'custom-agentic-coder.gguf'),
        description: 'Custom agentic assembly coder with advanced reasoning capabilities'
    },
    'your-custom-model': {
        name: 'Your Custom Model',
        type: 'python',
        configPath: path.join(__dirname, '..', 'models', 'your-custom-model', 'model-config.json'),
        executable: path.join(__dirname, '..', 'models', 'your-custom-model', 'generate.py'),
        description: 'Custom model trained on your codebase'
    }
};

// Health check endpoint
app.get('/health', (req, res) => {
    const availableModels = Object.keys(modelConfigs).filter(modelId => {
        const config = modelConfigs[modelId];
        return fs.existsSync(config.configPath) && fs.existsSync(config.executable);
    });
    
    res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        availableModels: availableModels.length,
        models: availableModels
    });
});

// List available models
app.get('/models', (req, res) => {
    const models = Object.entries(modelConfigs).map(([id, config]) => ({
        id: id,
        name: config.name,
        description: config.description,
        type: config.type,
        available: fs.existsSync(config.configPath) && fs.existsSync(config.executable)
    }));
    
    res.json({ models });
});

// Load model configuration
function loadModelConfig(modelId) {
    const config = modelConfigs[modelId];
    if (!config) {
        throw new Error(`Model ${modelId} not found`);
    }
    
    if (!fs.existsSync(config.configPath)) {
        throw new Error(`Model config not found: ${config.configPath}`);
    }
    
    return JSON.parse(fs.readFileSync(config.configPath, 'utf8'));
}

// Execute model
function executeModel(modelId, prompt, options = {}) {
    return new Promise((resolve, reject) => {
        const config = modelConfigs[modelId];
        if (!config) {
            reject(new Error(`Model ${modelId} not found`));
            return;
        }
        
        if (!fs.existsSync(config.executable)) {
            reject(new Error(`Model executable not found: ${config.executable}`));
            return;
        }
        
        const requestData = {
            prompt: prompt,
            model: modelId,
            options: options,
            timestamp: new Date().toISOString()
        };
        
        let executable, args;
        
        if (config.type === 'python') {
            executable = 'python';
            args = [config.executable];
        } else {
            executable = config.executable;
            args = [];
        }
        
        const process = spawn(executable, args, {
            stdio: ['pipe', 'pipe', 'pipe'],
            cwd: path.dirname(config.executable)
        });
        
        let output = '';
        let error = '';
        
        process.stdout.on('data', (data) => {
            output += data.toString();
        });
        
        process.stderr.on('data', (data) => {
            error += data.toString();
        });
        
        process.on('close', (code) => {
            if (code === 0) {
                resolve({
                    response: output.trim(),
                    model: modelId,
                    timestamp: new Date().toISOString()
                });
            } else {
                reject(new Error(`Model execution failed with code ${code}: ${error}`));
            }
        });
        
        process.on('error', (err) => {
            reject(new Error(`Failed to start model: ${err.message}`));
        });
        
        // Send request data to stdin
        process.stdin.write(JSON.stringify(requestData));
        process.stdin.end();
    });
}

// Chat completion endpoint
app.post('/v1/chat/completions', async (req, res) => {
    try {
        const { messages, model = 'your-custom-model', temperature = 0.7, max_tokens = 1000 } = req.body;
        
        if (!messages || !Array.isArray(messages)) {
            return res.status(400).json({ error: 'Messages array is required' });
        }
        
        // Convert messages to prompt
        const prompt = messages.map(msg => `${msg.role}: ${msg.content}`).join('\n');
        
        const response = await executeModel(model, prompt, { temperature, max_tokens });
        
        const cursorResponse = {
            id: `chatcmpl-${Date.now()}`,
            object: 'chat.completion',
            created: Math.floor(Date.now() / 1000),
            model: model,
            choices: [{
                index: 0,
                message: {
                    role: 'assistant',
                    content: response.response
                },
                finish_reason: 'stop'
            }],
            usage: {
                prompt_tokens: Math.ceil(prompt.length / 4),
                completion_tokens: Math.ceil(response.response.length / 4),
                total_tokens: Math.ceil((prompt.length + response.response.length) / 4)
            }
        };
        
        res.json(cursorResponse);
    } catch (error) {
        console.error('Error in chat completion:', error.message);
        res.status(500).json({
            error: 'Failed to generate completion',
            details: error.message
        });
    }
});

// Text completion endpoint
app.post('/v1/completions', async (req, res) => {
    try {
        const { prompt, model = 'your-custom-model', temperature = 0.7, max_tokens = 1000 } = req.body;
        
        if (!prompt) {
            return res.status(400).json({ error: 'Prompt is required' });
        }
        
        const response = await executeModel(model, prompt, { temperature, max_tokens });
        
        const cursorResponse = {
            id: `cmpl-${Date.now()}`,
            object: 'text_completion',
            created: Math.floor(Date.now() / 1000),
            model: model,
            choices: [{
                text: response.response,
                index: 0,
                finish_reason: 'stop'
            }],
            usage: {
                prompt_tokens: Math.ceil(prompt.length / 4),
                completion_tokens: Math.ceil(response.response.length / 4),
                total_tokens: Math.ceil((prompt.length + response.response.length) / 4)
            }
        };
        
        res.json(cursorResponse);
    } catch (error) {
        console.error('Error in text completion:', error.message);
        res.status(500).json({
            error: 'Failed to generate completion',
            details: error.message
        });
    }
});

// Model info endpoint
app.get('/v1/models', (req, res) => {
    const models = Object.entries(modelConfigs).map(([id, config]) => ({
        id: id,
        object: 'model',
        created: Math.floor(Date.now() / 1000),
        owned_by: 'bigdaddyg',
        name: config.name,
        description: config.description,
        type: config.type
    }));
    
    res.json({ object: 'list', data: models });
});

// Start server
app.listen(PORT, () => {
    console.log(`[SUCCESS] BigDaddyG Model Server running on port ${PORT}`);
    console.log(`[INFO] Available models:`);
    Object.entries(modelConfigs).forEach(([id, config]) => {
        const available = fs.existsSync(config.configPath) && fs.existsSync(config.executable);
        console.log(`[INFO]   - ${id}: ${available ? '[AVAILABLE]' : '[NOT FOUND]'} ${config.name}`);
    });
});

// Graceful shutdown
process.on('SIGINT', () => {
    console.log('\n[INFO] Shutting down BigDaddyG Model Server...');
    process.exit(0);
});

process.on('SIGTERM', () => {
    console.log('\n[INFO] Received SIGTERM, shutting down gracefully...');
    process.exit(0);
});
